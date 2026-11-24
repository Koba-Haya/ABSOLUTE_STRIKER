#pragma once
#include "BaseEffect.h"
#include "graphics/primitive/Ring.h"
#include <memory>

class TextureResource;

class RingEffect : public BaseEffect {
public:
    // texture は shared_ptr で受け取り、Effect自身が生存期間だけ参照を保持する。
    // EffectManagerはシーンをまたいで生存するシングルトンのため、呼び出し元(GameScene等)が
    // 破棄された後もこのEffectが生きている間はテクスチャが解放されないようにするため。
    // デバイスはUpdate()同様、内部でRenderer::GetInstance()から取得するため引数に取らない
    RingEffect(std::shared_ptr<TextureResource> texture, const Vector3& position);
    ~RingEffect() override = default;

    void Draw() override;

protected:
    // 進行度 progress に応じて、拡大しながら透明になる(寿命の管理は BaseEffect::Update が行う)
    void OnUpdate(float progress, float deltaTime, const Camera* camera) override;

private:
    static constexpr float kLifetime = 0.5f; // 寿命(秒)

    // 板の向きを、指定のカメラの行列に合わせて作り、描画に使う行列と色を設定する。
    // Update(更新時のカメラ)と、Draw(描画時の、いま Renderer が持つカメラ)の両方で呼ぶ。
    // 更新と描画の間に、カメラが動いたり、ずらされたりしても、リングが描画のカメラへ正しく向く
    void ApplyTransform_(const Matrix4x4& viewMatrix, const Matrix4x4& projMatrix);

    Ring ring_;
    Ring::Params ringParams_{};
    std::shared_ptr<TextureResource> texture_;
    float maxOuterRadius_ = 4.0f;
    float alpha_ = 1.0f; // 今の透明度(Update で時間から決める)
};
