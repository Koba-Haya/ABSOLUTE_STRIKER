#pragma once
#include "AbsoluteEngine/scene/Component.h"
#include "Type/Vector.h"
#include <string>

/// <summary>
/// ホーミング接近型敵の突進コンポーネント。
/// トリガー前は何もしない（StraightMoveComponent等、別の移動コンポーネントに
/// 動きを任せる想定）。プレイヤーとの距離がtriggerRange_以下になった瞬間、
/// その時点のプレイヤー位置への方向を一度だけ確定し、以後はその直線方向へ
/// 突進し続ける（以後は追尾しない）。
///
/// 予備動作(同じ敵に TelegraphComponent が付いているとき): 範囲に入ったら、すぐには突進せず、
/// TelegraphComponent の duration の間、自機と反対へ少し引きながら、体が光って震える。そのあとで突進する。
/// 引く動きは、この部品が位置を足して行う(StraightMoveComponent などの、位置を足す動きと競合しない)。
/// 当たり判定は、予備動作の間も、普通のまま(自機の弾で倒せる。自機に当たれば自滅する)。
///
/// 設定(JSON): triggerRange(突進を始める距離)、chargeSpeed(突進の速さ)、windupPullSpeed(予備動作の最初の引く速さ。だんだん 0 になる)
/// </summary>
class ChargeOnApproachComponent : public AbsoluteEngine::IComponent {
public:
    ChargeOnApproachComponent() = default;
    ~ChargeOnApproachComponent() override = default;

    void Update(float deltaTime) override;

    std::string GetTypeName() const override { return "ChargeOnApproachComponent"; }

    void Serialize(nlohmann::json& j) const override;
    void Deserialize(const nlohmann::json& j) override;

private:
    float triggerRange_ = 15.0f; // この距離以下でプレイヤーへの突進が発火する
    float chargeSpeed_  = 25.0f; // 突進中の移動速度
    float windupPullSpeed_ = 24.0f; // 予備動作で、自機と反対へ引く速さ(最初の速さ。予備動作の終わりに向けて 0 になる)

    bool windingUp_ = false;   // 予備動作の最中か
    bool windupDone_ = false;  // 予備動作が済んだか(済んだら、距離に関係なく突進を始める)
    float windupTimer_ = 0.0f; // 予備動作の経過時間
    Vector3 windupAwayDir_ = { 0, 0, 0 }; // 引く向き(予備動作の始めの、自機から離れる向き。長さ 1)

    bool triggered_ = false;
    // 発火時に一度だけ確定する突進方向。以後は毎フレーム再計算しない
    // （再計算すると、目標付近で行き過ぎては戻る振動が発生するため）。
    Vector3 chargeDir_ = { 0, 0, 0 };

    // カメラの移動速度を推定するための前フレームeye座標。
    // プレイヤーのワールド座標はカメラのeye基準で毎フレーム再計算されるため、
    // 操作していなくてもカメラのレール前進と同じ速度で流れ続ける。
    // 発火の瞬間、この推定速度ぶんだけ「到達する頃の位置」を先読みして狙う。
    Vector3 prevEye_ = { 0, 0, 0 };
    bool hasPrevEye_ = false;
};
