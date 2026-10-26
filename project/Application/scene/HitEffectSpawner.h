#pragma once
#include <memory>
#include "TextureResource.h"
#include "Type/Vector.h"

class BaseScene;

// 敵撃破時のヒットエフェクト（パーティクル＋リングエフェクト＋爆発ライト）。
// 発生を1クラスにまとめ、GameScene からは呼ぶだけにする。
class HitEffectSpawner {
public:
  void Initialize();

  // pos にヒットエフェクトを発生させる。爆発ライトは scene に追加される。
  void Spawn(BaseScene& scene, const Vector3& pos);

private:
  std::shared_ptr<TextureResource> texRing_;
};
