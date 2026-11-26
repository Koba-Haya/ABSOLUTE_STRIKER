#include "AshGlowingState.h"
#include "AshDriftingState.h"

std::unique_ptr<IAshState> AshGlowingState::Update(const AshStateContext &context) {
  if (context.ShouldReturnToDrifting()) {
    return std::make_unique<AshDriftingState>();
  }
  return nullptr;
}
