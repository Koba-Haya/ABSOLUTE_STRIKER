#define NOMINMAX

#include "ParticleManager.h"

#include "DirectXCommon.h"
#include "DirectXResourceUtils.h"
#include "SrvAllocator.h"
#include "../../resources/AssetManager.h"
#include "TextureResource.h"
#include "UnifiedPipeline.h"
#include "Method.h"
#include "Renderer.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <random>

namespace {
    std::mt19937& GetRNG() {
        static std::mt19937 rng{ std::random_device{}() };
        return rng;
    }
} // namespace

ParticleManager* ParticleManager::GetInstance() {
    static ParticleManager instance;
    return &instance;
}

bool ParticleManager::Initialize(DirectXCommon* dx) {
    dx_ = dx;
    if (!dx_) {
        return false;
    }
    device_.Reset();
    dx_->GetDevice()->QueryInterface(IID_PPV_ARGS(&device_));

    groups_.clear();
    quadReady_ = false;
    EnsureQuadGeometry_();
    return true;
}

void ParticleManager::Finalize() {
    groups_.clear();

    vb_.Reset();
    ib_.Reset();
    quadReady_ = false;
    device_.Reset();
    dx_ = nullptr;
}

bool ParticleManager::CreateParticleGroup(const std::string& name, const std::string& texturePath, uint32_t maxInstances, bool softAlpha) {
    assert(dx_);
    assert(device_);
    assert(maxInstances > 0);

    if (groups_.contains(name)) {
        // シーン切り替え等ですでに同名のグループが存在する場合は、
        // アサートで落とさずにパーティクルをリセットして再利用する
        ClearParticleGroup(name);
        groups_.at(name).softAlpha = softAlpha;
        return true;
    }

    ParticleGroup g{};
    g.softAlpha = softAlpha;
    g.maxInstances = maxInstances;
    g.instanceLimit = maxInstances;
    g.activeInstanceCount = 0;

    // Texture SRV (t0)
    g.texture = AbsoluteEngine::AssetManager::GetInstance()->Load<TextureResource>(texturePath);
    assert(g.texture);
    g.textureSrvGpu = g.texture->GetSrvGpu();

    // StructuredBuffer (t1)
    g.instanceBuffer = CreateBufferResource(device_, sizeof(ParticleForGPU) * maxInstances);
    g.instanceBuffer->Map(0, nullptr, reinterpret_cast<void**>(&g.instanceMapped));
    for (uint32_t i = 0; i < maxInstances; ++i) {
        g.instanceMapped[i].WVP = MakeIdentity4x4();
        g.instanceMapped[i].World = MakeIdentity4x4();
        g.instanceMapped[i].color = { 1,1,1,1 };
    }

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Buffer.NumElements = maxInstances;
    srvDesc.Buffer.StructureByteStride = sizeof(ParticleForGPU);
    srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

    SrvAllocator* alloc = &dx_->GetSrvAllocator();
    uint32_t index = alloc->Allocate();
    g.instanceSrv = SrvHandle(alloc, index);
    device_->CreateShaderResourceView(g.instanceBuffer.Get(), &srvDesc, alloc->Cpu(index));
    g.instanceSrvGpu = alloc->Gpu(index);

    // Material CB (b0)
    {
        D3D12_HEAP_PROPERTIES heapProps{};
        heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

        D3D12_RESOURCE_DESC resDesc{};
        resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        resDesc.Width = sizeof(ParticleMaterialData);
        resDesc.Height = 1;
        resDesc.DepthOrArraySize = 1;
        resDesc.MipLevels = 1;
        resDesc.SampleDesc.Count = 1;
        resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        HRESULT hr = device_->CreateCommittedResource(
            &heapProps, D3D12_HEAP_FLAG_NONE, &resDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(&g.materialCB));
        assert(SUCCEEDED(hr));

        g.materialCB->Map(0, nullptr, reinterpret_cast<void**>(&g.materialMapped));
        g.materialMapped->color = { 1,1,1,1 };
        g.materialMapped->enableLighting = 0;
        g.materialMapped->pad[0] = g.materialMapped->pad[1] = g.materialMapped->pad[2] = 0.0f;
        g.materialMapped->uvTransform = MakeIdentity4x4();
    }

    groups_.emplace(name, std::move(g));
    return true;
}

void ParticleManager::SetGroupInstanceLimit(const std::string& name, uint32_t limit) {
    auto it = groups_.find(name);
    if (it == groups_.end()) {
        return;
    }
    ParticleGroup& g = it->second;
    g.instanceLimit = std::min(limit, g.maxInstances);
}

void ParticleManager::ClearParticleGroup(const std::string& name) {
    auto it = groups_.find(name);
    if (it == groups_.end()) {
        return;
    }

    ParticleGroup& g = it->second;
    g.particles.clear();
    g.activeInstanceCount = 0;
}

