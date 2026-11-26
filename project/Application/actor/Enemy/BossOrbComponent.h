#pragma once
#include "AbsoluteEngine/scene/Component.h"
#include "Type/Vector.h"
#include <memory>
#include <string>

// ボスの誘導弾の「玉」1 つの動き(出現、ボスの周りでの待機、発射、自機へ向かう飛行、着弾)。
//
// 分けた理由: 直進弾の BulletComponent とは、動きの責務が違う(玉は、ボスの周りの決まった位置へ現れて待ち、
// あとで自機を追って飛ぶ)。見た目は、BossOrbVisualComponent が描く(動きと見た目を分ける。弾の分け方と同じ)。
// 玉の数と、並べる位置の計算、発射の順番は、BossOrbGroup が受け持つ。
//
// 流れ: 出現(ボスの中心から、周りの位置へ広がりながら現れる)-> 待機(固定。溜まるほど輝く)
//       -> 発射を命じられ、順番の遅れが過ぎたら、飛行(ベジェ曲線で、自機の位置を追い続ける)-> 着弾(自機に重なる)
// 飛行: 2 次ベジェ曲線 B(t) = (1-t)^2 P0 + 2(1-t)t P1 + t^2 P2。P0 は発射の位置、P1 は発射のときに 1 回だけ決める制御点
//       (ボスから離れる向きへ膨らんでから、自機へ向かう曲線にする)、P2 は、自機の最新の位置(毎フレーム読む)。
//       自機の弾(HomingBulletComponent)と同じ方式で、必ず追いつく。避けられない代わりに、速くしない(飛行の時間は、攻撃の表で決める)。
//       当たり判定(GameObject の位置)は、曲線の上。らせんは、見た目だけ(BossOrbLook::spiralOffset。着弾へ向けて、半径が 0 へ収束する)。
// 当たり: 自機に重なったら、玉は消える(ダメージは、自機側が与える。回避の無敵の間は、与えない。重なれば消えるので、戻ってこない)。
//         島に遮られたら、消える(自機の弾と同じ)。
//         自機の弾に当たったら、撃ち落とされる。通常弾は、どの玉にも当たる。ロックオン弾は、狙った玉にだけ当たる(ほかの玉は通り抜ける)。
// 構え: 発射を命じられた玉は、飛び出す直前の kAimSec の間、自機と反対側へ引かれながら強く光る。発射の順番の遅れは、その前に待つ。
// 終わり: 玉は、終わったことを BossOrbLook::end で伝える。GameObject を壊す後始末(尾の切り離し、着弾の演出)は、見た目の部品が行う。
namespace BossOrbDefaults {
constexpr float kPi = 3.14159265f;
// 玉を並べる円の半径(ワールド。ボスは幅 約 9 なので、体の外側)
constexpr float kSlotRadius = 5.5f;
// 1 つ目の玉の角度(ラジアン。画面の右を 0、上を +90 度)。上から、時計回りではなく、反時計回りに並べる
constexpr float kFirstSlotAngle = kPi * 0.5f;
// 出現のうち、玉を 1 つずつ順番に出すための遅れに使う割合(攻撃の予告の秒数に対する割合)。残りが、1 つの玉の現れる時間
constexpr float kAppearStaggerShare = 0.5f;
// 現れ始めのときの、ボスの中心からの距離の割合(周りの位置へ向けて、1 まで広がる)
constexpr float kAppearStartRadiusRatio = 0.25f;
// 待機の間の、ゆっくりした上下の揺れ(ワールドの振れ幅と、速さ ラジアン / 秒)
constexpr float kBobAmplitude = 0.35f;
constexpr float kBobSpeed = 2.0f;
// 玉ごとの、揺れと模様のずれ(玉の番号に掛ける)
constexpr float kBobPhasePerSlot = 1.3f;
constexpr float kSeedPerSlot = 1.37f;
// 消える(Dismiss)のにかける時間(秒)
constexpr float kDismissSec = 0.30f;

// 当たり判定の半径(ワールド)。見た目より小さめ。ロックオンで落とす前提なので、無理に大きくしない
constexpr float kColliderRadius = 0.6f;
// 構え(発射の直前): 発射を命じられた玉が、飛び出す前の、この秒数の間、自機と反対側へ引かれながら、強く光る(yuudou.gif の、剣が向きをそろえて溜める動き)。
// 引く距離(ワールド)。引いた位置から飛び出すので、押し出されるように見える
constexpr float kAimSec = 0.30f;
constexpr float kAimPullBackDistance = 0.9f;
// 飛行の進み方: 進行度 t に掛ける指数(1 = 一定の速さ。大きいほど、出だしが遅く、あとが速い)。出だしが遅いと、飛び出したことが分かりやすい。
// 飛行の時間(着くまで)は変わらない
constexpr float kFlightEasePower = 1.25f;
// 飛び出してから、飛行の見た目(玉の引き伸ばし、リボンの締まり)へ移るまでの秒数
constexpr float kFlightBlendSec = 0.20f;
// 飛行の制御点: 発射の位置から、ボスの中心から離れる向きへずらす距離(ワールド)と、自機へ向けて進める割合(始点から自機までの距離に対する割合)
constexpr float kControlOutwardDistance = 7.0f;
constexpr float kControlForwardRatio = 0.35f;
// 飛行の進行度が 1 になっても、自機に重ならなかったとき(自機がいないなど)に、消えるまでの猶予(秒)
constexpr float kLandingGraceSec = 0.5f;
// らせん(見た目だけ): 半径(ワールド)、回る速さ(回転 / 秒)、現れる(0 から広がる)秒数、収束し始める進行度(これを超えると、着弾へ向けて 0 へ)
constexpr float kSpiralRadius = 0.45f;
constexpr float kSpiralTurnsPerSec = 1.6f;
constexpr float kSpiralRampInSec = 0.25f;
constexpr float kSpiralConvergeStart = 0.55f;
// 終わった(BossOrbLook::end が None でない)のに、見た目の部品が GameObject を壊さなかったときの、念のための自壊までの秒数
constexpr float kEndedSafetySec = 1.0f;
} // namespace BossOrbDefaults

