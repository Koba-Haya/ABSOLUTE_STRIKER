#pragma once

#include <memory>
#include <vector>
#include <wrl.h>
#include <d3d12.h>

#include "graphics/texture/RenderTexture.h"
#include "graphics/texture/DepthTexture.h"
#include "graphics/texture/PlanarReflection.h"
#include "graphics/CloudSeaSettings.h"

#include "LightTypes.h"
#include "Matrix.h"
#include "Method.h"
#include "BlendMode.h"

class UnifiedPipeline;
class DirectXCommon;
class ModelInstance;
class Sprite;
class Skybox;
class PrimitiveDrawer;
class ParticleManager;
class Camera;
class Ring;
class Cylinder;
struct ID3D12Resource;
class TextureResource;

// GPU 定数バッファ型
struct CameraForGPU {
  Vector3 worldPosition{};
  float pad = 0.0f;
};

static constexpr int kMaxDirLights = 4;
struct alignas(16) DirectionalLightCB {
  float color[4];
  float direction[3];
  float intensity;
  int32_t enabled;
  float pad0[3];
};

struct alignas(16) DirectionalLightGroupCB {
  int32_t count;
  float padCount[3];
  DirectionalLightCB lights[kMaxDirLights];
  int32_t enabled;
  float padEnabled[3];
};

struct alignas(16) PointLightCB {
  float color[4];
  float position[3];
  float intensity;
  float radius;
  float decay;
  int32_t enabled;
  float pad0;
};

static constexpr int kMaxPointLights = 16;
struct alignas(16) PointLightGroupCB {
  int32_t count;
  float padCount[3];
  PointLightCB lights[kMaxPointLights];
  int32_t enabled;
  float padEnabled[3];
};

static constexpr int kMaxSpotLights = 8;
struct alignas(16) SpotLightCB {
  float color[4];
  float position[3];
  float intensity;
  float direction[3];
  float distance;
  float decay;
  float cosAngle;
  int32_t enabled;
  float pad0;
};

struct alignas(16) SpotLightGroupCB {
  int32_t count;
  float padCount[3];
  SpotLightCB lights[kMaxSpotLights];
  int32_t enabled;
  float padEnabled[3];
};

class Renderer {
public:
  template <class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

  // シングルトンの取得（実体は.cpp）
  static Renderer *GetInstance();

  void Initialize(DirectXCommon *dx);

  // シーン層向け API
  void SetCamera(const Camera &camera);
  void SetDirectionalLights(const std::vector<DirLight> &lights,
                            bool groupEnabled);
  void SetPointLights(const std::vector<PointLight> &lights, bool groupEnabled);
  void SetSpotLights(const std::vector<SpotLight> &lights, bool groupEnabled);

  float GetScreenWidth() const;
  float GetScreenHeight() const;
  float GetAspectRatio() const;

  DirectXCommon *GetDX() const { return dx_; }
  const Matrix4x4 &GetViewMatrix() const { return view_; }
  const Matrix4x4 &GetProjectionMatrix() const { return proj_; }

  ComPtr<ID3D12Resource> CreateBuffer(size_t size);
  ComPtr<ID3D12Resource> CreateUploadBuffer(size_t size);
  ComPtr<ID3D12Resource> CreateUAVBuffer(size_t size);

  // 環境マップ設定（CubeMap）を追加
  void SetEnvironmentMap(std::shared_ptr<TextureResource> texture) {
    environmentMap_ = texture;
  }