void ParticleManager::TranslateParticleGroup(const std::string& name, const Vector3& delta) {
    auto it = groups_.find(name);
    if (it == groups_.end()) {
        return;
    }
    for (Particle& p : it->second.particles) {
        p.transform.translate.x += delta.x;
        p.transform.translate.y += delta.y;
        p.transform.translate.z += delta.z;
    }
}

void ParticleManager::Emit(const std::string& name, const Vector3& position, const Vector3& velocity,
    const Vector3& scale, const Vector3& rotate, float lifetime, const Vector4& color,
    const Vector3* endScale) {
    auto it = groups_.find(name);
    assert(it != groups_.end());
    if (it == groups_.end()) {
        return;
    }
    ParticleGroup& g = it->second;

    Particle p{};
    p.transform.translate = position;
    p.transform.scale = scale;
    p.transform.rotate = rotate;
    p.velocity = velocity;
    p.lifetime = lifetime;
    p.age = 0.0f;
    p.color = color;
    if (endScale) {
        p.endScale = *endScale;
        p.changesScale = true;
    }
    g.particles.push_back(p);
}

void ParticleManager::Update(float deltaTime) {
    const Matrix4x4& view = Renderer::GetInstance()->GetViewMatrix();
    const Matrix4x4& proj = Renderer::GetInstance()->GetProjectionMatrix();

    for (auto& kv : groups_) {
        ParticleGroup& g = kv.second;
        if (!g.instanceMapped) {
            continue;
        }

        uint32_t capacity = g.instanceLimit;
        uint32_t gpuIndex = 0;

        for (auto it = g.particles.begin(); it != g.particles.end();) {
            Particle& p = *it;
            p.age += deltaTime;
            if (p.age >= p.lifetime) {
                it = g.particles.erase(it);
                continue;
            }

            if (enableAccelerationField_) {
                if (IsCollision_(accelerationField_.area, p.transform.translate)) {
                    p.velocity.x += accelerationField_.acceleration.x * deltaTime;
                    p.velocity.y += accelerationField_.acceleration.y * deltaTime;
                    p.velocity.z += accelerationField_.acceleration.z * deltaTime;
                }
            }

            p.transform.translate.x += p.velocity.x * deltaTime;
            p.transform.translate.y += p.velocity.y * deltaTime;
            p.transform.translate.z += p.velocity.z * deltaTime;

            float t = p.age / p.lifetime;
            // 発生時のアルファ(color.w)から 0 へ線形に消える
            float alpha = p.color.w * std::clamp(1.0f - t, 0.0f, 1.0f);

            if (gpuIndex < capacity) {
                const Vector3 scale = p.changesScale ? Lerp(p.transform.scale, p.endScale, t) : p.transform.scale;
                Matrix4x4 world = MakeBillboardMatrix_(scale, p.transform.rotate, p.transform.translate, view);
                Matrix4x4 wvp = Multiply(world, Multiply(view, proj));
                g.instanceMapped[gpuIndex].World = world;
                g.instanceMapped[gpuIndex].WVP = wvp;
                g.instanceMapped[gpuIndex].color = { p.color.x, p.color.y, p.color.z, alpha };
                ++gpuIndex;
            }

            ++it;
        }

        g.activeInstanceCount = gpuIndex;
    }
}

void ParticleManager::Draw(BlendMode blendMode, bool depthTest, ParticleDrawFilter filter) {
    Renderer::GetInstance()->DrawParticles(this, blendMode, depthTest, filter);
}

void ParticleManager::DrawInternal(ID3D12GraphicsCommandList* cmdList, ParticleDrawFilter filter) {
    assert(dx_);
    assert(cmdList);
    EnsureQuadGeometry_();

    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmdList->IASetVertexBuffers(0, 1, &vbView_);
    cmdList->IASetIndexBuffer(&ibView_);

    ID3D12DescriptorHeap* heaps[] = { dx_->GetSRVHeap() };
    cmdList->SetDescriptorHeaps(1, heaps);

    const UINT indexCount = 6;
    for (auto& kv : groups_) {
        ParticleGroup& g = kv.second;
        if (g.activeInstanceCount == 0) {
            continue;
        }
        // グループの選び方: 「柔らかいアルファ」の印で、描くかどうかを決める
        if ((filter == ParticleDrawFilter::NormalOnly && g.softAlpha) || (filter == ParticleDrawFilter::SoftAlphaOnly && !g.softAlpha)) {
            continue;
        }

        // RootParameter の並びは UnifiedPipeline::MakeParticleDesc に対応
        // 0: CBV(b0) / 1: Texture(t0) / 2: Instancing(t1)
        cmdList->SetGraphicsRootConstantBufferView(0, g.materialCB->GetGPUVirtualAddress());
        cmdList->SetGraphicsRootDescriptorTable(1, g.textureSrvGpu);
        cmdList->SetGraphicsRootDescriptorTable(2, g.instanceSrvGpu);

        cmdList->DrawIndexedInstanced(indexCount, g.activeInstanceCount, 0, 0, 0);
    }
}

