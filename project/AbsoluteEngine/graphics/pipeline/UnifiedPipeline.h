// UnifiedPipeline.h (省略なし)

#pragma once
#include "BlendMode.h"
#include <d3d12.h>
#include <string>
#include <vector>
#include <wrl.h>

// DXC 前方宣言
struct IDxcUtils;
struct IDxcCompiler3;
struct IDxcIncludeHandler;
struct IDxcBlob;

struct PipelineDesc {
  // 入力レイアウト
  std::vector<D3D12_INPUT_ELEMENT_DESC> inputElements;

  // シェーダのパス
  std::wstring vsPath;
  std::wstring psPath;
  const wchar_t *vsProfile = L"vs_6_0";
  const wchar_t *psProfile = L"ps_6_0";

  // ルートパラメータの有無
  bool usePSMaterial_b0 = true;          // PS: b0
  bool useVSTransform_b0 = true;         // VS: b0
  bool usePSTextureTable_t0 = true;      // PS: t0 (SRVテーブル)
  bool usePSDirectionalLight_b1 = false; // PS: b1
  bool useVSInstancingTable_t1 = false;  // VS: t1
  bool usePSEnvironmentMap_t1 = false; // PS: t1 (環境マップ用SRVテーブル)
  bool usePSMaskTexture_t2 = false; // 追加: PS: t2 (Dissolve用マスクなど)
  bool depthWrite = true;

  // カメラCB b2
  bool usePSCamera_b2 = false;
  // 点光源CB b3
  bool usePSPointLight_b3 = false;
  // スポットライトCB b4
  bool usePSSpotLight_b4 = false;

  // VS Skinning t2
  bool useVSSkinning_t2 = false;

  // ラスタ/ブレンド/深度
  bool enableDepth = true;
  bool alphaBlend = false;
  D3D12_CULL_MODE cullMode = D3D12_CULL_MODE_BACK;
  // true: 反時計回りを表面とする(既定は時計回りが表面)。鏡映(y を反転)して描くと三角形の巻き順が逆になるので、
  // 鏡像を描くパイプラインだけ true にして、表裏の判定(カリング)を元に戻す
  bool frontCounterClockwise = false;
  D3D12_FILL_MODE fillMode = D3D12_FILL_MODE_SOLID;
  BlendMode blendMode = BlendMode::Opaque;

  // 出力フォーマット
  DXGI_FORMAT rtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
  DXGI_FORMAT dsvFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
  D3D12_COMPARISON_FUNC depthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
};

class UnifiedPipeline {
public:
  template <class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

public:
  bool Initialize(ID3D12Device *device, IDxcUtils *dxcUtils,
                  IDxcCompiler3 *dxcCompiler,
                  IDxcIncludeHandler *includeHandler, const PipelineDesc &desc);

  ID3D12RootSignature *GetRootSignature() const { return rootSignature_.Get(); }
  ID3D12PipelineState *GetPipelineState() const { return pso_.Get(); }

  void SetPipelineState(ID3D12GraphicsCommandList *cmdList) const {
    if (!cmdList || !rootSignature_ || !pso_) {
      char buf[256];
      sprintf_s(buf,
                "[UnifiedPipeline] SetPipelineState failed! cmdList=%p, "
                "rootSig=%p, pso_=%p\n",
                cmdList, rootSignature_.Get(), pso_.Get());
      OutputDebugStringA(buf);
      return;
    }
    cmdList->SetGraphicsRootSignature(rootSignature_.Get());
    cmdList->SetPipelineState(pso_.Get());
  }

  static PipelineDesc MakeObject3DDesc();
  static PipelineDesc MakeSkinnedObject3DDesc();
  static PipelineDesc MakeSpriteDesc();
  static PipelineDesc MakeEmitterWireDesc();
  static PipelineDesc MakeEmitterAlphaDesc();
  static PipelineDesc MakeParticleDesc();
  static PipelineDesc MakeSkyboxDesc();
  // 全画面の手続き生成の空(Sky.VS / Sky.PS)
  static PipelineDesc MakeSkyDesc();
  // 全画面パスで描く水面(Sky.VS / Water.PS)
  static PipelineDesc MakeWaterDesc();
  static PipelineDesc MakePrimitiveDesc();
  // 3D空間用のエフェクト（ライティングなし）の記述
  static PipelineDesc MakeUnlitEffectDesc();
  static PipelineDesc MakeRingDesc();
  static PipelineDesc MakeCylinderDesc();
  static PipelineDesc MakeCopyImageDesc();
  static PipelineDesc MakeGrayscaleDesc();
  static PipelineDesc MakeSepiaDesc();
  static PipelineDesc MakeVignetteDesc();
  static PipelineDesc MakeBoxFilterDesc();
  static PipelineDesc MakeGaussianFilterDesc();
  static PipelineDesc MakeLuminanceBasedOutlineDesc();
  static PipelineDesc MakeDepthBasedOutlineDesc();
  static PipelineDesc MakeRadialBlurDesc();
  static PipelineDesc MakeDissolveDesc();
  static PipelineDesc MakeRandomDesc(); // 追加

  // Compute
  static PipelineDesc MakeParticleInitializeDesc();

private:
  ComPtr<ID3D12RootSignature> rootSignature_;
  ComPtr<ID3D12PipelineState> pso_;
};