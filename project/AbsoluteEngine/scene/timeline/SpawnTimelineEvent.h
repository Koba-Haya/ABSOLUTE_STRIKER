// ============================================================
// SpawnTimelineEvent.h
// 役割: プレハブをシーンへ出すイベント(スポーン系)の中間クラス。
//       ITimelineEvent(全イベント共通)と、具体的なスポーンイベント(SpawnEvent、FormationSpawnEvent)の間に挟む。
// ============================================================
#pragma once
#include "ITimelineEvent.h"
#include "RailSpawnPoint.h"
#include "../../Type/Transform.h"
#include <string>

namespace AbsoluteEngine {

// スポーン系イベントの中間クラス(スライドの Bird と Penguin の例の「FlyingBird」に当たる)。
//
// 作った理由: 以前は、1 体出す SpawnEvent と、編隊で出す FormationSpawnEvent が、同じ設定(プレハブ ID、置き場所、
// レール座標の指定)と、同じ処理(既定の置き場所の受け取り、レール座標からの出現位置の計算、JSON の読み書き、編集 UI)を、
// それぞれ重複して持っていた。また、エディタのプレビューは SpawnEvent だけを dynamic_cast で探していた。
// スポーン系に共通の部分をここへ集め、エディタ向けの通知と問い合わせ(ITimelineEvent の OnCreatedInEditor など)も、
// ここで 1 回だけ実装する。全イベント共通の基底(ITimelineEvent)には、スポーン系だけの機能を置かない。
//
// 設定は private。派生クラスは、読み取り(GetPrefabId など)と、下の protected のメンバ関数(UI と JSON)を通して使う。
class SpawnTimelineEvent : public ITimelineEvent {
public:
    ~SpawnTimelineEvent() override = default;

    // 出すプレハブの ID(PrefabRegistry で解決される)
    const std::string& GetPrefabId() const { return prefabId_; }

    // 出現時の Transform(世界座標)を求める。レール座標の指定ならレール座標から変換し、そうでなければ設定の Transform のまま。
    // outFrame: レールで置いたときの、出現位置のレールの座標系(置かなかったときは変更しない)
    // 戻り値: レール座標で置いたか。発火(Fire)とエディタの目印が、同じ手順で位置を求めるための関数
    bool ResolveSpawnTransform(const TimelineFireContext& context, Transform& outTransform, RailFrame& outFrame) const;

    // --- ITimelineEvent: エディタでの配置 ---
    // 既定の置き場所を受け取り、レール座標の指定にする
    void OnCreatedInEditor(const RailSpawnPoint& defaultPlacement) override;
    // 目印は、出すプレハブを、出現位置に置いたもの
    bool GetEditorHandle(const TimelineFireContext& context, TimelineEditorHandle& outHandle) const override;
    // 動かされた Transform を取り込む。レール座標の指定のときは、位置はレール座標だけで決まるので、回転とスケールだけを取り込む
    void OnEditorHandleEdited(const Transform& edited) override;

protected:
    // 以下は、派生クラスの UI と保存のための共通処理(データは private のまま、関数として渡す)

    // プレハブ ID を選ぶコンボを描く。未設定なら、一覧の先頭を選んだ状態にする(表示と設定値を一致させるため)。
    // label: ImGui のラベル(ID の重複を避けるため、派生ごとに変える)
    void DrawPrefabSelectUI(const char* label, bool& activated, bool& deactivatedAfterEdit);
    // 置き場所(レール座標か世界座標か、位置、回転、スケール)の編集欄を描く。
    // idSuffix: ImGui の ID の接尾辞(例: "##SpawnOffset")
    void DrawPlacementUI(const char* idSuffix, bool& activated, bool& deactivatedAfterEdit);

    // 共通の設定を JSON へ書き出す / JSON から読み込む。
    // transformKey: Transform を保存するキー(既存のステージのデータと互換を保つため、派生ごとに従来のキーを使う)
    void SerializeSpawnSettings(nlohmann::json& j, const char* transformKey) const;
    void DeserializeSpawnSettings(const nlohmann::json& j, const char* transformKey);

private:
    std::string prefabId_;       // 出すプレハブの ID
    Transform placement_{};      // 置き場所。useRail_ が true のときは translate を使わない(rail_ から求める)。rotate / scale は常に使う
    bool useRail_ = false;       // レール座標で位置を指定するか(false なら placement_.translate の世界座標)
    RailSpawnPoint rail_{};      // レール座標での位置(useRail_ が true のときだけ使う)
};

} // namespace AbsoluteEngine
