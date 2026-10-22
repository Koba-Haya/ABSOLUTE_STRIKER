#pragma once

#include "Vector.h"

#include <cstdint>
#include <string>

class ParticleManager;

enum class EmitterShape {
    Box = 0,
    Sphere = 1
};

enum class ParticleColorMode : int {
    RandomRGB = 0,
    RangeRGB = 1,
    RangeHSV = 2,
    Fixed = 3
};

// ParticleEmitter
// - 発生頻度や発生範囲など「発生ロジック」だけを担当
// - GPU/Draw 周りは ParticleManager に任せる（Emitを呼ぶ）
class ParticleEmitter {
public:
    struct Params {
        std::string groupName;

        EmitterShape shape = EmitterShape::Box;

        // 発生中心（親位置 + localCenter）
        Vector3 localCenter{ 0,0,0 };

        // Box: 半径（AABBの半分） / Sphere: xyz別スケール半径
        Vector3 extent{ 1,1,1 };

        Vector3 baseDir{ 0,1,0 };
        float dirRandomness = 0.5f;

        float speedMin = 0.5f;
        float speedMax = 2.0f;

        float lifeMin = 1.0f;
        float lifeMax = 3.0f;

        Vector3 particleScale{ 0.5f, 0.5f, 0.5f };

        float emitRate = 10.0f; // 1秒あたり

        ParticleColorMode colorMode = ParticleColorMode::RandomRGB;

        // Fixed / ベース色（Range系でもベースとして使う）
        Vector4 baseColor{ 1,1,1,1 };

        // RangeRGB: baseColor.rgb ± rgbRange（0..1にクランプ）
        Vector3 rgbRange{ 0.0f, 0.0f, 0.0f };

        // RangeHSV: baseHSV ± hsvRange（hはwrap、s/vはclamp）
        // h: 0..1, s:0..1, v:0..1
        Vector3 baseHSV{ 0.0f, 1.0f, 1.0f };
        Vector3 hsvRange{ 0.0f, 0.0f, 0.0f };

        float rotateMin = -3.14159265f;
        float rotateMax = 3.14159265f;
    };

    ParticleEmitter() = default;
    ParticleEmitter(ParticleManager* manager, const Params& params);

    void Initialize(ParticleManager* manager, const Params& params);

    // 親Transformに追従したい場合は parentTranslate を渡す
    void Update(float deltaTime, const Vector3& parentTranslate = { 0,0,0 });

    // その場で即時発生（発生頻度とは別）
    void Burst(uint32_t count, const Vector3& parentTranslate = { 0,0,0 });

    // 発生パラメータを取得する（読み取り専用）。変更は Initialize で渡し直す
    const Params& GetParams() const { return params_; }

private:
    Vector3 SamplePosition_(const Vector3& parentTranslate) const;
    Vector3 SampleVelocity_() const;
    Vector4 SampleColor_() const;

    float Rand01_() const;
    float RandRange_(float minV, float maxV) const;

private:
    ParticleManager* manager_ = nullptr;
    Params params_{};
    float emitAccum_ = 0.0f;
};
