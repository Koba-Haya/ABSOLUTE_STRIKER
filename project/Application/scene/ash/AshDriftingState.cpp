#include "AshDriftingState.h"
#include "AshGatheringState.h"

std::unique_ptr<IAshState> AshDriftingState::Update(const AshStateContext &context) {
  if (context.ShouldStartGathering()) {
    // 漂いは時間を使わないので、このフレームの集まりの時間は、まるごと集まりへ渡す(以前も、集まり始めたフレームで時計を進めていた)
    return std::make_unique<AshGatheringState>(context.gatherDeltaSeconds);
  }
  return nullptr;
}

std::unique_ptr<IAshState> AshDriftingState::OnGatherRequested() {
  // 漂っているときだけ、頼まれた瞬間から集まり始める(集まりの最初から)
  return std::make_unique<AshGatheringState>(0.0f);
}
