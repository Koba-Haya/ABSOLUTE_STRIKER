#include "BossEffectParticles.h"
#include "AbsoluteEngine/graphics/particle/ParticleManager.h"

namespace BossEffectParticles {
namespace {
// 無ければ作る(アルファ合成の側、かつ、雲のあとに描く)
void EnsureGroup(const char* name, const char* texturePath, uint32_t maxInstances) {
    auto* particles = ParticleManager::GetInstance();
    if (!particles->HasParticleGroup(name)) {
        particles->CreateParticleGroup(name, texturePath, maxInstances, /*softAlpha=*/true, /*afterClouds=*/true);
    }
}
} // namespace

void EnsureGroups() {
    EnsureGroup(kGlowGroupName, kGlowTexturePath, kGlowMaxInstances);
    EnsureGroup(kFlareGroupName, kFlareTexturePath, kFlareMaxInstances);
    EnsureGroup(kStreakGroupName, kStreakTexturePath, kStreakMaxInstances);
    EnsureGroup(kRingGroupName, kRingTexturePath, kRingMaxInstances);
}
} // namespace BossEffectParticles
