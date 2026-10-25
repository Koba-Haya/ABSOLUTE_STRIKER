#include "PlayerAnimator.h"
#include "AbsoluteEngine/scene/ModelComponent.h"
#include "PlayerModelDefs.h"

void PlayerAnimator::Reset() {
  state_ = State::Normal;
  hitRequested_ = false;
  deathRequested_ = false;
}

void PlayerAnimator::Update(AbsoluteEngine::ModelComponent* model) {
  if (!model) return;

  // 死亡: きりもみをループ再生する。以降は、被弾の通知も、羽ばたきへの復帰も行わない
  if (deathRequested_) {
    deathRequested_ = false;
    hitRequested_ = false;
    model->PlayAnimation(PlayerAnimation::kTumble, true, PlayerAnimatorDefaults::kDeathBlendSeconds);
    state_ = State::Dying;
    return;
  }
  if (state_ == State::Dying) return;

  // 被弾の通知があれば、Hit(ループしない)を再生する。Hit 中にもう一度被弾しても、よろめきは再開せず、続けて再生する
  // (同じアニメーションの再生中は、ModelInstance が再トリガーしない)
  if (hitRequested_) {
    hitRequested_ = false;
    model->PlayAnimation(PlayerAnimation::kHit, false, PlayerAnimatorDefaults::kHitBlendSeconds);
    state_ = State::Hit;
    return;
  }

  // Hit が最後まで再生されたら、羽ばたきへ戻る
  if (state_ == State::Hit && model->IsAnimationFinished()) {
    model->PlayAnimation(PlayerAnimation::kFlap, true, PlayerAnimatorDefaults::kReturnBlendSeconds);
    state_ = State::Normal;
  }
}
