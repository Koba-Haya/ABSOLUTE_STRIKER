#include "BossOrbGroup.h"
#include "BossOrbComponent.h"
#include "BossOrbTags.h"
#include "BossOrbVisualComponent.h"
#include "AbsoluteEngine/scene/ColliderComponent.h"
#include "AbsoluteEngine/scene/GameObject.h"
#include "AbsoluteEngine/scene/ISpawner.h"
#include <algorithm>

namespace {
// 玉の GameObject の名前。
// 名前に Boss を含めるので、自機に重なったとき、PlayerComponent が被弾のダメージを与える(自機の被弾の判定は、名前に Enemy か Boss を含むかで行う)。
// タグは BossOrbTags::kOrb。通常の敵(Enemy)と分けてあるので、撃破率などの集計は、玉を敵として数えない。
// 自機のロックオンと弾の当たり判定は、このタグを、LockOnTarget 経由で見て、玉を対象にする。
constexpr const char* kOrbName = "BossOrb";
}

void BossOrbGroup::Summon(AbsoluteEngine::ISpawner& spawner, int count, float appearSec, float chargeSec,
                          const Vector3& center, const Vector3& right, const Vector3& up) {
    // 待っている玉があれば、先に消し始める。発射した玉(飛行中)は、そのまま飛ばし、参照も残す(撃破のとき、一緒に消せるように)
    DismissHeld();

    for (int i = 0; i < count; ++i) {
        auto orb = std::make_shared<AbsoluteEngine::GameObject>(kOrbName);
        orb->SetTag(BossOrbTags::kOrb);

        auto motion = std::make_unique<BossOrbComponent>(i, count, appearSec, chargeSec);
        motion->SetAnchor(center, right, up);
        orb->AddComponent(std::move(motion));
        orb->AddComponent(std::make_unique<BossOrbVisualComponent>(spawner));

        // 当たり判定(球)。自機との重なりを調べるために付ける(玉は、飛行中だけ、重なりに反応する)
        AbsoluteEngine::ColliderComponent::Settings collider;
        collider.type = AbsoluteEngine::ColliderComponent::Type::Sphere;
        collider.radius = BossOrbDefaults::kColliderRadius;
        orb->AddComponent(std::make_unique<AbsoluteEngine::ColliderComponent>(collider));

        orb->SetPosition(center);

        spawner.Spawn(orb);
        orbs_.push_back(orb);
    }
}

void BossOrbGroup::Launch(const std::weak_ptr<AbsoluteEngine::GameObject>& target, float flightSec, float intervalSec) {
    // 待っている玉を、出した順に、順番の遅れ(0, interval, 2 x interval, ...)をつけて発射する
    int order = 0;
    for (const auto& weak : orbs_) {
        const auto orb = weak.lock();
        if (!orb) continue;
        auto* motion = orb->GetComponent<BossOrbComponent>();
        if (!motion || !motion->IsHeld()) continue;
        motion->Launch(target, static_cast<float>(order) * intervalSec, flightSec);
        ++order;
    }
}

void BossOrbGroup::Update(const Vector3& center, const Vector3& right, const Vector3& up) {
    // 消えた(シーンから取り除かれた)玉の参照を、先に取り除く
    orbs_.erase(std::remove_if(orbs_.begin(), orbs_.end(),
        [](const std::weak_ptr<AbsoluteEngine::GameObject>& weak) { return weak.expired(); }), orbs_.end());

    for (const auto& weak : orbs_) {
        const auto orb = weak.lock();
        if (!orb) continue;
        if (auto* motion = orb->GetComponent<BossOrbComponent>()) {
            motion->SetAnchor(center, right, up);
        }
    }
}

void BossOrbGroup::DismissHeld() {
    for (const auto& weak : orbs_) {
        const auto orb = weak.lock();
        if (!orb) continue;
        auto* motion = orb->GetComponent<BossOrbComponent>();
        if (motion && motion->IsHeld()) motion->Dismiss();
    }
}

void BossOrbGroup::DismissAll() {
    for (const auto& weak : orbs_) {
        const auto orb = weak.lock();
        if (!orb) continue;
        if (auto* motion = orb->GetComponent<BossOrbComponent>()) motion->Dismiss();
    }
}