// 玉の終わり方
enum class BossOrbEnd {
    None,      // まだ生きている
    Impact,    // 自機に重なった(または島に遮られた、猶予が過ぎた)。着弾の演出を出す
    ShotDown,  // 自機の弾(通常弾、ロックオン弾)に撃ち落とされた。撃墜の演出を出す
    Dismissed, // 消された(攻撃の中断、ボスの撃破)。着弾の演出は出さず、薄れて消える
};

// 玉の見た目を決めるための、いまの状態(BossOrbVisualComponent が読む)
struct BossOrbLook {
    float appear = 0.0f;  // 現れ具合(0 = まだ、1 = 現れきった)
    float charge = 0.0f;  // 溜まり具合(0 〜 1。現れきったあと、溜めの秒数で 1 へ)
    float dismiss = 0.0f; // 消え具合(0 = 消えていない、1 = 消えきった)
    float age = 0.0f;     // 出てからの時間(秒。模様の流れなどに使う)
    float seed = 0.0f;    // 玉ごとのずれ
    float aim = 0.0f;     // 構えの進み具合(0 = 構えていない、1 = 飛び出す直前)
    bool flying = false;  // 飛行中か
    float flight = 0.0f;  // 飛行の進行度(0 〜 1)
    float flightBlend = 0.0f; // 飛行の見た目への移り具合(飛び出してから kFlightBlendSec で 0 から 1 へ)
    Vector3 direction{ 0.0f, 0.0f, 1.0f };     // 飛んでいる向き(長さ 1)
    Vector3 spiralOffset{ 0.0f, 0.0f, 0.0f };  // らせんのずれ(ワールド)。見た目の位置 = GameObject の位置 + これ
    BossOrbEnd end = BossOrbEnd::None;         // 終わったか。None 以外になったら、見た目の部品が後始末をする
};

