#pragma once
#include "Type/Vector.h"
#include <memory>

namespace AbsoluteEngine {
class GameObject;
class ISceneQuery;
}
class TelegraphComponent;

// 突進する敵の設定(JSON で調整する値)
struct ChargeSettings {
    float triggerRange = 15.0f;    // この距離以下で、突進の準備(予備動作、または狙い)を始める
    float chargeSpeed = 25.0f;     // 突進中の移動速度
    float windupPullSpeed = 24.0f; // 予備動作で、自機と反対へ引く速さ(最初の速さ。予備動作の終わりに向けて 0 になる)
};

// 突進する敵の状態クラスへ、ChargeOnApproachComponent が毎フレーム渡す情報
struct ChargeStateContext {
    AbsoluteEngine::GameObject& owner;      // 動かす敵
    AbsoluteEngine::ISceneQuery& query;     // 自機を探す窓口
    TelegraphComponent* telegraph;          // 予備動作の光り方(同じ敵に付いていなければ nullptr)
    const ChargeSettings& settings;         // 設定
    Vector3 cameraVelocity;                 // カメラの推定移動速度(前のフレームとの目の位置の差。狙いの先読みに使う)
    float deltaTime;                        // 前のフレームからの経過秒
};

// 突進する敵の「状態」の共通の形(State パターンの、状態の基底クラス)。
//
// 状態の一覧(遷移):
//   ChargeWaitState(接近待ち)  -> ChargeWindupState(範囲に入った。予備動作の部品があるとき)
//                              -> ChargeRunState(範囲に入った。予備動作の部品がないとき。その場で狙いを決める)
//   ChargeWindupState(予備動作) -> ChargeAimState(予備動作の時間が過ぎた)
//   ChargeAimState(狙う)       -> ChargeRunState(自機が見つかった。距離に関係なく、狙いを決める)
//   ChargeRunState(突進)       -> なし(決めた向きへ進み続ける)
// 以前は bool 3 つ(windingUp_ / windupDone_ / triggered_)の組み合わせで状態を表していた。
// 組み合わせは 8 通り作れるが、正しいのは 4 通りだけなので、ありえない組み合わせを作れないよう、状態クラスにした。
// 次の状態は、状態クラス自身が Update の戻り値で決める。切り替えは、Update のあとに ChargeOnApproachComponent が行う(1 フレームに 1 回)。
class IChargeState {
public:
    virtual ~IChargeState() = default;

    // この状態に入ったときに 1 回呼ばれる
    virtual void Enter(const ChargeStateContext& context) { (void)context; }

    // 毎フレーム呼ばれる。戻り値: 次の状態へ進むときは、次の状態。留まるときは nullptr
    virtual std::unique_ptr<IChargeState> Update(const ChargeStateContext& context) = 0;

    // この状態が、カメラ(目の位置と、その動きの推定)を使うか。使う状態は、カメラが無いフレームは止まる。
    // 突進中は、決めた向きへ進むだけなので、使わない
    virtual bool UsesCamera() const { return true; }
};

// 突進の狙い(状態をまたいで使う、状態を持たない計算)
namespace ChargeAiming {
// 自機との距離が、これより短いときは、向きが決まらないので、狙わない
constexpr float kMinDistance = 0.001f;

// 自機を探して、位置と、敵からの距離を求める。自機がいないときは false
bool FindPlayer(const ChargeStateContext& context, Vector3& playerPos, float& dist);

// 突進の向きを決める。到達までのおおよその時間を見積もり、カメラの推定移動速度ぶんだけ「到達する頃にいるはずの位置」を先読みする。
// 先読みした位置が近すぎて向きが決まらないときは、今の自機の向き。dist: 今の自機までの距離(kMinDistance 以上)
Vector3 ComputeDirection(const Vector3& currentPos, const Vector3& playerPos, float dist, const Vector3& cameraVelocity, float chargeSpeed);
} // namespace ChargeAiming
