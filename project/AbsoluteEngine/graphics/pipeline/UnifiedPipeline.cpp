#include "UnifiedPipeline.h"
#include "ShaderCompilerUtils.h"
#include <cassert>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <Windows.h>
#include <format>

static void LogUnifiedPipelineError_(const std::wstring &msg) {
  OutputDebugStringW((L"[UnifiedPipeline] " + msg + L"\n").c_str());
}

static void LogUnifiedPipelineHr_(const std::wstring &what, HRESULT hr) {
  LogUnifiedPipelineError_(
      std::format(L"{} failed. hr=0x{:08X}", what, (unsigned)hr));
}

static D3D12_BLEND_DESC MakeBlendDesc(const PipelineDesc &desc) {
  D3D12_BLEND_DESC b{};
  b.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

  if (!desc.alphaBlend) {
    // ブレンドなし（Opaque）
    return b;
  }

  auto &rt = b.RenderTarget[0];
  rt.BlendEnable = TRUE;

  switch (desc.blendMode) {
  case BlendMode::Alpha: // 通常アルファ
  default:
    rt.SrcBlend = D3D12_BLEND_SRC_ALPHA;
    rt.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    rt.BlendOp = D3D12_BLEND_OP_ADD;
    rt.SrcBlendAlpha = D3D12_BLEND_ONE;
    rt.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
    rt.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    break;

  case BlendMode::Add: // 加算：dst + src * a
    rt.SrcBlend = D3D12_BLEND_SRC_ALPHA;
    rt.DestBlend = D3D12_BLEND_ONE;
    rt.BlendOp = D3D12_BLEND_OP_ADD;
    rt.SrcBlendAlpha = D3D12_BLEND_ONE;
    rt.DestBlendAlpha = D3D12_BLEND_ONE;
    rt.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    break;

  case BlendMode::Subtract: // 減算：dst - src * a
    rt.SrcBlend = D3D12_BLEND_SRC_ALPHA;
    rt.DestBlend = D3D12_BLEND_ONE;
    rt.BlendOp = D3D12_BLEND_OP_REV_SUBTRACT; // dst - src
    rt.SrcBlendAlpha = D3D12_BLEND_ONE;
    rt.DestBlendAlpha = D3D12_BLEND_ONE;
    rt.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    break;

  case BlendMode::Multiply: // 乗算：dst * src
    rt.SrcBlend = D3D12_BLEND_ZERO;
    rt.DestBlend = D3D12_BLEND_SRC_COLOR;
    rt.BlendOp = D3D12_BLEND_OP_ADD;
    rt.SrcBlendAlpha = D3D12_BLEND_ZERO;
    rt.DestBlendAlpha = D3D12_BLEND_SRC_ALPHA;
    rt.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    break;

  case BlendMode::PremultipliedAlpha: // src の色はすでに alpha を掛けてある前提
    rt.SrcBlend = D3D12_BLEND_ONE;
    rt.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    rt.BlendOp = D3D12_BLEND_OP_ADD;
    rt.SrcBlendAlpha = D3D12_BLEND_ONE;
    rt.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
    rt.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    break;

  case BlendMode::Screen: // スクリーンっぽい近似
    // 1 - (1-s) * (1-d) ≒ s + d - s*d
    rt.SrcBlend = D3D12_BLEND_INV_DEST_COLOR;
    rt.DestBlend = D3D12_BLEND_ONE;
    rt.BlendOp = D3D12_BLEND_OP_ADD;
    rt.SrcBlendAlpha = D3D12_BLEND_ONE;
    rt.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
    rt.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    break;
  }

  return b;
}

