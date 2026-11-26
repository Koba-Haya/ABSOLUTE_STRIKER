#include "BossComponent.h"
#include "BossModelSpec.h"
#include "BossIdleState.h"
#include "BossShieldState.h"
#include "AbsoluteEngine/scene/ColliderComponent.h"
#include "../Bullet/BossBulletVisualComponent.h"
#include "AbsoluteEngine/scene/GameObject.h"
#include "AbsoluteEngine/scene/ISceneQuery.h"
#include "AbsoluteEngine/scene/IScreenView.h"
#include "AbsoluteEngine/scene/ISpawner.h"
#include "AbsoluteEngine/scene/ModelComponent.h"
#include "../Bullet/BulletFactory.h"
#include "../Player/PlayerFinder.h"
#include "../Bullet/HomingBulletComponent.h"
#include "../Player/PlayerComponent.h"
#include "GameCamera.h"
#include <cmath>

namespace {
// 攻撃の間隔・弾速・弾の数は、攻撃の表(BossAttackPlan.h)に移した

// 左右上下の揺れ幅と速さ。カメラの前方に留まる距離は、BossPlacement::kHoverDistanceFromCamera(BossComponent.h)
constexpr float kWeaveAmplitudeX = 6.0f;  // 左右の振れ幅
constexpr float kWeaveAmplitudeY = 3.0f;  // 上下の振れ幅
constexpr float kWeaveSpeedX = 0.5f;      // rad/sec
constexpr float kWeaveSpeedY = 0.7f;      // rad/sec（Xと素数っぽい比率にして周期がズレるようにする）

// ボスのモデルの正面を、カメラの方へ向けるための補正(ラジアン)。実機で、正面が奥を向いていたため、半回転(180 度)させる
constexpr float kModelFrontYawOffset = 3.14159265f;

// アニメーションの切り替えの混ぜ合わせ時間(秒)。急に切り替わって見えないようにする
constexpr float kHoverBlendSeconds = 0.25f;  // 待機へ戻る
constexpr float kAttackBlendSeconds = 0.15f; // 攻撃の予告へ入る
constexpr float kChargeBlendSeconds = 0.10f; // 溜めへ入る(予告の終わりと姿勢がつながっているので、短く)
constexpr float kFireBlendSeconds = 0.05f;   // 発射へ入る(突き出しを、すぐに始める)
constexpr float kHitBlendSeconds = 0.05f;    // 被弾のよろめきへ入る(すぐに反応して見えるよう短く)
}

void BossComponent::Update(float deltaTime) {
    if (!isActive_ || !GetOwner()) return;

    UpdateMovement(deltaTime);
    // 誘導弾の玉は、ボスの動きに合わせる(ボスが動いたあとの、ボスの位置を基準に渡す)
    if (screen_.HasCamera()) {
        orbGroup_.Update(GetOwner()->GetTransform().translate, screen_.GetRight(), screen_.GetActualUp());
    }
    UpdatePhase();

    // 状態の切り替えを済ませてから、今の状態を 1 回進める。状態の中で ChangeState が呼ばれても、
    // 切り替えは、この Update が終わったあと(次の ApplyPendingState)に行うので、実行中の状態が消えることはない
    ApplyPendingState();
    if (state_) state_->Update(*this, deltaTime);
    ApplyPendingState();

    UpdateAnimation();

    // 攻撃の演出は、口の前(muzzle ボーン)の位置に出す。モデルにそのボーンが無いとき(鷲)は、演出を出さない
    Vector3 muzzle{ 0.0f, 0.0f, 0.0f };
    const auto* model = GetOwner()->GetComponent<AbsoluteEngine::ModelComponent>();
    const bool hasMuzzle = model && model->TryGetBoneWorldPosition(BossBone::kMuzzle, muzzle);
    attackEffects_.Update(deltaTime, hasMuzzle ? &muzzle : nullptr);

    // シールドのドームの中心: ボスの位置から、正面(カメラ側。あごの側)へ、少しずらす。ボスは、カメラのほうを向いている
    Vector3 domeCenter = GetOwner()->GetTransform().translate;
    Vector3 cameraForward{ 0.0f, 0.0f, 1.0f };
    if (screen_.HasCamera()) {
        cameraForward = screen_.GetForward();
        domeCenter = { domeCenter.x - cameraForward.x * BossShieldEffectDefaults::kDomeCenterShift,
                       domeCenter.y - cameraForward.y * BossShieldEffectDefaults::kDomeCenterShift,
                       domeCenter.z - cameraForward.z * BossShieldEffectDefaults::kDomeCenterShift };
    }
    shieldEffects_.Update(deltaTime, shieldView_, domeCenter, cameraForward);
    UpdateColliderForShield_();
}

