// ============================================================
// ITimelineEvent.h
// 役割: タイムライン上のすべてのイベントが実装する純粋インターフェース
//       Fire/Rewindパターンによりシーク時の状態不整合を防ぐ
// ============================================================
#pragma once
#include "../../../externals/nlohmann/json.hpp"
#include "../../Type/Transform.h"
#include "TimelineFireContext.h"
#include "RailSpawnPoint.h"
#include <string>

namespace AbsoluteEngine {

// エディタでイベントを選んだときに、シーンに出す目印の中身
struct TimelineEditorHandle {
    std::string prefabId; // 目印として置くプレハブの ID(PrefabRegistry で解決する)
    Transform placement;  // 目印を置く、世界座標の Transform
};

class ITimelineEvent {
public:
    virtual ~ITimelineEvent() = default;

    // 発火時刻（秒）と発火済みフラグは、このイベントを持つ TimelineTrack が管理する
    // （イベントは「何をするか」だけを持ち、いつ発火するかは知らない）

    // -----------------------------------------------------------
    // 必須プロパティ
    // -----------------------------------------------------------

    // エディタ上での表示名（デバッグ・UI用）
    virtual std::string GetLabel() const = 0;

    // -----------------------------------------------------------
    // コアメソッド（Fire/Rewindパターン）
    // -----------------------------------------------------------

    // 発火: タイムラインが発火時刻を前進方向に通過したときに呼ばれる
    // context: 発火時刻と、その時刻のレール上の距離を得るための情報(レール座標の出現位置の変換に使う)
    virtual void Fire(const TimelineFireContext& context) = 0;

    // 巻き戻し: タイムラインが発火時刻を後退方向に通過したとき（シーク時など）に呼ばれる
    // 実装側は Fire() で生成したオブジェクト等を安全に破棄しなければならない
    virtual void Rewind() = 0;

    // -----------------------------------------------------------
    // シリアライズ
    // -----------------------------------------------------------

    // イベントの種別識別子（JSON保存時のタグ）
    virtual std::string GetEventType() const = 0;

    // イベントのデータをJSONに書き出す（発火時刻はTimelineTrack側で管理）
    virtual void Serialize(nlohmann::json& j) const = 0;

    // JSONからイベントのデータを復元する
    virtual void Deserialize(const nlohmann::json& j) = 0;

    // -----------------------------------------------------------
    // エディタインスペクタ用UI（USE_IMGUI無効時は各実装側で空実装にする）
    // -----------------------------------------------------------
    // イベント種別ごとの固有プロパティを描画する。IComponent::DrawInspectorUI()と
    // 同じ考え方で、呼び出し側（エディタ）が具象クラスを知らなくても
    // ポリモーフィックにUIを出せるようにする。
    // activated/deactivatedAfterEdit は、描画した各ウィジェットの直後で
    // ImGui::IsItemActivated() / IsItemDeactivatedAfterEdit() をOR集計して
    // 呼び出し元に伝えるための出力引数（Undo用スナップショットのタイミング判定に使う）。
    virtual void DrawInspectorUI(bool& activated, bool& deactivatedAfterEdit) {}

    // 新規追加ポップアップ内で、型固有の作成時入力（例: SpawnEventのプレハブ選択）を描画する。
    // 既定ではDrawInspectorUI()をそのまま流用する（作成時と編集時で見た目を変える必要が無い
    // 型はこれをオーバーライドしなくてよい）。プレハブ選択のように「配置後は変更不可だが
    // 作成時だけ選ばせたい」ような項目がある型だけオーバーライドする
    virtual void DrawCreationUI(bool& activated, bool& deactivatedAfterEdit) {
        DrawInspectorUI(activated, deactivatedAfterEdit);
    }

    // -----------------------------------------------------------
    // エディタでの配置(シーンの中の置き場所を持つイベント用の、通知と問い合わせ)
    // -----------------------------------------------------------
    // 全てのイベントが意味のある答えを返せる形にしてある(置き場所を使わないイベントは、無視する / 「目印なし」と答える)。
    // 種類ごとの機能(「スポーン系なら」など)を基底に足さないので、効果音・カメラ切り替え・演出などのイベントを足しても、
    // エディタと、この基底は変わらない(新しいクラスで override するだけ)。
    // 例: 敵の出現 = 出現位置と敵のプレハブ、位置のある効果音 = 鳴らす位置、カメラ切り替え = カメラの位置。画面全体の演出 = 使わない

    // エディタで新しく作られたとき(追加のポップアップで種別が選ばれたとき)に、既定の置き場所を伝える。
    // defaultPlacement: カメラの前方の、レール座標の位置(TimelineManager::ComputeSpawnAnchorRail の結果)
    virtual void OnCreatedInEditor(const RailSpawnPoint& defaultPlacement) { (void)defaultPlacement; }

    // エディタでこのイベントを選んだときに、シーンに出す目印(ギズモで動かせる仮のオブジェクト)を問い合わせる。
    // context: 発火時刻のレール位置などを求めるための情報(実際の発火と同じ手順で、置き場所を求めるため)
    // 戻り値: 目印があれば true(outHandle に中身を入れる)。既定は目印なし
    virtual bool GetEditorHandle(const TimelineFireContext& context, TimelineEditorHandle& outHandle) const {
        (void)context;
        (void)outHandle;
        return false;
    }

    // 目印がギズモなどで動かされたときに、その Transform を伝える(イベントが、自分の置き場所の設定へ取り込む)。
    // どの成分を取り込むか(例: レール座標の指定なら、位置は取り込まない)は、イベント自身が決める
    virtual void OnEditorHandleEdited(const Transform& edited) { (void)edited; }
};

} // namespace AbsoluteEngine
