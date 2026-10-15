#define NOMINMAX
#include "GPUParticleManager.h"
#include "DirectXCommon.h"
#include "Renderer.h"
#include "Method.h"
#include "texture/TextureResource.h"
#include "../../base/AssetPath.h"
#include <cassert>
#include <cstring>
#include <dxcapi.h>

namespace {
static constexpr UINT Align256_(UINT n) { return (n + 255u) & ~255u; }

// ブレンドモードに対応した D3D12_BLEND_DESC を生成する
// UnifiedPipeline.cpp の MakeBlendDesc と同じロジック
static D3D12_BLEND_DESC MakeGPUParticleBlendDesc(BlendMode mode) {
    D3D12_BLEND_DESC desc{};
    auto& rt = desc.RenderTarget[0];
    rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    rt.BlendEnable = TRUE;

    switch (mode) {
    default:
    case BlendMode::Opaque:
    case BlendMode::Alpha: // 通常アルファ: dst*(1-a) + src*a
        rt.SrcBlend        = D3D12_BLEND_SRC_ALPHA;
        rt.DestBlend       = D3D12_BLEND_INV_SRC_ALPHA;
        rt.BlendOp         = D3D12_BLEND_OP_ADD;
        rt.SrcBlendAlpha   = D3D12_BLEND_ONE;
        rt.DestBlendAlpha  = D3D12_BLEND_INV_SRC_ALPHA;
        rt.BlendOpAlpha    = D3D12_BLEND_OP_ADD;
        break;

    case BlendMode::Add: // 加算: dst + src*a
        rt.SrcBlend        = D3D12_BLEND_SRC_ALPHA;
        rt.DestBlend       = D3D12_BLEND_ONE;
        rt.BlendOp         = D3D12_BLEND_OP_ADD;
        rt.SrcBlendAlpha   = D3D12_BLEND_ONE;
        rt.DestBlendAlpha  = D3D12_BLEND_ONE;
        rt.BlendOpAlpha    = D3D12_BLEND_OP_ADD;
        break;

    case BlendMode::Subtract: // 減算: dst - src*a
        rt.SrcBlend        = D3D12_BLEND_SRC_ALPHA;
        rt.DestBlend       = D3D12_BLEND_ONE;
        rt.BlendOp         = D3D12_BLEND_OP_REV_SUBTRACT;
        rt.SrcBlendAlpha   = D3D12_BLEND_ONE;
        rt.DestBlendAlpha  = D3D12_BLEND_ONE;
        rt.BlendOpAlpha    = D3D12_BLEND_OP_ADD;
        break;

    case BlendMode::Multiply: // 乗算: dst * src
        rt.SrcBlend        = D3D12_BLEND_ZERO;
        rt.DestBlend       = D3D12_BLEND_SRC_COLOR;
        rt.BlendOp         = D3D12_BLEND_OP_ADD;
        rt.SrcBlendAlpha   = D3D12_BLEND_ZERO;
        rt.DestBlendAlpha  = D3D12_BLEND_SRC_ALPHA;
        rt.BlendOpAlpha    = D3D12_BLEND_OP_ADD;
        break;

    case BlendMode::Screen: // スクリーン: 1-(1-s)*(1-d)
        rt.SrcBlend        = D3D12_BLEND_INV_DEST_COLOR;
        rt.DestBlend       = D3D12_BLEND_ONE;
        rt.BlendOp         = D3D12_BLEND_OP_ADD;
        rt.SrcBlendAlpha   = D3D12_BLEND_ONE;
        rt.DestBlendAlpha  = D3D12_BLEND_INV_SRC_ALPHA;
        rt.BlendOpAlpha    = D3D12_BLEND_OP_ADD;
        break;
    }
    return desc;
}

Microsoft::WRL::ComPtr<IDxcBlob> CompileShader(
    const std::wstring& filePath,
    const std::wstring& profile,
    IDxcUtils* dxcUtils,
    IDxcCompiler3* dxcCompiler,
    IDxcIncludeHandler* includeHandler) {
    
    // exe 基準の実パスに解決する(#include もこのパスを起点に解決される)
    const std::wstring resolvedPath = AbsoluteEngine::AssetPath::Resolve(filePath).wstring();

    Microsoft::WRL::ComPtr<IDxcBlobEncoding> shaderSource;
    HRESULT hr = dxcUtils->LoadFile(resolvedPath.c_str(), nullptr, &shaderSource);
    if (FAILED(hr) || !shaderSource) {
        OutputDebugStringA("Failed to load shader file: ");
        OutputDebugStringW(resolvedPath.c_str());
        OutputDebugStringA("\n");
        assert(false);
    }

    DxcBuffer sourceBuffer;
    sourceBuffer.Ptr = shaderSource->GetBufferPointer();
    sourceBuffer.Size = shaderSource->GetBufferSize();
    sourceBuffer.Encoding = DXC_CP_UTF8;

    std::vector<LPCWSTR> arguments;
    arguments.push_back(resolvedPath.c_str());
    arguments.push_back(L"-E");
    arguments.push_back(L"main");
    arguments.push_back(L"-T");
    arguments.push_back(profile.c_str());
    arguments.push_back(L"-Zi");
    arguments.push_back(L"-Od");
    arguments.push_back(L"-Zpr");

    Microsoft::WRL::ComPtr<IDxcResult> shaderResult;
    dxcCompiler->Compile(&sourceBuffer, arguments.data(), (uint32_t)arguments.size(), includeHandler, IID_PPV_ARGS(&shaderResult));

    Microsoft::WRL::ComPtr<IDxcBlobUtf8> shaderError;
    shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr);
    if (shaderError != nullptr && shaderError->GetStringLength() != 0) {
        OutputDebugStringA(shaderError->GetStringPointer());
        assert(false);
    }