void BossComponent::DrawAfterClouds() {
    // ドームを先に描き、口の前の輪や粒を、その手前に重ねる。画面全体の白いフラッシュは、すべての手前に出すので、最後に描く
    shieldEffects_.Draw();
    attackEffects_.Draw();
    shieldEffects_.DrawFlash();
}

void BossComponent::UpdateColliderForShield_() {
    auto* collider = GetOwner()->GetComponent<AbsoluteEngine::ColliderComponent>();
    if (!collider) return;

    // 元の半径を、最初に覚える(ボスの種類ごとの定義で決まった値)
    AbsoluteEngine::ColliderComponent::Settings settings = collider->GetSettings();
    if (baseColliderRadius_ < 0.0f) baseColliderRadius_ = settings.radius;

    // シールドのドームがある間(展開と維持)は、当たり判定をドームの外側まで広げる。
    // 自機の弾が、ドームの表面で止まり、波紋が弾の当たった所から出るようにするため。ドームの中心は、ボスの位置から前へずれているので、その分を足す
    const bool domeActive = shieldView_.stage == BossShieldStage::Deploy || shieldView_.stage == BossShieldStage::Hold;
    const float target = domeActive ? BossShieldEffectDefaults::kDomeRadius + BossShieldEffectDefaults::kDomeCenterShift : baseColliderRadius_;
    if (settings.radius != target) {
        settings.radius = target;
        collider->SetSettings(settings);
    }
}

void BossComponent::ApplyPendingState() {
    // 最初の 1 回: まだ状態が無いので、待機から始める(表の 0 番目の攻撃を待つ)
    if (!state_ && !pendingState_) {
        pendingState_ = std::make_unique<BossIdleState>(0);
    }
    if (!pendingState_) return;

    if (state_) state_->Exit(*this);
    state_ = std::move(pendingState_);
    state_->Enter(*this);
}

void BossComponent::UpdateAnimation() {
    auto* model = GetOwner()->GetComponent<AbsoluteEngine::ModelComponent>();
    if (!model) return;

    // 被弾: すぐによろめく(攻撃の途中でも割り込む)。連射を受けたときも、毎回頭から再生し直す(restart)。
    // 攻撃の状態そのもの(弾を撃つ時刻)は止まらない。よろめきのアニメーションだけが、攻撃のアニメーションを一時的に上書きする
    if (hitRequested_) {
        hitRequested_ = false;
        attackStageChanged_ = false;
        model->PlayAnimation(BossAnimation::kHit, false, kHitBlendSeconds, true);
        animState_ = AnimState::Hit;
        return;
    }

    // 攻撃の段が変わったら、その段のクリップへ切り替える。よろめいている間は、切り替えない(Hit が終わってから)。
    // よろめきが終わったときに、予告か溜めの途中なら、待機へ戻らず、いまの段のクリップへ戻る(構えた姿勢へ戻る)。
    // 発射の途中なら、突き出しをやり直さず、待機へ戻る
    const bool hitFinished = animState_ == AnimState::Hit && model->IsAnimationFinished();
    if (animState_ != AnimState::Hit || hitFinished) {
        const bool backToHold = hitFinished &&
            (attackStage_ == BossAttackStage::Telegraph || attackStage_ == BossAttackStage::Charge);
        if (attackStageChanged_ || backToHold) {
            attackStageChanged_ = false;
            if (PlayAttackStageClip(*model)) return;
        }
    }

    // 予告と溜めの間は、クリップが最後まで再生されても、待機へ戻らず、最後の姿勢を保つ(次の段が始まるまで)
    const bool holdingAttackPose = animState_ == AnimState::Attack &&
        (attackStage_ == BossAttackStage::Telegraph || attackStage_ == BossAttackStage::Charge);
    // 最初の 1 回(None)と、Hit / Attack が最後まで再生されたあとは、待機へ戻る
    if (animState_ == AnimState::None || (animState_ != AnimState::Hover && !holdingAttackPose && model->IsAnimationFinished())) {
        model->PlayAnimation(BossAnimation::kHover, true, kHoverBlendSeconds, false);
        animState_ = AnimState::Hover;
    }
}

