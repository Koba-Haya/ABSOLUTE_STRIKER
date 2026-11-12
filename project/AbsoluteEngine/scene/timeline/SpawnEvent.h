// ============================================================
// SpawnEvent.h
// 役割: プレハブIDで指定されたGameObjectをスポーンするタイムラインイベント
//       生成したオブジェクトは weak_ptr で保持し、Rewind時に安全に破棄する
// ============================================================
#pragma once
#include "ITimelineEvent.h"
#include "RailSpawnPoint.h"
#include "../../Type/Transform.h"
#include <memory>
#include <string>

namespace AbsoluteEngine {

class GameObject;

class SpawnEvent : public ITimelineEvent {
public:
    SpawnEvent() = default;
    // デストラクタ: unique_ptr による消滅時（トラック削除など）に
    // プレビューオブジェクトが確実に破棄されるよう Rewind() を呼ぶ
    ~SpawnEvent() override { Rewind(); }

    // -----------------------------------------------------------
    // 設定値（エディタUIまたはデシリアライズ時に設定される）
    // -----------------------------------------------------------

    // スポーンするプレハブのID（PrefabRegistryで解決される）
    std::string prefabId_ = "";

    // スポーン位置・回転・スケール
    // useRail_ が true のときは、translate は使わない(rail_ から出現時に求める)。rotate / scale は常に使う
    Transform spawnTransform_{};

    // レール座標で位置を指定するか(false なら、spawnTransform_.translate の世界座標。従来の JSON はこちら)
    bool useRail_ = false;

    // レール座標での出現位置(useRail_ が true のときだけ使う)
    RailSpawnPoint rail_{};

    // -----------------------------------------------------------
    // ITimelineEvent 実装
    // -----------------------------------------------------------

    std::string GetLabel() const override;

    // PrefabRegistryからパスを解決し、プレハブをスポーンして rootObjects に追加する
    // スポーンしたオブジェクトは weak_ptr で保持する
    void Fire(const TimelineFireContext& context) override;

    // 出現時の Transform(世界座標)を求める。useRail_ ならレール座標から変換し、そうでなければ spawnTransform_ のまま。
    // outFrame は、レールで置いたときの、出現位置のレールの座標系(置かなかったときは変更しない)。
    // 戻り値: レール座標で置いたか。Fire() とエディタのプレビューが、同じ手順で位置を求めるための関数
    bool ResolveSpawnTransform(const TimelineFireContext& context, Transform& outTransform, RailFrame& outFrame) const;

    // weak_ptr 経由でオブジェクトの生存を確認してから安全に破棄する
    // （ダングリングポインタによるクラッシュを100%防ぐ）
    void Rewind() override;

    std::string GetEventType() const override { return "SpawnEvent"; }
    void Serialize(nlohmann::json& j) const override;
    void Deserialize(const nlohmann::json& j) override;

    void DrawInspectorUI(bool& activated, bool& deactivatedAfterEdit) override;

    // 作成時のみ、プレハブIDを選択可能なコンボとして描画する
    // （DrawInspectorUI()では配置後の誤操作防止のため表示のみ・変更不可にしている）
    void DrawCreationUI(bool& activated, bool& deactivatedAfterEdit) override;

    // 追加時の既定位置。レール座標の指定にして、その値を入れる
    void SetSpawnAnchorRail(const RailSpawnPoint& rail) override {
        useRail_ = true;
        rail_ = rail;
    }

private:
    // Fire()でスポーンしたオブジェクトへの弱参照
    // shared_ptr を直接持つと所有権の衝突が起きるため weak_ptr を使用する
    std::weak_ptr<GameObject> spawnedObject_;
};

} // namespace AbsoluteEngine
