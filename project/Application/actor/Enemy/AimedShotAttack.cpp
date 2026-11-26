#include "AimedShotAttack.h"
#include "BossActions.h"

void AimedShotAttack::Fire(IBossActions& actions) const {
    actions.FireAimedShot(speed_);
}