void ParticleManager::EnsureQuadGeometry_() {
    if (quadReady_) {
        return;
    }
    assert(device_);

    struct Vtx { float px, py, pz; float u, v; };
    Vtx quad[4] = {
        {-0.5f,  0.5f, 0.0f, 0.0f, 0.0f},
        { 0.5f,  0.5f, 0.0f, 1.0f, 0.0f},
        {-0.5f, -0.5f, 0.0f, 0.0f, 1.0f},
        { 0.5f, -0.5f, 0.0f, 1.0f, 1.0f},
    };
    uint16_t idx[6] = { 0,1,2, 2,1,3 };

    // VB
    {
        const UINT vbSize = static_cast<UINT>(sizeof(quad));
        D3D12_HEAP_PROPERTIES heap{ D3D12_HEAP_TYPE_UPLOAD };
        D3D12_RESOURCE_DESC desc{};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Width = vbSize;
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        HRESULT hr = device_->CreateCommittedResource(
            &heap, D3D12_HEAP_FLAG_NONE, &desc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(&vb_));
        assert(SUCCEEDED(hr));

        void* mapped = nullptr;
        vb_->Map(0, nullptr, &mapped);
        std::memcpy(mapped, quad, vbSize);
        vb_->Unmap(0, nullptr);

        vbView_.BufferLocation = vb_->GetGPUVirtualAddress();
        vbView_.StrideInBytes = sizeof(Vtx);
        vbView_.SizeInBytes = vbSize;
    }

    // IB
    {
        const UINT ibSize = static_cast<UINT>(sizeof(idx));
        D3D12_HEAP_PROPERTIES heap{ D3D12_HEAP_TYPE_UPLOAD };
        D3D12_RESOURCE_DESC desc{};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Width = ibSize;
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        HRESULT hr = device_->CreateCommittedResource(
            &heap, D3D12_HEAP_FLAG_NONE, &desc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(&ib_));
        assert(SUCCEEDED(hr));

        void* mapped = nullptr;
        ib_->Map(0, nullptr, &mapped);
        std::memcpy(mapped, idx, ibSize);
        ib_->Unmap(0, nullptr);

        ibView_.BufferLocation = ib_->GetGPUVirtualAddress();
        ibView_.Format = DXGI_FORMAT_R16_UINT;
        ibView_.SizeInBytes = ibSize;
    }

    quadReady_ = true;
}

Matrix4x4 ParticleManager::MakeBillboardMatrix_(const Vector3& scale, const Vector3& rotate, const Vector3& translate, const Matrix4x4& viewMatrix) const {
    Matrix4x4 camWorld = Inverse(viewMatrix);
    Vector3 right{ camWorld.m[0][0], camWorld.m[0][1], camWorld.m[0][2] };
    Vector3 up{ camWorld.m[1][0], camWorld.m[1][1], camWorld.m[1][2] };
    Vector3 forward{ camWorld.m[2][0], camWorld.m[2][1], camWorld.m[2][2] };
    forward = { -forward.x, -forward.y, -forward.z };

    // Z軸（視線方向）周りの回転（ロール）を適用
    float angle = rotate.z;
    float c = std::cos(angle);
    float s = std::sin(angle);

    Vector3 rotatedRight = {
        right.x * c + up.x * s,
        right.y * c + up.y * s,
        right.z * c + up.z * s
    };
    Vector3 rotatedUp = {
        -right.x * s + up.x * c,
        -right.y * s + up.y * c,
        -right.z * s + up.z * c
    };

    Matrix4x4 m{};
    m.m[0][0] = scale.x * rotatedRight.x;
    m.m[0][1] = scale.x * rotatedRight.y;
    m.m[0][2] = scale.x * rotatedRight.z;
    m.m[0][3] = 0;

    m.m[1][0] = scale.y * rotatedUp.x;
    m.m[1][1] = scale.y * rotatedUp.y;
    m.m[1][2] = scale.y * rotatedUp.z;
    m.m[1][3] = 0;

    m.m[2][0] = scale.z * forward.x;
    m.m[2][1] = scale.z * forward.y;
    m.m[2][2] = scale.z * forward.z;
    m.m[2][3] = 0;

    m.m[3][0] = translate.x;
    m.m[3][1] = translate.y;
    m.m[3][2] = translate.z;
    m.m[3][3] = 1;
    return m;
}

bool ParticleManager::IsCollision_(const AABB& aabb, const Vector3& point) const {
    if (point.x < aabb.min.x || point.x > aabb.max.x) return false;
    if (point.y < aabb.min.y || point.y > aabb.max.y) return false;
    if (point.z < aabb.min.z || point.z > aabb.max.z) return false;
    return true;
}
