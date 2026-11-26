#include "PlayerDeathDirector.h"
#include "death/DeathCoverPhase.h"
#include "death/DeathHitStopPhase.h"
#include "death/DeathPhaseMath.h"
#include <algorithm>
#include <cmath>
#include <utility>

namespace {
// 演出が始まっていないときの、世界の時間の速さ(通常)
constexpr float kNormalTimeScale = 1.0f;
} // namespace

void PlayerDeathDirector::Begin(const PlayerDeathSettings& settings, const PlayerDeathStart& start) {
  if (IsActive()) return;
  settings_ = settings;
  start_ = start;
  elapsed_ = 0.0f;
  signals_ = DeathPhaseSignals{};
  ChangeTo_(std::make_unique<DeathHitStopPhase>(0.0f));
}

void PlayerDeathDirector::Reset() {
  phase_.reset();
  elapsed_ = 0.0f;
  signals_ = DeathPhaseSignals{};
}

float PlayerDeathDirector::GetPhaseEndTime(PlayerDeathPhase phase) const {
  // 外(死亡カメラのキーフレーム)が、段階の終わる時刻を、設定の表から前もって求めるための計算(今の段階には関係しない)
  float end = settings_.hitStopSeconds;
  if (phase == PlayerDeathPhase::HitStop) return end;
  end += settings_.crumbleSeconds;
  if (phase == PlayerDeathPhase::Crumble) return end;
  end += settings_.fallSeconds;
  if (phase == PlayerDeathPhase::Fall) return end;
  return end + settings_.impactSeconds;
}

void PlayerDeathDirector::ChangeTo_(std::unique_ptr<IDeathPhase> next) {
  phase_ = std::move(next);
  if (phase_) {
    phase_->Enter(signals_);
  }
}

void PlayerDeathDirector::Update(float deltaTime, bool canSkip, bool skipRequested) {
  if (!IsActive()) return;

  elapsed_ += deltaTime;

  // スキップ: 猶予を過ぎていれば、画面を覆う段階(遷移を要求する段階)へ直接飛ぶ。受け付けるかは、今の段階が答える。
  // 飛ばされる段階が、飛ばしても出すべき合図を立てる(ヒットストップなら、崩壊の始まり)
  if (canSkip && skipRequested && elapsed_ >= settings_.skipGuardSeconds && phase_->CanSkip()) {
    phase_->OnSkipped(signals_);
    ChangeTo_(std::make_unique<DeathCoverPhase>());
    return;
  }

  // 今の段階の時間を進める。次の段階が返されたら切り替え、切り替えた段階を時間 0 でもう一度進める
  // (1 フレームで複数の段階を越えても、余りを次へ持ち越す)
  std::unique_ptr<IDeathPhase> next = phase_->Advance(settings_, deltaTime);
  for (int i = 0; next && i < kMaxTransitionsPerFrame; ++i) {
    ChangeTo_(std::move(next));
    next = phase_->Advance(settings_, 0.0f);
  }
}

float PlayerDeathDirector::GetPhaseProgress() const {
  return phase_ ? phase_->GetProgress(settings_) : 0.0f;
}

float PlayerDeathDirector::GetWorldTimeScale() const {
  return phase_ ? phase_->GetWorldTimeScale(settings_, elapsed_) : kNormalTimeScale;
}

PlayerDeathPose PlayerDeathDirector::GetPlayerPose() const {
  PlayerDeathPose pose;
  pose.position = start_.position;
  pose.rotate = start_.rotate;
  if (!IsActive()) return pose;

  // デバッグ用: 位置も向きも動かさない
  if (settings_.debugHoldPlayerPosition) return pose;

  // ヒットストップの間は、死亡の瞬間の位置で止まる。落下は、そのあとの時間で数える
  const float t = std::max(0.0f, elapsed_ - settings_.hitStopSeconds);

  // 前へ進む勢いは、指数的に減る。距離 = 初速 * (1 - e^(-k t)) / k(速さの積分)
  const float k = settings_.forwardDecayPerSecond;
  const float forwardSpeed = start_.railSpeed * settings_.forwardSpeedScale;
  const float forwardDistance = k > 0.0f ? forwardSpeed * (1.0f - std::exp(-k * t)) / k : forwardSpeed * t;

  pose.position = {start_.position.x + start_.forward.x * forwardDistance,
                   start_.position.y + start_.forward.y * forwardDistance - FallDistance_(t),
                   start_.position.z + start_.forward.z * forwardDistance};

  // 機首が、だんだん下を向く(Transform の x まわりの正の回転が、機首を下げる向き)
  const float nose = settings_.noseDropSeconds > 0.0f ? DeathPhaseMath::SmoothStep(t / settings_.noseDropSeconds) : 1.0f;
  pose.rotate.x += settings_.noseDropRadians * nose;
  return pose;
}

