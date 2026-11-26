#include "EnergyOrb.h"
#include <algorithm>

bool EnergyOrb::Initialize(ID3D12Device* device) {
    return quad_.Initialize(device, sizeof(ParamsCB));
}

void EnergyOrb::SetParams(const Params& params) {
    ParamsCB cb{};
    cb.bodyColor = params.bodyColor;
    cb.glowColor = params.glowColor;
    cb.coreColor = params.coreColor;
    cb.outlineColor = params.outlineColor;
    cb.time = params.time;
    cb.seed = params.seed;
    cb.appear = std::clamp(params.appear, 0.0f, 1.0f);
    cb.charge = std::clamp(params.charge, 0.0f, 1.0f);
    // Windows.h の max マクロと衝突しないよう、(std::max) と括弧で囲む
    cb.orbRadius = (std::max)(params.orbRadius, 0.01f);
    cb.haloStrength = (std::max)(params.haloStrength, 0.0f);
    cb.intensity = (std::max)(params.intensity, 0.0f);
    cb.alpha = std::clamp(params.alpha, 0.0f, 1.0f);
    cb.flowX = params.flow.x;
    cb.flowY = params.flow.y;
    cb.stretch = (std::max)(params.stretch, 0.0f);
    quad_.SetParamsData(&cb, sizeof(cb));
}
