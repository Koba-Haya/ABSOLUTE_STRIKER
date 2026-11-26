#include "BossOrbComponent.h"
#include "AbsoluteEngine/scene/GameObject.h"
#include "../Bullet/HomingBulletComponent.h"
#include "Application/occlusion/OcclusionWorld.h"
#include "Logger.h"
#include "Method.h"
#include <algorithm>
#include <cmath>

namespace {
using namespace BossOrbDefaults;

constexpr float kTwoPi = 2.0f * kPi;
// この値より短いベクトルは、向きを持たないものとして扱う
constexpr float kMinDirectionLength = 0.0001f;
// 飛んでいる向きと、基準の向きが、平行に近いと見なす、y 成分の絶対値(外積の向きが決まらなくなるので、別の基準へ替える)
constexpr float kParallelLimit = 0.9f;
// 自機と、自機の弾のタグ(GameObject::GetTag の値)
constexpr const char* kPlayerTag = "Player";
constexpr const char* kPlayerBulletTag = "PlayerBullet";

// 0 〜 1 の t を、始めが速く、終わりが遅いように返す(広がって、周りの位置へ着く動き用)
float EaseOut(float t) {
    const float c = std::clamp(t, 0.0f, 1.0f);
    return 1.0f - (1.0f - c) * (1.0f - c);
}

// 0 〜 1 の t を、始めと終わりをなめらかにして返す
float SmoothStep(float edge0, float edge1, float x) {
    const float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

Vector3 Sub(const Vector3& a, const Vector3& b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }

// 2 次ベジェ曲線: B(t) = (1-t)^2 p0 + 2(1-t)t p1 + t^2 p2
Vector3 EvaluateBezier(const Vector3& p0, const Vector3& p1, const Vector3& p2, float t) {
    const float t1 = 1.0f - t;
    const float c0 = t1 * t1;
    const float c1 = 2.0f * t1 * t;
    const float c2 = t * t;
    return { c0 * p0.x + c1 * p1.x + c2 * p2.x, c0 * p0.y + c1 * p1.y + c2 * p2.y, c0 * p0.z + c1 * p1.z + c2 * p2.z };
}
} // namespace

BossOrbComponent::BossOrbComponent(int slot, int slotCount, float appearSec, float chargeSec)
    : slot_(slot), slotCount_((std::max)(slotCount, 1)), appearSec_(appearSec), chargeSec_(chargeSec) {
    look_.seed = static_cast<float>(slot) * kSeedPerSlot;
}

void BossOrbComponent::SetAnchor(const Vector3& center, const Vector3& right, const Vector3& up) {
    anchorCenter_ = center;
    anchorRight_ = right;
    anchorUp_ = up;
}

void BossOrbComponent::Dismiss() {
    if (dismissing_ || state_ == State::Ended) return;
    dismissing_ = true;
    dismissTimer_ = 0.0f;
}

void BossOrbComponent::Launch(std::weak_ptr<AbsoluteEngine::GameObject> target, float delaySec, float flightSec) {
    if (!IsHeld()) return;
    committed_ = true;
    target_ = std::move(target);
    // 順番の遅れのあとに、構え(kAimSec)が来て、そのあと飛び出す
    launchTimer_ = (std::max)(delaySec, 0.0f) + kAimSec;
    flightSec_ = (std::max)(flightSec, 0.1f);
}

Vector3 BossOrbComponent::ComputeHoldPosition_() const {
    // 玉は、円周に等間隔で並ぶ(画面に平行な面の上)
    const float angle = kFirstSlotAngle + kTwoPi * static_cast<float>(slot_) / static_cast<float>(slotCount_);
    // 現れるときは、ボスの中心の近くから、周りの位置へ広がる
    const float radius = kSlotRadius * (kAppearStartRadiusRatio + (1.0f - kAppearStartRadiusRatio) * EaseOut(look_.appear));
    const float bob = kBobAmplitude * std::sin(look_.age * kBobSpeed + static_cast<float>(slot_) * kBobPhasePerSlot) * look_.appear;
    const float c = std::cos(angle) * radius;
    const float s = std::sin(angle) * radius + bob;
    return {
        anchorCenter_.x + anchorRight_.x * c + anchorUp_.x * s,
        anchorCenter_.y + anchorRight_.y * c + anchorUp_.y * s,
        anchorCenter_.z + anchorRight_.z * c + anchorUp_.z * s,
    };
}

void BossOrbComponent::StartFlight_() {
    const auto target = target_.lock();
    if (!target) {
        Dismiss(); // 追う相手がいない
        return;
    }
    state_ = State::Flying;
    startPos_ = GetOwner()->GetTransform().translate; // 構えで引かれた位置から、飛び出す
    previousPos_ = startPos_;
    progress_ = 0.0f;
    flightAge_ = 0.0f;
    landingTimer_ = 0.0f;
    look_.aim = 0.0f;

    // 制御点: 始点から、ボスの中心から離れる向きへ膨らませ、自機へ向けても少し進める。
    // 玉ごとに膨らむ向きが違うので、曲線が、ばらけて見える
    const Vector3 targetPos = target->GetTransform().translate;
    Vector3 outward = Sub(startPos_, anchorCenter_);
    if (Length(outward) < kMinDirectionLength) outward = anchorUp_;
    outward = Normalize(outward);
    const Vector3 toTarget = Sub(targetPos, startPos_);
    controlPos_ = {
        startPos_.x + outward.x * kControlOutwardDistance + toTarget.x * kControlForwardRatio,
        startPos_.y + outward.y * kControlOutwardDistance + toTarget.y * kControlForwardRatio,
        startPos_.z + outward.z * kControlOutwardDistance + toTarget.z * kControlForwardRatio,
    };
    look_.flying = true;
    // 飛んでいる向きの初期値: 曲線の出だしの向き(始点から制御点へ)。出だしは動きが小さいので、位置の差からは、向きが決まらない
    look_.direction = Normalize(Sub(controlPos_, startPos_));
}

void BossOrbComponent::UpdateSpiral_(const Vector3& direction) {
    look_.direction = direction;
    // 飛んでいる向きに垂直な 2 本の軸を作り、その面の上で回る
    const Vector3 reference = (std::abs(direction.y) < kParallelLimit) ? Vector3{ 0.0f, 1.0f, 0.0f } : Vector3{ 1.0f, 0.0f, 0.0f };
    Vector3 axisA = Cross(direction, reference);
    if (Length(axisA) < kMinDirectionLength) axisA = Vector3{ 1.0f, 0.0f, 0.0f };
    axisA = Normalize(axisA);
    const Vector3 axisB = Cross(direction, axisA);

    // 半径: 飛び出しから少しずつ広がり、着弾へ向けて 0 へ収束する(最後は、曲線の真上に重なる)
    const float ramp = std::clamp(flightAge_ / kSpiralRampInSec, 0.0f, 1.0f);
    const float converge = 1.0f - SmoothStep(kSpiralConvergeStart, 1.0f, progress_);
    const float radius = kSpiralRadius * ramp * converge;
    const float angle = flightAge_ * kSpiralTurnsPerSec * kTwoPi + look_.seed;
    const float a = std::cos(angle) * radius;
    const float b = std::sin(angle) * radius;
    look_.spiralOffset = {
        axisA.x * a + axisB.x * b,
        axisA.y * a + axisB.y * b,
        axisA.z * a + axisB.z * b,
    };
}

void BossOrbComponent::UpdateFlight_(float deltaTime) {
    const auto target = target_.lock();
    if (!target) {
        End_(BossOrbEnd::Dismissed, "target-lost"); // 追う相手が消えた
        return;
    }
    flightAge_ += deltaTime;
    progress_ = (std::min)(progress_ + deltaTime / flightSec_, 1.0f);
    look_.flightBlend = std::clamp(flightAge_ / kFlightBlendSec, 0.0f, 1.0f);

    // 終点は、自機の最新の位置(自機が動いても、追いつく)。曲線の上の位置は、進行度に指数を掛けた値で決める(出だしが遅く、あとが速い)
    const Vector3 targetPos = target->GetTransform().translate;
    const Vector3 position = EvaluateBezier(startPos_, controlPos_, targetPos, std::pow(progress_, kFlightEasePower));

    // 島に遮られたら、消える(自機の弾と同じ。前フレームの位置から今フレームの位置までの線分で調べる)
    if (OcclusionWorld::GetInstance().IsSegmentBlocked(previousPos_, position, OccluderKind::kIsland)) {
        GetOwner()->SetPosition(position);
        End_(BossOrbEnd::Impact, "island");
        return;
    }

    Vector3 direction = Sub(position, previousPos_);
    if (Length(direction) > kMinDirectionLength) {
        direction = Normalize(direction);
    } else {
        direction = look_.direction; // ほとんど動かなかったときは、前の向きのまま
    }
    GetOwner()->SetPosition(position);
    previousPos_ = position;
    look_.flight = progress_;
    UpdateSpiral_(direction);

    // 進行度が 1 になっても自機に重ならない(当たり判定が働かなかった)ときの、念のための終わり
    if (progress_ >= 1.0f) {
        landingTimer_ += deltaTime;
        if (landingTimer_ >= kLandingGraceSec) End_(BossOrbEnd::Impact, "grace");
    }
}

void BossOrbComponent::End_(BossOrbEnd kind, const char* reason) {
    if (state_ == State::Ended) return;
    // 玉が何で終わったかを、ログに残す(自動撮影での確認に使う。自機に重なったか、島に遮られたか、猶予が切れたか、など)
    Logger::GetInstance().Log("[BossOrb] end reason=%s flight=%.2f", reason, progress_);
    state_ = State::Ended;
    look_.flying = false;
    look_.end = kind;
    endedTimer_ = 0.0f;
}

void BossOrbComponent::OnCollision(AbsoluteEngine::GameObject* other) {
    if (state_ == State::Ended || !other) return;
    const std::string& tag = other->GetTag();

    // 自機の弾に撃ち落とされる(待機中も飛行中も)。通常弾はどの玉にも当たり、ロックオン弾は、狙った玉にだけ当たる
    if (tag == kPlayerBulletTag) {
        if (IsTargetable() && HomingBulletComponent::CanHit(*other, GetOwner())) {
            End_(BossOrbEnd::ShotDown, "shot");
        }
        return;
    }

    // 自機に重なったら、消える(飛行中だけ)。回避の無敵の間も同じ(ダメージは、自機側が、無敵のときは与えない)。
    // 重なれば消えるので、避けた玉が戻ってくることはない
    if (state_ == State::Flying && tag == kPlayerTag) {
        End_(BossOrbEnd::Impact, "player");
    }
}

void BossOrbComponent::Update(float deltaTime) {
    if (!GetOwner()) return;
    look_.age += deltaTime;

    if (state_ == State::Ended) {
        // 見た目の部品が後始末をして GameObject を壊すはずだが、念のため、一定時間で自分も壊す
        endedTimer_ += deltaTime;
        if (endedTimer_ >= kEndedSafetySec) GetOwner()->Destroy();
        return;
    }

    if (state_ == State::Hold) {
        // 現れ具合: 玉の番号の順に、少しずつ遅れて現れる(遅れの合計は、出現の秒数の kAppearStaggerShare)
        const float stagger = appearSec_ * kAppearStaggerShare / static_cast<float>(slotCount_);
        const float duration = (std::max)(appearSec_ * (1.0f - kAppearStaggerShare), 0.0001f);
        look_.appear = std::clamp((look_.age - stagger * static_cast<float>(slot_)) / duration, 0.0f, 1.0f);
        // 溜まり具合: すべての玉が現れきったあと(出現の秒数が過ぎたあと)、溜めの秒数をかけて 1 へ。発射を命じられたあとは、溜まりきっている
        look_.charge = (chargeSec_ > 0.0f) ? std::clamp((look_.age - appearSec_) / chargeSec_, 0.0f, 1.0f) : 1.0f;
        if (committed_) look_.charge = 1.0f;

        Vector3 position = ComputeHoldPosition_();
        look_.aim = 0.0f;

        // 発射を命じられていたら、順番の遅れのあとの構え(自機と反対側へ引かれながら、強く光る)を経て、飛び出す
        if (committed_ && !dismissing_) {
            launchTimer_ -= deltaTime;
            if (launchTimer_ < kAimSec) {
                look_.aim = std::clamp(1.0f - launchTimer_ / kAimSec, 0.0f, 1.0f);
                if (const auto target = target_.lock()) {
                    Vector3 toTarget = Sub(target->GetTransform().translate, position);
                    if (Length(toTarget) > kMinDirectionLength) {
                        toTarget = Normalize(toTarget);
                        const float pull = kAimPullBackDistance * EaseOut(look_.aim);
                        position = { position.x - toTarget.x * pull, position.y - toTarget.y * pull, position.z - toTarget.z * pull };
                    }
                }
            }
            GetOwner()->SetPosition(position);
            if (launchTimer_ <= 0.0f) StartFlight_();
        } else {
            GetOwner()->SetPosition(position);
        }
    } else {
        // 飛行中: 現れきって、溜まりきった状態のまま
        look_.appear = 1.0f;
        look_.charge = 1.0f;
        UpdateFlight_(deltaTime);
    }

    if (dismissing_ && state_ != State::Ended) {
        dismissTimer_ += deltaTime;
        look_.dismiss = std::clamp(dismissTimer_ / kDismissSec, 0.0f, 1.0f);
        if (look_.dismiss >= 1.0f) End_(BossOrbEnd::Dismissed, "dismissed"); // 消えきった。後始末は、見た目の部品が行う
    }
}
