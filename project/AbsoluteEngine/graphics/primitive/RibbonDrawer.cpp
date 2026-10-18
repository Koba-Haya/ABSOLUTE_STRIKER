#include "RibbonDrawer.h"
#include <cstring>

RibbonDrawer::~RibbonDrawer() {
    if (vertexBuffer_) {
        vertexBuffer_->Unmap(0, nullptr);
    }
}

void RibbonDrawer::Initialize(ID3D12Device* device, uint32_t maxVertices) {
    maxVertices_ = maxVertices;
    usedVertices_ = 0;

    const UINT64 sizeInBytes = sizeof(RibbonVertex) * static_cast<UINT64>(maxVertices);

    D3D12_HEAP_PROPERTIES heapProps{};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD; // CPU から書けるヒープ

    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = sizeInBytes;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&vertexBuffer_));

    vertexBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&mappedVertices_));
}

void RibbonDrawer::Reset() {
    usedVertices_ = 0;
}

void RibbonDrawer::Draw(ID3D12GraphicsCommandList* cmdList, const RibbonTrail& trail, const Vector3& cameraPosition) {
    if (!vertexBuffer_) return;

    trail.BuildVertices(cameraPosition, scratch_);
    if (scratch_.empty()) return;

    const uint32_t count = static_cast<uint32_t>(scratch_.size());
    if (usedVertices_ + count > maxVertices_) return; // バッファの空きが足りない

    // 共有バッファの空き位置(usedVertices_)へ追記する。
    // 前に描いたリボンの頂点は上書きしない(GPU はコマンドの実行時にまとめて読むため)
    std::memcpy(mappedVertices_ + usedVertices_, scratch_.data(), sizeof(RibbonVertex) * count);

    D3D12_VERTEX_BUFFER_VIEW view{};
    view.BufferLocation = vertexBuffer_->GetGPUVirtualAddress() + sizeof(RibbonVertex) * usedVertices_;
    view.SizeInBytes = static_cast<UINT>(sizeof(RibbonVertex) * count);
    view.StrideInBytes = sizeof(RibbonVertex);

    cmdList->IASetVertexBuffers(0, 1, &view);
    // 左・右・左・右...と並んだ頂点を、隣り合う3点ずつで三角形にする
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    cmdList->DrawInstanced(count, 1, 0, 0);

    usedVertices_ += count;
}
