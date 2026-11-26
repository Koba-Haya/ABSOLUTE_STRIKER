#include "ChargeWindupState.h"
#include "ChargeAimState.h"
#include "../TelegraphComponent.h"
#include "AbsoluteEngine/scene/GameObject.h"
#include <algorithm>

namespace {
// 予備動作が終わった進み具合
constexpr float kWindupCompleted = 1.0f;
} // namespace

void ChargeWindupState::Enter(const ChargeStateContext& context) {
    if (context.telegraph) context.telegraph->Begin();
}

std::unique_ptr<IChargeState> ChargeWindupState::Update(const ChargeStateContext& context) {
    timer_ += context.deltaTime;
    const float duration = context.telegraph ? context.telegraph->GetDuration() : 0.0f;
    const float progress = (duration > 0.0f) ? std::min(timer_ / duration, kWindupCompleted) : kWindupCompleted;
    // 自機と反対へ引く。予備動作の終わりに向けて、だんだん遅くなる
    const float pull = context.settings.windupPullSpeed * (kWindupCompleted - progress) * context.deltaTime;
    context.owner.Translate({ awayDir_.x * pull, awayDir_.y * pull, awayDir_.z * pull });
    if (progress >= kWindupCompleted) {
        if (context.telegraph) context.telegraph->End();
        return std::make_unique<ChargeAimState>();
    }
    return nullptr;
}