    Microsoft::WRL::ComPtr<IDxcBlob> shaderBlob;
    shaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
    return shaderBlob;
}
}

GPUParticleManager* GPUParticleManager::GetInstance() {
    static GPUParticleManager instance;
    return &instance;
}

void GPUParticleManager::Initialize(DirectXCommon* dx) {
    dx_ = dx;
    CreateResources();
    CreateComputePipeline();
    CreateGraphicsPipeline();
    CreateQuad();

    // Run Initialization CS
    auto* cmdList = dx_->GetCommandList();
    
    // Transition to UAV (Particles, FreeListIndex, FreeList)
    D3D12_RESOURCE_BARRIER barriers[3]{};
    barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barriers[0].Transition.pResource = particleBuffer_.Get();
    barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
    barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    
    barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barriers[1].Transition.pResource = freeListIndexBuffer_.Get();
    barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
    barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    barriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    barriers[2].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barriers[2].Transition.pResource = freeListBuffer_.Get();
    barriers[2].Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
    barriers[2].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    barriers[2].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    cmdList->ResourceBarrier(3, barriers);

    ID3D12DescriptorHeap* heaps[] = { dx_->GetSRVHeap() };
    cmdList->SetDescriptorHeaps(1, heaps);

    cmdList->SetComputeRootSignature(computeRootSignature_.Get());
    cmdList->SetPipelineState(computePipelineState_.Get());
    
    auto& srvAlloc = dx_->GetSrvAllocator();
    // gParticles(u0), gFreeListIndex(u1), gFreeList(u2) を含むテーブルをセット
    cmdList->SetComputeRootDescriptorTable(0, srvAlloc.Gpu(uavIndex_));
    
    cmdList->Dispatch(1, 1, 1); 

    // Resource Barrier (UAV -> SRV/Vertex)
    // particleBuffer_ のみを SRV に戻す。freeListIndexBuffer_, freeListBuffer_ は UAV のままで運用。
    D3D12_RESOURCE_BARRIER srvBarrier{};
    srvBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    srvBarrier.Transition.pResource = particleBuffer_.Get();
    srvBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    srvBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    srvBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    cmdList->ResourceBarrier(1, &srvBarrier);
}

