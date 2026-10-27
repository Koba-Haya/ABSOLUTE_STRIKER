#define NOMINMAX
#include "Renderer.h"
#include "Camera.h"
#include "CloudNoiseTextures.h"
#include "DirectXCommon.h"
#include "ModelInstance.h"
#include "ModelResource.h"
#include "ModelUtils.h"
#include "ParticleManager.h"
#include "particle/GPUParticleManager.h"
#include "Ring.h"
#include "Cylinder.h"
#include "ShaderCompilerUtils.h"
#include "Skybox.h"
#include "Sprite.h"
#include "SpriteResource.h"
#include "TextureResource.h"
#include "SkinCluster.h"
#include "UnifiedPipeline.h"
#include "PrimitiveDrawer.h"
#include "RibbonDrawer.h"
#include "RibbonTrail.h"
#include <algorithm>
#include <cassert>
#include <cstring>
#include <d3d12.h>
#include <stdexcept>

#define CHECK_INIT(call)                                                       \
  {                                                                            \
    bool _res = (call);                                                        \
    if (!_res)                                                                 \
      throw std::runtime_error(#call " failed!");                              \
  }

namespace {
// 1フレームに描けるリボンの頂点の総数(共有バッファの大きさ)。
// 1本のリボンは最大 (RibbonTrail::Params::maxPoints + 1) * 2 頂点なので、同時に数十本は描ける
constexpr uint32_t kMaxRibbonVertices = 16384;
} // namespace

// シングルトンの実体を .cpp 内に隠蔽
Renderer *Renderer::GetInstance() {
  static Renderer inst;
  return &inst;
}

Renderer::Renderer() = default;
Renderer::~Renderer() = default;

void Renderer::Initialize(DirectXCommon *dx) {
  assert(dx);
  dx_ = dx;

  auto *device = dx_->GetDevice();
  auto *utils = dx_->GetDXCUtils();
  auto *compiler = dx_->GetDXCCompiler();
  auto *includeHandler = dx_->GetDXCIncludeHandler();

  // 3D Pipelines
  {
    PipelineDesc desc = UnifiedPipeline::MakeObject3DDesc();
    objPipelineOpaque_ = std::make_unique<UnifiedPipeline>();
    CHECK_INIT(objPipelineOpaque_->Initialize(device, utils, compiler,
                                              includeHandler, desc));
    desc.fillMode = D3D12_FILL_MODE_WIREFRAME;
    objPipelineWireframe_ = std::make_unique<UnifiedPipeline>();
    CHECK_INIT(objPipelineWireframe_->Initialize(device, utils, compiler,
                                                 includeHandler, desc));
    // 反射パス用: 鏡映すると三角形の巻き順が逆になるので、表裏の判定を反転する
    desc.fillMode = D3D12_FILL_MODE_SOLID;
    desc.frontCounterClockwise = true;
    objPipelineMirrored_ = std::make_unique<UnifiedPipeline>();
    CHECK_INIT(objPipelineMirrored_->Initialize(device, utils, compiler,
                                                includeHandler, desc));
  }

  // Skinned 3D Pipelines
  {
    PipelineDesc desc = UnifiedPipeline::MakeSkinnedObject3DDesc();
    skinnedPipelineOpaque_ = std::make_unique<UnifiedPipeline>();
    CHECK_INIT(skinnedPipelineOpaque_->Initialize(device, utils, compiler,
                                                  includeHandler, desc));
    desc.fillMode = D3D12_FILL_MODE_WIREFRAME;
    skinnedPipelineWireframe_ = std::make_unique<UnifiedPipeline>();
    CHECK_INIT(skinnedPipelineWireframe_->Initialize(device, utils, compiler,
                                                     includeHandler, desc));
  }

  // Skybox Pipeline
  {
    PipelineDesc desc = UnifiedPipeline::MakeSkyboxDesc();
    skyboxPipeline_ = std::make_unique<UnifiedPipeline>();
    CHECK_INIT(skyboxPipeline_->Initialize(device, utils, compiler,
                                           includeHandler, desc));
  }

  // Sky Pipeline
  {
    skyPipeline_ = std::make_unique<UnifiedPipeline>();
    CHECK_INIT(skyPipeline_->Initialize(device, utils, compiler, includeHandler,
                                        UnifiedPipeline::MakeSkyDesc()));
  }

  // Water Pipeline
  {
    waterPipeline_ = std::make_unique<UnifiedPipeline>();
    CHECK_INIT(waterPipeline_->Initialize(device, utils, compiler, includeHandler,
                                          UnifiedPipeline::MakeWaterDesc()));
  }

  // Transition Pipeline(画面遷移の最終パス)
  {
    transitionPipeline_ = std::make_unique<UnifiedPipeline>();
    CHECK_INIT(transitionPipeline_->Initialize(device, utils, compiler, includeHandler,
                                               UnifiedPipeline::MakeTransitionDesc()));
    // 画面遷移の 2 つ目のパス(レンズの水滴)
    lensDropsPipeline_ = std::make_unique<UnifiedPipeline>();
    CHECK_INIT(lensDropsPipeline_->Initialize(device, utils, compiler, includeHandler,
                                              UnifiedPipeline::MakeLensDropsDesc()));
  }

  // CloudSea Pipeline
  {
    cloudSeaPipeline_ = std::make_unique<UnifiedPipeline>();
    CHECK_INIT(cloudSeaPipeline_->Initialize(device, utils, compiler, includeHandler,
                                             UnifiedPipeline::MakeCloudSeaDesc()));
  }

  // VolumeCloud Pipeline(ボリューム雲の全画面パス)
  {
    volumeCloudDepthPipeline_ = std::make_unique<UnifiedPipeline>();
    CHECK_INIT(volumeCloudDepthPipeline_->Initialize(device, utils, compiler, includeHandler,
                                                     UnifiedPipeline::MakeVolumeCloudDepthDesc()));
    volumeCloudPipeline_ = std::make_unique<UnifiedPipeline>();
    CHECK_INIT(volumeCloudPipeline_->Initialize(device, utils, compiler, includeHandler,
                                                UnifiedPipeline::MakeVolumeCloudDesc()));
    volumeCloudCompositePipeline_ = std::make_unique<UnifiedPipeline>();
    CHECK_INIT(volumeCloudCompositePipeline_->Initialize(device, utils, compiler, includeHandler,
                                                         UnifiedPipeline::MakeVolumeCloudCompositeDesc()));
  }

  // SeaSky Pipeline(新方式の雲海の背景)
  {
    seaSkyPipeline_ = std::make_unique<UnifiedPipeline>();
    CHECK_INIT(seaSkyPipeline_->Initialize(device, utils, compiler, includeHandler,
                                           UnifiedPipeline::MakeSeaSkyDesc()));
  }

  // Sprite Pipelines
  {
    PipelineDesc desc = UnifiedPipeline::MakeSpriteDesc();
    desc.blendMode = BlendMode::Alpha;
    spritePipelineAlpha_ = std::make_unique<UnifiedPipeline>();
    CHECK_INIT(spritePipelineAlpha_->Initialize(device, utils, compiler,
                                                includeHandler, desc));
    desc.blendMode = BlendMode::Add;
    spritePipelineAdd_ = std::make_unique<UnifiedPipeline>();
    CHECK_INIT(spritePipelineAdd_->Initialize(device, utils, compiler,
                                              includeHandler, desc));
    desc.blendMode = BlendMode::Subtract;
    spritePipelineSub_ = std::make_unique<UnifiedPipeline>();
    CHECK_INIT(spritePipelineSub_->Initialize(device, utils, compiler,
                                              includeHandler, desc));
    desc.blendMode = BlendMode::Multiply;
    spritePipelineMul_ = std::make_unique<UnifiedPipeline>();
    CHECK_INIT(spritePipelineMul_->Initialize(device, utils, compiler,
                                              includeHandler, desc));
    desc.blendMode = BlendMode::Screen;
    spritePipelineScreen_ = std::make_unique<UnifiedPipeline>();
    CHECK_INIT(spritePipelineScreen_->Initialize(device, utils, compiler,
                                                 includeHandler, desc));
  }

  // Particle Pipelines
  {
    PipelineDesc desc = UnifiedPipeline::MakeParticleDesc();
    desc.blendMode = BlendMode::Alpha;
    particlePipelineAlpha_ = std::make_unique<UnifiedPipeline>();
    CHECK_INIT(particlePipelineAlpha_->Initialize(device, utils, compiler,
                                                  includeHandler, desc));
    {
      // 深度テストあり(書き込みなし)の Alpha。他の 3D オブジェクトの後ろに回る粒を正しく隠すため
      PipelineDesc depthDesc = desc;
      depthDesc.enableDepth = true;
      depthDesc.depthWrite = false;
      depthDesc.depthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
      particlePipelineAlphaDepth_ = std::make_unique<UnifiedPipeline>();
      CHECK_INIT(particlePipelineAlphaDepth_->Initialize(device, utils, compiler,
                                                         includeHandler, depthDesc));
    }
    desc.blendMode = BlendMode::Add;
    particlePipelineAdd_ = std::make_unique<UnifiedPipeline>();
    CHECK_INIT(particlePipelineAdd_->Initialize(device, utils, compiler,
                                                includeHandler, desc));
    desc.blendMode = BlendMode::Subtract;
    particlePipelineSub_ = std::make_unique<UnifiedPipeline>();
    CHECK_INIT(particlePipelineSub_->Initialize(device, utils, compiler,
                                                includeHandler, desc));
    desc.blendMode = BlendMode::Multiply;
    particlePipelineMul_ = std::make_unique<UnifiedPipeline>();
    CHECK_INIT(particlePipelineMul_->Initialize(device, utils, compiler,
                                                includeHandler, desc));
    desc.blendMode = BlendMode::Screen;
    particlePipelineScreen_ = std::make_unique<UnifiedPipeline>();
    CHECK_INIT(particlePipelineScreen_->Initialize(device, utils, compiler,
                                                   includeHandler, desc));
  }

  // Primitive Pipeline
  {
    PipelineDesc desc = UnifiedPipeline::MakePrimitiveDesc();
    primitivePipeline_ = std::make_unique<UnifiedPipeline>();
    CHECK_INIT(primitivePipeline_->Initialize(device, utils, compiler,
                                              includeHandler, desc));
  }

  // Effect Pipeline
  {
    PipelineDesc desc = UnifiedPipeline::MakeUnlitEffectDesc();
    effectPipeline_ = std::make_unique<UnifiedPipeline>();
    CHECK_INIT(effectPipeline_->Initialize(device, utils, compiler,
                                           includeHandler, desc));
    {
      // 深度を書き込む版。他は同じ(アルファブレンドと、アルファ 0.5 以下を捨てる処理は、そのまま)
      PipelineDesc depthDesc = desc;
      depthDesc.depthWrite = true;
      effectDepthPipeline_ = std::make_unique<UnifiedPipeline>();
      CHECK_INIT(effectDepthPipeline_->Initialize(device, utils, compiler,
                                                  includeHandler, depthDesc));
    }
    PipelineDesc ringDesc = UnifiedPipeline::MakeRingDesc();
    ringPipeline_ = std::make_unique<UnifiedPipeline>();
    CHECK_INIT(ringPipeline_->Initialize(device, utils, compiler,
                                         includeHandler, ringDesc));
    PipelineDesc ribbonDesc = UnifiedPipeline::MakeRibbonDesc();
    ribbonPipeline_ = std::make_unique<UnifiedPipeline>();
    CHECK_INIT(ribbonPipeline_->Initialize(device, utils, compiler,
                                           includeHandler, ribbonDesc));
    PipelineDesc cylinderDesc = UnifiedPipeline::MakeCylinderDesc();
    cylinderPipeline_ = std::make_unique<UnifiedPipeline>();
    CHECK_INIT(cylinderPipeline_->Initialize(device, utils, compiler,
                                             includeHandler, cylinderDesc));
  }

  // CopyImage Pipeline
  {
    PipelineDesc desc = UnifiedPipeline::MakeCopyImageDesc();
    copyImagePipeline_ = std::make_unique<UnifiedPipeline>();
    CHECK_INIT(copyImagePipeline_->Initialize(device, utils, compiler,
                                              includeHandler, desc));
    
    PipelineDesc grayDesc = UnifiedPipeline::MakeGrayscaleDesc();
    grayscalePipeline_ = std::make_unique<UnifiedPipeline>();
    CHECK_INIT(grayscalePipeline_->Initialize(device, utils, compiler,
                                               includeHandler, grayDesc));

    PipelineDesc sepiaDesc = UnifiedPipeline::MakeSepiaDesc();
    sepiaPipeline_ = std::make_unique<UnifiedPipeline>();
    CHECK_INIT(sepiaPipeline_->Initialize(device, utils, compiler,
                                             includeHandler, sepiaDesc));

    PipelineDesc vignetteDesc = UnifiedPipeline::MakeVignetteDesc();
    vignettePipeline_ = std::make_unique<UnifiedPipeline>();
    CHECK_INIT(vignettePipeline_->Initialize(device, utils, compiler,
                                             includeHandler, vignetteDesc));

    PipelineDesc boxFilterDesc = UnifiedPipeline::MakeBoxFilterDesc();
    boxFilterPipeline_ = std::make_unique<UnifiedPipeline>();
    CHECK_INIT(boxFilterPipeline_->Initialize(device, utils, compiler,
                                              includeHandler, boxFilterDesc));

    PipelineDesc gaussianFilterDesc = UnifiedPipeline::MakeGaussianFilterDesc();
    gaussianFilterPipeline_ = std::make_unique<UnifiedPipeline>();
    CHECK_INIT(gaussianFilterPipeline_->Initialize(device, utils, compiler,
                                                   includeHandler, gaussianFilterDesc));

    luminanceBasedOutlinePipeline_ = std::make_unique<UnifiedPipeline>();
    luminanceBasedOutlinePipeline_->Initialize(
        device, utils, compiler, includeHandler,
        UnifiedPipeline::MakeLuminanceBasedOutlineDesc());

    pipelineDepthBasedOutline_ = std::make_unique<UnifiedPipeline>();
    pipelineDepthBasedOutline_->Initialize(
        device, utils, compiler, includeHandler,
        UnifiedPipeline::MakeDepthBasedOutlineDesc());

    radialBlurPipeline_ = std::make_unique<UnifiedPipeline>();
    radialBlurPipeline_->Initialize(
        device, utils, compiler, includeHandler,
        UnifiedPipeline::MakeRadialBlurDesc());

    dissolvePipeline_ = std::make_unique<UnifiedPipeline>();
    dissolvePipeline_->Initialize(
        device, utils, compiler, includeHandler,
        UnifiedPipeline::MakeDissolveDesc());

    randomPipeline_ = std::make_unique<UnifiedPipeline>();
    randomPipeline_->Initialize(
        device, utils, compiler, includeHandler,
        UnifiedPipeline::MakeRandomDesc());
  }

  // Primitive Drawer
  primitiveDrawer_ = std::make_unique<PrimitiveDrawer>();
  primitiveDrawer_->Initialize(dx_);

  // Ribbon Drawer(軌跡の帯。頂点バッファは全リボンで共有)
  ribbonDrawer_ = std::make_unique<RibbonDrawer>();
  ribbonDrawer_->Initialize(device, kMaxRibbonVertices);

  auto align256 = [](size_t size) -> size_t { return (size + 255) & ~255; };

  cameraCB_ = CreateUploadBuffer(align256(sizeof(CameraForGPU)));
  cameraCB_->Map(0, nullptr, reinterpret_cast<void **>(&cameraMapped_));
  directionalLightCB_ =
      CreateUploadBuffer(align256(sizeof(DirectionalLightGroupCB)));
  directionalLightCB_->Map(0, nullptr,
                           reinterpret_cast<void **>(&directionalLightMapped_));
  pointLightCB_ = CreateUploadBuffer(align256(sizeof(PointLightGroupCB)));
  pointLightCB_->Map(0, nullptr, reinterpret_cast<void **>(&pointLightMapped_));
  spotLightCB_ = CreateUploadBuffer(align256(sizeof(SpotLightGroupCB)));
  spotLightCB_->Map(0, nullptr, reinterpret_cast<void **>(&spotLightMapped_));

  skinningInformationCB_ = CreateUploadBuffer(align256(sizeof(SkinningInformation)));
  skinningInformationCB_->Map(0, nullptr, reinterpret_cast<void **>(&skinningInformationMapped_));

  GPUParticleManager::GetInstance()->Initialize(dx_);

  primitiveTransformCB_ = CreateUploadBuffer(sizeof(TransformCB));
  primitiveTransformCB_->Map(
      0, nullptr, reinterpret_cast<void **>(&primitiveTransformMapped_));

  ribbonTransformCB_ = CreateUploadBuffer(align256(sizeof(TransformCB)));
  ribbonTransformCB_->Map(
      0, nullptr, reinterpret_cast<void **>(&ribbonTransformMapped_));

  vignetteParamCB_ = CreateUploadBuffer(align256(sizeof(VignetteParam)));
  vignetteParamCB_->Map(0, nullptr, reinterpret_cast<void **>(&vignetteParamMapped_));
  if (vignetteParamMapped_) {
    vignetteParamMapped_->scale = 16.0f;
    vignetteParamMapped_->powValue = 0.8f;
    vignetteParamMapped_->tint[0] = 0.0f; // 既定は黒の暗転(強さ 1)
    vignetteParamMapped_->tint[1] = 0.0f;
    vignetteParamMapped_->tint[2] = 0.0f;
    vignetteParamMapped_->tint[3] = 1.0f;
  }

  boxFilterParamCB_ = CreateUploadBuffer(align256(sizeof(BoxFilterParam)));
  boxFilterParamCB_->Map(0, nullptr, reinterpret_cast<void **>(&boxFilterParamMapped_));
  if (boxFilterParamMapped_) {
    boxFilterParamMapped_->k = 1;
  }

  gaussianFilterParamCB_ = CreateUploadBuffer(align256(sizeof(GaussianFilterParam)));
  gaussianFilterParamCB_->Map(0, nullptr, reinterpret_cast<void **>(&gaussianFilterParamMapped_));
  if (gaussianFilterParamMapped_) {
    gaussianFilterParamMapped_->k = 1;
    gaussianFilterParamMapped_->sigma = 1.0f;
    gaussianFilterParamMapped_->direction[0] = 1.0f;
    gaussianFilterParamMapped_->direction[1] = 0.0f;
  }

  depthBasedOutlineParamCB_ = CreateUploadBuffer(align256(sizeof(DepthBasedOutlineParam)));
  depthBasedOutlineParamCB_->Map(0, nullptr, reinterpret_cast<void **>(&depthBasedOutlineParamMapped_));

  radialBlurParamCB_ = CreateUploadBuffer(align256(sizeof(RadialBlurParam)));
  radialBlurParamCB_->Map(0, nullptr, reinterpret_cast<void **>(&radialBlurParamCBMap_));
  if (radialBlurParamCBMap_) {
    radialBlurParamCBMap_->center = {0.5f, 0.5f};
    radialBlurParamCBMap_->blurWidth = 0.01f;
  }

  dissolveParamCB_ = CreateUploadBuffer(align256(sizeof(DissolveParam)));
  dissolveParamCB_->Map(0, nullptr, reinterpret_cast<void **>(&dissolveParamMapped_));
  if (dissolveParamMapped_) {
    dissolveParamMapped_->threshold = 0.5f;
    dissolveParamMapped_->edgeRange = 0.05f;
    dissolveParamMapped_->edgeColor = {1.0f, 0.4f, 0.3f};
    dissolveParamMapped_->maskColor = {0.0f, 0.0f, 0.0f};
  }

  randomParamCB_ = CreateUploadBuffer(align256(sizeof(RandomParam)));
  randomParamCB_->Map(0, nullptr, reinterpret_cast<void **>(&randomParamMapped_));
  if (randomParamMapped_) {
      randomParamMapped_->time = 0.0f;
  }

  skyParamCB_ = CreateUploadBuffer(align256(sizeof(SkyParam)));
  skyParamCB_->Map(0, nullptr, reinterpret_cast<void **>(&skyParamMapped_));

  waterParamCB_ = CreateUploadBuffer(align256(sizeof(WaterParam)));
  waterParamCB_->Map(0, nullptr, reinterpret_cast<void **>(&waterParamMapped_));

  cloudSeaParamCB_ = CreateUploadBuffer(align256(sizeof(CloudSeaParam)));
  cloudSeaParamCB_->Map(0, nullptr, reinterpret_cast<void **>(&cloudSeaParamMapped_));

  seaSkyParamCB_ = CreateUploadBuffer(align256(sizeof(SeaSkyParam)));
  seaSkyParamCB_->Map(0, nullptr, reinterpret_cast<void **>(&seaSkyParamMapped_));

  for (size_t i = 0; i < kMaxVolumeClouds; ++i) {
    volumeCloudParamCBs_[i] = CreateUploadBuffer(align256(sizeof(VolumeCloudParam)));
    volumeCloudParamCBs_[i]->Map(0, nullptr, reinterpret_cast<void **>(&volumeCloudParamMapped_[i]));
  }
  volumeCloudRequests_.reserve(kMaxVolumeClouds);

  transitionParamCB_ = CreateUploadBuffer(align256(sizeof(TransitionParam)));
  transitionParamCB_->Map(0, nullptr, reinterpret_cast<void **>(&transitionParamMapped_));

  InitSkinningPipeline_();

  // ボリューム雲用の 3D ノイズ。全シーンで共有するので、起動時に 1 回だけ作る
  cloudNoise_ = std::make_unique<CloudNoiseTextures>();
  cloudNoise_->Initialize(dx_);
}

void Renderer::InitSkinningPipeline_() {
  auto* device = dx_->GetDevice();

  // Root Signature
  D3D12_DESCRIPTOR_RANGE ranges[4]{};
  // t0: Palette
  ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
  ranges[0].NumDescriptors = 1;
  ranges[0].BaseShaderRegister = 0;
  // t1: InputVertices
  ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
  ranges[1].NumDescriptors = 1;
  ranges[1].BaseShaderRegister = 1;
  // t2: Influences
  ranges[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
  ranges[2].NumDescriptors = 1;
  ranges[2].BaseShaderRegister = 2;
  // u0: OutputVertices
  ranges[3].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
  ranges[3].NumDescriptors = 1;
  ranges[3].BaseShaderRegister = 0;

  D3D12_ROOT_PARAMETER params[5]{};
  params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  params[0].DescriptorTable.NumDescriptorRanges = 1;
  params[0].DescriptorTable.pDescriptorRanges = &ranges[0];
  params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

  params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  params[1].DescriptorTable.NumDescriptorRanges = 1;
  params[1].DescriptorTable.pDescriptorRanges = &ranges[1];
  params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

  params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  params[2].DescriptorTable.NumDescriptorRanges = 1;
  params[2].DescriptorTable.pDescriptorRanges = &ranges[2];
  params[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

  params[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  params[3].DescriptorTable.NumDescriptorRanges = 1;
  params[3].DescriptorTable.pDescriptorRanges = &ranges[3];
  params[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

  params[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
  params[4].Descriptor.ShaderRegister = 0;
  params[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

  D3D12_ROOT_SIGNATURE_DESC rsDesc{};
  rsDesc.NumParameters = 5;
  rsDesc.pParameters = params;
  rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

  ComPtr<ID3DBlob> blob, err;
  D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &err);
  if (err) {
    OutputDebugStringA((const char*)err->GetBufferPointer());
  }
  device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&skinningRootSignature_));

  // Pipeline State
  ComPtr<IDxcBlob> csBlob = CompileShader(L"resources/engine/shaders/Skinning.CS.hlsl", L"cs_6_0", dx_->GetDXCUtils(), dx_->GetDXCCompiler(), dx_->GetDXCIncludeHandler());
  assert(csBlob);

  D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc{};
  psoDesc.pRootSignature = skinningRootSignature_.Get();
  psoDesc.CS = { csBlob->GetBufferPointer(), csBlob->GetBufferSize() };
  psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

  device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&skinningPipelineState_));
}

void Renderer::DispatchSkinning(ModelInstance* instance) {
  if (!instance || !instance->GetResource() || !instance->GetSkinCluster()) return;
  auto* resource = instance->GetResource();
  auto* skinCluster = instance->GetSkinCluster();
  auto* cmdList = dx_->GetCommandList();

  // 1. バリア設定 (VertexBuffer -> UAV)
  D3D12_RESOURCE_BARRIER barrier{};
  barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Transition.pResource = skinCluster->skinnedVertexBuffer.Get();
  barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
  barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
  barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
  cmdList->ResourceBarrier(1, &barrier);

  // 2. パイプラインセット
  cmdList->SetComputeRootSignature(skinningRootSignature_.Get());
  cmdList->SetPipelineState(skinningPipelineState_.Get());

  // 3. ディスクリプタセット
  ID3D12DescriptorHeap* heaps[] = { dx_->GetSRVHeap() };
  cmdList->SetDescriptorHeaps(1, heaps);

  auto& srvAlloc = dx_->GetSrvAllocator();
  // t0: Palette
  cmdList->SetComputeRootDescriptorTable(0, srvAlloc.Gpu(skinCluster->srvIndex));
  // t1: InputVertices
  cmdList->SetComputeRootDescriptorTable(1, srvAlloc.Gpu(resource->GetVertexSRVIndex()));
  // t2: Influences
  cmdList->SetComputeRootDescriptorTable(2, srvAlloc.Gpu(resource->GetBoneSRVIndex()));
  // u0: OutputVertices
  cmdList->SetComputeRootDescriptorTable(3, srvAlloc.Gpu(skinCluster->uavIndex));
  // b0: SkinningInformation
  skinningInformationMapped_->numVertices = resource->GetVertexCount();
  cmdList->SetComputeRootConstantBufferView(4, skinningInformationCB_->GetGPUVirtualAddress());

  // 4. Dispatch
  uint32_t numVertices = resource->GetVertexCount();
  cmdList->Dispatch((numVertices + 1023) / 1024, 1, 1);

  // 5. バリア設定 (UAV -> VertexBuffer)
  barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
  barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
  cmdList->ResourceBarrier(1, &barrier);
}

void Renderer::SetCamera(const Camera &camera) {
  view_ = camera.GetViewMatrix();
  proj_ = camera.GetProjectionMatrix();
  if (cameraMapped_) {
    Matrix4x4 invView = Inverse(view_);
    cameraMapped_->worldPosition = {invView.m[3][0], invView.m[3][1],
                                    invView.m[3][2]};
    cameraMapped_->pad = 0.0f;
  }
}

void Renderer::SetDirectionalLights(const std::vector<DirLight> &lights,
                                    bool groupEnabled) {
  if (!directionalLightMapped_)
    return;
  std::memset(directionalLightMapped_, 0, sizeof(DirectionalLightGroupCB));
  int count = static_cast<int>(
      std::min(lights.size(), static_cast<size_t>(kMaxDirLights)));
  directionalLightMapped_->count = count;
  directionalLightMapped_->enabled = groupEnabled ? 1 : 0;
  for (int i = 0; i < count; ++i) {
    const auto &src = lights[i];
    auto &dst = directionalLightMapped_->lights[i];
    dst.color[0] = src.color.x;
    dst.color[1] = src.color.y;
    dst.color[2] = src.color.z;
    dst.color[3] = 1.0f;
    dst.direction[0] = src.direction.x;
    dst.direction[1] = src.direction.y;
    dst.direction[2] = src.direction.z;
    dst.intensity = src.intensity;
    dst.enabled = src.enabled ? 1 : 0;
  }
}

void Renderer::SetPointLights(const std::vector<PointLight> &lights,
                              bool groupEnabled) {
  if (!pointLightMapped_)
    return;
  std::memset(pointLightMapped_, 0, sizeof(PointLightGroupCB));
  int count = static_cast<int>(
      std::min(lights.size(), static_cast<size_t>(kMaxPointLights)));
  pointLightMapped_->count = count;
  pointLightMapped_->enabled = groupEnabled ? 1 : 0;
  for (int i = 0; i < count; ++i) {
    const auto &src = lights[i];
    auto &dst = pointLightMapped_->lights[i];
    dst.color[0] = src.color.x;
    dst.color[1] = src.color.y;
    dst.color[2] = src.color.z;
    dst.color[3] = 1.0f;
    dst.position[0] = src.position.x;
    dst.position[1] = src.position.y;
    dst.position[2] = src.position.z;
    dst.intensity = src.intensity;
    dst.radius = src.radius;
    dst.decay = src.decay;
    dst.enabled = src.enabled ? 1 : 0;
  }
}

void Renderer::SetSpotLights(const std::vector<SpotLight> &lights,
                             bool groupEnabled) {
  if (!spotLightMapped_)
    return;
  std::memset(spotLightMapped_, 0, sizeof(SpotLightGroupCB));
  int count = static_cast<int>(
      std::min(lights.size(), static_cast<size_t>(kMaxSpotLights)));
  spotLightMapped_->count = count;
  spotLightMapped_->enabled = groupEnabled ? 1 : 0;
  static constexpr float kPi = 3.14159265f;
  for (int i = 0; i < count; ++i) {
    const auto &src = lights[i];
    auto &dst = spotLightMapped_->lights[i];
    dst.color[0] = src.color.x;
    dst.color[1] = src.color.y;
    dst.color[2] = src.color.z;
    dst.color[3] = 1.0f;
    dst.position[0] = src.position.x;
    dst.position[1] = src.position.y;
    dst.position[2] = src.position.z;
    dst.direction[0] = src.direction.x;
    dst.direction[1] = src.direction.y;
    dst.direction[2] = src.direction.z;
    dst.intensity = src.intensity;
    dst.distance = src.distance;
    dst.decay = src.decay;
    dst.cosAngle = std::cos(src.coneAngleDeg * (kPi / 180.0f));
    dst.enabled = src.enabled ? 1 : 0;
  }
}

float Renderer::GetScreenWidth() const { return dx_->GetViewport().Width; }
float Renderer::GetScreenHeight() const { return dx_->GetViewport().Height; }
float Renderer::GetAspectRatio() const {
  float h = GetScreenHeight();
  return (h != 0.0f) ? (GetScreenWidth() / h) : 1.0f;
}

void Renderer::DrawModel(ModelInstance *instance) {
  if (!instance || !instance->GetResource())
    return;
  auto *cmdList = dx_->GetCommandList();
  auto *resource = instance->GetResource();

  // 反射パスの間は、水面を軸に上下反転した鏡像として描く
  const bool mirrored = planarReflection_ && planarReflection_->IsActive();

  // 描画直前に WVP 行列を最終計算して GPU に送る
  D3D12_GPU_VIRTUAL_ADDRESS transformCBAddress = instance->GetTransformCBAddress();
  if (mirrored) {
    // 鏡像は専用の CB に書く(通常の描画と同じ CB を使うと、後から書いた内容で両方が描かれてしまう)
    auto *cbMirror = instance->GetMirrorTransformMapped();
    cbMirror->World = Multiply(instance->GetWorld(), planarReflection_->GetMirrorMatrix());
    cbMirror->WorldInverseTranspose = Transpose(Inverse(cbMirror->World));
    cbMirror->WVP = Multiply(Multiply(cbMirror->World, view_), proj_);
    transformCBAddress = instance->GetMirrorTransformCBAddress();
  } else if (auto *cbTrans = instance->GetTransformMapped()) {
    // WVP = (World * View) * Projection
    Matrix4x4 worldView = Multiply(instance->GetWorld(), view_);
    cbTrans->WVP = Multiply(worldView, proj_);
  }

  // 1. パイプライン設定
  bool hasBones = resource->HasBones();
  SkinCluster* skinCluster = instance->GetSkinCluster();

  UnifiedPipeline *pipeline = nullptr;
  if (mirrored) {
    // 反射パスはワイヤーフレームにしない(巻き順を反転した専用パイプライン)
    pipeline = objPipelineMirrored_.get();
  } else if (hasBones && skinCluster) {
    // CSでスキニング済みのため、非スキニング用パイプラインを使用する
    pipeline = instance->IsWireframe() ? objPipelineWireframe_.get()
                                       : objPipelineOpaque_.get();
  } else {
    pipeline = instance->IsWireframe() ? objPipelineWireframe_.get()
                                       : objPipelineOpaque_.get();
  }
  pipeline->SetPipelineState(cmdList);

  // 2. 頂点バッファ設定
  D3D12_VERTEX_BUFFER_VIEW vbv{};
  if (hasBones && skinCluster) {
    // スキニング済みバッファを使用
    vbv = skinCluster->vbView;
  } else {
    // 静的バッファを使用
    vbv.BufferLocation = resource->GetVBVAddress();
    vbv.SizeInBytes = resource->GetVBVSize();
    vbv.StrideInBytes = resource->GetVBVStride();
  }
  cmdList->IASetVertexBuffers(0, 1, &vbv);
  
  // Index Buffer
  if (resource->GetIndexCount() > 0) {
    D3D12_INDEX_BUFFER_VIEW ibv{};
    ibv.BufferLocation = resource->GetIBVAddress();
    ibv.Format = DXGI_FORMAT_R32_UINT;
    ibv.SizeInBytes = resource->GetIBVSize();
    cmdList->IASetIndexBuffer(&ibv);
  }

  // 3. 定数バッファ (Material / Transform)
  cmdList->SetGraphicsRootConstantBufferView(0,
                                             instance->GetMaterialCBAddress());
  cmdList->SetGraphicsRootConstantBufferView(1, transformCBAddress);

  // 4. テクスチャ設定
  ID3D12DescriptorHeap *heaps[] = {dx_->GetSRVHeap()};
  cmdList->SetDescriptorHeaps(1, heaps);

  // t1: 環境マップ
  if (environmentMap_) {
    cmdList->SetGraphicsRootDescriptorTable(7, environmentMap_->GetSrvGpu());
  }
  
  // t2: マスクテクスチャ (Dissolve用)。インスタンス専用のマスクがあればそれ、なければ共有のマスク
  if (TextureResource* instanceMask = instance->GetDissolveMaskTexture()) {
    cmdList->SetGraphicsRootDescriptorTable(8, instanceMask->GetSrvGpu());
  } else if (dissolveMaskTexture_) {
    cmdList->SetGraphicsRootDescriptorTable(8, dissolveMaskTexture_->GetSrvGpu());
  }

  // 5. ライト・カメラ設定
  cmdList->SetGraphicsRootConstantBufferView(
      3, directionalLightCB_->GetGPUVirtualAddress());
  cmdList->SetGraphicsRootConstantBufferView(4,
                                             cameraCB_->GetGPUVirtualAddress());
  cmdList->SetGraphicsRootConstantBufferView(
      5, pointLightCB_->GetGPUVirtualAddress());
  cmdList->SetGraphicsRootConstantBufferView(
      6, spotLightCB_->GetGPUVirtualAddress());


  // 6. 描画
  cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  const uint32_t subMeshCount = resource->GetSubMeshCount();
  if (subMeshCount == 0 || instance->GetOverrideTexture()) {
    // 単一メッシュ、またはオーバーライドテクスチャ指定時は従来通り1回の描画で済ませる
    D3D12_GPU_DESCRIPTOR_HANDLE texHandle{};
    if (instance->GetOverrideTexture()) {
      texHandle.ptr = instance->GetOverrideTexture()->GetSrvGpu().ptr;
    } else {
      texHandle.ptr = resource->GetTextureHandleGPUAsUInt64();
    }
    cmdList->SetGraphicsRootDescriptorTable(2, texHandle);

    if (resource->GetIndexCount() > 0) {
      cmdList->DrawIndexedInstanced(resource->GetIndexCount(), 1, 0, 0, 0);
    } else {
      cmdList->DrawInstanced(resource->GetVertexCount(), 1, 0, 0);
    }
  } else {
    // MultiMesh & MultiMaterial対応：サブメッシュごとにテクスチャを差し替えて個別に描画する
    for (uint32_t i = 0; i < subMeshCount; ++i) {
      D3D12_GPU_DESCRIPTOR_HANDLE texHandle{};
      texHandle.ptr = resource->GetSubMeshTextureHandleGPUAsUInt64(i);
      cmdList->SetGraphicsRootDescriptorTable(2, texHandle);

      const SubMeshRange &range = resource->GetSubMeshRange(i);
      cmdList->DrawIndexedInstanced(range.indexCount, 1, range.indexStart, 0, 0);
    }
  }
}

void Renderer::DrawSprite(Sprite *sprite) {
  if (!sprite || !sprite->GetResource())
    return;
  auto *cmdList = dx_->GetCommandList();
  auto *res = sprite->GetResource();

  // WVP行列の合成 (Spriteは通常、カメラのViewを無視してProjectionのみ掛ける)
  auto *cbTrans = sprite->GetTransformMapped();
  if (cbTrans) {
    float w = GetScreenWidth();
    float h = GetScreenHeight();
    Matrix4x4 orthoProj = MakeOrthographicMatrix(0.0f, 0.0f, w, h, 0.0f, 100.0f);
    cbTrans->WVP = Multiply(sprite->GetWorldMatrix(), orthoProj);
  }

  UnifiedPipeline *pipeline = GetSpritePipeline_(sprite->GetBlendMode());
  pipeline->SetPipelineState(cmdList);

  // 頂点・インデックスバッファ
  D3D12_VERTEX_BUFFER_VIEW vbv{};
  vbv.BufferLocation = res->GetVBAddress();
  vbv.SizeInBytes = res->GetVBSize();
  vbv.StrideInBytes = res->GetVBStride();
  cmdList->IASetVertexBuffers(0, 1, &vbv);

  D3D12_INDEX_BUFFER_VIEW ibv{};
  ibv.BufferLocation = res->GetIBAddress();
  ibv.SizeInBytes = res->GetIBSize();
  ibv.Format = DXGI_FORMAT_R16_UINT;
  cmdList->IASetIndexBuffer(&ibv);

  // 定数バッファ
  cmdList->SetGraphicsRootConstantBufferView(0, sprite->GetMaterialCBAddress());
  cmdList->SetGraphicsRootConstantBufferView(1,
                                             sprite->GetTransformCBAddress());

  // テクスチャ
  ID3D12DescriptorHeap *heaps[] = {dx_->GetSRVHeap()};
  cmdList->SetDescriptorHeaps(1, heaps);
  cmdList->SetGraphicsRootDescriptorTable(2, sprite->GetTextureHandle());

  cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  cmdList->DrawIndexedInstanced(res->GetIndexCount(), 1, 0, 0, 0);
}

void Renderer::DrawSkybox(Skybox *skybox) {
  if (!skybox)
    return;
  auto *cmdList = dx_->GetCommandList();
  skyboxPipeline_->SetPipelineState(cmdList);

  D3D12_VERTEX_BUFFER_VIEW vbv{};
  vbv.BufferLocation = skybox->GetVBAddress();
  vbv.SizeInBytes = skybox->GetVBSize();
  vbv.StrideInBytes = skybox->GetVBStride();
  cmdList->IASetVertexBuffers(0, 1, &vbv);

  D3D12_INDEX_BUFFER_VIEW ibv{};
  ibv.BufferLocation = skybox->GetIBAddress();
  ibv.SizeInBytes = skybox->GetIBSize();
  ibv.Format = DXGI_FORMAT_R32_UINT;
  cmdList->IASetIndexBuffer(&ibv);

  cmdList->SetGraphicsRootConstantBufferView(0, skybox->GetMaterialCBAddress());
  cmdList->SetGraphicsRootConstantBufferView(1,
                                             skybox->GetTransformCBAddress());

  ID3D12DescriptorHeap *heaps[] = {dx_->GetSRVHeap()};
  cmdList->SetDescriptorHeaps(1, heaps);
  cmdList->SetGraphicsRootDescriptorTable(2, skybox->GetTexture()->GetSrvGpu());

  cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  cmdList->DrawIndexedInstanced(skybox->GetIndexCount(), 1, 0, 0, 0);
}

void Renderer::DrawSky(const Vector3 &sunDirection, float time, const Vector3 &worldOffset) {
  if (!skyPipeline_ || !skyParamMapped_)
    return;
  auto *cmdList = dx_->GetCommandList();

  // ピクセルの NDC 座標から視線方向を復元するための逆行列
  const Matrix4x4 invView = Inverse(view_);
  skyParamMapped_->invViewProj = Inverse(Multiply(view_, proj_));
  skyParamMapped_->sunDirection = sunDirection;
  skyParamMapped_->time = time;
  skyParamMapped_->cameraPosition = {invView.m[3][0] + worldOffset.x, invView.m[3][1] + worldOffset.y, invView.m[3][2] + worldOffset.z};

  skyPipeline_->SetPipelineState(cmdList);
  cmdList->SetGraphicsRootConstantBufferView(0, skyParamCB_->GetGPUVirtualAddress());

  // 頂点バッファなしで3頂点描画(大きな三角形1つで全画面を覆う)
  cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  cmdList->DrawInstanced(3, 1, 0, 0);
}

void Renderer::DrawWater(const Vector3 &sunDirection, float time, const Vector3 &worldOffset, float worldWrapLength) {
  // 水面は平面反射のテクスチャを必ず読むので、InitializePlanarReflection 済みでなければ描かない
  if (!waterPipeline_ || !waterParamMapped_ || !planarReflection_ || !planarReflection_->IsInitialized())
    return;
  auto *cmdList = dx_->GetCommandList();

  // 視線の復元用の逆行列と、水面との交点計算に使うカメラ位置(view の逆行列の平行移動成分)
  const Matrix4x4 invView = Inverse(view_);
  waterParamMapped_->invViewProj = Inverse(Multiply(view_, proj_));
  waterParamMapped_->cameraPosition = {invView.m[3][0] + worldOffset.x, invView.m[3][1] + worldOffset.y, invView.m[3][2] + worldOffset.z};
  waterParamMapped_->time = time;
  waterParamMapped_->sunDirection = sunDirection;
  waterParamMapped_->localCameraPosition = {invView.m[3][0], invView.m[3][1], invView.m[3][2]};
  waterParamMapped_->worldWrapLength = worldWrapLength;
  waterParamMapped_->worldWrapCount = worldWrapLength > 0.0f ? std::round(worldOffset.z / worldWrapLength) : 0.0f;

  waterPipeline_->SetPipelineState(cmdList);
  cmdList->SetGraphicsRootConstantBufferView(0, waterParamCB_->GetGPUVirtualAddress());

  // t0: 平面反射のテクスチャ(ルートパラメータの 1 番)
  ID3D12DescriptorHeap *heaps[] = {dx_->GetSRVHeap()};
  cmdList->SetDescriptorHeaps(1, heaps);
  cmdList->SetGraphicsRootDescriptorTable(1, planarReflection_->GetSrvGpuHandle());

  cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  cmdList->DrawInstanced(3, 1, 0, 0);
}

namespace {
// 雲海のノイズの層ごとの倍率(主ノイズ gScale に対する比)。シェーダー側へは定数バッファで渡すので、値はここだけで決める
constexpr float kCloudSeaLowRatioA = 0.18f;   // 大きなうねり 1 層目
constexpr float kCloudSeaLowRatioB = 0.37f;   // 大きなうねり 2 層目
constexpr float kCloudSeaFloorRatio = 0.8f;   // 切れ間から見える下の雲層
} // namespace

// カメラ位置(xz)にノイズ空間の倍率 cellsPerUnit を掛け、整数のマス番号と小数に分ける(double で計算する)
void Renderer::SplitCloudSeaCell(const Vector3 &cameraPosition, double cellsPerUnit, CloudSeaCellOrigin &out) {
  const double cellX = static_cast<double>(cameraPosition.x) * cellsPerUnit;
  const double cellZ = static_cast<double>(cameraPosition.z) * cellsPerUnit;
  const double floorX = std::floor(cellX);
  const double floorZ = std::floor(cellZ);
  out.cell[0] = static_cast<int32_t>(floorX);
  out.cell[1] = static_cast<int32_t>(floorZ);
  out.frac[0] = static_cast<float>(cellX - floorX);
  out.frac[1] = static_cast<float>(cellZ - floorZ);
}

void Renderer::DrawCloudSea(const CloudSeaSettings &settings) {
  if (!cloudSeaPipeline_ || !cloudSeaParamMapped_)
    return;
  auto *cmdList = dx_->GetCommandList();

  const Matrix4x4 invView = Inverse(view_);
  const Vector3 cameraPosition{invView.m[3][0], invView.m[3][1], invView.m[3][2]};

  CloudSeaParam *param = cloudSeaParamMapped_;
  param->invViewProj = Inverse(Multiply(view_, proj_));
  param->cameraPosition = cameraPosition;
  param->seaHeight = settings.seaHeight;
  param->sunDirection = Normalize(settings.SunDirection());
  param->thickness = settings.thickness;
  param->bump = settings.bump;
  param->scale = settings.scale;
  param->threshold = settings.threshold;
  param->softness = settings.softness;
  param->density = settings.density;
  param->fog = settings.fog;
  param->steps = static_cast<float>(settings.steps);
  param->jitter = settings.jitter ? 1.0f : 0.0f;
  param->swellHeight = settings.swellHeight;
  param->swellBlend = settings.swellBlend;
  param->billow = settings.billow;
  param->shadowStrength = settings.shadowStrength;
  param->shadowLength = settings.shadowLength;
  param->gapDarkness = settings.gapDarkness;
  param->aerial = settings.aerial;
  param->contrast = settings.contrast;
  param->lowRatioA = kCloudSeaLowRatioA;
  param->lowRatioB = kCloudSeaLowRatioB;
  param->floorRatio = kCloudSeaFloorRatio;

  // 雲の模様の位置(ノイズ空間 = ワールド座標 * 倍率)を、double で整数部と小数部に分ける。
  // シェーダーは視線上の点をカメラからの相対位置だけで計算するので、ワールド座標が大きくなっても精度が落ちない
  const double scale = static_cast<double>(settings.scale);
  SplitCloudSeaCell(cameraPosition, scale, param->base);
  SplitCloudSeaCell(cameraPosition, scale * kCloudSeaLowRatioA, param->lowA);
  SplitCloudSeaCell(cameraPosition, scale * kCloudSeaLowRatioB, param->lowB);
  SplitCloudSeaCell(cameraPosition, scale * kCloudSeaFloorRatio, param->floor);

  // ここから関数の終わり(全画面の描画)までの GPU 時間を、"CloudSea" として測る
  GpuTimer::Scope gpuScope(dx_->GetGpuTimer(), cmdList, "CloudSea");

  cloudSeaPipeline_->SetPipelineState(cmdList);
  cmdList->SetGraphicsRootConstantBufferView(0, cloudSeaParamCB_->GetGPUVirtualAddress());

  // 頂点バッファなしで3頂点描画(大きな三角形1つで全画面を覆う)
  cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  cmdList->DrawInstanced(3, 1, 0, 0);
}

void Renderer::DrawSeaSky(const SeaSkySettings &settings) {
  if (!seaSkyPipeline_ || !seaSkyParamMapped_) {
    return;
  }
  auto *cmdList = dx_->GetCommandList();
  GpuTimer::Scope gpuScope(dx_->GetGpuTimer(), cmdList, "SeaSky");

  const Matrix4x4 invView = Inverse(view_);
  seaSkyParamMapped_->invViewProj = Inverse(Multiply(view_, proj_));
  seaSkyParamMapped_->cameraPosition = {invView.m[3][0], invView.m[3][1], invView.m[3][2]};
  seaSkyParamMapped_->floorHeight = settings.floorHeight;
  seaSkyParamMapped_->sunDirection = Normalize(settings.SunDirection());
  seaSkyParamMapped_->gapDarkness = settings.gapDarkness;

  seaSkyPipeline_->SetPipelineState(cmdList);
  cmdList->SetGraphicsRootConstantBufferView(0, seaSkyParamCB_->GetGPUVirtualAddress());

  // 頂点バッファなしで 3 頂点描画(大きな三角形 1 つで全画面を覆う)
  cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  cmdList->DrawInstanced(3, 1, 0, 0);
}

void Renderer::AddVolumeCloud(const VolumeCloudSettings &settings, const Vector3 &sunDirection, float time) {
  if (!settings.enabled || volumeCloudRequests_.size() >= kMaxVolumeClouds) {
    return;
  }
  volumeCloudRequests_.push_back({settings, Normalize(sunDirection), time});
}

void Renderer::FillVolumeCloudParam_(VolumeCloudParam &param, const VolumeCloudSettings &s, const Vector3 &sunDirection, float time,
                                     const Matrix4x4 &invViewProj, const Vector3 &cameraPosition, uint32_t sceneWidth,
                                     uint32_t sceneHeight, int32_t divisor) {
  // 低解像度の範囲の大きさ(分母で割って、切り上げる)
  const uint32_t lowWidth = (sceneWidth + static_cast<uint32_t>(divisor) - 1u) / static_cast<uint32_t>(divisor);
  const uint32_t lowHeight = (sceneHeight + static_cast<uint32_t>(divisor) - 1u) / static_cast<uint32_t>(divisor);

  param.invViewProj = invViewProj;
  param.cameraPosition = cameraPosition;
  param.time = time;
  param.sunDirection = sunDirection;
  param.extinction = s.density;
  param.boxCenter = s.center;
  param.noiseScale = s.noiseScale;
  param.boxHalfSize = s.halfSize;
  param.detailScale = s.detailScale;
  param.windVelocity = s.wind;
  param.threshold = s.threshold;
  param.edgeFade = s.edgeFade;
  param.erosion = s.erosion;
  param.steps = static_cast<float>(s.steps);
  param.lightSteps = static_cast<float>(s.lightSteps);
  param.lightStepLength = s.lightStepLength;
  param.phaseG = s.phaseG;
  param.sunIntensity = s.sunIntensity;
  param.ambientIntensity = s.ambientIntensity;
  param.fullSize[0] = static_cast<float>(sceneWidth);
  param.fullSize[1] = static_cast<float>(sceneHeight);
  param.lowSize[0] = static_cast<float>(lowWidth);
  param.lowSize[1] = static_cast<float>(lowHeight);
  param.divisor = static_cast<float>(divisor);
  param.shapeMode = s.shape == VolumeCloudShape::Layer ? 1.0f : 0.0f;
  param.layerBottom = s.layerBottom;
  param.layerTop = s.layerTop;
  param.maxDistance = s.maxDistance;
  param.fogDensity = s.fogDensity;
  param.highlightKnee = s.highlightKnee;
  param.shadowLift = s.shadowLift;

  // 着地の反応(層のときだけ有効にする。箱は強さ 0 のままにして、シェーダーが何もしないようにする)
  const VolumeCloudImpact &impact = s.impact;
  const bool impactEnabled = s.shape == VolumeCloudShape::Layer;
  param.impactCenter[0] = impact.centerX;
  param.impactCenter[1] = impact.centerZ;
  param.impactAge = impact.age;
  param.impactStrength = impactEnabled ? impact.strength : 0.0f;
  param.impactCraterRadius = impact.craterRadius;
  param.impactCraterDepth = impact.craterDepth;
  param.impactRingWidth = impact.ringWidth;
  param.impactRingHeight = impact.ringHeight;
  param.impactRingSpeed = impact.ringSpeed;
  param.impactRefillSeconds = impact.refillSeconds;
  param.impactRingFadeSeconds = impact.ringFadeSeconds;
  param.seaFill = impactEnabled ? impact.seaFill : 0.0f;
}

void Renderer::DrawVolumeCloudPass_() {
  // 依頼は 1 フレーム分。毎フレーム AddVolumeCloud で渡し直す(シーンが渡さなくなれば、自然に描かれなくなる)。
  // 描いても描かなくても、この関数の終わりで空にする
  const size_t count = volumeCloudRequests_.size();
  const bool ready = count > 0 && volumeCloudDepthPipeline_ && volumeCloudPipeline_ && volumeCloudCompositePipeline_ &&
                     cloudNoise_ && cloudNoise_->IsInitialized() && renderTexture_ && depthTexture_ && volumeCloudDistanceLow_ &&
                     volumeCloudColorLow_;
  if (!ready) {
    volumeCloudRequests_.clear();
    return;
  }
  auto *cmdList = dx_->GetCommandList();

  // 全体と、パスごとの GPU 時間を測る(区間名: VolumeCloud / VolumeCloud.Depth / .March / .Composite。
  // 雲が複数あるときは、同じ名前の区間が合計される)
  GpuTimer &gpuTimer = dx_->GetGpuTimer();
  GpuTimer::Scope totalScope(gpuTimer, cmdList, "VolumeCloud");

  // 品質の自動調整: 直近の GPU の 1 フレームの時間で段を決める(1 フレームに 1 回)。画面遷移の覆いは一時的に重いので、その分は除く
  const auto now = std::chrono::steady_clock::now();
  const float deltaSec = volumeCloudHasLastTime_ ? std::chrono::duration<float>(now - volumeCloudLastTime_).count() : 0.0f;
  volumeCloudLastTime_ = now;
  volumeCloudHasLastTime_ = true;
  volumeCloudQuality_.Update(gpuTimer.GetLastFrameMs("Frame") - gpuTimer.GetLastFrameMs("Transition"), deltaSec);

  // 描く値(基準の設定を、品質の段で軽くしたもの)。解像度の分母は、同じフレームの雲で共通にする(いちばん粗い分母に合わせる)
  std::array<VolumeCloudSettings, kMaxVolumeClouds> effective;
  int32_t divisor = kVolumeCloudMinResolutionDivisor;
  for (size_t i = 0; i < count; ++i) {
    effective[i] = volumeCloudQuality_.Apply(volumeCloudRequests_[i].settings);
    divisor = (std::max)(divisor, effective[i].resolutionDivisor);
  }
  divisor = std::clamp(divisor, kVolumeCloudMinResolutionDivisor, kVolumeCloudMaxResolutionDivisor);
  const uint32_t lowWidth = (sceneWidth_ + static_cast<uint32_t>(divisor) - 1u) / static_cast<uint32_t>(divisor);
  const uint32_t lowHeight = (sceneHeight_ + static_cast<uint32_t>(divisor) - 1u) / static_cast<uint32_t>(divisor);

  // 雲ごとの定数バッファを埋める(3 つのパスが読む)
  const Matrix4x4 invView = Inverse(view_);
  const Matrix4x4 invViewProj = Inverse(Multiply(view_, proj_));
  const Vector3 cameraPosition{invView.m[3][0], invView.m[3][1], invView.m[3][2]};
  for (size_t i = 0; i < count; ++i) {
    FillVolumeCloudParam_(*volumeCloudParamMapped_[i], effective[i], volumeCloudRequests_[i].sunDirection,
                          volumeCloudRequests_[i].time, invViewProj, cameraPosition, sceneWidth_, sceneHeight_, divisor);
  }

  ID3D12DescriptorHeap *heaps[] = {dx_->GetSRVHeap()};
  cmdList->SetDescriptorHeaps(1, heaps);
  cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  // 深度を、テクスチャとして読める状態にする(以降のパスすべてが読む)
  dx_->EnsureDepthReadable(depthTexture_.get());

  // パス 1(フレームに 1 回): 深度を低解像度へ縮める(ブロック内で一番近い深度 -> カメラからの距離)。
  // 使う項目(カメラ・画面の大きさ・分母)は、どの雲の定数バッファでも同じなので、1 つ目を使う
  dx_->SetViewportAndScissor(lowWidth, lowHeight);
  {
    GpuTimer::Scope scope(gpuTimer, cmdList, "VolumeCloud.Depth");
    dx_->SetRenderTargetColorOnly(volumeCloudDistanceLow_.get());
    volumeCloudDepthPipeline_->SetPipelineState(cmdList);
    // ルートパラメータの並び: 0 = b0、1 = t0(深度)
    cmdList->SetGraphicsRootConstantBufferView(0, volumeCloudParamCBs_[0]->GetGPUVirtualAddress());
    cmdList->SetGraphicsRootDescriptorTable(1, depthTexture_->GetSrvGpuHandle());
    cmdList->DrawInstanced(3, 1, 0, 0); // 頂点バッファなしで 3 頂点(大きな三角形 1 つで全画面を覆う)
    dx_->TransitionToShaderResource(volumeCloudDistanceLow_.get());
  }

  // 雲ごとに、パス 2(低解像度で描く)とパス 3(拡大してシーンへ重ねる)。追加した順(奥から手前)に重ねる
  for (size_t i = 0; i < count; ++i) {
    const D3D12_GPU_VIRTUAL_ADDRESS paramAddress = volumeCloudParamCBs_[i]->GetGPUVirtualAddress();

    // パス 2: 雲を低解像度で描く(描く範囲を低解像度の大きさへ)
    {
      GpuTimer::Scope scope(gpuTimer, cmdList, "VolumeCloud.March");
      dx_->SetViewportAndScissor(lowWidth, lowHeight);
      dx_->SetRenderTargetColorOnly(volumeCloudColorLow_.get());
      volumeCloudPipeline_->SetPipelineState(cmdList);
      // ルートパラメータの並び: 0 = b0、1 = t0(形のノイズ)、2 = t1(細かい模様)、3 = t2(縮めた距離)、4 = t3(青色雑音)
      cmdList->SetGraphicsRootConstantBufferView(0, paramAddress);
      cmdList->SetGraphicsRootDescriptorTable(1, cloudNoise_->GetShapeSrvGpu());
      cmdList->SetGraphicsRootDescriptorTable(2, cloudNoise_->GetDetailSrvGpu());
      cmdList->SetGraphicsRootDescriptorTable(3, volumeCloudDistanceLow_->GetSrvGpuHandle());
      cmdList->SetGraphicsRootDescriptorTable(4, cloudNoise_->GetBlueNoiseSrvGpu());
      cmdList->DrawInstanced(3, 1, 0, 0);
      dx_->TransitionToShaderResource(volumeCloudColorLow_.get());
    }

    // パス 3: 元の解像度へ拡大して、シーンの色へ重ねる(描く範囲を画面全体へ戻す)
    {
      GpuTimer::Scope scope(gpuTimer, cmdList, "VolumeCloud.Composite");
      dx_->ResetViewportAndScissor();
      dx_->BeginColorPassReadingDepth(renderTexture_.get(), depthTexture_.get());
      volumeCloudCompositePipeline_->SetPipelineState(cmdList);
      // ルートパラメータの並び: 0 = b0、1 = t0(低解像度の雲)、2 = t1(縮めた距離)、3 = t2(元の深度)
      cmdList->SetGraphicsRootConstantBufferView(0, paramAddress);
      cmdList->SetGraphicsRootDescriptorTable(1, volumeCloudColorLow_->GetSrvGpuHandle());
      cmdList->SetGraphicsRootDescriptorTable(2, volumeCloudDistanceLow_->GetSrvGpuHandle());
      cmdList->SetGraphicsRootDescriptorTable(3, depthTexture_->GetSrvGpuHandle());
      cmdList->DrawInstanced(3, 1, 0, 0);
    }
  }

  volumeCloudRequests_.clear();
}

void Renderer::DrawParticles(ParticleManager *pm, BlendMode blendMode, bool depthTest, ParticleDrawFilter filter) {
  if (!pm)
    return;
  auto *cmdList = dx_->GetCommandList();
  UnifiedPipeline *pipeline = (depthTest && blendMode == BlendMode::Alpha)
                                  ? particlePipelineAlphaDepth_.get()
                                  : GetParticlePipeline_(blendMode);
  pipeline->SetPipelineState(cmdList);
  pm->DrawInternal(cmdList, filter);
}

void Renderer::InitializePlanarReflection(uint32_t width, uint32_t height) {
    if (planarReflection_) return;

    planarReflection_ = std::make_unique<PlanarReflection>();
    planarReflection_->Initialize(dx_, width, height);
}

void Renderer::BeginReflectionPass(float planeHeight) {
    if (planarReflection_ && planarReflection_->IsInitialized()) {
        planarReflection_->Begin(dx_, planeHeight);
    }
}

void Renderer::EndReflectionPass() {
    if (planarReflection_ && planarReflection_->IsActive()) {
        planarReflection_->End(dx_);
    }
}

void Renderer::InitializePostProcess(uint32_t width, uint32_t height) {
    if (renderTexture_) return;

    renderTexture_ = std::make_unique<RenderTexture>();
    renderTexture_->Initialize(dx_, width, height, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, { 0.1f, 0.25f, 0.5f, 1.0f });

    depthTexture_ = std::make_unique<DepthTexture>();

    depthTexture_->Initialize(dx_, width, height);

    postProcessTexture_ = std::make_unique<RenderTexture>();
    postProcessTexture_->Initialize(dx_, width, height, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, { 0.1f, 0.25f, 0.5f, 1.0f });

    gaussianTempTexture_ = std::make_unique<RenderTexture>();
    gaussianTempTexture_->Initialize(dx_, width, height, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, { 0.1f, 0.25f, 0.5f, 1.0f });

    // ボリューム雲の低解像度の描画先(距離と、雲の光)。毎フレーム全部書くので、クリア色に意味はない
    sceneWidth_ = width;
    sceneHeight_ = height;
    volumeCloudDistanceLow_ = std::make_unique<RenderTexture>();
    volumeCloudDistanceLow_->Initialize(dx_, width, height, DXGI_FORMAT_R32_FLOAT, { 0.0f, 0.0f, 0.0f, 0.0f });
    volumeCloudColorLow_ = std::make_unique<RenderTexture>();
    volumeCloudColorLow_->Initialize(dx_, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, { 0.0f, 0.0f, 0.0f, 0.0f });
}

void Renderer::BeginRenderScene() {
    // リボンの共有頂点バッファの書き込み位置を、フレームの最初に先頭へ戻す
    if (ribbonDrawer_) ribbonDrawer_->Reset();

    if (renderTexture_ && depthTexture_) {
        dx_->SetRenderTargetWithDepth(renderTexture_.get(), depthTexture_.get());
        float clearColor[] = { 0.1f, 0.25f, 0.5f, 1.0f };
        dx_->GetCommandList()->ClearRenderTargetView(renderTexture_->GetRtvHandle(), clearColor, 0, nullptr);
        dx_->GetCommandList()->ClearDepthStencilView(depthTexture_->GetDsvHandle(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    }
}

void Renderer::EndRenderScene(const Matrix4x4& projInverse) {
    // ボリューム雲(依頼があるときだけ)。深度を読みたいので、深度を読み取り用へ切り替える前に、物体を描き終えたここで行う
    DrawVolumeCloudPass_();

    if (renderTexture_ && depthTexture_) {
        dx_->FinishRenderingWithDepth(renderTexture_.get(), depthTexture_.get());
    }

    if (renderTexture_ && postProcessTexture_) {
        if (postProcessMode_ == PostProcessMode::GaussianFilter && gaussianTempTexture_) {
            // パス1: 横方向
            dx_->SetRenderTarget(gaussianTempTexture_.get());
            if (gaussianFilterParamMapped_) {
                SetGaussianFilterParam(gaussianFilterParamMapped_->k, gaussianFilterParamMapped_->sigma, { 1.0f, 0.0f });
            }
            DrawFullscreen(renderTexture_->GetSrvGpuHandle(), postProcessMode_);
            dx_->FinishRendering(gaussianTempTexture_.get());

            // パス2: 縦方向
            dx_->SetRenderTarget(postProcessTexture_.get());
            if (gaussianFilterParamMapped_) {
                SetGaussianFilterParam(gaussianFilterParamMapped_->k, gaussianFilterParamMapped_->sigma, { 0.0f, 1.0f });
            }
            DrawFullscreen(gaussianTempTexture_->GetSrvGpuHandle(), postProcessMode_);
            dx_->FinishRendering(postProcessTexture_.get());
        }
        else if (postProcessMode_ == PostProcessMode::DepthBasedOutline) {
            dx_->SetRenderTarget(postProcessTexture_.get());
            SetDepthBasedOutlineParam(projInverse);
            DrawFullscreen(renderTexture_->GetSrvGpuHandle(), postProcessMode_, depthTexture_->GetSrvGpuHandle());
            dx_->FinishRendering(postProcessTexture_.get());
        }
        else if (postProcessMode_ == PostProcessMode::Dissolve && dissolveMaskTexture_) {
            dx_->SetRenderTarget(postProcessTexture_.get());
            DrawFullscreen(renderTexture_->GetSrvGpuHandle(), postProcessMode_, dissolveMaskTexture_->GetSrvGpu());
            dx_->FinishRendering(postProcessTexture_.get());
        }
        else {
            dx_->SetRenderTarget(postProcessTexture_.get());
            DrawFullscreen(renderTexture_->GetSrvGpuHandle(), postProcessMode_);
            dx_->FinishRendering(postProcessTexture_.get());
        }
    }

    // 画面遷移の覆い(ポストエフェクトのあと。Debug のビューポートにも、Release の画面にも反映される)
    ApplyTransitionPass_();

#ifndef USE_IMGUI
    if (postProcessTexture_) {
        dx_->ResetRenderTarget();
        DrawFullscreen(postProcessTexture_->GetSrvGpuHandle(), PostProcessMode::Normal);
    }
#endif
}

void Renderer::SetTransition(const TransitionStyle &style, float cover, float time, float duration) {
    transitionStyle_ = style;
    transitionCover_ = cover;
    transitionTime_ = time;
    transitionDuration_ = duration;
}

void Renderer::ApplyTransitionPass_() {
    if (transitionCover_ <= 0.0f || !transitionPipeline_ || !transitionParamMapped_ || !postProcessTexture_ || !gaussianTempTexture_) {
        return;
    }

    // パラメータ
    TransitionParam *param = transitionParamMapped_;
    // 描画したシーンのカメラ(最後に SetCamera された行列)。世界の視線の復元・深度から位置への変換・太陽の画面位置に使う
    const Matrix4x4 invView = Inverse(view_);
    const Matrix4x4 viewProj = Multiply(view_, proj_);
    param->invViewProj = Inverse(viewProj);
    param->viewProj = viewProj;
    param->cameraPosition[0] = invView.m[3][0];
    param->cameraPosition[1] = invView.m[3][1];
    param->cameraPosition[2] = invView.m[3][2];
    param->slabBottom = transitionStyle_.slabBottom;
    param->slabTop = transitionStyle_.slabTop;
    const Vector3 sunDirection = Normalize(transitionStyle_.sunDirection);
    param->sunDirection[0] = sunDirection.x;
    param->sunDirection[1] = sunDirection.y;
    param->sunDirection[2] = sunDirection.z;
    param->color[0] = transitionStyle_.color.x;
    param->color[1] = transitionStyle_.color.y;
    param->color[2] = transitionStyle_.color.z;
    param->cover = (std::min)(transitionCover_, 1.0f);
    param->kind = static_cast<float>(static_cast<int32_t>(transitionStyle_.kind));
    param->softness = transitionStyle_.softness;
    param->noiseScale = transitionStyle_.noiseScale;
    param->blurStrength = transitionStyle_.blurStrength;
    param->center[0] = transitionStyle_.center.x;
    param->center[1] = transitionStyle_.center.y;
    param->aspect = GetAspectRatio();
    param->time = transitionTime_;
    param->duration = transitionDuration_;
    param->mistFog[0] = transitionStyle_.mist.desaturate;
    param->mistFog[1] = transitionStyle_.mist.blurRadius;
    param->mistFog[2] = transitionStyle_.mist.visibilityNear;
    param->mistFog[3] = transitionStyle_.mist.visibilityFar;
    param->mistFog2[0] = transitionStyle_.mist.backgroundDistance;
    param->mistFog2[1] = transitionStyle_.mist.wrapStart;
    param->mistFog2[2] = transitionStyle_.mist.sunGlow;
    param->mistFog2[3] = transitionStyle_.mist.debugDistance ? 1.0f : 0.0f;
    param->mistDrop[0] = transitionStyle_.mist.wetness;
    param->mistDrop[1] = transitionStyle_.mist.fogScale;
    param->mistDrop[2] = transitionStyle_.mist.lensTime;
    param->mistDrop[3] = transitionStyle_.mist.lensDrops ? 1.0f : 0.0f;
    param->mistPlane[0] = transitionStyle_.mist.backgroundPlaneY;
    param->mistPlane[1] = transitionStyle_.mist.backgroundPlane ? 1.0f : 0.0f;
    param->mistPlane[2] = 0.0f;
    param->mistPlane[3] = 0.0f;

    // パス1: postProcessTexture_ を読み、遷移の効果をかけて一時のテクスチャへ描く
    auto *cmdList = dx_->GetCommandList();
    // 画面遷移の覆いの 2 パス分の GPU 時間を、"Transition" として測る(雲を抜ける遷移は重いはずなので、実測する)
    GpuTimer::Scope gpuScope(dx_->GetGpuTimer(), cmdList, "Transition");
    dx_->SetRenderTarget(gaussianTempTexture_.get());
    ID3D12DescriptorHeap *heaps[] = {dx_->GetSRVHeap()};
    cmdList->SetDescriptorHeaps(1, heaps);
    transitionPipeline_->SetPipelineState(cmdList);
    cmdList->SetGraphicsRootConstantBufferView(0, transitionParamCB_->GetGPUVirtualAddress());
    cmdList->SetGraphicsRootDescriptorTable(1, postProcessTexture_->GetSrvGpuHandle());
    if (depthTexture_) {
        cmdList->SetGraphicsRootDescriptorTable(2, depthTexture_->GetSrvGpuHandle()); // t1: シーンの深度
    }
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmdList->DrawInstanced(3, 1, 0, 0);
    dx_->FinishRendering(gaussianTempTexture_.get());

    // パス2: 結果を postProcessTexture_ へ戻す(以降の処理は、これまで通り postProcessTexture_ を読む)。
    // 水滴を使う遷移では、パス1の結果(霧や雲がかかった画面)を、レンズの水滴で屈折させて描く。それ以外は、そのまま写す
    dx_->SetRenderTarget(postProcessTexture_.get());
    if (transitionStyle_.mist.lensDrops && lensDropsPipeline_) {
        lensDropsPipeline_->SetPipelineState(cmdList);
        cmdList->SetGraphicsRootConstantBufferView(0, transitionParamCB_->GetGPUVirtualAddress());
        cmdList->SetGraphicsRootDescriptorTable(1, gaussianTempTexture_->GetSrvGpuHandle());
        cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        cmdList->DrawInstanced(3, 1, 0, 0);
    } else {
        DrawFullscreen(gaussianTempTexture_->GetSrvGpuHandle(), PostProcessMode::Normal);
    }
    dx_->FinishRendering(postProcessTexture_.get());
}

void Renderer::RenderPrimitives() {
  auto *cmdList = dx_->GetCommandList();

  // WVP計算
  if (primitiveTransformMapped_) {
    // Primitiveは基本的にワールド座標で指定されるため、World行列は単位行列
    primitiveTransformMapped_->WVP = Multiply(view_, proj_);
  }

  primitivePipeline_->SetPipelineState(cmdList);
  // 他のパイプラインと共通の構造（1番をTransform）にする
  cmdList->SetGraphicsRootConstantBufferView(
      1, primitiveTransformCB_->GetGPUVirtualAddress());

  primitiveDrawer_->Draw(cmdList);
  primitiveDrawer_->Reset(); // 描画後にリセット
}

void Renderer::DrawEffectModel(ModelInstance *instance, bool writeDepth) {
  if (!instance || !instance->GetResource())
    return;
  auto *cmdList = dx_->GetCommandList();
  auto *resource = instance->GetResource();

  // WVP 行列の計算と転送
  auto *cbTrans = instance->GetTransformMapped();
  if (cbTrans) {
    Matrix4x4 worldView = Multiply(instance->GetWorld(), view_);
    cbTrans->WVP = Multiply(worldView, proj_);
    cbTrans->World = instance->GetWorld();
    cbTrans->WorldInverseTranspose = Transpose(Inverse(instance->GetWorld()));
  }

  // エフェクト用パイプラインをセット(深度を書き込むかどうかで切り替える)
  (writeDepth ? effectDepthPipeline_ : effectPipeline_)->SetPipelineState(cmdList);

  // 頂点バッファ
  D3D12_VERTEX_BUFFER_VIEW vbv{};
  vbv.BufferLocation = resource->GetVBVAddress();
  vbv.SizeInBytes = resource->GetVBVSize();
  vbv.StrideInBytes = resource->GetVBVStride();
  cmdList->IASetVertexBuffers(0, 1, &vbv);

  // Index Buffer
  if (resource->GetIndexCount() > 0) {
    D3D12_INDEX_BUFFER_VIEW ibv{};
    ibv.BufferLocation = resource->GetIBVAddress();
    ibv.Format = DXGI_FORMAT_R32_UINT;
    ibv.SizeInBytes = resource->GetIBVSize();
    cmdList->IASetIndexBuffer(&ibv);
  }

  // 定数バッファ (0:マテリアル, 1:トランスフォーム)
  cmdList->SetGraphicsRootConstantBufferView(0,
                                             instance->GetMaterialCBAddress());
  cmdList->SetGraphicsRootConstantBufferView(1,
                                             instance->GetTransformCBAddress());

  // テクスチャ
  ID3D12DescriptorHeap *heaps[] = {dx_->GetSRVHeap()};
  cmdList->SetDescriptorHeaps(1, heaps);
  D3D12_GPU_DESCRIPTOR_HANDLE texHandle{};
  texHandle.ptr = resource->GetTextureHandleGPUAsUInt64();
  cmdList->SetGraphicsRootDescriptorTable(2, texHandle);

  // ライト・カメラ (シェーダがこれらを参照しているため、Unlitでもセットが必要)
  cmdList->SetGraphicsRootConstantBufferView(
      3, directionalLightCB_->GetGPUVirtualAddress());
  cmdList->SetGraphicsRootConstantBufferView(4,
                                             cameraCB_->GetGPUVirtualAddress());
  cmdList->SetGraphicsRootConstantBufferView(
      5, pointLightCB_->GetGPUVirtualAddress());
  cmdList->SetGraphicsRootConstantBufferView(
      6, spotLightCB_->GetGPUVirtualAddress());

  // 描画実行
  cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  if (resource->GetIndexCount() > 0) {
    cmdList->DrawIndexedInstanced(resource->GetIndexCount(), 1, 0, 0, 0);
  } else {
    cmdList->DrawInstanced(resource->GetVertexCount(), 1, 0, 0);
  }
}

void Renderer::DrawRing(Ring *ring, D3D12_GPU_DESCRIPTOR_HANDLE textureHandle) {
  if (!ring || !textureHandle.ptr)
    return;
  auto *cmdList = dx_->GetCommandList();

  // パイプライン設定
  ringPipeline_->SetPipelineState(cmdList);

  // 定数バッファ (0:マテリアル, 1:トランスフォーム)
  cmdList->SetGraphicsRootConstantBufferView(0, ring->GetMaterialCBAddress());
  cmdList->SetGraphicsRootConstantBufferView(1, ring->GetTransformCBAddress());

  // テクスチャ
  ID3D12DescriptorHeap *heaps[] = {dx_->GetSRVHeap()};
  cmdList->SetDescriptorHeaps(1, heaps);
  cmdList->SetGraphicsRootDescriptorTable(2, textureHandle);

  // 描画
  ring->Draw(cmdList);
}

void Renderer::DrawCylinder(Cylinder *cylinder, D3D12_GPU_DESCRIPTOR_HANDLE textureHandle) {
  if (!cylinder || !textureHandle.ptr)
    return;
  auto *cmdList = dx_->GetCommandList();

  // パイプライン設定
  cylinderPipeline_->SetPipelineState(cmdList);

  // 定数バッファ (0:マテリアル, 1:トランスフォーム)
  cmdList->SetGraphicsRootConstantBufferView(0, cylinder->GetMaterialCBAddress());
  cmdList->SetGraphicsRootConstantBufferView(1, cylinder->GetTransformCBAddress());

  // テクスチャ
  ID3D12DescriptorHeap *heaps[] = {dx_->GetSRVHeap()};
  cmdList->SetDescriptorHeaps(1, heaps);
  cmdList->SetGraphicsRootDescriptorTable(2, textureHandle);

  // 描画
  cylinder->Draw(cmdList);
}

void Renderer::DrawRibbon(const RibbonTrail &trail) {
  if (!ribbonDrawer_ || !ribbonTransformMapped_)
    return;
  auto *cmdList = dx_->GetCommandList();

  // リボンの頂点はワールド座標なので、ビュー射影行列だけを渡す。
  // 同じフレームの全リボンで同じ値になるため、定数バッファは1つを共有してよい
  ribbonTransformMapped_->WVP = Multiply(view_, proj_);

  // カメラのワールド座標は、ビュー行列の逆行列の平行移動成分から求める。
  // 帯の幅方向(カメラに面を向ける向き)を決めるのに使う
  const Matrix4x4 invView = Inverse(view_);
  const Vector3 cameraPosition = {invView.m[3][0], invView.m[3][1], invView.m[3][2]};

  ribbonPipeline_->SetPipelineState(cmdList);
  cmdList->SetGraphicsRootConstantBufferView(
      0, ribbonTransformCB_->GetGPUVirtualAddress());

  ribbonDrawer_->Draw(cmdList, trail, cameraPosition);
}

UnifiedPipeline *Renderer::GetSpritePipeline_(BlendMode mode) {
  switch (mode) {
  case BlendMode::Add:
    return spritePipelineAdd_.get();
  case BlendMode::Subtract:
    return spritePipelineSub_.get();
  case BlendMode::Multiply:
    return spritePipelineMul_.get();
  case BlendMode::Screen:
    return spritePipelineScreen_.get();
  default:
    return spritePipelineAlpha_.get();
  }
}

UnifiedPipeline *Renderer::GetParticlePipeline_(BlendMode mode) {
  switch (mode) {
  case BlendMode::Add:
    return particlePipelineAdd_.get();
  case BlendMode::Subtract:
    return particlePipelineSub_.get();
  case BlendMode::Multiply:
    return particlePipelineMul_.get();
  case BlendMode::Screen:
    return particlePipelineScreen_.get();
  default:
    return particlePipelineAlpha_.get();
  }
}

Microsoft::WRL::ComPtr<ID3D12Resource>
Renderer::CreateUploadBuffer(size_t size) {
  auto device = dx_->GetDevice();
  Microsoft::WRL::ComPtr<ID3D12Resource> res;
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
  device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc,
                                  D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                  IID_PPV_ARGS(&res));
  return res;
}

Microsoft::WRL::ComPtr<ID3D12Resource> Renderer::CreateBuffer(size_t size) {
  return CreateUploadBuffer(size);
}

Microsoft::WRL::ComPtr<ID3D12Resource> Renderer::CreateUAVBuffer(size_t size) {
  auto device = dx_->GetDevice();
  Microsoft::WRL::ComPtr<ID3D12Resource> res;
  D3D12_HEAP_PROPERTIES heapProps{};
  heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
  D3D12_RESOURCE_DESC desc{};
  desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  desc.Width = size;
  desc.Height = 1;
  desc.DepthOrArraySize = 1;
  desc.MipLevels = 1;
  desc.SampleDesc.Count = 1;
  desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

  HRESULT hr = device->CreateCommittedResource(
      &heapProps, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COMMON,
      nullptr, IID_PPV_ARGS(&res));
  if (FAILED(hr)) {
    return nullptr;
  }
  return res;
}

// GPU パーティクルの更新と描画（blendMode でブレンドモードを選択）
void Renderer::DrawGPUParticles(BlendMode blendMode) {
  GPUParticleManager::GetInstance()->Update();
  GPUParticleManager::GetInstance()->Draw(blendMode);
}

void Renderer::DrawLine(const Vector3 &start, const Vector3 &end,
                        const Vector4 &color) {
  primitiveDrawer_->AddLine(start, end, color);
}

void Renderer::DrawGrid(float size, int divisions, const Vector4 &color) {
  primitiveDrawer_->AddGrid(size, divisions, color);
}

void Renderer::SetDepthBasedOutlineParam(const Matrix4x4& projectionInverse) {
  if (depthBasedOutlineParamMapped_) {
    depthBasedOutlineParamMapped_->projectionInverse = projectionInverse;
  }
}

void Renderer::SetRadialBlurParam(const Vector2& center, float blurWidth) {
  if (radialBlurParamCBMap_) {
    radialBlurParamCBMap_->center = center;
    radialBlurParamCBMap_->blurWidth = blurWidth;
  }
}

void Renderer::SetDissolveParam(float threshold, float edgeRange, const Vector3& edgeColor, const Vector3& maskColor) {
  if (dissolveParamMapped_) {
    dissolveParamMapped_->threshold = threshold;
    dissolveParamMapped_->edgeRange = edgeRange;
    dissolveParamMapped_->edgeColor = edgeColor;
    dissolveParamMapped_->maskColor = maskColor;
  }
}

void Renderer::SetRandomParam(float time) {
  if (randomParamMapped_) {
    randomParamMapped_->time = time;
  }
}

void Renderer::DrawFullscreen(D3D12_GPU_DESCRIPTOR_HANDLE textureHandle, PostProcessMode mode, D3D12_GPU_DESCRIPTOR_HANDLE depthOrMaskTextureHandle) {
  UnifiedPipeline *pipeline = nullptr;
  switch (mode) {
  case PostProcessMode::Normal:
    pipeline = copyImagePipeline_.get();
    break;
  case PostProcessMode::Grayscale:
    pipeline = grayscalePipeline_.get();
    break;
  case PostProcessMode::Sepia:
    pipeline = sepiaPipeline_.get();
    break;
  case PostProcessMode::Vignette:
    pipeline = vignettePipeline_.get();
    break;
  case PostProcessMode::BoxFilter:
    pipeline = boxFilterPipeline_.get();
    break;
  case PostProcessMode::GaussianFilter:
    pipeline = gaussianFilterPipeline_.get();
    break;
  case PostProcessMode::LuminanceBasedOutline:
    pipeline = luminanceBasedOutlinePipeline_.get();
    break;
  case PostProcessMode::DepthBasedOutline:
    pipeline = pipelineDepthBasedOutline_.get();
    break;
  case PostProcessMode::RadialBlur:
    pipeline = radialBlurPipeline_.get();
    break;
  case PostProcessMode::Dissolve:
    pipeline = dissolvePipeline_.get();
    break;
  case PostProcessMode::Random:
    pipeline = randomPipeline_.get();
    break;
  }

  if (!pipeline)
    return;

  auto *cmdList = dx_->GetCommandList();

  // ディスクリプタヒープの設定
  ID3D12DescriptorHeap* heaps[] = { dx_->GetSRVHeap() };
  cmdList->SetDescriptorHeaps(1, heaps);

  // パイプライン設定
  pipeline->SetPipelineState(cmdList);

  if (mode == PostProcessMode::Vignette) {
    cmdList->SetGraphicsRootConstantBufferView(0, vignetteParamCB_->GetGPUVirtualAddress());
    cmdList->SetGraphicsRootDescriptorTable(1, textureHandle);
  } else if (mode == PostProcessMode::BoxFilter) {
    cmdList->SetGraphicsRootConstantBufferView(0, boxFilterParamCB_->GetGPUVirtualAddress());
    cmdList->SetGraphicsRootDescriptorTable(1, textureHandle);
  } else if (mode == PostProcessMode::GaussianFilter) {
    cmdList->SetGraphicsRootConstantBufferView(0, gaussianFilterParamCB_->GetGPUVirtualAddress());
    cmdList->SetGraphicsRootDescriptorTable(1, textureHandle);
  } else if (mode == PostProcessMode::DepthBasedOutline) {
    cmdList->SetGraphicsRootConstantBufferView(0, depthBasedOutlineParamCB_->GetGPUVirtualAddress());
    cmdList->SetGraphicsRootDescriptorTable(1, textureHandle);
    if (depthOrMaskTextureHandle.ptr != 0) {
      cmdList->SetGraphicsRootDescriptorTable(2, depthOrMaskTextureHandle);
    }
  } else if (mode == PostProcessMode::RadialBlur) {
    cmdList->SetGraphicsRootConstantBufferView(0, radialBlurParamCB_->GetGPUVirtualAddress());
    cmdList->SetGraphicsRootDescriptorTable(1, textureHandle);
  } else if (mode == PostProcessMode::Dissolve) {
    cmdList->SetGraphicsRootConstantBufferView(0, dissolveParamCB_->GetGPUVirtualAddress());
    cmdList->SetGraphicsRootDescriptorTable(1, textureHandle); // t0
    if (depthOrMaskTextureHandle.ptr != 0) {
      cmdList->SetGraphicsRootDescriptorTable(2, depthOrMaskTextureHandle); // t1: mask
    }
  } else if (mode == PostProcessMode::Random) {
    cmdList->SetGraphicsRootConstantBufferView(0, randomParamCB_->GetGPUVirtualAddress());
    cmdList->SetGraphicsRootDescriptorTable(1, textureHandle);
  } else {
    // テクスチャをセット (t0)
    cmdList->SetGraphicsRootDescriptorTable(0, textureHandle);
  }

  // 頂点バッファなしで3頂点描画（大きな三角形1つで全画面を覆う）
  cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  cmdList->DrawInstanced(3, 1, 0, 0);
}

void Renderer::SetVignetteParam(float scale, float powValue) {
  SetVignetteParam(scale, powValue, Vector3{0.0f, 0.0f, 0.0f}, 1.0f);
}

void Renderer::SetVignetteParam(float scale, float powValue, const Vector3& color, float strength) {
  if (vignetteParamMapped_) {
    vignetteParamMapped_->scale = scale;
    vignetteParamMapped_->powValue = powValue;
    vignetteParamMapped_->tint[0] = color.x;
    vignetteParamMapped_->tint[1] = color.y;
    vignetteParamMapped_->tint[2] = color.z;
    vignetteParamMapped_->tint[3] = strength;
  }
}

void Renderer::SetBoxFilterParam(int32_t k) {
  if (boxFilterParamMapped_) {
    boxFilterParamMapped_->k = k;
  }
}

void Renderer::SetGaussianFilterParam(int32_t k, float sigma, const Vector2& direction) {
  if (gaussianFilterParamMapped_) {
    gaussianFilterParamMapped_->k = k;
    gaussianFilterParamMapped_->sigma = sigma;
    gaussianFilterParamMapped_->direction[0] = direction.x;
    gaussianFilterParamMapped_->direction[1] = direction.y;
  }
}