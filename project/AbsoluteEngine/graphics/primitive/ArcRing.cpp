#include "ArcRing.h"
#include "Method.h"
#include <algorithm>
#include <cstring>

namespace {
// 定数バッファは 256 バイト単位で確保する(D3D12 の決まり)
constexpr UINT64 kConstantBufferAlignment = 256;
// 板の頂点数とインデックス数(正方形 1 枚 = 三角形 2 つ)
constexpr UINT kQuadVertexCount = 4;
constexpr UINT kQuadIndexCount = 6;
// 板の半分の長さを 1 としたときの、角の座標
constexpr float kQuadHalf = 1.0f;
}

Microsoft::WRL::ComPtr<ID3D12Resource> ArcRing::CreateUploadBuffer_(ID3D12Device* device, UINT64 size) {
    D3D12_HEAP_PROPERTIES heapProps{};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = size;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    Microsoft::WRL::ComPtr<ID3D12Resource> buffer;
    const HRESULT hr = device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&buffer));
    if (FAILED(hr)) return nullptr;
    return buffer;
}

bool ArcRing::Initialize(ID3D12Device* device) {
    if (!device) return false;

    // 板(XY 平面。時計回りが表。裏から見ても描くので、向きは気にしない)
    const Vertex vertices[kQuadVertexCount] = {
        { { -kQuadHalf,  kQuadHalf, 0.0f, 1.0f }, { -kQuadHalf,  kQuadHalf } },
        { {  kQuadHalf,  kQuadHalf, 0.0f, 1.0f }, {  kQuadHalf,  kQuadHalf } },
        { { -kQuadHalf, -kQuadHalf, 0.0f, 1.0f }, { -kQuadHalf, -kQuadHalf } },
        { {  kQuadHalf, -kQuadHalf, 0.0f, 1.0f }, {  kQuadHalf, -kQuadHalf } },
    };
    const uint32_t indices[kQuadIndexCount] = { 0, 1, 2, 2, 1, 3 };

    vertexBuffer_ = CreateUploadBuffer_(device, sizeof(vertices));
    indexBuffer_ = CreateUploadBuffer_(device, sizeof(indices));
    transformCB_ = CreateUploadBuffer_(device, (sizeof(TransformCB) + kConstantBufferAlignment - 1) & ~(kConstantBufferAlignment - 1));
    paramsCB_ = CreateUploadBuffer_(device, (sizeof(ParamsCB) + kConstantBufferAlignment - 1) & ~(kConstantBufferAlignment - 1));
    if (!IsReady()) return false;

    void* mapped = nullptr;
    vertexBuffer_->Map(0, nullptr, &mapped);
    std::memcpy(mapped, vertices, sizeof(vertices));
    vertexBuffer_->Unmap(0, nullptr);
    vbView_.BufferLocation = vertexBuffer_->GetGPUVirtualAddress();
    vbView_.SizeInBytes = static_cast<UINT>(sizeof(vertices));
    vbView_.StrideInBytes = sizeof(Vertex);

    indexBuffer_->Map(0, nullptr, &mapped);
    std::memcpy(mapped, indices, sizeof(indices));
    indexBuffer_->Unmap(0, nullptr);
    ibView_.BufferLocation = indexBuffer_->GetGPUVirtualAddress();
    ibView_.SizeInBytes = static_cast<UINT>(sizeof(indices));
    ibView_.Format = DXGI_FORMAT_R32_UINT;
    return true;
}

void ArcRing::SetParams(const Params& params) {
    if (!paramsCB_) return;
    ParamsCB cb{};
    cb.edgeColor = params.edgeColor;
    cb.coreColor = params.coreColor;
    cb.innerRadius = params.innerRadius;
    cb.outerRadius = params.outerRadius;
    // Windows.h の max マクロと衝突しないよう、(std::max) と括弧で囲む
    cb.arcCount = static_cast<float>((std::max<uint32_t>)(params.arcCount, 1));
    cb.gapRatio = std::clamp(params.gapRatio, 0.0f, 0.99f);
    cb.rotation = params.rotation;
    cb.tipSoftness = (std::max)(params.tipSoftness, 0.0f);
    cb.edgeSoftness = std::clamp(params.edgeSoftness, 0.0f, 0.5f);
    cb.haloStrength = (std::max)(params.haloStrength, 0.0f);
    cb.haloWidth = (std::max)(params.haloWidth, 0.0001f);
    cb.corePower = (std::max)(params.corePower, 0.0f);
    cb.intensity = (std::max)(params.intensity, 0.0f);
    cb.alpha = std::clamp(params.alpha, 0.0f, 1.0f);

    void* mapped = nullptr;
    paramsCB_->Map(0, nullptr, &mapped);
    if (mapped) {
        std::memcpy(mapped, &cb, sizeof(cb));
        paramsCB_->Unmap(0, nullptr);
    }
}

void ArcRing::SetTransform(const Matrix4x4& world, const Matrix4x4& view, const Matrix4x4& proj) {
    if (!transformCB_) return;
    void* mapped = nullptr;
    transformCB_->Map(0, nullptr, &mapped);
    if (mapped) {
        TransformCB* cb = static_cast<TransformCB*>(mapped);
        cb->World = world;
        cb->WVP = Multiply(Multiply(world, view), proj);
        transformCB_->Unmap(0, nullptr);
    }
}

void ArcRing::SetBillboardTransform(const Vector3& position, float worldRadius, const Matrix4x4& view, const Matrix4x4& proj) {
    // ビルボード: ビュー行列の回転だけを取り出して逆にし、板をカメラへ正対させる(RingEffect と同じ作り方)
    Matrix4x4 billboard = view;
    billboard.m[3][0] = 0.0f;
    billboard.m[3][1] = 0.0f;
    billboard.m[3][2] = 0.0f;
    billboard = Inverse(billboard);
    billboard.m[3][0] = position.x;
    billboard.m[3][1] = position.y;
    billboard.m[3][2] = position.z;

    const Matrix4x4 scale = MakeScaleMatrix(Vector3{ worldRadius, worldRadius, worldRadius });
    SetTransform(Multiply(scale, billboard), view, proj);
}

void ArcRing::Draw(ID3D12GraphicsCommandList* cmdList) {
    if (!IsReady() || !cmdList) return;
    cmdList->IASetVertexBuffers(0, 1, &vbView_);
    cmdList->IASetIndexBuffer(&ibView_);
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmdList->DrawIndexedInstanced(kQuadIndexCount, 1, 0, 0, 0);
}