void GPUParticleManager::Update() {
    const float kDeltaTime = 1.0f / 60.0f; // エンジン全体が固定タイムステップのため合わせる
    static float totalTime = 0.0f;
    totalTime += kDeltaTime;

    // 1. 前フレームでEmit CSに消費させ終えたバーストスロットのemitフラグをここでクリアする。
    //    （このUpdate()が呼ばれる時点で前フレームのGPUコマンドは実行済みという前提。
    //     emittersCB_はダブルバッファリングされていない単一のアップロードバッファのため、
    //     EmitBurst()直後にここでクリアすると今フレーム分のDispatchに反映されなくなってしまう。
    //     そのため「立てた次のUpdate()」まで1フレーム遅らせてクリアする。）
    for (uint32_t i = 0; i < kBurstPoolSize; ++i) {
        if (burstPendingClear_[i]) {
            emittersMapped_->emitters[kPersistentEmitterSlots + i].emit = 0;
            burstPendingClear_[i] = false;
        }
    }

    // 2. 常駐エミッタ（頻度タイマー方式）の射出判定
    for (uint32_t i = 0; i < kPersistentEmitterSlots; ++i) {
        GPUEmitter& em = emittersMapped_->emitters[i];
        if (em.enabled == 0 || em.frequency <= 0.0f) {
            em.emit = 0;
            continue;
        }
        em.frequencyTime += kDeltaTime;
        if (em.frequency <= em.frequencyTime) {
            em.frequencyTime -= em.frequency;
            em.emit = 1;
        } else {
            em.emit = 0;
        }
    }

    // 3. このフレームでEmitBurst()等によりemit==1になっているバーストスロットは、
    //    次回のUpdate()でクリアするよう予約する。
    for (uint32_t i = 0; i < kBurstPoolSize; ++i) {
        if (emittersMapped_->emitters[kPersistentEmitterSlots + i].emit != 0) {
            burstPendingClear_[i] = true;
        }
    }

    // PerFrame Update
    perFrameMapped_->time = totalTime;
    perFrameMapped_->deltaTime = kDeltaTime;

    // PerView Update (Billboard用)
    const Matrix4x4& view = Renderer::GetInstance()->GetViewMatrix();
    const Matrix4x4& proj = Renderer::GetInstance()->GetProjectionMatrix();
    perViewMapped_->viewProjection = Multiply(view, proj);
    
    Matrix4x4 billboard = Inverse(view);
    billboard.m[3][0] = billboard.m[3][1] = billboard.m[3][2] = 0.0f;
    perViewMapped_->billboardMatrix = billboard;

    // --- Emit CS & Update CS の実行 ---
    auto* cmdList = dx_->GetCommandList();
    auto& srvAlloc = dx_->GetSrvAllocator();

    // UAVへ遷移 (particleBuffer_ のみ)
    D3D12_RESOURCE_BARRIER uavTransitionBarrier{};
    uavTransitionBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    uavTransitionBarrier.Transition.pResource = particleBuffer_.Get();
    uavTransitionBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    uavTransitionBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    uavTransitionBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    cmdList->ResourceBarrier(1, &uavTransitionBarrier);

    ID3D12DescriptorHeap* heaps[] = { dx_->GetSRVHeap() };
    cmdList->SetDescriptorHeaps(1, heaps);

    // --- Emit/Update共有のRootSignatureを1度だけセットする ---
    cmdList->SetComputeRootSignature(emitRootSignature_.Get());
    // 0: Emitter[](b0), 1: PerFrame(b1), 2: Field[](b2), 3: UAVs(u0, u1, u2)
    cmdList->SetComputeRootConstantBufferView(0, emittersCB_->GetGPUVirtualAddress());
    cmdList->SetComputeRootConstantBufferView(1, perFrameCB_->GetGPUVirtualAddress());
    cmdList->SetComputeRootConstantBufferView(2, fieldsCB_->GetGPUVirtualAddress());
    cmdList->SetComputeRootDescriptorTable(3, srvAlloc.Gpu(uavIndex_));

    // --- Emit CS 起動（エミッタ数ぶんのスレッドグループを並列実行） ---
    cmdList->SetPipelineState(emitPipelineState_.Get());
    cmdList->Dispatch(kMaxGPUEmitters, 1, 1);

    // UAV Barrier (Emit CS で更新されたリソースが Update CS での読み書きに入るため同期をとる)
    D3D12_RESOURCE_BARRIER uavBarriers[3]{};
    uavBarriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    uavBarriers[0].UAV.pResource = particleBuffer_.Get();
    uavBarriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    uavBarriers[1].UAV.pResource = freeListIndexBuffer_.Get();
    uavBarriers[2].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    uavBarriers[2].UAV.pResource = freeListBuffer_.Get();
    cmdList->ResourceBarrier(3, uavBarriers);

    // --- Update CS 起動（RootSignatureはEmitと共有のため再セット不要、PSOのみ切り替える） ---
    cmdList->SetPipelineState(updatePipelineState_.Get());
    cmdList->Dispatch(1, 1, 1);

    // SRVへ戻す (particleBuffer_ のみ)
    D3D12_RESOURCE_BARRIER srvTransitionBarrier{};
    srvTransitionBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    srvTransitionBarrier.Transition.pResource = particleBuffer_.Get();
    srvTransitionBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    srvTransitionBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    srvTransitionBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    cmdList->ResourceBarrier(1, &srvTransitionBarrier);
}