Vector3 PlayerDeathDirector::GetPlayerVelocity() const {
  if (!IsActive() || settings_.debugHoldPlayerPosition) return {0.0f, 0.0f, 0.0f};
  const float t = std::max(0.0f, elapsed_ - settings_.hitStopSeconds);
  // 位置の式(GetPlayerPose)を、時間で微分したもの: 前へ進む速さは指数的に減り、下へ落ちる速さは重力に比例して増える
  const float forwardSpeed = start_.railSpeed * settings_.forwardSpeedScale * std::exp(-settings_.forwardDecayPerSecond * t);
  return {start_.forward.x * forwardSpeed, start_.forward.y * forwardSpeed - FallSpeed_(t), start_.forward.z * forwardSpeed};
}

float PlayerDeathDirector::LandingTime_() const {
  // ヒットストップのあと、崩壊と落下の終わりまで(雲の上面に着く時刻)
  return std::max(0.001f, settings_.crumbleSeconds + settings_.fallSeconds);
}

float PlayerDeathDirector::FallDistance_(float t) const {
  const float depth = std::max(0.0f, start_.position.y - settings_.landingHeight); // 着地まで落ちる高さ
  const float landing = LandingTime_();
  if (t <= landing) {
    // 着く時刻に、ちょうど depth だけ落ちる放物線: 落ち始めは 0 で、だんだん速くなる
    const float ratio = t / landing;
    return depth * ratio * ratio;
  }
  // 着いたあとは、着いた瞬間の速さ(2 * depth / landing)のまま、雲の中へ沈みながら、指数的に減速する
  const float landingSpeed = 2.0f * depth / landing;
  const float s = t - landing;
  const float k = settings_.sinkDecayPerSecond;
  const float sink = k > 0.0f ? landingSpeed * (1.0f - std::exp(-k * s)) / k : landingSpeed * s;
  return depth + sink;
}

float PlayerDeathDirector::FallSpeed_(float t) const {
  // FallDistance_ の、時間での微分
  const float depth = std::max(0.0f, start_.position.y - settings_.landingHeight);
  const float landing = LandingTime_();
  if (t <= landing) {
    return 2.0f * depth * t / (landing * landing);
  }
  const float landingSpeed = 2.0f * depth / landing;
  return landingSpeed * std::exp(-settings_.sinkDecayPerSecond * (t - landing));
}

float PlayerDeathDirector::GetCrumbleProgress() const {
  if (!HasCrumbleBegun_()) return 0.0f; // 始まっていない、またはヒットストップの間
  const float t = std::max(0.0f, elapsed_ - settings_.hitStopSeconds);
  return std::clamp(t / LandingTime_(), 0.0f, 1.0f);
}

PlayerDeathDissolve PlayerDeathDirector::GetDissolve() const {
  PlayerDeathDissolve dissolve;
  if (!HasCrumbleBegun_()) return dissolve; // 始まっていない、またはヒットストップの間は無効

  // ディゾルブの進み具合: 崩壊の始まりから、着地のあと kDissolveTailSeconds 秒後まで(胴は着地まで残り、沈みながら消える)
  const float t = std::max(0.0f, elapsed_ - settings_.hitStopSeconds);
  const float progress = std::clamp(t / (LandingTime_() + std::max(settings_.dissolveTailSeconds, 0.0f)), 0.0f, 1.0f);
  dissolve.enabled = true;
  dissolve.threshold = settings_.dissolveStartThreshold +
                       (settings_.dissolveEndThreshold - settings_.dissolveStartThreshold) * progress;
  dissolve.edgeRange = settings_.dissolveEdgeRange;
  dissolve.edgeColor = settings_.dissolveEdgeColor;
  dissolve.emberAmount = settings_.dissolveEmberAmount;
  dissolve.progress = progress;
  return dissolve;
}

bool PlayerDeathDirector::ConsumeCoverStart() {
  const bool pending = signals_.coverStarted;
  signals_.coverStarted = false;
  return pending;
}

bool PlayerDeathDirector::ConsumeCrumbleStart() {
  const bool pending = signals_.crumbleStarted;
  signals_.crumbleStarted = false;
  return pending;
}