  // 描画メソッド群
  void DrawModel(ModelInstance *model);
  void DrawSprite(Sprite *sprite);
  void DrawSkybox(Skybox *skybox);
  // 手続き生成の空(グラデーション・太陽・雲)を全画面で描く(最奥)。sunDirection は太陽へ向かう向き、
  // time は雲が風で流れるアニメーション用の秒。雲の位置はカメラ位置から求める。
  // worldOffset: カメラ位置に足す「ワールドの原点のずれ」。描画用の座標を小さく保つために巻き戻している場合、
  // 巻き戻した分を渡すと、雲の模様が巻き戻しをまたいで連続する(巻き戻していなければ {0,0,0})
  void DrawSky(const Vector3 &sunDirection, float time, const Vector3 &worldOffset = {0.0f, 0.0f, 0.0f});
  // 水面を全画面パスで描く(空の直後・オブジェクトの前)。time は波のアニメーション用の秒
  // worldOffset は DrawSky と同じ(波の模様が巻き戻しをまたいで連続する)。
  // worldWrapLength: 巻き戻し 1 回分の長さ(z)。worldOffset.z はその整数倍で渡す。波の位相はこの 2 つから厳密に求める
  // (大きな座標を float に載せると sin の位相が荒れるため)。巻き戻していなければ 0
  void DrawWater(const Vector3 &sunDirection, float time, const Vector3 &worldOffset = {0.0f, 0.0f, 0.0f}, float worldWrapLength = 0.0f);
  // 雲海(空と、自機の下に広がる雲の海)を全画面パスで描く(最奥。スカイボックスの代わり)。
  // カメラは SetCamera で渡したものを使う。雲の模様の位置はカメラ位置から求める(ワールドに固定され、自機が進むと後ろへ流れる)。
  // 座標が大きくなっても模様が崩れないよう、カメラ位置は「整数のマス番号 + 小数」に分けてシェーダーへ渡す
  void DrawCloudSea(const CloudSeaSettings &settings);
  // depthTest: 深度テストあり(書き込みなし)で描く。Alpha ブレンドのみ対応(他では無視)
  void DrawParticles(ParticleManager *pm,
                     BlendMode blendMode = BlendMode::Alpha,
                     bool depthTest = false);
  // blendMode: GPU パーティクルの描画に使うブレンドモード（デフォルト Alpha）
  void DrawGPUParticles(BlendMode blendMode = BlendMode::Alpha);
  void DrawLine(const Vector3 &start, const Vector3 &end, const Vector4 &color);
  void DrawGrid(float size, int divisions, const Vector4 &color);
  void RenderPrimitives();
  // エフェクト用描画メソッド（中身はDrawModelとほぼ同じだがパイプラインが違う）
  void DrawEffectModel(ModelInstance *model);
  void DrawRing(Ring *ring, D3D12_GPU_DESCRIPTOR_HANDLE textureHandle);
  void DrawCylinder(Cylinder *cylinder, D3D12_GPU_DESCRIPTOR_HANDLE textureHandle);
  
  enum class PostProcessMode {
    Normal,
    Grayscale,
    Sepia,
    Vignette,
    BoxFilter,
    GaussianFilter,
    LuminanceBasedOutline,
    DepthBasedOutline,
    RadialBlur,
    Dissolve,
    Random
  };
  void DrawFullscreen(D3D12_GPU_DESCRIPTOR_HANDLE textureHandle, PostProcessMode mode = PostProcessMode::Normal, D3D12_GPU_DESCRIPTOR_HANDLE depthOrMaskTextureHandle = {});
  void SetVignetteParam(float scale, float powValue);
  void SetBoxFilterParam(int32_t k);
  void SetGaussianFilterParam(int32_t k, float sigma, const Vector2& direction);
  void SetDepthBasedOutlineParam(const Matrix4x4& projectionInverse);
  void SetRadialBlurParam(const Vector2& center, float blurWidth);
  void SetDissolveParam(float threshold, float edgeRange, const Vector3& edgeColor, const Vector3& maskColor);
  void SetDissolveMaskTexture(std::shared_ptr<TextureResource> tex) { dissolveMaskTexture_ = tex; }
  void SetRandomParam(float time);

  // --- 平面反射(水面に映る物体) ---
  // 反射用の描画先を作る(2 回目以降は何もしない)。大きさは本編の描画先と同じにする(画面座標が揃うため)
  void InitializePlanarReflection(uint32_t width, uint32_t height);
  // 反射パスの開始/終了。水面を描く(DrawWater)前に毎フレーム行う(物体が無くても、背景を透明にするために呼ぶ)。
  // 間に呼んだ DrawModel は、planeHeight を軸に上下反転した鏡像として反射用の描画先へ描かれる。
  // 本編の描画先は BeginRenderScene で切り替わるので、この 2 つは BeginRenderScene より前に呼ぶこと
  void BeginReflectionPass(float planeHeight);
  void EndReflectionPass();

  // --- 高レベル レンダリングAPI ---
  void InitializePostProcess(uint32_t width, uint32_t height);
  void BeginRenderScene();
  void EndRenderScene(const Matrix4x4& projInverse);
  
  PostProcessMode GetPostProcessMode() const { return postProcessMode_; }
  void SetPostProcessMode(PostProcessMode mode) { postProcessMode_ = mode; }
  D3D12_GPU_DESCRIPTOR_HANDLE GetPostProcessTextureSrv() const {
      if (postProcessTexture_) return postProcessTexture_->GetSrvGpuHandle();
      return {};
  }

  void DispatchSkinning(ModelInstance* instance);

  ~Renderer();

private:
  Renderer();

  void InitSkinningPipeline_();

  DirectXCommon *dx_ = nullptr;
  