void GPUParticleManager::Draw(BlendMode blendMode) {
    auto* cmdList = dx_->GetCommandList();
    auto& srvAlloc = dx_->GetSrvAllocator();

    // ブレンドモードに対応するパイプラインを選択（範囲外は Alpha にフォールバック）
    int psoIndex = static_cast<int>(blendMode);
    if (psoIndex < 0 || psoIndex >= kBlendModeCount || !graphicsPipelineStates_[psoIndex]) {
        psoIndex = static_cast<int>(BlendMode::Alpha);
    }

    cmdList->SetGraphicsRootSignature(graphicsRootSignature_.Get());
    cmdList->SetPipelineState(graphicsPipelineStates_[psoIndex].Get());

    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmdList->IASetVertexBuffers(0, 1, &vbView_);
    cmdList->IASetIndexBuffer(&ibView_);

    ID3D12DescriptorHeap* heaps[] = { dx_->GetSRVHeap() };
    cmdList->SetDescriptorHeaps(1, heaps);

    // 0: CBV(b0)
    cmdList->SetGraphicsRootConstantBufferView(0, perViewCB_->GetGPUVirtualAddress());
    // 1: SRV(t0)
    cmdList->SetGraphicsRootDescriptorTable(1, srvAlloc.Gpu(srvIndex_));
    // 2: Texture(t1)
    cmdList->SetGraphicsRootDescriptorTable(2, texture_->GetSrvGpu());

    cmdList->DrawIndexedInstanced(6, kMaxParticles, 0, 0, 0);
}

uint32_t GPUParticleManager::CreateEmitter(const EmitterDesc& desc) {
    if (persistentEmitterCount_ >= kPersistentEmitterSlots) {
        assert(false && "GPUParticleManager: persistent emitter slots exhausted");
        return UINT32_MAX;
    }
    uint32_t index = persistentEmitterCount_++;
    GPUEmitter& em = emittersMapped_->emitters[index];
    em.translate = desc.translate;
    em.radius = desc.radius;
    em.halfExtents = desc.halfExtents;
    em.pad0 = 0.0f;
    em.shape = static_cast<uint32_t>(desc.shape);
    em.count = desc.count;
    em.frequency = desc.frequency;
    em.frequencyTime = 0.0f;
    em.emit = 0;
    em.enabled = 1;
    em.pad1[0] = 0.0f;
    em.pad1[1] = 0.0f;
    return index;
}