bool BossComponent::PlayAttackStageClip(AbsoluteEngine::ModelComponent& model) {
    const char* clip = nullptr;
    bool loop = false;
    float blendSeconds = 0.0f;
    switch (attackStage_) {
    case BossAttackStage::Telegraph:
        clip = attackClips_.telegraph;
        blendSeconds = kAttackBlendSeconds;
        break;
    case BossAttackStage::Charge:
        clip = attackClips_.charge;
        loop = true; // 溜めは、段の長さが決まっていないので、ループさせる
        blendSeconds = kChargeBlendSeconds;
        break;
    case BossAttackStage::Fire:
        clip = attackClips_.fire;
        blendSeconds = kFireBlendSeconds;
        break;
    case BossAttackStage::None:
        break;
    }
    if (!clip) return false;

    model.PlayAnimation(clip, loop, blendSeconds, false);
    animState_ = AnimState::Attack;
    return true;
}

std::shared_ptr<AbsoluteEngine::GameObject> BossComponent::FindPlayer() const {
    return PlayerFinder::FindPlayer(query_);
}

void BossComponent::UpdateMovement(float deltaTime) {
    if (!screen_.HasCamera()) return;

    weaveTimer_ += deltaTime;

    // 位置の基準はカメラの目の位置。カメラはレールに沿って動くので、ボスもレールに追従する
    const Vector3 basePos = screen_.GetEye();
    const Vector3 forward = screen_.GetForward();
    const Vector3 right    = screen_.GetRight();
    const Vector3 up       = screen_.GetActualUp();

    const float weaveX = std::sin(weaveTimer_ * kWeaveSpeedX) * kWeaveAmplitudeX;
    const float weaveY = std::sin(weaveTimer_ * kWeaveSpeedY) * kWeaveAmplitudeY;

    GetOwner()->SetPosition({
        basePos.x + forward.x * BossPlacement::kHoverDistanceFromCamera + right.x * weaveX + up.x * weaveY,
        basePos.y + forward.y * BossPlacement::kHoverDistanceFromCamera + right.y * weaveX + up.y * weaveY,
        basePos.z + forward.z * BossPlacement::kHoverDistanceFromCamera + right.z * weaveX + up.z * weaveY
    });

    // 正面(モデルの +Z)を、カメラの方へ向ける。自機は、モデルの +Z を進行方向へ向けて飛ぶ(PlayerComponent と同じ規約)。
    // ボスは、その自機と向かい合うので、進行方向の逆(カメラの向きの逆)へ向く。左右(Y 軸)の向きだけを合わせる
    // (ボスのモデルは、自機のモデルと、書き出しの前後の向きが逆なので、kModelFrontYawOffset だけ回して合わせる)
    GetOwner()->SetRotation({ 0.0f, std::atan2(-forward.x, -forward.z) + kModelFrontYawOffset, 0.0f });
}

namespace {
// フェーズの境目(最大 HP に対する、残りの割合。この割合以下で、次のフェーズへ進む)
constexpr float kPhase2HpRatio = 2.0f / 3.0f;
constexpr float kPhase3HpRatio = 1.0f / 3.0f;
// 割合から HP の値を求めるときの、浮動小数点の誤差(30 x 2/3 = 20 が、19.9999 になって切り捨てられないようにする)
constexpr float kHpBoundaryEpsilon = 0.0001f;

// 次のフェーズ(nextPhase)に入る HP の境目。この値以下になると、そのフェーズへ進む。Phase1 には「入る」境目がないので 0
int BoundaryHpOf(BossPhase nextPhase, int maxHp) {
    float ratio = 0.0f;
    if (nextPhase == BossPhase::Phase2) ratio = kPhase2HpRatio;
    if (nextPhase == BossPhase::Phase3) ratio = kPhase3HpRatio;
    return static_cast<int>(std::floor(static_cast<float>(maxHp) * ratio + kHpBoundaryEpsilon));
}

// HP の残りから、いるべきフェーズ
BossPhase PhaseFromHp(int hp, int maxHp) {
    const float hpRatio = static_cast<float>(hp) / static_cast<float>(maxHp);
    if (hpRatio <= kPhase3HpRatio) return BossPhase::Phase3;
    if (hpRatio <= kPhase2HpRatio) return BossPhase::Phase2;
    return BossPhase::Phase1;
}
} // namespace

