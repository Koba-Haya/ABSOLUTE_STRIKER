// ============================================================
// TimelineEditorWindow.cpp
// ImGuiベースのタイムラインエディタUI実装
// ============================================================
#include "TimelineEditorWindow.h"
#include "PrefabRegistry.h"
#include "TimelineEventFactory.h"
#include "../../base/EnginePath.h"
// CommandManagerとTimelineCommandのインクルード（Undo/Redo: タスクF）
#include "../../editor/CommandManager.h"
#include <algorithm>
#include <iostream>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace AbsoluteEngine {

void TimelineEditorWindow::Draw(const std::string& timelineFilePath) {
#ifdef USE_IMGUI
    if (!manager_) return;

    ImGui::Begin("Timeline Editor");

    DrawToolbar(timelineFilePath);
    // トップレベル（トラックのPushIDに包まれていない）のAddEventPopupを処理する
    // DrawTracks内の各トラックの"+"ボタンが開くポップアップとはIDスタックが異なるため独立して開閉できる
    DrawAddEventPopup();
    ImGui::Separator();
    DrawSeekBar();
    ImGui::Separator();
    DrawTracks();

    ImGui::End();
#endif
}

#ifdef USE_IMGUI

unsigned int TimelineEditorWindow::GetTrackColor(size_t trackIndex) {
    static const ImU32 kPalette[] = {
        IM_COL32(29, 158, 117, 230),  // teal
        IM_COL32(127, 119, 221, 230), // purple
        IM_COL32(216, 90, 48, 230),   // coral
        IM_COL32(212, 83, 126, 230),  // pink
        IM_COL32(186, 117, 23, 230),  // amber
        IM_COL32(55, 138, 221, 230),  // blue
    };
    constexpr size_t kPaletteSize = sizeof(kPalette) / sizeof(kPalette[0]);
    return kPalette[trackIndex % kPaletteSize];
}

void TimelineEditorWindow::DrawToolbar(const std::string& timelineFilePath) {
    (void)timelineFilePath; // Save/Loadボタン廃止（オートセーブに統一）によりファイルパスは直接使わない

    const auto state = manager_->GetPlayState();

    // --- 1行目: 再生コントロール + Debug Cameraトグル ---
    // ここでのPlay/Pause/StopはPlayModeを触らずタイムラインだけを動かす単体プレビュー用
    // （ゲーム全体を再生する場合はBaseSceneのメインToolbarのPlayボタンを使う。そちらは内部で
    //   TimelineManager::Play/Stopも連動して呼んでいる）
    if (state != TimelinePlayState::Playing) {
        if (ImGui::Button("Play")) {
            manager_->Play();
        }
    } else {
        if (ImGui::Button("Pause")) {
            manager_->Pause();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Stop")) {
        manager_->Stop();
    }

    ImGui::SameLine();
    ImGui::Separator();
    ImGui::SameLine();

    // Debug/Game カメラ切替トグル（GameScene側の重複UIを廃止しここに一本化）
    if (debugCameraFlag_) {
        const bool isDebugCamera = *debugCameraFlag_;
        if (isDebugCamera) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 1.0f, 1.0f));
        }
        if (ImGui::Button("Debug Camera (Free Camera)")) {
            *debugCameraFlag_ = !*debugCameraFlag_;
        }
        if (isDebugCamera) {
            ImGui::PopStyleColor();
            ImGui::SameLine();
            ImGui::TextDisabled("(Free Camera Mode)");
        }
    }

    // --- 2行目: プレハブ関連操作 ---
    // Save/Loadは既存のオートセーブ経路（SetSceneModified）に統一したため廃止
    // Reload Prefabsは、エンジン外からプレハブJSONを直接追加・編集した場合の手動リカバリ用に残す
    // （通常のフローでは EditorUIManager の「Save as Prefab」実行時に自動でスキャンされる）
    if (ImGui::Button("Reload Prefabs")) {
        const std::string prefabDir = EnginePath::Resolve("resources/prefabs");
        PrefabRegistry::GetInstance().ScanDirectory(prefabDir);
    }

    ImGui::SameLine();

    // トラックの存在を意識せずワンクリックでイベント追加できるようにする
    // （トラックが1つも無いとDrawTracks側の per-track "+" ボタンが出てこず、機能が見つけにくかったため）
    // 追加できるイベント種別はTimelineEventFactoryに登録されているもの全て（ポップアップ内で選択）
    if (ImGui::Button("+ Add Event")) {
        if (manager_->GetTracks().empty()) {
            // トラックが無ければデフォルトトラックを自動作成する（Undo対応）
            const std::string beforeSnapshot = manager_->SerializeToString();
            auto track = std::make_unique<TimelineTrack>();
            track->SetName("Main");
            manager_->AddTrack(std::move(track));
            if (commandManager_) {
                const std::string afterSnapshot = manager_->SerializeToString();
                auto cmd = std::make_shared<TimelineCommand>(
                    [mgr = manager_, beforeSnapshot]() { mgr->LoadFromString(beforeSnapshot); },
                    [mgr = manager_, afterSnapshot]() { mgr->LoadFromString(afterSnapshot); }
                );
                commandManager_->AddCommand(cmd);
            }
            if (onModified_) onModified_();
        }
        addEventTargetTrackIndex_ = static_cast<int>(manager_->GetTracks().size()) - 1;
        addEventTime_ = manager_->GetCurrentTime();
        selectedEventTypeIndex_ = -1; // DrawAddEventPopup側で既定種別を選び直させる
        pendingEvent_.reset();
        ImGui::OpenPopup("AddEventPopup");
    }
}