void GPUParticleManager::SetEmitterTransform(uint32_t emitterIndex, const Vector3& translate) {
    if (emitterIndex >= kPersistentEmitterSlots) return;
    emittersMapped_->emitters[emitterIndex].translate = translate;
}

void GPUParticleManager::SetEmitterEnabled(uint32_t emitterIndex, bool enabled) {
    if (emitterIndex >= kPersistentEmitterSlots) return;
    emittersMapped_->emitters[emitterIndex].enabled = enabled ? 1 : 0;
}

void GPUParticleManager::EmitBurst(const Vector3& position, uint32_t count, float radius) {
    uint32_t slot = kPersistentEmitterSlots + (burstCursor_ % kBurstPoolSize);
    burstCursor_++;

    GPUEmitter& em = emittersMapped_->emitters[slot];
    em.translate = position;
    em.radius = radius;
    em.halfExtents = { radius, radius, radius };
    em.pad0 = 0.0f;
    em.shape = static_cast<uint32_t>(EmitterShape::Sphere);
    em.count = count;
    em.frequency = 0.0f; // バースト枠は頻度タイマーを使わずワンショットで直接emitを立てる
    em.frequencyTime = 0.0f;
    em.enabled = 1;
    em.emit = 1;
    em.pad1[0] = 0.0f;
    em.pad1[1] = 0.0f;
}

void GPUParticleManager::SetField(int slot, const FieldDesc& desc) {
    if (slot < 0 || slot >= static_cast<int>(kMaxGPUFields)) return;

    GPUField& f = fieldsMapped_->fields[slot];
    f.target = desc.target;
    f.strength = desc.strength;
    f.direction = desc.direction;
    f.type = static_cast<uint32_t>(desc.type);

    // countは「有効なフィールドが存在する末尾のslot+1」として管理する（HLSL側は0..count-1をループする）
    uint32_t activeCount = 0;
    for (uint32_t i = 0; i < kMaxGPUFields; ++i) {
        if (fieldsMapped_->fields[i].type != 0) {
            activeCount = i + 1;
        }
    }
    fieldsMapped_->count = activeCount;
}