int BossComponent::GetNextPhaseBoundaryHp() const {
    switch (phase_) {
    case BossPhase::Phase1: return BoundaryHpOf(BossPhase::Phase2, maxHp_);
    case BossPhase::Phase2: return BoundaryHpOf(BossPhase::Phase3, maxHp_);
    case BossPhase::Phase3: return 0;
    }
    return 0;
}

void BossComponent::UpdatePhase() {
    // シールドの最中は、フェーズの判定を止める(シールドが終わったときに、FinishShield がフェーズを進める)
    if (maxHp_ <= 0 || shieldActive_) return;

    // HP が、いまのフェーズより先の境目を切ったら、すぐにはフェーズを進めず、シールドを始める。
    // 一度に 2 つの境目を越えても、1 段ずつ進める(まず次のフェーズのシールド。そのあと、残りの境目で、もう一度)
    if (static_cast<int>(PhaseFromHp(hp_, maxHp_)) > static_cast<int>(phase_)) {
        StartShield_(static_cast<BossPhase>(static_cast<int>(phase_) + 1));
    }
}

void BossComponent::StartShield_(BossPhase nextPhase) {
    // 境目を大きく越えて減っていたら、境目の値へ戻す(シールドを壊すと、次のフェーズから、境目の HP で始まる)
    hp_ = (std::max)(hp_, BoundaryHpOf(nextPhase, maxHp_));
    shieldActive_ = true;
    pendingState_ = std::make_unique<BossShieldState>(nextPhase);
}

void BossComponent::ApplyBlastDamage(int damage) {
    auto playerObj = FindPlayer();
    if (!playerObj) return;
    auto* player = playerObj->GetComponent<PlayerComponent>();
    // 回避の無敵の間は受けない(通常の被弾と同じ扱い)
    if (!player || player->IsDead() || player->IsInvincible()) return;
    player->TakeDamage(damage);
}

namespace {
// 発射位置(spawnPos)から速度(vel)の直進弾を1発生成して、窓口(spawner)からシーンへ出す
void SpawnBossBullet(AbsoluteEngine::ISpawner& spawner, const Vector3& spawnPos, const Vector3& vel) {
    BulletSpec spec;
    spec.name     = "BossBullet";
    spec.position = spawnPos;
    spec.velocity = vel;
    spec.visible  = false; // 球のモデルは付けない。見た目は、下の三日月の衝撃波(BossBulletVisualComponent)が描く
    auto bullet = BulletFactory::CreateEnemy(spec);
    bullet->AddComponent(std::make_unique<BossBulletVisualComponent>(vel));
    spawner.Spawn(bullet);
}
} // namespace

Vector3 BossComponent::GetBulletOrigin_() const {
    // 弾は、攻撃の演出(輪、粒)と同じ、口の前(muzzle ボーン)から出す。モデルにそのボーンが無いとき(鷲)は、ボスの位置から出す
    Vector3 muzzle{ 0.0f, 0.0f, 0.0f };
    const auto* model = GetOwner()->GetComponent<AbsoluteEngine::ModelComponent>();
    if (model && model->TryGetBoneWorldPosition(BossBone::kMuzzle, muzzle)) {
        return muzzle;
    }
    return GetOwner()->GetTransform().translate;
}

void BossComponent::SummonOrbs(int count, float appearSec, float chargeSec) {
    if (!GetOwner() || !screen_.HasCamera()) return;
    // 玉は、ボスの中心を基準に、画面に平行な面の上へ並べる(カメラは振り返らないので、必ず見える)
    orbGroup_.Summon(spawner_, count, appearSec, chargeSec,
                     GetOwner()->GetTransform().translate, screen_.GetRight(), screen_.GetActualUp());
}

void BossComponent::LaunchOrbs(float flightSec, float intervalSec) {
    const auto player = FindPlayer();
    if (!player) {
        orbGroup_.DismissHeld(); // 狙う相手がいないので、待っている玉は消す
        return;
    }
    orbGroup_.Launch(player, flightSec, intervalSec);
}