bool UnifiedPipeline::Initialize(ID3D12Device *device, IDxcUtils *dxcUtils,
                                 IDxcCompiler3 *dxcCompiler,
                                 IDxcIncludeHandler *includeHandler,
                                 const PipelineDesc &desc) {
  if (!device) {
    LogUnifiedPipelineError_(L"Initialize: device is null");
    return false;
  }
  if (!dxcUtils) {
    LogUnifiedPipelineError_(L"Initialize: dxcUtils is null");
    return false;
  }
  if (!dxcCompiler) {
    LogUnifiedPipelineError_(L"Initialize: dxcCompiler is null");
    return false;
  }
  if (!includeHandler) {
    LogUnifiedPipelineError_(L"Initialize: includeHandler is null");
    return false;
  }

  HRESULT hr = S_OK;

  // t0: 通常テクスチャ (PS)
  D3D12_DESCRIPTOR_RANGE srvRangeTex{};
  srvRangeTex.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
  srvRangeTex.BaseShaderRegister = 0; // t0
  srvRangeTex.NumDescriptors = 1;
  srvRangeTex.OffsetInDescriptorsFromTableStart =
      D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

  // t1: インスタンシング (VS)
  D3D12_DESCRIPTOR_RANGE srvRangeInst{};
  srvRangeInst.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
  srvRangeInst.BaseShaderRegister = 1; // t1 (VS用)
  srvRangeInst.NumDescriptors = 1;
  srvRangeInst.OffsetInDescriptorsFromTableStart =
      D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

  // t1: 環境マップ (PS)
  D3D12_DESCRIPTOR_RANGE srvRangeEnv{};
  srvRangeEnv.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
  srvRangeEnv.BaseShaderRegister = 1; // t1 (PS用)
  srvRangeEnv.NumDescriptors = 1;
  srvRangeEnv.OffsetInDescriptorsFromTableStart =
      D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

  // t2: Skinning (VS)
  D3D12_DESCRIPTOR_RANGE srvRangeSkin{};
  srvRangeSkin.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
  srvRangeSkin.BaseShaderRegister = 2; // t2
  srvRangeSkin.NumDescriptors = 1;
  srvRangeSkin.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

  // t2: Mask Texture (PS)
  D3D12_DESCRIPTOR_RANGE srvRangeMask{};
  srvRangeMask.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
  srvRangeMask.BaseShaderRegister = 2; // t2 (PS用)
  srvRangeMask.NumDescriptors = 1;
  srvRangeMask.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

  // --- Root Parameters（フラグに応じて詰める） ---
  D3D12_ROOT_PARAMETER params[16]{};
  UINT numParams = 0;

  if (desc.usePSMaterial_b0) { // Index 0
    auto &p = params[numParams++];
    p.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    p.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    p.Descriptor.ShaderRegister = 0;
  }
  if (desc.useVSTransform_b0) { // Index 1
    auto &p = params[numParams++];
    p.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    p.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    p.Descriptor.ShaderRegister = 0;
  }
  if (desc.usePSTextureTable_t0) { // Index 2
    auto &p = params[numParams++];
    p.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    p.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    p.DescriptorTable.pDescriptorRanges = &srvRangeTex;
    p.DescriptorTable.NumDescriptorRanges = 1;
  }
  if (desc.useVSInstancingTable_t1) { // (Object3Dでは通常 false)
    auto &p = params[numParams++];
    p.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    p.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    p.DescriptorTable.pDescriptorRanges = &srvRangeInst;
    p.DescriptorTable.NumDescriptorRanges = 1;
  }
  if (desc.usePSDirectionalLight_b1) { // Index 3
    auto &p = params[numParams++];
    p.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    p.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    p.Descriptor.ShaderRegister = 1;
  }
  if (desc.usePSCamera_b2) { // Index 4
    auto &p = params[numParams++];
    p.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    p.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    p.Descriptor.ShaderRegister = 2;
  }
  if (desc.usePSPointLight_b3) { // Index 5
    auto &p = params[numParams++];
    p.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    p.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    p.Descriptor.ShaderRegister = 3;
  }
  if (desc.usePSSpotLight_b4) { // Index 6
    auto &p = params[numParams++];
    p.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    p.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    p.Descriptor.ShaderRegister = 4;
  }  
  if (desc.usePSEnvironmentMap_t1) {// Index 7
    auto &p = params[numParams++];
    p.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    p.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    p.DescriptorTable.pDescriptorRanges = &srvRangeEnv;
    p.DescriptorTable.NumDescriptorRanges = 1;
  }
  if (desc.usePSMaskTexture_t2) { // Index 8
    auto &p = params[numParams++];
    p.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    p.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    p.DescriptorTable.pDescriptorRanges = &srvRangeMask;
    p.DescriptorTable.NumDescriptorRanges = 1;
  }
  if (desc.useVSSkinning_t2) { // Index 9 (Skinningの場合)
    auto &p = params[numParams++];
    p.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    p.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    p.DescriptorTable.pDescriptorRanges = &srvRangeSkin;
    p.DescriptorTable.NumDescriptorRanges = 1;
  }

  D3D12_STATIC_SAMPLER_DESC samp{};
  samp.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
  samp.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
  samp.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
  samp.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
  samp.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
  samp.MaxLOD = D3D12_FLOAT32_MAX;
  samp.ShaderRegister = 0;
  samp.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

  // s1: 線形補間 + 繰り返し(usePSWrapSampler_s1 のときだけ)
  D3D12_STATIC_SAMPLER_DESC samplers[2]{};
  samplers[0] = samp;
  UINT numSamplers = 1;
  if (desc.usePSWrapSampler_s1) {
    D3D12_STATIC_SAMPLER_DESC &wrap = samplers[numSamplers++];
    wrap = samp;
    wrap.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    wrap.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    wrap.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    wrap.ShaderRegister = 1;
  }

  D3D12_ROOT_SIGNATURE_DESC rs{};
  rs.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
  rs.pParameters = params;
  rs.NumParameters = numParams;
  rs.pStaticSamplers = samplers;
  rs.NumStaticSamplers = numSamplers;

  Microsoft::WRL::ComPtr<ID3DBlob> sig, err;
  hr = D3D12SerializeRootSignature(&rs, D3D_ROOT_SIGNATURE_VERSION_1,
                                   sig.GetAddressOf(), err.GetAddressOf());
  if (FAILED(hr)) {
    if (err) {
      OutputDebugStringA((const char *)err->GetBufferPointer());
    }
    LogUnifiedPipelineHr_(L"D3D12SerializeRootSignature", hr);
    assert(false);
    return false;
  }

  OutputDebugStringW(
      (L"CreatePSO: " + desc.vsPath + L" / " + desc.psPath + L"\n").c_str());

  hr = device->CreateRootSignature(0, sig->GetBufferPointer(),
                                   sig->GetBufferSize(),
                                   IID_PPV_ARGS(&rootSignature_));
  if (FAILED(hr)) {
    LogUnifiedPipelineHr_(L"CreateRootSignature", hr);
    return false;
  }

  // --- InputLayout ---
  D3D12_INPUT_LAYOUT_DESC layout{};
  layout.pInputElementDescs = desc.inputElements.data();
  layout.NumElements = (UINT)desc.inputElements.size();

  // --- Shaders ---
  Microsoft::WRL::ComPtr<IDxcBlob> vs;
  Microsoft::WRL::ComPtr<IDxcBlob> ps;

  {
    IDxcBlob *vsRaw = CompileShader(desc.vsPath, desc.vsProfile, dxcUtils,
                                    dxcCompiler, includeHandler);
    if (!vsRaw) {
      LogUnifiedPipelineError_(
          std::format(L"VS compile failed: {}", desc.vsPath));
      return false;
    }
    vs.Attach(vsRaw);
  }

  {
    IDxcBlob *psRaw = CompileShader(desc.psPath, desc.psProfile, dxcUtils,
                                    dxcCompiler, includeHandler);
    if (!psRaw) {
      LogUnifiedPipelineError_(
          std::format(L"PS compile failed: {}", desc.psPath));
      return false;
    }
    ps.Attach(psRaw);
  }

  // --- Raster / Blend / Depth ---
  D3D12_RASTERIZER_DESC rast{};
  rast.CullMode = desc.cullMode;
  rast.FrontCounterClockwise = desc.frontCounterClockwise ? TRUE : FALSE;
  rast.FillMode = desc.fillMode;

  D3D12_BLEND_DESC blend = MakeBlendDesc(desc);

  // Initialize() 内 depth 設定だけ差し替え
  D3D12_DEPTH_STENCIL_DESC depth{};
  depth.DepthEnable = desc.enableDepth ? TRUE : FALSE;
  depth.DepthWriteMask = desc.depthWrite ? D3D12_DEPTH_WRITE_MASK_ALL
                                         : D3D12_DEPTH_WRITE_MASK_ZERO;
  depth.DepthFunc = desc.depthFunc;

  // --- PSO ---
  D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
  pso.pRootSignature = rootSignature_.Get();
  pso.InputLayout = layout;
  pso.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
  pso.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
  pso.BlendState = blend;
  pso.RasterizerState = rast;
  pso.NumRenderTargets = 1;
  pso.RTVFormats[0] = desc.rtvFormat;
  if (desc.vsPath.find(L"Primitive") != std::wstring::npos) {
    pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
  } else {
    pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  }
  pso.SampleDesc.Count = 1;
  pso.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
  pso.DepthStencilState = depth;
  pso.DSVFormat = desc.dsvFormat;

  pso.InputLayout.pInputElementDescs = desc.inputElements.data();
  pso.InputLayout.NumElements = (UINT)desc.inputElements.size();

  hr = device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&pso_));
  if (FAILED(hr)) {
    LogUnifiedPipelineHr_(L"CreateGraphicsPipelineState", hr);
    return false;
  }
  return true;
}

