#include "EnergyRibbon.h"
#include <algorithm>
#include <cstring>

namespace {
// 定数バッファは 256 バイト単位で確保する(D3D12 の決まり)。1 つの枠も、この大きさにする
constexpr UINT64 kSlotSize = 256;
}

bool EnergyRibbonParamsRing::Initialize(ID3D12Device* device, uint32_t maxSlots) {
    if (!device || maxSlots == 0) return false;

    D3D12_HEAP_PROPERTIES heapProps{};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = kSlotSize * maxSlots;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    const HRESULT hr = device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&buffer_));
    if (FAILED(hr)) return false;

    void* mapped = nullptr;
    if (FAILED(buffer_->Map(0, nullptr, &mapped)) || !mapped) return false;
    mapped_ = static_cast<uint8_t*>(mapped);
    maxSlots_ = maxSlots;
    usedSlots_ = 0;
    return true;
}

D3D12_GPU_VIRTUAL_ADDRESS EnergyRibbonParamsRing::Write(const EnergyRibbonStyle& style) {
    if (!mapped_ || usedSlots_ >= maxSlots_) return 0;

    ParamsCB cb{};
    cb.glowColor = style.glowColor;
    cb.coreColor = style.coreColor;
    cb.outlineColor = style.outlineColor;
    cb.time = style.time;
    cb.seed = style.seed;
    // Windows.h の max マクロと衝突しないよう、(std::max) と括弧で囲む
    cb.intensity = (std::max)(style.intensity, 0.0f);
    cb.flowSpeed = style.flowSpeed;

    const UINT64 offset = kSlotSize * usedSlots_;
    std::memcpy(mapped_ + offset, &cb, sizeof(cb));
    ++usedSlots_;
    return buffer_->GetGPUVirtualAddress() + offset;
}
