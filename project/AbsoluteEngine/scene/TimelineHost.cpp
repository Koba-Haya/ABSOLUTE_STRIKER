#include "TimelineHost.h"

#include <filesystem>

#include "../base/AssetPath.h"
#include "timeline/TimelineEditorWindow.h"
#include "timeline/TimelineManager.h"

namespace AbsoluteEngine {

TimelineHost::TimelineHost()
    : manager_(std::make_unique<TimelineManager>()), editorWindow_(std::make_unique<TimelineEditorWindow>()) {
  editorWindow_->SetManager(manager_.get());
}

TimelineHost::~TimelineHost() = default;

void TimelineHost::SetCamera(ITimelineCamera *camera) { manager_->SetCamera(camera); }

void TimelineHost::SetSpawnAheadDistance(float distance) { manager_->SetSpawnAheadDistance(distance); }

void TimelineHost::SetHoldAtEnd(bool hold) { manager_->SetHoldAtEnd(hold); }

void TimelineHost::Update(float deltaTime) { manager_->Update(deltaTime); }

void TimelineHost::Play() { manager_->Play(); }

void TimelineHost::Pause() { manager_->Pause(); }

void TimelineHost::Stop() { manager_->Stop(); }

bool TimelineHost::IsPlaying() const { return manager_->GetPlayState() == TimelinePlayState::Playing; }

bool TimelineHost::HasReachedEnd() const { return manager_->HasReachedEnd(); }

void TimelineHost::SeekToEnd() { manager_->Seek(manager_->GetDuration()); }

void TimelineHost::Load(const std::string &filePath) {
  // タイムラインデータを読み込む(エディタビルドでは、ファイルが無い場合は空の状態を維持)
  if (std::filesystem::exists(filePath)) {
    manager_->LoadFromFile(filePath);
  }
#ifndef USE_IMGUI
  else {
    // 配布ビルドで無いと、敵が一切出ないまま進行してしまう。探した場所を表示して止める
    AssetPath::ReportMissingAndExit("timeline file", filePath);
  }
#endif
  // 読み込み直後に必ず Stop() を呼び、シーン上のカメラ等を
  // タイムラインの 0.0s の状態に強制スナップする(タスクB)
  manager_->Stop();
}

void TimelineHost::Save(const std::string &filePath) const {
#ifdef USE_IMGUI
  // タイムラインデータを JSON ファイルに保存する(保存はエディタビルドのみ)
  std::filesystem::create_directories(std::filesystem::path(filePath).parent_path().string());
  manager_->SaveToFile(filePath);
#else
  (void)filePath;
#endif
}

void TimelineHost::DrawEditorWindow(const std::string &timelineFilePath, bool *debugCameraFlag,
                                    std::function<void()> onModified) {
  // Debug/Game カメラ切替フラグを接続する(nullptr ならトグルボタンは出ない)
  editorWindow_->SetDebugCameraFlag(debugCameraFlag);
  // トラック/イベントの追加・削除等を、既存のオートセーブ経路に接続する
  editorWindow_->SetOnModifiedCallback(std::move(onModified));
  editorWindow_->Draw(timelineFilePath);
}

void TimelineHost::UpdatePreview(const TimelineSpawnPreview::AddObjectFunc &addObject,
                                 const TimelineSpawnPreview::SelectObjectFunc &selectObject) {
  preview_.Update(*manager_, addObject, selectObject);
}

} // namespace AbsoluteEngine
