// ============================================================
// FormationSpawnEvent.h
// 役割: 同一プレハブを複数体（3〜5体）まとめてクラスタ状にスポーンする
//       タイムラインイベント。SpawnEventと違い1イベントで編隊全体を生成する。
//       生成した各メンバーには StraightMoveComponent（移動）と
//       FormationMemberComponent（GameScene側の編隊全滅ボーナス判定用）を付与する。
//
//       StraightMoveComponent/FormationMemberComponentはApplication（ゲーム）層の
//       コンポーネントのため、このクラス自身もApplication層に置く。
//       AbsoluteEngine（エンジン）層のTimelineTrack/エディタからは
//       TimelineEventFactory経由で名前だけで生成・利用され、具象型には依存しない。
// ============================================================
#pragma once
#include "AbsoluteEngine/scene/timeline/ITimelineEvent.h"
#include "AbsoluteEngine/scene/timeline/RailSpawnPoint.h"
#include "Type/Transform.h"
#include <memory>
#include <string>
#include <vector>

namespace AbsoluteEngine { class GameObject; }

class FormationSpawnEvent : public AbsoluteEngine::ITimelineEvent {
public:
    FormationSpawnEvent() = default;
    ~FormationSpawnEvent() override { Rewind(); }

    // -----------------------------------------------------------
    // 設定値（エディタUIまたはデシリアライズ時に設定される）
    // -----------------------------------------------------------

    // スポーンするプレハブのID（PrefabRegistryで解決される。編隊メンバー全員が同一プレハブ）
    std::string prefabId_ = "";

    // 編隊の中心位置・回転・スケール（各メンバーはこの位置を基準にXY平面でオフセットされる）
    // useRail_ が true のときは、translate は使わない(rail_ から出現時に求める)。rotate / scale は常に使う
    Transform basePosition_{};

    // レール座標で位置を指定するか(false なら、basePosition_.translate の世界座標。従来の JSON はこちら)
    bool useRail_ = false;

    // レール座標での編隊の中心位置(useRail_ が true のときだけ使う)。
    // 各メンバーの散らしは、レール座標のときは道の右・上方向(世界座標のときは世界の X・Y 軸)
    AbsoluteEngine::RailSpawnPoint rail_{};

    // 編隊のメンバー数（3〜5にクランプ）
    int memberCount_ = 4;

    // クラスタ配置のばらつき半径
    float clusterRadius_ = 3.0f;

    // -----------------------------------------------------------
    // ITimelineEvent 実装
    // -----------------------------------------------------------

    std::string GetLabel() const override;

    // memberCount_ 体をbasePosition_周辺にランダム配置し、rootObjects に追加する
    void Fire(const AbsoluteEngine::TimelineFireContext& context) override;

    // Fire()でスポーンした全メンバーを安全に破棄する
    void Rewind() override;

    std::string GetEventType() const override { return "FormationSpawnEvent"; }
    void Serialize(nlohmann::json& j) const override;
    void Deserialize(const nlohmann::json& j) override;

    void DrawInspectorUI(bool& activated, bool& deactivatedAfterEdit) override;
    // 追加時の既定位置。レール座標の指定にして、その値を入れる
    void SetSpawnAnchorRail(const AbsoluteEngine::RailSpawnPoint& rail) override {
        useRail_ = true;
        rail_ = rail;
    }

private:
    // Fire()でスポーンした各メンバーへの弱参照
    std::vector<std::weak_ptr<AbsoluteEngine::GameObject>> spawnedObjects_;
};
