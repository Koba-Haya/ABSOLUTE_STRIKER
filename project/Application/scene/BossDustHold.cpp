#include "BossDustHold.h"

#include "AbsoluteEngine/graphics/particle/GPUParticleManager.h"

namespace {
// 渦の軸(ワールドの上向き。ボスのまわりを、水平に回る)
const Vector3 kSwirlAxis{0.0f, 1.0f, 0.0f};
// 島へ向かう軸の長さ(の 2 乗)の下限。これ以下は、向きが決められない
constexpr float kMinAxisLengthSquared = 1e-6f;

// 力を 1 つ、枠へ書く
void WriteField(int slot, GPUParticleManager::FieldType type, const Vector3 &target, const Vector3 &direction, float strength) {
  GPUParticleManager::FieldDesc desc;
  desc.type = type;
  desc.target = target;
  desc.direction = direction;
  desc.strength = strength;
  GPUParticleManager::GetInstance()->SetField(slot, desc);
}

// 枠を空にする
void EmptyField(int slot) {
  GPUParticleManager::GetInstance()->SetField(slot, GPUParticleManager::FieldDesc{}); // type は None
}
} // namespace

void BossDustHold::Begin(const BossDustHoldSettings &settings) {
  settings_ = settings;
  Clear();
}

void BossDustHold::Update(ClearPhase phase, float phaseProgress, const Vector3 &center, const Vector3 *flightTarget) {
  using namespace BossDustHoldDefaults;
  using Type = GPUParticleManager::FieldType;
  const Vector3 noDirection{0.0f, 0.0f, 0.0f};

  if (phase == ClearPhase::Peel) {
    // 溜める: 中心へ引くばねと、渦と、空気抵抗。外向きに押す力は、まだ入れない
    WriteField(kSlotSpring, Type::Spring, center, noDirection, settings_.holdSpring);
    WriteField(kSlotSwirl, Type::Vortex, center, kSwirlAxis, settings_.holdSwirl);
    WriteField(kSlotDrag, Type::Drag, noDirection, noDirection, settings_.holdDrag);
    EmptyField(kSlotRelease);
  } else if ((phase == ClearPhase::Flow || phase == ClearPhase::Release) && flightTarget != nullptr) {
    // 次の島へ飛ぶ: 目的地へ引く一定の力(Attractor。外向きに押す枠を、引く向きで使う)+ 弱いばね + 島へ向かう軸まわりの渦 + 空気抵抗
    WriteField(kSlotRelease, Type::Attractor, *flightTarget, noDirection, settings_.flightPull);
    WriteField(kSlotSpring, Type::Spring, *flightTarget, noDirection, settings_.flightSpring);
    // 渦の軸: 体の中心から島へ向かう向き。長さが 0 に近く、向きが決められないときは、渦を入れない
    const Vector3 axis{flightTarget->x - center.x, flightTarget->y - center.y, flightTarget->z - center.z};
    if (axis.x * axis.x + axis.y * axis.y + axis.z * axis.z > kMinAxisLengthSquared) {
      WriteField(kSlotSwirl, Type::Vortex, *flightTarget, axis, settings_.flightSwirl);
    } else {
      EmptyField(kSlotSwirl);
    }
    WriteField(kSlotDrag, Type::Drag, noDirection, noDirection, settings_.flightDrag);
  } else if (phase == ClearPhase::Burst || phase == ClearPhase::Flow || phase == ClearPhase::Release || phase == ClearPhase::Cover) {
    // 解き放つ: ばねと渦を外す。Burst の始めの間だけ、中心から外向きに押す(負の強さの Attractor)。そのあとは、ゆるい空気抵抗だけ
    EmptyField(kSlotSpring);
    EmptyField(kSlotSwirl);
    WriteField(kSlotDrag, Type::Drag, noDirection, noDirection, settings_.coastDrag);
    const bool pushing = phase == ClearPhase::Burst && phaseProgress < settings_.releaseShare;
    if (pushing) {
      WriteField(kSlotRelease, Type::Attractor, center, noDirection, -settings_.releaseStrength);
    } else {
      EmptyField(kSlotRelease);
    }
  }
  // それ以外(Idle、HitStop、Crack)は、何もしない(枠は、Begin の時点で空)
}

void BossDustHold::Clear() {
  EmptyField(BossDustHoldDefaults::kSlotSpring);
  EmptyField(BossDustHoldDefaults::kSlotSwirl);
  EmptyField(BossDustHoldDefaults::kSlotDrag);
  EmptyField(BossDustHoldDefaults::kSlotRelease);
}
