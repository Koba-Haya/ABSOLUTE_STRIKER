#define NOMINMAX // Windows の min / max マクロが std::min / std::max と衝突するのを防ぐ
#include "PlayerDeathDust.h"

#include "PlayerDeathDirector.h"
#include "../actor/Player/WingShatter.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr float kPi = 3.14159265f;
} // namespace

void PlayerDeathDust::Update(bool active, const PlayerDeathDissolve& dissolve, const WingShatter& shatter,
                             const Vector3& bodyPosition, const Vector3& bodyVelocity) {
  // 最初の呼び出しで、エミッタを借りて、前の演出の粒を消す(シーンの開始時と同じ)
  if (!dust_.IsInitialized()) {
    dust_.Initialize(dust_.GetSettings());
  }

  if (active) {
    // 1. 破片が解放された瞬間: 破片の位置から、大きさに応じた量を、飛ぶ向きへ少し押し出して出す
    for (const WingShardRelease& release : shatter.GetReleasesThisFrame()) {
      const float scaledCount = static_cast<float>(settings_.shardBurstBase) + settings_.shardBurstPerTriangle * static_cast<float>(release.triangleCount);
      const uint32_t count = std::min(static_cast<uint32_t>(scaledCount), settings_.shardBurstMax);
      const Vector3 velocity{bodyVelocity.x + release.direction.x * settings_.shardKickSpeed,
                             bodyVelocity.y + release.direction.y * settings_.shardKickSpeed,
                             bodyVelocity.z + release.direction.z * settings_.shardKickSpeed};
      dust_.Burst(release.position, count, settings_.shardBurstRadius, velocity);
    }

    // 2. 朽ちる縁: ディゾルブが進んでいる間、翅のまわりから続けて出す。
    // 進みの半ばで最も多く(sin の山)、縁が内側へ寄るにつれて、発生源の球を縮める
    if (dissolve.enabled && dissolve.progress < 1.0f) {
      const float progress = std::clamp(dissolve.progress, 0.0f, 1.0f);
      const float rate = settings_.edgeRatePeak * std::sin(kPi * progress);
      const float radius = settings_.bodyRadius * (1.0f - settings_.edgeShrink * progress);
      dust_.SetAmbient(bodyPosition, radius, rate, bodyVelocity);
    }
  }

  dust_.Update();
}

void PlayerDeathDust::Shutdown() { dust_.Shutdown(); }