  // CS Skinning
  ComPtr<ID3D12RootSignature> skinningRootSignature_;
  ComPtr<ID3D12PipelineState> skinningPipelineState_;

  Matrix4x4 view_ = MakeIdentity4x4();
  Matrix4x4 proj_ = MakeIdentity4x4();

  ComPtr<ID3D12Resource> cameraCB_;
  CameraForGPU *cameraMapped_ = nullptr;

  ComPtr<ID3D12Resource> directionalLightCB_;
  DirectionalLightGroupCB *directionalLightMapped_ = nullptr;

  ComPtr<ID3D12Resource> pointLightCB_;
  PointLightGroupCB *pointLightMapped_ = nullptr;

  ComPtr<ID3D12Resource> spotLightCB_;
  SpotLightGroupCB *spotLightMapped_ = nullptr;

  struct SkinningInformation {
    uint32_t numVertices;
  };
  ComPtr<ID3D12Resource> skinningInformationCB_;
  SkinningInformation* skinningInformationMapped_ = nullptr;

  std::unique_ptr<UnifiedPipeline> objPipelineOpaque_;
  std::unique_ptr<UnifiedPipeline> objPipelineWireframe_;
  std::unique_ptr<UnifiedPipeline> objPipelineMirrored_; // 反射パス用(鏡映で巻き順が逆になるので、表裏の判定を反転)
  std::unique_ptr<UnifiedPipeline> skinnedPipelineOpaque_;
  std::unique_ptr<UnifiedPipeline> skinnedPipelineWireframe_;
  std::unique_ptr<UnifiedPipeline> skyboxPipeline_;
  std::unique_ptr<UnifiedPipeline> skyPipeline_;
  std::unique_ptr<UnifiedPipeline> waterPipeline_;
  std::unique_ptr<UnifiedPipeline> cloudSeaPipeline_;

  std::unique_ptr<UnifiedPipeline> spritePipelineAlpha_;
  std::unique_ptr<UnifiedPipeline> spritePipelineAdd_;
  std::unique_ptr<UnifiedPipeline> spritePipelineSub_;
  std::unique_ptr<UnifiedPipeline> spritePipelineMul_;
  std::unique_ptr<UnifiedPipeline> spritePipelineScreen_;

  std::unique_ptr<UnifiedPipeline> particlePipelineAlpha_;
  std::unique_ptr<UnifiedPipeline> particlePipelineAlphaDepth_; // 深度テストあり版
  std::unique_ptr<UnifiedPipeline> particlePipelineAdd_;
  std::unique_ptr<UnifiedPipeline> particlePipelineSub_;
  std::unique_ptr<UnifiedPipeline> particlePipelineMul_;
  std::unique_ptr<UnifiedPipeline> particlePipelineScreen_;

  UnifiedPipeline *GetSpritePipeline_(BlendMode mode);
  UnifiedPipeline *GetParticlePipeline_(BlendMode mode);

  std::shared_ptr<TextureResource> environmentMap_;

  struct TransformCB {
    Matrix4x4 WVP;
  };

  std::unique_ptr<UnifiedPipeline> primitivePipeline_;
  std::unique_ptr<PrimitiveDrawer> primitiveDrawer_;
  ComPtr<ID3D12Resource> primitiveTransformCB_;
  TransformCB *primitiveTransformMapped_ = nullptr;

  std::unique_ptr<UnifiedPipeline> effectPipeline_; // エフェクト用
  std::unique_ptr<UnifiedPipeline> ringPipeline_;
  std::unique_ptr<UnifiedPipeline> cylinderPipeline_;
  std::unique_ptr<UnifiedPipeline> copyImagePipeline_;
  std::unique_ptr<UnifiedPipeline> grayscalePipeline_;
  std::unique_ptr<UnifiedPipeline> sepiaPipeline_;
  std::unique_ptr<UnifiedPipeline> vignettePipeline_;
  std::unique_ptr<UnifiedPipeline> boxFilterPipeline_;
  std::unique_ptr<UnifiedPipeline> gaussianFilterPipeline_;
  std::unique_ptr<UnifiedPipeline> luminanceBasedOutlinePipeline_;
  std::unique_ptr<UnifiedPipeline> pipelineDepthBasedOutline_;
  std::unique_ptr<UnifiedPipeline> radialBlurPipeline_;
  std::unique_ptr<UnifiedPipeline> randomPipeline_;

  PostProcessMode postProcessMode_ = PostProcessMode::Normal;

  struct alignas(16) VignetteParam {
    float scale;
    float powValue;
    float pad[2];
  };
  ComPtr<ID3D12Resource> vignetteParamCB_;
  VignetteParam* vignetteParamMapped_ = nullptr;