void TimelineEditorWindow::DrawSeekBar() {
    const float duration = manager_->GetDuration();
    float currentTime = manager_->GetCurrentTime();

    ImGui::Text("Time: %.2f / %.2f s", currentTime, duration);

    // ラベル列ぶんインデントし、トラックレーンと同じX位置・同じ幅でルーラーを描画する
    // （以前はシークバーが全幅、トラックレーンが固定幅で座標系が食い違っていた不具合の修正）
    ImGui::Dummy(ImVec2(kTrackLabelWidth, 1.0f));
    ImGui::SameLine(0.0f, 0.0f);

    const float laneWidth = ImGui::GetContentRegionAvail().x;
    const ImVec2 rulerMin = ImGui::GetCursorScreenPos();
    const float rulerHeight = 20.0f;
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(rulerMin, { rulerMin.x + laneWidth, rulerMin.y + rulerHeight }, IM_COL32(40, 40, 40, 200));

    // 目盛り（5分割）
    constexpr int kTickCount = 5;
    for (int i = 0; i <= kTickCount; ++i) {
        const float tickTime = duration * static_cast<float>(i) / static_cast<float>(kTickCount);
        const float x = rulerMin.x + laneWidth * static_cast<float>(i) / static_cast<float>(kTickCount);
        drawList->AddLine({ x, rulerMin.y }, { x, rulerMin.y + rulerHeight }, IM_COL32(120, 120, 120, 255));

        char label[16];
        snprintf(label, sizeof(label), "%.0fs", tickTime);
        drawList->AddText({ x + 2.0f, rulerMin.y + 2.0f }, IM_COL32(200, 200, 200, 255), label);
    }

    // 再生ヘッド（現在時刻の縦線）。トラックレーン側の縦線と同じ計算式で揃える
    if (duration > 0.0f) {
        const float cursorX = rulerMin.x + (currentTime / duration) * laneWidth;
        drawList->AddLine({ cursorX, rulerMin.y }, { cursorX, rulerMin.y + rulerHeight }, IM_COL32(255, 220, 0, 255), 2.0f);
    }

    ImGui::Dummy(ImVec2(laneWidth, rulerHeight));

    // シークスライダー（同じラベル列オフセット・同じ幅でトラックと座標を揃える）
    ImGui::Dummy(ImVec2(kTrackLabelWidth, 1.0f));
    ImGui::SameLine(0.0f, 0.0f);
    ImGui::SetNextItemWidth(laneWidth);
    if (ImGui::SliderFloat("##SeekBar", &currentTime, 0.0f, duration, "%.2f s")) {
        // スライダーが動いたらシークを実行する
        // （ドラッグ中も毎フレーム呼ばれるが、スイープ判定で正しく処理される）
        manager_->Seek(currentTime);
    }
}

