#pragma once
#include "ITelegraphGlow.h"
#include <memory>

namespace AbsoluteEngine {
class ISpawner;
}

// 光り方: 弾の出口に光る玉を出し、予備動作の間、大きくする(蜂の針、蜘蛛の口など)
class MuzzleOrbGlow final : public ITelegraphGlow {
public:
    // 設定の束(TelegraphComponent の設定から渡す。値を並べただけのデータの入れ物)
    struct Settings {
        Vector3 muzzleOffset;   // 弾の出口の位置(敵のモデルの座標。敵の拡大率を掛けて使う)
        Vector4 glowColor;      // 光る玉の色
        float orbScaleStart;    // 光る玉の大きさ(予備動作の最初。拡大率 1 の敵のとき)
        float orbScaleEnd;      // 光る玉の大きさ(予備動作の最後)
    };

    // spawner: 光る玉をシーンへ出す窓口(所有しない。このクラスより長く生きること)
    MuzzleOrbGlow(AbsoluteEngine::ISpawner& spawner, const Settings& settings) : spawner_(spawner), settings_(settings) {}
    // 光る玉が残っていれば消す(敵が消えたとき、設定の読み直しで作り直されたとき)
    ~MuzzleOrbGlow() override;

    void OnBegin(AbsoluteEngine::GameObject& owner, float progress) override;
    void OnUpdate(AbsoluteEngine::GameObject& owner, float progress) override;
    void OnEnd(AbsoluteEngine::GameObject* owner, const Vector4& baseColor) override;

private:
    // 光る玉を作る(すでにあれば何もしない)
    void CreateOrb_();
    // 光る玉を消す
    void DestroyOrb_();

    AbsoluteEngine::ISpawner& spawner_;           // 光る玉を出す窓口
    Settings settings_;                           // 設定
    std::weak_ptr<AbsoluteEngine::GameObject> orb_; // 光る玉
};