void GPUParticleManager::CreateResources() {
    auto* renderer = Renderer::GetInstance();
    auto& srvAlloc = dx_->GetSrvAllocator();
    auto* device = dx_->GetDevice();

    // Particle Buffer
    size_t bufferSize = sizeof(GPUParticle) * kMaxParticles;
    particleBuffer_ = renderer->CreateUAVBuffer(bufferSize);
    particleBuffer_->SetName(L"GPUParticleManager::ParticleBuffer");

    // SRV
    srvIndex_ = srvAlloc.Allocate();
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Buffer.NumElements = kMaxParticles;
    srvDesc.Buffer.StructureByteStride = sizeof(GPUParticle);
    srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
    device->CreateShaderResourceView(particleBuffer_.Get(), &srvDesc, srvAlloc.Cpu(srvIndex_));

    // UAV
    uavIndex_ = srvAlloc.Allocate();
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
    uavDesc.Format = DXGI_FORMAT_UNKNOWN;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.FirstElement = 0;
    uavDesc.Buffer.NumElements = kMaxParticles;
    uavDesc.Buffer.StructureByteStride = sizeof(GPUParticle);
    uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
    device->CreateUnorderedAccessView(particleBuffer_.Get(), nullptr, &uavDesc, srvAlloc.Cpu(uavIndex_));

    // FreeListIndex Buffer
    freeListIndexBuffer_ = renderer->CreateUAVBuffer(sizeof(int32_t));
    freeListIndexBuffer_->SetName(L"GPUParticleManager::FreeListIndexBuffer");
    freeListIndexUavIndex_ = srvAlloc.Allocate();
    D3D12_UNORDERED_ACCESS_VIEW_DESC freeListIndexUavDesc{};
    freeListIndexUavDesc.Format = DXGI_FORMAT_UNKNOWN;
    freeListIndexUavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    freeListIndexUavDesc.Buffer.FirstElement = 0;
    freeListIndexUavDesc.Buffer.NumElements = 1;
    freeListIndexUavDesc.Buffer.StructureByteStride = sizeof(int32_t);
    freeListIndexUavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
    device->CreateUnorderedAccessView(freeListIndexBuffer_.Get(), nullptr, &freeListIndexUavDesc, srvAlloc.Cpu(freeListIndexUavIndex_));

    // FreeList Buffer
    freeListBuffer_ = renderer->CreateUAVBuffer(sizeof(uint32_t) * kMaxParticles);
    freeListBuffer_->SetName(L"GPUParticleManager::FreeListBuffer");
    freeListUavIndex_ = srvAlloc.Allocate();
    D3D12_UNORDERED_ACCESS_VIEW_DESC freeListUavDesc{};
    freeListUavDesc.Format = DXGI_FORMAT_UNKNOWN;
    freeListUavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    freeListUavDesc.Buffer.FirstElement = 0;
    freeListUavDesc.Buffer.NumElements = kMaxParticles;
    freeListUavDesc.Buffer.StructureByteStride = sizeof(uint32_t);
    freeListUavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
    device->CreateUnorderedAccessView(freeListBuffer_.Get(), nullptr, &freeListUavDesc, srvAlloc.Cpu(freeListUavIndex_));

    // PerView CB
    perViewCB_ = renderer->CreateUploadBuffer(Align256_(sizeof(PerView)));
    perViewCB_->SetName(L"GPUParticleManager::PerViewCB");
    perViewCB_->Map(0, nullptr, reinterpret_cast<void**>(&perViewMapped_));

    // PerFrame CB
    perFrameCB_ = renderer->CreateUploadBuffer(Align256_(sizeof(PerFrame)));
    perFrameCB_->SetName(L"GPUParticleManager::PerFrameCB");
    perFrameCB_->Map(0, nullptr, reinterpret_cast<void**>(&perFrameMapped_));

    // Emitter[] CB（複数エミッタ対応。ゼロ初期化しないとenabled/countが不定値になり
    // Emit CS側で巨大なストライドループやゴミ座標での射出が起きうるため必ずmemsetする）
    emittersCB_ = renderer->CreateUploadBuffer(Align256_(sizeof(GPUEmitterArray)));
    emittersCB_->SetName(L"GPUParticleManager::EmittersCB");
    emittersCB_->Map(0, nullptr, reinterpret_cast<void**>(&emittersMapped_));
    std::memset(emittersMapped_, 0, sizeof(GPUEmitterArray));

    // Field[] CB（同様にゼロ初期化。type==0(None)がデフォルトで何も影響しない状態になる）
    fieldsCB_ = renderer->CreateUploadBuffer(Align256_(sizeof(GPUFieldArray)));
    fieldsCB_->SetName(L"GPUParticleManager::FieldsCB");
    fieldsCB_->Map(0, nullptr, reinterpret_cast<void**>(&fieldsMapped_));
    std::memset(fieldsMapped_, 0, sizeof(GPUFieldArray));

    // Load Texture
    texture_ = std::make_unique<TextureResource>();
    bool loadResult = texture_->CreateFromFile(dx_, "resources/engine/particle/circle2.png");
    assert(loadResult && "Failed to load resources/engine/particle/circle2.png");
}