void TimelineEditorWindow::DrawTracks() {
    const auto& tracks = manager_->GetTracks();

    if (tracks.empty()) {
        ImGui::TextDisabled("トラックがありません。下の [+ Add Track] で追加してください。");
    } else {
        for (size_t i = 0; i < tracks.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));

            const auto& track = tracks[i]; // 表示のみなので読み取り専用で参照する
            const ImVec2 rowStart = ImGui::GetCursorScreenPos();
            bool trackRemoved = false;

            // --- ラベル列（トラック名 + 削除/イベント追加ボタン） ---
            ImGui::BeginGroup();
            ImGui::TextUnformatted(track->GetName().c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton("x")) {
                // Undo用に変更前のスナップショットを保存する
                const std::string beforeSnapshot = manager_->SerializeToString();
                manager_->RemoveTrack(i);
                if (commandManager_) {
                    const std::string afterSnapshot = manager_->SerializeToString();
                    auto cmd = std::make_shared<TimelineCommand>(
                        [mgr = manager_, beforeSnapshot]() { mgr->LoadFromString(beforeSnapshot); },
                        [mgr = manager_, afterSnapshot]() { mgr->LoadFromString(afterSnapshot); }
                    );
                    commandManager_->AddCommand(cmd);
                }
                if (onModified_) onModified_();
                trackRemoved = true;
            }
            if (!trackRemoved) {
                ImGui::SameLine();
                if (ImGui::SmallButton("+")) {
                    addEventTargetTrackIndex_ = static_cast<int>(i);
                    addEventTime_ = manager_->GetCurrentTime(); // 現在位置にデフォルト設定
                    selectedEventTypeIndex_ = -1; // DrawAddEventPopup側で既定種別を選び直させる
                    pendingEvent_.reset();
                    ImGui::OpenPopup("AddEventPopup");
                }
            }
            ImGui::EndGroup();

            if (trackRemoved) {
                // トラックが削除されイテレータが無効になったのでループを抜ける
                ImGui::PopID();
                break;
            }

            // ポップアップはOpenPopupと同じIDスタック（PushID内）で描画する必要がある
            DrawAddEventPopup();

            // --- レーン列（ラベル列の右側、可変幅） ---
            ImGui::SetCursorScreenPos({ rowStart.x + kTrackLabelWidth, rowStart.y });
            const float laneWidth = ImGui::GetContentRegionAvail().x;
            DrawTrackLane(i, laneWidth);

            ImGui::PopID();
        }
    }

    // --- トラック追加（リスト最下部の全幅ボタン→ポップアップで名前入力） ---
    ImGui::Separator();
    if (ImGui::Button("+ Add Track", ImVec2(-FLT_MIN, 0.0f))) {
        ImGui::OpenPopup("AddTrackPopup");
    }
    if (ImGui::BeginPopup("AddTrackPopup")) {
        ImGui::Text("新規トラックを追加");
        ImGui::Separator();
        ImGui::InputText("トラック名", newTrackName_, sizeof(newTrackName_));
        ImGui::Separator();
        if (ImGui::Button("追加")) {
            // Undo用に変更前のスナップショットを保存する
            const std::string beforeSnapshot = manager_->SerializeToString();
            auto track = std::make_unique<TimelineTrack>();
            track->SetName(newTrackName_);
            manager_->AddTrack(std::move(track));
            // 変更後のスナップショットを保存し、Undo/Redoコマンドを発行する
            if (commandManager_) {
                const std::string afterSnapshot = manager_->SerializeToString();
                auto cmd = std::make_shared<TimelineCommand>(
                    [mgr = manager_, beforeSnapshot]() { mgr->LoadFromString(beforeSnapshot); },
                    [mgr = manager_, afterSnapshot]() { mgr->LoadFromString(afterSnapshot); }
                );
                commandManager_->AddCommand(cmd);
            }
            if (onModified_) onModified_();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("キャンセル")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void TimelineEditorWindow::DrawTrackLane(size_t trackIndex, float laneWidth) {
    TimelineTrack* track = manager_->GetTrack(trackIndex);
    if (!track) return;

    const float duration = manager_->GetDuration();
    const float pixelsPerSecond = (duration > 0.0f) ? (laneWidth / duration) : 0.0f;

    // トラックの背景バー
    const ImVec2 laneStart = ImGui::GetCursorScreenPos();
    const ImVec2 laneBgMin = { laneStart.x, laneStart.y };
    const ImVec2 laneBgMax = { laneStart.x + laneWidth, laneStart.y + kTrackHeight };

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(laneBgMin, laneBgMax, IM_COL32(50, 50, 50, 200));
    drawList->AddRect(laneBgMin, laneBgMax, IM_COL32(100, 100, 100, 255));

    // 現在時刻のカーソルライン（ルーラー側と同じ計算式で揃える）
    const float currentTime = manager_->GetCurrentTime();
    const float cursorX = laneBgMin.x + currentTime * pixelsPerSecond;
    drawList->AddLine({ cursorX, laneBgMin.y }, { cursorX, laneBgMax.y }, IM_COL32(255, 220, 0, 255), 2.0f);

    const size_t eventCount = track->GetEventCount();
    const ImU32 trackColor = GetTrackColor(trackIndex);

    // --- 同時刻（近接）に複数イベントが重なる場合の検出 ---
    // 発火時刻順に並べ、ピクセル距離が閾値未満のものを1つのクラスタとしてまとめ、
    // クラスタ内では横方向に少しずつオフセットして完全な重なり・クリック不能化を防ぐ
    // （トラックのイベントは発火時刻の昇順に並んでいるため、インデックス順がそのまま時刻順になる）
    std::vector<size_t> order(eventCount);
    for (size_t i = 0; i < eventCount; ++i) order[i] = i;

    constexpr float kClusterPixelThreshold = 16.0f; // これ未満の間隔は「重なっている」とみなす
    constexpr float kClusterSpacing = 14.0f;         // 重なり回避のための横オフセット幅

    std::vector<std::vector<size_t>> clusters;
    {
        std::vector<size_t> current;
        float lastX = -1e9f;
        for (size_t idx : order) {
            const float x = track->GetTriggerTime(idx) * pixelsPerSecond;
            if (!current.empty() && (x - lastX) < kClusterPixelThreshold) {
                current.push_back(idx);
            } else {
                if (!current.empty()) clusters.push_back(current);
                current = { idx };
            }
            lastX = x;
        }
        if (!current.empty()) clusters.push_back(current);
    }

    std::vector<float> offsetX(eventCount, 0.0f);
    for (const auto& cluster : clusters) {
        if (cluster.size() <= 1) continue;
        for (size_t k = 0; k < cluster.size(); ++k) {
            offsetX[cluster[k]] = (static_cast<float>(k) - (static_cast<float>(cluster.size()) - 1.0f) * 0.5f) * kClusterSpacing;
        }
    }

    // 各イベントをひし形（Diamond）でレーンに描画する（タスク13）
    size_t removeIndex = SIZE_MAX;
    const float kDiamondHalfW = 9.0f;  // ひし形の半幅（水平方向）
    const float kDiamondHalfH = 10.0f; // ひし形の半高（垂直方向）

    for (size_t j = 0; j < eventCount; ++j) {
        ITimelineEvent* event = track->GetEvent(j);
        const float triggerTime = track->GetTriggerTime(j);
        const float baseX = laneBgMin.x + triggerTime * pixelsPerSecond;
        const float eventX = baseX + offsetX[j];
        const float centerY = laneBgMin.y + kTrackHeight * 0.5f;

        // ひし形の4頂点（上・右・下・左）を計算する
        const ImVec2 ptTop   = { eventX,                centerY - kDiamondHalfH };
        const ImVec2 ptRight = { eventX + kDiamondHalfW, centerY };
        const ImVec2 ptBot   = { eventX,                centerY + kDiamondHalfH };
        const ImVec2 ptLeft  = { eventX - kDiamondHalfW, centerY };

        // 選択状態に応じて色を変える（選択中は常にゴールドで強調、通常時はトラックごとの固定色）
        const bool isSelected = (manager_->GetSelectedEvent() == event);
        const ImU32 fillColor = isSelected
            ? IM_COL32(255, 180, 50, 240)   // 選択中: ゴールド
            : trackColor;                    // 通常: トラックごとの固定パレット色
        const ImU32 outlineColor = isSelected
            ? IM_COL32(255, 120, 0, 255)    // 選択中: オレンジ枠
            : IM_COL32(20, 20, 20, 180);     // 通常: 暗めの枠でどのパレット色でも視認できるようにする

        // ひし形を塗りつぶしと枠線で描画する
        ImVec2 pts[4] = { ptTop, ptRight, ptBot, ptLeft };
        drawList->AddConvexPolyFilled(pts, 4, fillColor);
        drawList->AddPolyline(pts, 4, outlineColor, ImDrawFlags_Closed, 1.5f);

        // イベント名をひし形の右側に表示する（フォントが小さめなので横に出す）
        drawList->AddText({ eventX + kDiamondHalfW + 2.0f, centerY - 6.0f },
            IM_COL32(220, 220, 220, 255), event->GetLabel().c_str());

        // ひし形のAABB領域をImGuiのヒット領域とする（InvisibleButton）
        const ImVec2 evMin = { eventX - kDiamondHalfW, centerY - kDiamondHalfH };
        const ImVec2 evMax = { eventX + kDiamondHalfW, centerY + kDiamondHalfH };
        ImGui::SetCursorScreenPos(evMin);
        ImGui::InvisibleButton(("ev_" + std::to_string(trackIndex) + "_" + std::to_string(j)).c_str(),
            { evMax.x - evMin.x, evMax.y - evMin.y });

        // 左クリックでイベントを選択する（タスク13: selectedEvent_に通知）
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
            manager_->SetSelectedEvent(event);
        }

        // ホバー時にツールチップを表示する。重なりクラスタに属する場合は全メンバーを一覧表示する
        if (ImGui::IsItemHovered()) {
            const auto clusterIt = std::find_if(clusters.begin(), clusters.end(), [&](const std::vector<size_t>& c) {
                return c.size() > 1 && std::find(c.begin(), c.end(), j) != c.end();
            });
            ImGui::BeginTooltip();
            if (clusterIt != clusters.end()) {
                ImGui::Text("%zu events overlapping ~%.2fs", clusterIt->size(), triggerTime);
                ImGui::Separator();
                for (size_t idx : *clusterIt) {
                    ImGui::Text("%.2fs - %s", track->GetTriggerTime(idx), track->GetEvent(idx)->GetLabel().c_str());
                }
            } else {
                ImGui::Text("%.2fs - %s", triggerTime, event->GetLabel().c_str());
            }
            ImGui::EndTooltip();
        }

        // 右クリックで削除コンテキストメニューを表示する
        if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            ImGui::OpenPopup(("EventCtx_" + std::to_string(trackIndex) + "_" + std::to_string(j)).c_str());
        }
        if (ImGui::BeginPopup(("EventCtx_" + std::to_string(trackIndex) + "_" + std::to_string(j)).c_str())) {
            ImGui::Text("%s @ %.2fs", event->GetLabel().c_str(), triggerTime);
            ImGui::Separator();
            if (ImGui::MenuItem("削除")) {
                // 削除前に選択状態をクリアする（ダングリングポインタ防止: タスク14）
                manager_->ClearSelectedEventIfMatch(event);
                removeIndex = j;
            }
            ImGui::EndPopup();
        }
    }

    // 削除が要求されたイベントを処理する（ループ外でイテレータ無効化を防ぐ）
    if (removeIndex != SIZE_MAX) {
        // Undo用に変更前のスナップショットを保存する（トラック削除等と同じパターン）
        const std::string beforeSnapshot = manager_->SerializeToString();
        track->RemoveEvent(removeIndex);
        if (commandManager_) {
            const std::string afterSnapshot = manager_->SerializeToString();
            auto cmd = std::make_shared<TimelineCommand>(
                [mgr = manager_, beforeSnapshot]() { mgr->LoadFromString(beforeSnapshot); },
                [mgr = manager_, afterSnapshot]() { mgr->LoadFromString(afterSnapshot); }
            );
            commandManager_->AddCommand(cmd);
        }
        if (onModified_) onModified_();
    }

    // InvisibleButton でレーン全体を ImGui のヒット領域にする
    ImGui::SetCursorScreenPos(laneBgMin);
    ImGui::InvisibleButton(("lane_" + std::to_string(trackIndex)).c_str(),
        { laneWidth, kTrackHeight });

    ImGui::Dummy({ 0, 4 }); // トラック間のスペース
}


