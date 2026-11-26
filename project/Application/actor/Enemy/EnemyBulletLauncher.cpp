#include "EnemyBulletLauncher.h"

#include "AbsoluteEngine/scene/GameObject.h"
#include "AbsoluteEngine/scene/ISpawner.h"
#include "Logger.h"
#include "../Bullet/BulletBuilder.h"
#include "../Bullet/BulletCatalog.h"
#include "../Bullet/BulletTag.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace {
// JSON の項目名
constexpr const char *kKeyBulletId = "bulletId";
} // namespace

std::shared_ptr<AbsoluteEngine::GameObject> EnemyBulletLauncher::Fire(AbsoluteEngine::ISpawner &spawner, const Vector3 &position,
                                                                      const Vector3 &velocity) {
  const BulletDefinition *def = ResolveDefinition_();
  if (!def) return nullptr;

  // 定義から組み立てて、今回の位置と速度を入れて出す
  std::shared_ptr<AbsoluteEngine::GameObject> bullet = BulletBuilder::Build(*def, BulletTag::kEnemy);
  BulletBuilder::Launch(*bullet, position, velocity);
  spawner.Spawn(bullet);
  return bullet;
}

void EnemyBulletLauncher::Serialize(nlohmann::json &j) const {
  j[kKeyBulletId] = bulletId_;
}

void EnemyBulletLauncher::Deserialize(const nlohmann::json &j) {
  SetBulletId_(j.value(kKeyBulletId, bulletId_));
}

bool EnemyBulletLauncher::DrawInspectorUI() {
#ifdef USE_IMGUI
  bool changed = false;
  // 未設定のときは、選ばれていないことが分かる文字を出す
  const char *preview = bulletId_.empty() ? "(none)" : bulletId_.c_str();
  if (ImGui::BeginCombo("Bullet", preview)) {
    for (const BulletDefinition &def : catalog_.GetBullets()) {
      const bool selected = (def.id == bulletId_);
      if (ImGui::Selectable(def.id.c_str(), selected) && !selected) {
        SetBulletId_(def.id);
        changed = true;
      }
      if (selected) ImGui::SetItemDefaultFocus();
    }
    ImGui::EndCombo();
  }
  return changed;
#else
  return false;
#endif
}

void EnemyBulletLauncher::SetBulletId_(const std::string &bulletId) {
  if (bulletId == bulletId_) return;
  bulletId_ = bulletId;
  reportedMissing_ = false;
}

const BulletDefinition *EnemyBulletLauncher::ResolveDefinition_() {
  const BulletDefinition *def = catalog_.Find(bulletId_);
  if (!def && !reportedMissing_) {
    // プレハブに bulletId が無い、または bullets.json に無い id を指している。撃たずに、理由を 1 回だけ出す
    Logger::GetInstance().Log("[EnemyBulletLauncher] bullet \"%s\" is not defined in bullets.json. this enemy does not fire", bulletId_.c_str());
    reportedMissing_ = true;
  }
  return def;
}