// ===== プリセット =====
PipelineDesc UnifiedPipeline::MakeObject3DDesc() {
  PipelineDesc d{};
  d.inputElements = {
      D3D12_INPUT_ELEMENT_DESC{"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
                               D3D12_APPEND_ALIGNED_ELEMENT,
                               D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
      D3D12_INPUT_ELEMENT_DESC{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
                               D3D12_APPEND_ALIGNED_ELEMENT,
                               D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
      D3D12_INPUT_ELEMENT_DESC{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
                               D3D12_APPEND_ALIGNED_ELEMENT,
                               D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
  };
  d.vsPath = L"resources/engine/shaders/Object3D.VS.hlsl";
  d.psPath = L"resources/engine/shaders/Object3D.PS.hlsl";
  d.usePSMaterial_b0 = true;
  d.useVSTransform_b0 = true;
  d.usePSTextureTable_t0 = true;
  d.usePSDirectionalLight_b1 = true;
  d.usePSCamera_b2 = true;
  d.usePSPointLight_b3 = true;
  d.usePSSpotLight_b4 = true;
  d.usePSEnvironmentMap_t1 = true; // 環境マッピングを有効化
  d.usePSMaskTexture_t2 = true; // Dissolve用マスクテクスチャ

  d.enableDepth = true;
  d.alphaBlend = false;
  d.cullMode = D3D12_CULL_MODE_BACK;
  d.fillMode = D3D12_FILL_MODE_SOLID;
  return d;
}

PipelineDesc UnifiedPipeline::MakeSkinnedObject3DDesc() {
  PipelineDesc d = MakeObject3DDesc();
  d.vsPath = L"resources/engine/shaders/SkinnedObject3D.VS.hlsl";
  d.useVSSkinning_t2 = true;
  
  d.inputElements.push_back({"BONEIDS", 0, DXGI_FORMAT_R32G32B32A32_UINT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0});
  d.inputElements.push_back({"WEIGHTS", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0});
  
  return d;
}

PipelineDesc UnifiedPipeline::MakeSpriteDesc() {
  PipelineDesc d{};
  d.inputElements = {
      {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
       D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
       0},
      {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT,
       D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
  };
  d.vsPath = L"resources/engine/shaders/Sprite.VS.hlsl";
  d.psPath = L"resources/engine/shaders/Sprite.PS.hlsl";
  d.usePSMaterial_b0 = true;
  d.useVSTransform_b0 = true;
  d.usePSTextureTable_t0 = true;
  d.enableDepth = false;
  d.alphaBlend = true;
  d.cullMode = D3D12_CULL_MODE_NONE;
  d.fillMode = D3D12_FILL_MODE_SOLID;

  // ★ Sprite のデフォルトは Alpha ブレンド
  d.blendMode = BlendMode::Alpha;

  return d;
}

PipelineDesc UnifiedPipeline::MakeEmitterWireDesc() {
  PipelineDesc d = MakeObject3DDesc();
  d.alphaBlend = false;
  d.enableDepth = true;
  d.cullMode = D3D12_CULL_MODE_NONE;
  d.fillMode = D3D12_FILL_MODE_WIREFRAME;
  return d;
}

PipelineDesc UnifiedPipeline::MakeEmitterAlphaDesc() {
  PipelineDesc d = MakeObject3DDesc();
  d.alphaBlend = true;
  d.blendMode = BlendMode::Alpha;
  d.enableDepth = true;
  d.cullMode = D3D12_CULL_MODE_NONE;
  d.fillMode = D3D12_FILL_MODE_SOLID;
  return d;
}

PipelineDesc UnifiedPipeline::MakeParticleDesc() {
  PipelineDesc d{};
  d.inputElements = {
      {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
       D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
       0},
      {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT,
       D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
  };
  d.vsPath = L"resources/engine/shaders/Particle.VS.hlsl";
  d.psPath = L"resources/engine/shaders/Particle.PS.hlsl";
  d.usePSMaterial_b0 = true;
  d.useVSTransform_b0 = false; // Instancingで行列はSRV
  d.usePSTextureTable_t0 = true;
  d.useVSInstancingTable_t1 = true; // t1 StructuredBuffer
  d.usePSDirectionalLight_b1 = false;
  d.enableDepth = false;
  d.alphaBlend = true;            // ← 透過フェードのため有効化
  d.blendMode = BlendMode::Alpha; // ← 通常アルファ
  d.cullMode = D3D12_CULL_MODE_NONE;
  return d;
}

// Skybox用パイプラインプリセット
PipelineDesc UnifiedPipeline::MakeSkyDesc() {
  PipelineDesc d{};
  d.inputElements = {}; // 頂点バッファなし(SV_VertexID で全画面三角形を作る)
  d.vsPath = L"resources/engine/shaders/Sky.VS.hlsl";
  d.psPath = L"resources/engine/shaders/Sky.PS.hlsl";

  d.usePSMaterial_b0 = true; // SkyParam
  d.useVSTransform_b0 = false;
  d.usePSTextureTable_t0 = false;

  // MakeSkyboxDesc を踏襲: 深度は読むが書かない。VS が z=1 で出すので LESS_EQUAL で通る
  d.enableDepth = true;
  d.depthWrite = false;
  d.depthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
  d.cullMode = D3D12_CULL_MODE_NONE;
  d.blendMode = BlendMode::Opaque;
  return d;
}

PipelineDesc UnifiedPipeline::MakeWaterDesc() {
  // 空と同じ全画面パス(深度設定も同じ)で、ピクセルシェーダーだけ水面にする
  PipelineDesc d = MakeSkyDesc();
  d.psPath = L"resources/engine/shaders/Water.PS.hlsl";
  d.usePSTextureTable_t0 = true; // t0: 平面反射のテクスチャ(ルートパラメータは b0 の次の 1 番)
  return d;
}

PipelineDesc UnifiedPipeline::MakeCloudSeaDesc() {
  // 空と同じ全画面パス(深度設定も同じ)で、ピクセルシェーダーだけ雲海にする。b0 は CloudSeaParam
  PipelineDesc d = MakeSkyDesc();
  d.psPath = L"resources/engine/shaders/CloudSea.PS.hlsl";
  return d;
}

PipelineDesc UnifiedPipeline::MakeSkyboxDesc() {
  PipelineDesc d{};
  d.inputElements = {
      D3D12_INPUT_ELEMENT_DESC{"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
                               D3D12_APPEND_ALIGNED_ELEMENT,
                               D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
  };

  d.vsPath = L"resources/engine/shaders/Skybox.VS.hlsl";
  d.psPath = L"resources/engine/shaders/Skybox.PS.hlsl";

  // RootParameter: [0]=PS b0, [1]=VS b0, [2]=PS t0
  d.usePSMaterial_b0 = true;
  d.useVSTransform_b0 = true;
  d.usePSTextureTable_t0 = true;

  d.usePSDirectionalLight_b1 = false;
  d.useVSInstancingTable_t1 = false;
  d.usePSCamera_b2 = false;
  d.usePSPointLight_b3 = false;
  d.usePSSpotLight_b4 = false;

  d.enableDepth = true;
  d.depthWrite = false; // Skyboxは深度書き込み不要
  d.depthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

  d.alphaBlend = false;
  d.cullMode = D3D12_CULL_MODE_NONE; // キューブ内側を描く
  d.fillMode = D3D12_FILL_MODE_SOLID;
  return d;
}

PipelineDesc UnifiedPipeline::MakePrimitiveDesc() {
  PipelineDesc d{};
  d.inputElements = {
      {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
       D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
       0},
      {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
       D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
       0},
  };
  d.vsPath = L"resources/engine/shaders/Primitive.VS.hlsl";
  d.psPath = L"resources/engine/shaders/Primitive.PS.hlsl";

  d.usePSMaterial_b0 = true; // Index 0 (Dummy)
  d.useVSTransform_b0 = true; // Index 1
  d.usePSTextureTable_t0 = false;
  d.enableDepth = false; // スケルトンがメッシュに埋もれないよう最前面に表示
  d.alphaBlend = true; // 線も透過できるようにしておく
  d.blendMode = BlendMode::Alpha;
  d.cullMode = D3D12_CULL_MODE_NONE;
  d.fillMode = D3D12_FILL_MODE_SOLID;
  return d;
}

PipelineDesc UnifiedPipeline::MakeUnlitEffectDesc() {
  // 基本はObject3Dと同じ形（Root Signatureを維持）にする
  PipelineDesc d = MakeObject3DDesc();

  // ライトのフラグを false にしてはいけない（シェーダと不整合が起きるため）
  // 代わりに、描画時のマテリアル設定でライティングをOFFにする運用にします

  d.cullMode = D3D12_CULL_MODE_NONE;
  d.alphaBlend = true;            // 半透明を有効化
  d.blendMode = BlendMode::Alpha; // アルファブレンド
  d.depthWrite = false;           // エフェクト越しに後ろが透けるように

  // エフェクトをより強調したい場合は、加算合成にするのもアリです
  // d.blendMode = BlendMode::Add;

  return d;
}

PipelineDesc UnifiedPipeline::MakeRingDesc() {
  PipelineDesc d{};
  d.inputElements = {
      {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
       D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
       0},
      {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT,
       D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
      {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
       D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
       0},
  };
  d.vsPath = L"resources/engine/shaders/Ring.VS.hlsl";
  d.psPath = L"resources/engine/shaders/Ring.PS.hlsl";
  d.usePSMaterial_b0 = true;
  d.useVSTransform_b0 = true;
  d.usePSTextureTable_t0 = true;
  d.enableDepth = true;
  d.depthWrite = false;
  d.alphaBlend = true;
  d.blendMode = BlendMode::Add;
  d.cullMode = D3D12_CULL_MODE_NONE;
  return d;
}

PipelineDesc UnifiedPipeline::MakeCopyImageDesc() {
  PipelineDesc d{};
  d.inputElements = {}; // 頂点バッファなし
  d.vsPath = L"resources/engine/shaders/Fullscreen.VS.hlsl";
  d.psPath = L"resources/engine/shaders/Fullscreen.PS.hlsl";
  d.usePSMaterial_b0 = false;
  d.useVSTransform_b0 = false;
  d.usePSTextureTable_t0 = true; // t0: RenderTexture
  d.enableDepth = false;
  d.alphaBlend = false;
  d.cullMode = D3D12_CULL_MODE_NONE;
  d.fillMode = D3D12_FILL_MODE_SOLID;
  return d;
}

PipelineDesc UnifiedPipeline::MakeRadialBlurDesc() {
  PipelineDesc d{};
  d.inputElements = {};
  d.vsPath = L"resources/engine/shaders/Fullscreen.VS.hlsl";
  d.psPath = L"resources/engine/shaders/RadialBlur.PS.hlsl";
  d.usePSMaterial_b0 = true; // RadialBlurParam
  d.useVSTransform_b0 = false;
  d.usePSTextureTable_t0 = true;
  d.enableDepth = false;
  d.depthWrite = false;
  d.cullMode = D3D12_CULL_MODE_NONE;
  d.blendMode = BlendMode::Opaque;
  return d;
}

PipelineDesc UnifiedPipeline::MakeDissolveDesc() {
  PipelineDesc d{};
  d.inputElements = {};
  d.vsPath = L"resources/engine/shaders/Fullscreen.VS.hlsl";
  d.psPath = L"resources/engine/shaders/Dissolve.PS.hlsl";
  d.usePSMaterial_b0 = true;      // DissolveParam
  d.useVSTransform_b0 = false;
  d.usePSTextureTable_t0 = true;  // メインテクスチャ
  d.usePSEnvironmentMap_t1 = true;// マスクテクスチャ(t1)を使用
  d.enableDepth = false;
  d.depthWrite = false;
  d.cullMode = D3D12_CULL_MODE_NONE;
  d.blendMode = BlendMode::Opaque;
  return d;
}

PipelineDesc UnifiedPipeline::MakeRandomDesc() {
  PipelineDesc d{};
  d.inputElements = {};
  d.vsPath = L"resources/engine/shaders/Fullscreen.VS.hlsl";
  d.psPath = L"resources/engine/shaders/Random.PS.hlsl";
  d.usePSMaterial_b0 = true; // RandomParam
  d.useVSTransform_b0 = false;
  d.usePSTextureTable_t0 = true;
  d.enableDepth = false;
  d.depthWrite = false;
  d.cullMode = D3D12_CULL_MODE_NONE;
  d.blendMode = BlendMode::Opaque;
  return d;
}

PipelineDesc UnifiedPipeline::MakeCylinderDesc() {
  PipelineDesc d{};
  d.inputElements = {
      {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
       D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
       0},
      {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT,
       D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
      {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
       D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
       0},
  };
  d.vsPath = L"resources/engine/shaders/Cylinder.VS.hlsl";
  d.psPath = L"resources/engine/shaders/Cylinder.PS.hlsl";
  d.usePSMaterial_b0 = true;
  d.useVSTransform_b0 = true;
  d.usePSTextureTable_t0 = true;
  d.enableDepth = true;
  d.depthWrite = false;
  d.alphaBlend = true;
  d.blendMode = BlendMode::Add;
  d.cullMode = D3D12_CULL_MODE_BACK;
  return d;
}

PipelineDesc UnifiedPipeline::MakeGrayscaleDesc() {
  PipelineDesc d = MakeCopyImageDesc();
  d.psPath = L"resources/engine/shaders/Grayscale.PS.hlsl";
  d.usePSMaterial_b0 = false;
  return d;
}

PipelineDesc UnifiedPipeline::MakeSepiaDesc() {
  PipelineDesc d = MakeCopyImageDesc();
  d.psPath = L"resources/engine/shaders/Sepia.PS.hlsl";
  d.usePSMaterial_b0 = false;
  return d;
}

PipelineDesc UnifiedPipeline::MakeLuminanceBasedOutlineDesc() {
  PipelineDesc d = MakeCopyImageDesc();
  d.psPath = L"resources/engine/shaders/LuminanceBasedOutline.PS.hlsl";
  d.usePSMaterial_b0 = false;
  return d;
}

PipelineDesc UnifiedPipeline::MakeDepthBasedOutlineDesc() {
  PipelineDesc d = MakeCopyImageDesc();
  d.psPath = L"resources/engine/shaders/DepthBasedOutline.PS.hlsl";
  d.usePSMaterial_b0 = true;
  d.usePSEnvironmentMap_t1 = true;
  return d;
}

PipelineDesc UnifiedPipeline::MakeVignetteDesc() {
  PipelineDesc d = MakeCopyImageDesc();
  d.psPath = L"resources/engine/shaders/Vignette.PS.hlsl";
  d.usePSMaterial_b0 = true;
  return d;
}

PipelineDesc UnifiedPipeline::MakeTransitionDesc() {
  // ポストエフェクトと同じ全画面コピーの形(b0 = パラメータ、t0 = 画面のテクスチャ)で、ピクセルシェーダーだけ遷移用にする
  PipelineDesc d = MakeCopyImageDesc();
  d.psPath = L"resources/engine/shaders/Transition.PS.hlsl";
  d.usePSMaterial_b0 = true;
  d.usePSEnvironmentMap_t1 = true; // t1: シーンの深度(雲を抜ける遷移で、自機の手前の雲だけをかぶせる)
  return d;
}

PipelineDesc UnifiedPipeline::MakeVolumeCloudDepthDesc() {
  PipelineDesc d = MakeCopyImageDesc(); // 頂点バッファなしの全画面三角形、深度テストなし
  d.psPath = L"resources/engine/shaders/VolumeCloudDepth.PS.hlsl";
  d.usePSMaterial_b0 = true;          // b0: VolumeCloudParam
  d.usePSTextureTable_t0 = true;      // t0: シーンの深度
  d.rtvFormat = DXGI_FORMAT_R32_FLOAT; // カメラから物体までの距離(無限遠は大きな値)
  d.dsvFormat = DXGI_FORMAT_UNKNOWN;  // 深度バッファは付けずに描く
  return d;
}

PipelineDesc UnifiedPipeline::MakeVolumeCloudDesc() {
  PipelineDesc d = MakeCopyImageDesc();
  d.psPath = L"resources/engine/shaders/VolumeCloud.PS.hlsl";
  d.usePSMaterial_b0 = true;          // b0: VolumeCloudParam
  d.usePSTextureTable_t0 = true;      // t0: 形のノイズ(3D)
  d.usePSEnvironmentMap_t1 = true;    // t1: 細かい模様(3D)。ルートの枠は環境マップ用のものを借りる
  d.usePSMaskTexture_t2 = true;       // t2: 縮めた距離。ルートの枠はマスク用のものを借りる
  d.usePSWrapSampler_s1 = true;       // s1: 繰り返し
  d.rtvFormat = DXGI_FORMAT_R16G16B16A16_FLOAT; // 光はリニア、1 を超えてもよい(HDR)。ブレンドなしで、そのまま書く
  d.dsvFormat = DXGI_FORMAT_UNKNOWN;
  return d;
}

PipelineDesc UnifiedPipeline::MakeVolumeCloudCompositeDesc() {
  PipelineDesc d = MakeCopyImageDesc();
  d.psPath = L"resources/engine/shaders/VolumeCloudComposite.PS.hlsl";
  d.usePSMaterial_b0 = true;          // b0: VolumeCloudParam
  d.usePSTextureTable_t0 = true;      // t0: 低解像度の雲
  d.usePSEnvironmentMap_t1 = true;    // t1: 縮めた距離。ルートの枠は環境マップ用のものを借りる
  d.usePSMaskTexture_t2 = true;       // t2: 元の深度。ルートの枠はマスク用のものを借りる
  d.alphaBlend = true;
  d.blendMode = BlendMode::PremultipliedAlpha;
  d.depthWrite = false;
  d.dsvFormat = DXGI_FORMAT_UNKNOWN;  // 深度バッファは付けずに描く(深度はテクスチャとして読む)
  return d;
}

PipelineDesc UnifiedPipeline::MakeBoxFilterDesc() {
  PipelineDesc d = MakeCopyImageDesc();
  d.psPath = L"resources/engine/shaders/BoxFilter.PS.hlsl";
  d.usePSMaterial_b0 = true;
  return d;
}

PipelineDesc UnifiedPipeline::MakeGaussianFilterDesc() {
  PipelineDesc d = MakeCopyImageDesc();
  d.psPath = L"resources/engine/shaders/GaussianFilter.PS.hlsl";
  d.usePSMaterial_b0 = true;
  return d;
}