  struct alignas(16) SkyParam {
    Matrix4x4 invViewProj;
    Vector3 sunDirection;
    float time;
    Vector3 cameraPosition;
    float pad;
  };
  ComPtr<ID3D12Resource> skyParamCB_;
  SkyParam* skyParamMapped_ = nullptr;

  // CloudSea.hlsli の cbuffer CloudSeaParam と同じ並び(16 バイト区切り)にすること
  // ノイズの層ごとに、カメラ位置を「整数のマス番号 + マス内の小数」に分けた値を持つ(倍率が層ごとに違うため)
  struct CloudSeaCellOrigin {
    int32_t cell[2];
    float frac[2];
  };
  struct alignas(16) CloudSeaParam {
    Matrix4x4 invViewProj;
    Vector3 cameraPosition;
    float seaHeight;
    Vector3 sunDirection;
    float thickness;
    CloudSeaCellOrigin base;   // 主ノイズ
    CloudSeaCellOrigin lowA;   // 大きなうねり 1 層目
    CloudSeaCellOrigin lowB;   // 大きなうねり 2 層目
    CloudSeaCellOrigin floor;  // 切れ間から見える下の雲層
    float bump;
    float scale;
    float threshold;
    float softness;
    float density;
    float fog;
    float steps;
    float jitter;
    float swellHeight;
    float swellBlend;
    float billow;
    float shadowStrength;
    float shadowLength;
    float gapDarkness;
    float aerial;
    float contrast;
    float lowRatioA;
    float lowRatioB;
    float floorRatio;
    float padding;
  };
  // カメラ位置にノイズ空間の倍率を掛け、整数のマス番号と小数に分ける(double で計算し、座標が大きくなっても精度を保つ)
  static void SplitCloudSeaCell(const Vector3 &cameraPosition, double cellsPerUnit, CloudSeaCellOrigin &out);
  ComPtr<ID3D12Resource> cloudSeaParamCB_;
  CloudSeaParam* cloudSeaParamMapped_ = nullptr;

  struct alignas(16) WaterParam {
    Matrix4x4 invViewProj;
    Vector3 cameraPosition;
    float time;
    Vector3 sunDirection;
    float worldWrapLength;
    Vector3 localCameraPosition;
    float worldWrapCount;
  };
  ComPtr<ID3D12Resource> waterParamCB_;
  WaterParam* waterParamMapped_ = nullptr;

  struct alignas(16) RandomParam {
    float time;
    float pad[3];
  };
  ComPtr<ID3D12Resource> randomParamCB_;
  RandomParam* randomParamMapped_ = nullptr;

  struct alignas(16) BoxFilterParam {
    int32_t k;
    float pad[3];
  };
  ComPtr<ID3D12Resource> boxFilterParamCB_;
  BoxFilterParam* boxFilterParamMapped_ = nullptr;

  struct alignas(16) GaussianFilterParam {
    int32_t k;
    float sigma;
    float direction[2];
  };
  ComPtr<ID3D12Resource> gaussianFilterParamCB_;
  GaussianFilterParam* gaussianFilterParamMapped_ = nullptr;

  struct DepthBasedOutlineParam {
      Matrix4x4 projectionInverse;
  };
  ComPtr<ID3D12Resource> depthBasedOutlineParamCB_;
  DepthBasedOutlineParam* depthBasedOutlineParamMapped_ = nullptr;

  struct alignas(16) RadialBlurParam {
    Vector2 center;
    float blurWidth;
    float padding;
  };
  ComPtr<ID3D12Resource> radialBlurParamCB_;
  RadialBlurParam* radialBlurParamCBMap_ = nullptr;

  std::unique_ptr<UnifiedPipeline> dissolvePipeline_;
  struct alignas(16) DissolveParam {
    float threshold;
    float edgeRange;
    float padding[2];
    Vector3 edgeColor;
    float padding2;
    Vector3 maskColor;
    float padding3;
  };
  ComPtr<ID3D12Resource> dissolveParamCB_;
  DissolveParam* dissolveParamMapped_ = nullptr;
  std::shared_ptr<TextureResource> dissolveMaskTexture_;

  // レンダリング・ポストプロセス用バッファ
  std::unique_ptr<PlanarReflection> planarReflection_;
  std::unique_ptr<RenderTexture> renderTexture_;
  std::unique_ptr<DepthTexture> depthTexture_;
  std::unique_ptr<RenderTexture> postProcessTexture_;
  std::unique_ptr<RenderTexture> gaussianTempTexture_;
};