void BossComponent::FireAimedShot(float speed) {
    if (!GetOwner()) return;
    auto playerObj = FindPlayer();
    if (!playerObj) return;

    const Vector3 spawnPos = GetBulletOrigin_();
    Vector3 toPlayer = {
        playerObj->GetTransform().translate.x - spawnPos.x,
        playerObj->GetTransform().translate.y - spawnPos.y,
        playerObj->GetTransform().translate.z - spawnPos.z
    };
    const float len = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.y * toPlayer.y + toPlayer.z * toPlayer.z);
    if (len < 0.001f) return;
    toPlayer.x /= len; toPlayer.y /= len; toPlayer.z /= len;

    SpawnBossBullet(spawner_, spawnPos, { toPlayer.x * speed, toPlayer.y * speed, toPlayer.z * speed });
}

void BossComponent::FireSpreadShot(int count, float spreadAngleDeg, float speed) {
    if (!GetOwner() || count <= 0) return;
    auto playerObj = FindPlayer();
    if (!playerObj) return;

    const Vector3 spawnPos = GetBulletOrigin_();
    Vector3 toPlayer = {
        playerObj->GetTransform().translate.x - spawnPos.x,
        playerObj->GetTransform().translate.y - spawnPos.y,
        playerObj->GetTransform().translate.z - spawnPos.z
    };
    const float len = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.y * toPlayer.y + toPlayer.z * toPlayer.z);
    if (len < 0.001f) return;
    toPlayer.x /= len; toPlayer.y /= len; toPlayer.z /= len;

    // プレイヤーは主にXY平面（画面平面）上を動くため、Z軸周りに回転させて扇状に振り分ける。
    // 中心(0発分)から見て左右対称になるよう、-half〜+halfの角度で等間隔に配置する。
    const float spreadRad = spreadAngleDeg * 3.14159265f / 180.0f;
    const float half = static_cast<float>(count - 1) * 0.5f;
    for (int i = 0; i < count; ++i) {
        const float angle = (static_cast<float>(i) - half) * spreadRad;
        const float c = std::cos(angle);
        const float s = std::sin(angle);
        Vector3 dir = {
            toPlayer.x * c - toPlayer.y * s,
            toPlayer.x * s + toPlayer.y * c,
            toPlayer.z
        };
        SpawnBossBullet(spawner_, spawnPos, { dir.x * speed, dir.y * speed, dir.z * speed });
    }
}

void BossComponent::TakeDamage(int damage, const Vector3* hitPosition) {
    if (!isActive_) return;

    // いまの状態がダメージを受け止めるなら(シールド)、ボスの HP は減らさず、よろめきもしない。ドームには、被弾の波紋を出す
    if (state_ && state_->AbsorbDamage(*this, damage)) {
        if (hitPosition) shieldEffects_.AddHit(*hitPosition);
        return;
    }

    hp_ -= damage;
    if (hp_ <= 0) {
        hp_ = 0;
        // 撃破。オブジェクトはここでは消さない(撃破演出がボスの姿を使うため。消えるのはシーンの終わり)。
        // isActive_ が false になるので、以降は、移動・攻撃・被弾の処理が止まる。撃破の検知は IsActive() で行う
        isActive_ = false;
        attackEffects_.Stop(); // 撃破: 攻撃の演出(輪)を消す
        shieldEffects_.Stop();
        orbGroup_.DismissAll(); // 撃破: 誘導弾の玉を、飛行中のものも含めて、すべて消す
    } else {
        // 即死しなかった場合の被弾リアクション(よろめきのアニメーション。次の UpdateAnimation で再生する)
        hitRequested_ = true;
    }
}

void BossComponent::OnCollision(AbsoluteEngine::GameObject* other) {
    if (!isActive_ || !other) return;

    // ロックオン弾(ホーミング弾)は、ロック対象にしか当たらない
    if ((other->GetName().find("PlayerBullet") != std::string::npos || other->GetTag() == "PlayerBullet") &&
        HomingBulletComponent::CanHit(*other, GetOwner())) {
        const Vector3 hitPosition = other->GetTransform().translate;
        TakeDamage(1, &hitPosition);
    }
}
