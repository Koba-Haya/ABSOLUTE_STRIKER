#include "ShaderQuad.h"
#include "Method.h"
#include <cstring>

namespace {
// 定数バッファは 256 バイト単位で確保する(D3D12 の決まり)
constexpr UINT64 kConstantBufferAlignment = 256;
// 板の頂点数とインデックス数(正方形 1 枚 = 三角形 2 つ)
constexpr UINT kQuadVertexCount = 4;
constexpr UINT kQuadIndexCount = 6;
// 板の半分の長さを 1 としたときの、角の座標
constexpr float kQuadHalf = 1.0f;

// size を、256 バイトの倍数へ切り上げる
UINT64 AlignConstantBuffer(UINT64 size) {
    return (size + kConstantBufferAlignment - 1) & ~(kConstantBufferAlignment - 1);
}
}

Microsoft::WRL::ComPtr<ID3D12Resource> ShaderQuad::CreateUploadBuffer_(ID3D12Device* device, UINT64 size) {
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

bool ShaderQuad::Initialize(ID3D12Device* device, std::size_t paramsByteSize) {
    if (!device || paramsByteSize == 0) return false;
    paramsByteSize_ = paramsByteSize;

    // 板(XY 平面。裏から見ても描くので、向きは気にしない)
    const Vertex vertices[kQuadVertexCount] = {
        { { -kQuadHalf,  kQuadHalf, 0.0f, 1.0f }, { -kQuadHalf,  kQuadHalf } },
        { {  kQuadHalf,  kQuadHalf, 0.0f, 1.0f }, {  kQuadHalf,  kQuadHalf } },
        { { -kQuadHalf, -kQuadHalf, 0.0f, 1.0f }, { -kQuadHalf, -kQuadHalf } },
        { {  kQuadHalf, -kQuadHalf, 0.0f, 1.0f }, {  kQuadHalf, -kQuadHalf } },
    };
    const uint32_t indices[kQuadIndexCount] = { 0, 1, 2, 2, 1, 3 };

    vertexBuffer_ = CreateUploadBuffer_(device, sizeof(vertices));
    indexBuffer_ = CreateUploadBuffer_(device, sizeof(indices));
    transformCB_ = CreateUploadBuffer_(device, AlignConstantBuffer(sizeof(TransformCB)));
    paramsCB_ = CreateUploadBuffer_(device, AlignConstantBuffer(paramsByteSize));
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

void ShaderQuad::SetParamsData(const void* data, std::size_t byteSize) {
    if (!paramsCB_ || !data || byteSize > paramsByteSize_) return;
    void* mapped = nullptr;
    paramsCB_->Map(0, nullptr, &mapped);
    if (mapped) {
        std::memcpy(mapped, data, byteSize);
        paramsCB_->Unmap(0, nullptr);
    }
}

void ShaderQuad::SetTransform(const Matrix4x4& world, const Matrix4x4& view, const Matrix4x4& proj) {
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

void ShaderQuad::SetBillboardTransform(const Vector3& position, float worldRadius, const Matrix4x4& view, const Matrix4x4& proj) {
    // ビルボード: ビュー行列の回転だけを取り出して逆にし、板をカメラへ正対させる(ArcRing と同じ作り方)
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

void ShaderQuad::Draw(ID3D12GraphicsCommandList* cmdList) {
    if (!IsReady() || !cmdList) return;
    cmdList->IASetVertexBuffers(0, 1, &vbView_);
    cmdList->IASetIndexBuffer(&ibView_);
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmdList->DrawIndexedInstanced(kQuadIndexCount, 1, 0, 0, 0);
}
