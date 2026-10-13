#pragma once
#include <memory>
#include <vector>
#include "ModelInstance.h"
#include "ModelResource.h"
#include "TextureResource.h"
#include "Type/Vector.h"

class BaseScene;
class Renderer;

// 敵撃破時のヒットエフェクト（パーティクル＋リングエフェクト＋爆発ライト＋旧型スケールアニメーション）。
// 発生・更新・描画を1クラスにまとめ、GameScene からは呼ぶだけにする。
class HitEffectSpawner {
public:
  void Initialize();

  // pos にヒットエフェクトを発生させる。爆発ライトは scene に追加される。
  void Spawn(BaseScene& scene, const Vector3& pos);

  // 旧型スケールアニメーションを1フレーム進める
  void Update();
  void Draw(Renderer* renderer);

private:
  // 旧型：モデルを拡大しながらフェードアウトさせる
  struct ScaleEffect {
    ScaleEffect() = default;
    ~ScaleEffect() = default;
    ScaleEffect(ScaleEffect&&) noexcept = default;
    ScaleEffect& operator=(ScaleEffect&&) noexcept = default;

    ModelInstance instance;
    float frame = 0.0f;
    float maxFrame = 20.0f;
    bool isActive = false;
    Vector3 position;
  };

  std::shared_ptr<ModelResource> resEffect_;
  std::shared_ptr<TextureResource> texRing_;
  std::vector<ScaleEffect> scaleEffects_;
};
