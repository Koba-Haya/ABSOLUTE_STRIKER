#include "AshGatheringState.h"
#include "AshDriftingState.h"
#include "AshGlowingState.h"
#include <algorithm>

namespace {
// 集まりきった進み
constexpr float kGatherCompleted = 1.0f;
} // namespace

std::unique_ptr<IAshState> AshGatheringState::Update(const AshStateContext &context) {
  if (context.ShouldReturnToDrifting()) {
    return std::make_unique<AshDriftingState>();
  }
  gatherClock_ += context.gatherDeltaSeconds;
  if (GetGatherProgress(context.gatherLengthSeconds) >= kGatherCompleted) {
    return std::make_unique<AshGlowingState>();
  }
  return nullptr;
}

float AshGatheringState::GetGatherProgress(float gatherLengthSeconds) const {
  return std::clamp(gatherClock_ / gatherLengthSeconds, 0.0f, kGatherCompleted);
}