void GPUParticleManager::CreateComputePipeline() {
    auto* device = dx_->GetDevice();

    // Initialize Root Signature
    {
        D3D12_DESCRIPTOR_RANGE range{};
        range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
        range.NumDescriptors = 3; // gParticles(u0), gFreeListIndex(u1), gFreeList(u2)
        range.BaseShaderRegister = 0;

        D3D12_ROOT_PARAMETER param{};
        param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        param.DescriptorTable.NumDescriptorRanges = 1;
        param.DescriptorTable.pDescriptorRanges = &range;
        param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        D3D12_ROOT_SIGNATURE_DESC rsDesc{};
        rsDesc.NumParameters = 1;
        rsDesc.pParameters = &param;
        rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

        ComPtr<ID3DBlob> blob, err;
        D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &err);
        device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&computeRootSignature_));
    }

    // Emit/Update共有 Root Signature
    // （フレーム内でRootSignatureを切り替える構成にすると、D3D12デバッグレイヤーの
    //  ValidateReferencedDeviceChildObjectsAreAlive検証に起因すると見られるDXGIDebug.dll内の
    //  クラッシュ(CInfoQueue::AddMessage)を誘発することを確認したため、Emit/Updateで
    //  1つのRootSignatureを共有し、Fieldもここに含める。Emit CS側はb2(Field)を単に参照しない。）
    {
        D3D12_DESCRIPTOR_RANGE uavRange{};
        uavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
        uavRange.NumDescriptors = 3; // u0, u1, u2
        uavRange.BaseShaderRegister = 0;

        D3D12_ROOT_PARAMETER params[4]{};
        // 0: Emitter[] (b0)
        params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        params[0].Descriptor.ShaderRegister = 0;
        // 1: PerFrame (b1)
        params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        params[1].Descriptor.ShaderRegister = 1;
        // 2: Field[] (b2)
        params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        params[2].Descriptor.ShaderRegister = 2;
        // 3: UAVs (u0, u1, u2)
        params[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        params[3].DescriptorTable.NumDescriptorRanges = 1;
        params[3].DescriptorTable.pDescriptorRanges = &uavRange;

        D3D12_ROOT_SIGNATURE_DESC rsDesc{};
        rsDesc.NumParameters = 4;
        rsDesc.pParameters = params;
        rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

        ComPtr<ID3DBlob> blob, err;
        D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &err);
        device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&emitRootSignature_));
    }

    // PSOs
    {
        ComPtr<IDxcBlob> csBlob = CompileShader(L"resources/engine/shaders/InitializeGPUParticle.CS.hlsl", L"cs_6_0", dx_->GetDXCUtils(), dx_->GetDXCCompiler(), dx_->GetDXCIncludeHandler());
        D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc{};
        psoDesc.pRootSignature = computeRootSignature_.Get();
        psoDesc.CS = { csBlob->GetBufferPointer(), csBlob->GetBufferSize() };
        device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&computePipelineState_));
    }
    {
        ComPtr<IDxcBlob> csBlob = CompileShader(L"resources/engine/shaders/EmitParticle.CS.hlsl", L"cs_6_0", dx_->GetDXCUtils(), dx_->GetDXCCompiler(), dx_->GetDXCIncludeHandler());
        D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc{};
        psoDesc.pRootSignature = emitRootSignature_.Get();
        psoDesc.CS = { csBlob->GetBufferPointer(), csBlob->GetBufferSize() };
        device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&emitPipelineState_));
    }
    {
        ComPtr<IDxcBlob> csBlob = CompileShader(L"resources/engine/shaders/UpdateParticle.CS.hlsl", L"cs_6_0", dx_->GetDXCUtils(), dx_->GetDXCCompiler(), dx_->GetDXCIncludeHandler());
        D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc{};
        psoDesc.pRootSignature = emitRootSignature_.Get(); // Emitと共有（Fieldもこのシグネチャに含まれる）
        psoDesc.CS = { csBlob->GetBufferPointer(), csBlob->GetBufferSize() };
        device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&updatePipelineState_));
    }
}

