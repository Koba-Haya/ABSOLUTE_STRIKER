#pragma once
#include "AbsoluteEngine/scene/Component.h"
#include "Type/Vector.h"
#include "charge/ChargeWaitState.h"
#include "charge/IChargeState.h"
#include <memory>
#include <string>

namespace AbsoluteEngine {
class IScreenView;
class ISceneQuery;
}

/// <summary>
/// ホーミング接近型敵の突進コンポーネント。
/// 接近待ちの間は何もしない（StraightMoveComponent等、別の移動コンポーネントに動きを任せる想定）。
/// プレイヤーとの距離が triggerRange 以下になったら、その時点のプレイヤー位置への方向を一度だけ確定し、
/// 以後はその直線方向へ突進し続ける（以後は追尾しない）。
///
/// 予備動作(同じ敵に TelegraphComponent が付いているとき): 範囲に入ったら、すぐには突進せず、
/// TelegraphComponent の duration の間、自機と反対へ少し引きながら、体が光って震える。そのあとで突進する。
///
/// State パターン: 接近待ち / 予備動作 / 狙う / 突進 を、状態クラス(charge/ の ChargeWaitState など。基底は IChargeState)で持つ。
/// この部品は、カメラの移動速度を推定し、今の状態へ情報(ChargeStateContext)を渡して Update を呼び、返された次の状態へ切り替えるだけ。
///
/// 設定(JSON): triggerRange(突進を始める距離)、chargeSpeed(突進の速さ)、windupPullSpeed(予備動作の最初の引く速さ。だんだん 0 になる)
/// </summary>
class ChargeOnApproachComponent : public AbsoluteEngine::IComponent {
public:
    // screen: カメラの位置(移動速度の推定に使う)を知る窓口。query: 自機を探す窓口。
    // いずれも所有しない。このコンポーネントより長く生きること
    ChargeOnApproachComponent(AbsoluteEngine::IScreenView& screen, AbsoluteEngine::ISceneQuery& query) : screen_(screen), query_(query) {}
    ~ChargeOnApproachComponent() override = default;

    void Update(float deltaTime) override;

    std::string GetTypeName() const override { return "ChargeOnApproachComponent"; }

    void Serialize(nlohmann::json& j) const override;
    void Deserialize(const nlohmann::json& j) override;

private:
    // カメラの移動速度を推定する(前のフレームとの目の位置の差)。
    // プレイヤーのワールド座標はカメラの目の位置を基準に毎フレーム計算し直されるため、操作していなくても、
    // カメラのレール前進と同じ速度で流れ続ける。狙いを決める瞬間、この推定速度ぶんだけ「到達する頃の位置」を先読みする
    Vector3 EstimateCameraVelocity_(float deltaTime);

    AbsoluteEngine::IScreenView& screen_; // 画面の見え方(カメラの位置、画面内か)を知る窓口
    AbsoluteEngine::ISceneQuery& query_; // 自機などを探すために、シーンのオブジェクト一覧を調べる窓口
    ChargeSettings settings_;            // 設定(JSON で保存する値)

    std::unique_ptr<IChargeState> state_ = std::make_unique<ChargeWaitState>(); // 今の状態(最初は接近待ち)

    Vector3 prevEye_ = { 0, 0, 0 }; // カメラの移動速度を推定するための、前のフレームの目の位置
    bool hasPrevEye_ = false;
};