class BossOrbComponent : public AbsoluteEngine::IComponent {
public:
    // slot: この玉の番号(0 〜 slotCount - 1)。slotCount: 玉の総数
    // appearSec: 出現にかける秒数(攻撃の予告の長さ)。chargeSec: 溜めの秒数(現れきったあと、輝きが最大になるまで)
    BossOrbComponent(int slot, int slotCount, float appearSec, float chargeSec);
    ~BossOrbComponent() override = default;

    void Update(float deltaTime) override;
    void OnCollision(AbsoluteEngine::GameObject* other) override;

    std::string GetTypeName() const override { return "BossOrbComponent"; }

    // 基準(ボスの中心と、画面の右と上の向き)を決める。ボスが動くので、毎フレーム渡す
    void SetAnchor(const Vector3& center, const Vector3& right, const Vector3& up);

    // 発射を命じる。target: 追う相手(自機)。delaySec: 命じてから飛び出すまでの遅れ(順番に飛ばすため)。flightSec: 飛行の時間(秒)。
    // 飛行前の待機の玉にだけ効く(すでに命じた玉、飛行中の玉、消え始めた玉には、何もしない)
    void Launch(std::weak_ptr<AbsoluteEngine::GameObject> target, float delaySec, float flightSec);

    // まだ発射を命じられていない、待機の玉か(攻撃の中断で消す対象)
    bool IsHeld() const { return state_ == State::Hold && !committed_ && !dismissing_; }

    // 自機の弾が当たる(ロックオンできる、撃ち落とせる)状態か。現れきっていて、終わっておらず、消され始めてもいない玉。
    // 待機中も飛行中も、撃ち落とせる(待機の間が、撃ち落とす猶予)
    bool IsTargetable() const { return state_ != State::Ended && !dismissing_ && look_.appear >= 1.0f; }

    // 玉を消し始める(攻撃の中断、ボスの撃破など。飛行中でも効く)。もう消え始めているときは、何もしない
    void Dismiss();

    const BossOrbLook& GetLook() const { return look_; }

private:
    enum class State { Hold, Flying, Ended };

    // 基準と、現れ具合と、時間から、待機の間の位置を求める
    Vector3 ComputeHoldPosition_() const;
    // 飛行を始める(いまの位置を始点にして、制御点を決める)
    void StartFlight_();
    // 飛行を 1 フレーム進める
    void UpdateFlight_(float deltaTime);
    // 飛行中の、らせんのずれと、飛んでいる向きを、look_ へ書く
    void UpdateSpiral_(const Vector3& direction);
    // 終わらせる。種類を look_.end へ書く(GameObject は壊さない。後始末は、見た目の部品が行う)。reason: 終わった理由(ログ用の短い文字列)
    void End_(BossOrbEnd kind, const char* reason);

    int slot_;
    int slotCount_;
    float appearSec_;
    float chargeSec_;

    Vector3 anchorCenter_{ 0.0f, 0.0f, 0.0f };
    Vector3 anchorRight_{ 1.0f, 0.0f, 0.0f };
    Vector3 anchorUp_{ 0.0f, 1.0f, 0.0f };

    State state_ = State::Hold;
    bool dismissing_ = false;
    float dismissTimer_ = 0.0f;

    // 発射
    bool committed_ = false;     // 発射を命じられた
    float launchTimer_ = 0.0f;   // 飛び出すまでの残り(秒)
    float flightSec_ = 2.0f;
    std::weak_ptr<AbsoluteEngine::GameObject> target_;
    // 飛行
    Vector3 startPos_{ 0.0f, 0.0f, 0.0f };    // P0
    Vector3 controlPos_{ 0.0f, 0.0f, 0.0f };  // P1
    Vector3 previousPos_{ 0.0f, 0.0f, 0.0f };
    float progress_ = 0.0f;                   // t(0 〜 1)
    float flightAge_ = 0.0f;                  // 飛び出してからの時間(秒。らせんの角度に使う)
    float landingTimer_ = 0.0f;               // 進行度が 1 になってからの時間(秒)
    float endedTimer_ = 0.0f;                 // 終わってからの時間(秒。念のための自壊に使う)

    BossOrbLook look_;
};