void GPUParticleManager::CreateGraphicsPipeline() {
    auto* device = dx_->GetDevice();

    // Root Signature
    D3D12_DESCRIPTOR_RANGE srvRange{};
    srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srvRange.NumDescriptors = 1;
    srvRange.BaseShaderRegister = 0;

    D3D12_DESCRIPTOR_RANGE texRange{};
    texRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    texRange.NumDescriptors = 1;
    texRange.BaseShaderRegister = 1;

    D3D12_ROOT_PARAMETER params[3]{};
    // 0: PerView (b0)
    params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    params[0].Descriptor.ShaderRegister = 0;
    params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    // 1: Particles (t0)
    params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[1].DescriptorTable.NumDescriptorRanges = 1;
    params[1].DescriptorTable.pDescriptorRanges = &srvRange;
    params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    // 2: Texture (t1)
    params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[2].DescriptorTable.NumDescriptorRanges = 1;
    params[2].DescriptorTable.pDescriptorRanges = &texRange;
    params[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_STATIC_SAMPLER_DESC sampler{};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderRegister = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC rsDesc{};
    rsDesc.NumParameters = 3;
    rsDesc.pParameters = params;
    rsDesc.NumStaticSamplers = 1;
    rsDesc.pStaticSamplers = &sampler;
    rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> blob, err;
    D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &err);
    device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&graphicsRootSignature_));

    // PSO: VS と PS はすべてのブレンドモードで共通
    ComPtr<IDxcBlob> vsBlob = CompileShader(L"resources/engine/shaders/GPUParticle.VS.hlsl", L"vs_6_0", dx_->GetDXCUtils(), dx_->GetDXCCompiler(), dx_->GetDXCIncludeHandler());
    ComPtr<IDxcBlob> psBlob = CompileShader(L"resources/engine/shaders/GPUParticle.PS.hlsl", L"ps_6_0", dx_->GetDXCUtils(), dx_->GetDXCCompiler(), dx_->GetDXCIncludeHandler());

    D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    // 共通 PSO ベースを構築し、ブレンドモードごとに差し替えて生成する
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.pRootSignature = graphicsRootSignature_.Get();
    psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
    psoDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };
    psoDesc.InputLayout = { inputLayout, _countof(inputLayout) };
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);

    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; // パーティクルは深度書き込みしない

    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    psoDesc.SampleDesc.Count = 1;

    // ブレンドモード (Opaque=0) 〜 (Screen=5) の全パイプラインを生成する
    for (int i = 0; i < kBlendModeCount; ++i) {
        psoDesc.BlendState = MakeGPUParticleBlendDesc(static_cast<BlendMode>(i));
        HRESULT hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&graphicsPipelineStates_[i]));
        assert(SUCCEEDED(hr) && "GPUParticle PSO 生成失敗");
    }
}

void GPUParticleManager::CreateQuad() {
    auto* renderer = Renderer::GetInstance();
    
    struct Vtx { float px, py, pz; float u, v; };
    Vtx quad[4] = {
        {-0.5f,  0.5f, 0.0f, 0.0f, 0.0f},
        { 0.5f,  0.5f, 0.0f, 1.0f, 0.0f},
        {-0.5f, -0.5f, 0.0f, 0.0f, 1.0f},
        { 0.5f, -0.5f, 0.0f, 1.0f, 1.0f},
    };
    uint16_t idx[6] = { 0,1,2, 2,1,3 };

    vb_ = renderer->CreateUploadBuffer(sizeof(quad));
    void* vbMapped = nullptr;
    vb_->Map(0, nullptr, &vbMapped);
    std::memcpy(vbMapped, quad, sizeof(quad));
    vb_->Unmap(0, nullptr);

    vbView_.BufferLocation = vb_->GetGPUVirtualAddress();
    vbView_.StrideInBytes = sizeof(Vtx);
    vbView_.SizeInBytes = sizeof(quad);

    ib_ = renderer->CreateUploadBuffer(sizeof(idx));
    void* ibMapped = nullptr;
    ib_->Map(0, nullptr, &ibMapped);
    std::memcpy(ibMapped, idx, sizeof(idx));
    ib_->Unmap(0, nullptr);

    ibView_.BufferLocation = ib_->GetGPUVirtualAddress();
    ibView_.Format = DXGI_FORMAT_R16_UINT;
    ibView_.SizeInBytes = sizeof(idx);
}