void TimelineEditorWindow::DrawAddEventPopup() {
    if (!ImGui::BeginPopup("AddEventPopup")) return;

    ImGui::Text("新規イベントを追加");
    ImGui::Separator();

    // イベント種別を選択する。選択肢はTimelineEventFactoryに登録されている全種別
    // （エンジン組み込みのSpawnEventも、ゲーム側がRegister()した独自種別も同列に並ぶ）
    const std::vector<std::string> eventTypeNames = TimelineEventFactory::GetInstance().GetRegisteredEventNames();
    if (eventTypeNames.empty()) {
        ImGui::TextColored({ 1.0f, 0.5f, 0.5f, 1.0f }, "登録されているイベント種別がありません。");
        ImGui::EndPopup();
        return;
    }

    if (selectedEventTypeIndex_ < 0 || selectedEventTypeIndex_ >= static_cast<int>(eventTypeNames.size())) {
        // 未初期化(-1)、または前回より種別数が減った場合はSpawnEventを既定選択にする
        // （従来の「+Add Spawn Event」の使用感を踏襲する）
        selectedEventTypeIndex_ = 0;
        for (size_t i = 0; i < eventTypeNames.size(); ++i) {
            if (eventTypeNames[i] == "SpawnEvent") {
                selectedEventTypeIndex_ = static_cast<int>(i);
                break;
            }
        }
    }

    std::vector<const char*> typeCStrs;
    typeCStrs.reserve(eventTypeNames.size());
    for (const auto& name : eventTypeNames) {
        typeCStrs.push_back(name.c_str());
    }

    const int prevTypeIndex = selectedEventTypeIndex_;
    ImGui::Combo("種別", &selectedEventTypeIndex_, typeCStrs.data(), static_cast<int>(typeCStrs.size()));

    // 初回描画時、または種別を選び直した時は新しくインスタンスを生成し直す
    if (!pendingEvent_ || selectedEventTypeIndex_ != prevTypeIndex) {
        pendingEvent_ = TimelineEventFactory::GetInstance().Create(eventTypeNames[static_cast<size_t>(selectedEventTypeIndex_)]);
        if (pendingEvent_) {
            pendingEvent_->SetSpawnAnchorTransform(manager_->ComputeSpawnAnchorTransform(addEventTime_));
        }
    }

    // 発火時刻を変更したら、その時刻のタイムラインカメラ前方アンカーに位置を追従させる
    if (ImGui::InputFloat("発火時刻 (s)", &addEventTime_, 0.1f, 1.0f, "%.2f")) {
        if (pendingEvent_) {
            pendingEvent_->SetSpawnAnchorTransform(manager_->ComputeSpawnAnchorTransform(addEventTime_));
        }
    }
    ImGui::Separator();

    // 型固有の入力フィールドは ITimelineEvent::DrawCreationUI() をポリモーフィックに呼んで描画する
    // （エディタ側はSpawnEvent以外の具象型を一切知らなくてよい）。まだトラックに追加していない
    // 一時インスタンスの編集なのでUndo判定用の出力は捨てる。
    bool dummyActivated = false;
    bool dummyDeactivated = false;
    if (pendingEvent_) {
        pendingEvent_->DrawCreationUI(dummyActivated, dummyDeactivated);
    }
    ImGui::Separator();

    if (ImGui::Button("追加") && pendingEvent_) {
        if (addEventTargetTrackIndex_ >= 0 &&
            addEventTargetTrackIndex_ < static_cast<int>(manager_->GetTracks().size())) {

            // Undo用に変更前のスナップショットを保存する（トラック追加等と同じパターン）
            const std::string beforeSnapshot = manager_->SerializeToString();
            manager_->GetTrack(static_cast<size_t>(addEventTargetTrackIndex_))
                ->AddEvent(std::move(pendingEvent_), addEventTime_);
            if (commandManager_) {
                const std::string afterSnapshot = manager_->SerializeToString();
                auto cmd = std::make_shared<TimelineCommand>(
                    [mgr = manager_, beforeSnapshot]() { mgr->LoadFromString(beforeSnapshot); },
                    [mgr = manager_, afterSnapshot]() { mgr->LoadFromString(afterSnapshot); }
                );
                commandManager_->AddCommand(cmd);
            }
            if (onModified_) onModified_();
        }
        pendingEvent_.reset();
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("キャンセル")) {
        pendingEvent_.reset();
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

#endif // USE_IMGUI

} // namespace AbsoluteEngine
