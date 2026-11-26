#include "BossAttackPatternFactory.h"
#include "AimedShotAttack.h"
#include "HomingOrbsAttack.h"
#include "SpreadShotAttack.h"
#include "Logger.h"

namespace BossAttackPatternFactory {

std::unique_ptr<IBossAttackPattern> Create(const BossAttackSpec& spec) {
    switch (spec.kind) {
    case BossAttackKind::Aimed:
        return std::make_unique<AimedShotAttack>(spec.speed);
    case BossAttackKind::Spread:
        return std::make_unique<SpreadShotAttack>(spec.count, spec.spreadAngleDeg, spec.speed);
    case BossAttackKind::HomingOrbs:
        return std::make_unique<HomingOrbsAttack>(spec.count, spec.telegraphSec, spec.chargeSec, spec.flightSec, spec.launchIntervalSec);
    }
    Logger::GetInstance().Log("[BossAttackPatternFactory] unknown attack kind=%d. use Aimed", static_cast<int>(spec.kind));
    return std::make_unique<AimedShotAttack>(spec.speed);
}

} // namespace BossAttackPatternFactory
