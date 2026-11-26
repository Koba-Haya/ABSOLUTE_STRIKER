#include "ClearDirector.h"
#include "clear/ClearPhaseMath.h"
#include "clear/CoverPhase.h"
#include "clear/HitStopPhase.h"
#include <utility>

void ClearDirector::Begin(const ClearSettings &settings) {
  if (IsActive()) return;
  settings_ = settings;
  elapsed_ = 0.0f;
  signals_ = ClearPhaseSignals{};
  ChangeTo_(std::make_unique<HitStopPhase>(0.0f));
}

void ClearDirector::Reset() {
  phase_.reset();
  elapsed_ = 0.0f;
  signals_ = ClearPhaseSignals{};
}

float ClearDirector::GetPhaseEndTime(ClearPhase phase) const {
  // 外(カメラのキーフレームなど)が、段階の終わる時刻を、設定の表から前もって求めるための計算(今の段階には関係しない)
  float end = settings_.hitStopSeconds;
  if (phase == ClearPhase::HitStop) return end;
  end += settings_.crackSeconds;
  if (phase == ClearPhase::Crack) return end;
  end += settings_.peelSeconds;
  if (phase == ClearPhase::Peel) return end;
  end += settings_.burstSeconds;
  if (phase == ClearPhase::Burst) return end;
  end += settings_.flowSeconds;
  if (phase == ClearPhase::Flow) return end;
  return end + settings_.releaseSeconds; // Release と Cover は、同じ時刻(Release が終わったところで Cover へ入る)
}

void ClearDirector::ChangeTo_(std::unique_ptr<IClearPhase> next) {
  phase_ = std::move(next);
  if (phase_) {
    phase_->Enter(signals_);
  }
}

void ClearDirector::Update(float deltaTime, bool canSkip, bool skipRequested) {
  if (!IsActive()) return;

  elapsed_ += deltaTime;

  // スキップ: 猶予を過ぎていれば、画面を覆う段階(遷移を要求する段階)へ直接飛ぶ。受け付けるかは、今の段階が答える
  if (canSkip && skipRequested && elapsed_ >= settings_.skipGuardSeconds && phase_->CanSkip()) {
    ChangeTo_(std::make_unique<CoverPhase>());
    return;
  }

  // 今の段階の時間を進める。次の段階が返されたら切り替え、切り替えた段階を時間 0 でもう一度進める
  // (1 フレームで複数の段階を越えても、余りを次へ持ち越す)
  std::unique_ptr<IClearPhase> next = phase_->Advance(settings_, deltaTime);
  for (int i = 0; next && i < kMaxTransitionsPerFrame; ++i) {
    ChangeTo_(std::move(next));
    next = phase_->Advance(settings_, 0.0f);
  }
}

float ClearDirector::GetPhaseProgress() const {
  return phase_ ? phase_->GetProgress(settings_) : 0.0f;
}

float ClearDirector::GetWorldTimeScale() const {
  return phase_ ? phase_->GetWorldTimeScale(settings_, elapsed_) : ClearPhaseMath::kNormalTimeScale;
}

bool ClearDirector::ConsumeCoverStart() {
  const bool pending = signals_.coverStarted;
  signals_.coverStarted = false;
  return pending;
}
