#pragma once

#include <array>
#include <chrono>
#include <memory>
#include <vector>
#include <wrl.h>
#include <d3d12.h>

#include "graphics/texture/RenderTexture.h"
#include "graphics/texture/DepthTexture.h"
#include "graphics/texture/PlanarReflection.h"
#include "graphics/CloudSeaSettings.h"
#include "graphics/VolumeCloudSettings.h"
#include "graphics/VolumeCloudQuality.h"
#include "graphics/SeaSkySettings.h"
#include "graphics/TransitionStyle.h"

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
class RibbonDrawer;
class RibbonTrail;
class ParticleManager;
class Camera;
class Ring;
class Cylinder;
class CloudNoiseTextures;
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
  // ボリューム雲が共通で使う 3D ノイズ(起動時に生成済み。生成に失敗していたら IsInitialized() が false)
  const CloudNoiseTextures *GetCloudNoise() const { return cloudNoise_.get(); }
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
  // 新方式の雲海の背景(空)を全画面パスで描く(最奥。旧方式の DrawCloudSea の代わり)。
  // 空のグラデーション・太陽・雲の切れ間の下の層だけを描く(雲は、AddVolumeCloud の層が担う)。カメラは SetCamera で渡したものを使う
  void DrawSeaSky(const SeaSkySettings &settings);
  // 同じフレームに依頼できるボリューム雲の最大数
  static constexpr size_t kMaxVolumeClouds = 4;
  // ボリューム雲(箱や層)を、このフレームに描くよう依頼する。毎フレーム、シーンの Draw の中で、描きたい数だけ呼ぶ
  // (最大 kMaxVolumeClouds 個。それ以上は無視する)。
  // 実際の描画は EndRenderScene の中で、物体とパーティクルを描いたあと・ポストエフェクトの前に行う
  // (シーンの深度を読んで、物体の手前の雲だけを重ねるため)。カメラは SetCamera で渡したものを使う。
  // 重なる雲は、奥(遠い方)から手前の順に追加すること(追加した順に重ねるため)。
  // 同じフレームの雲は、解像度の分母をそろえて描く(深度の縮小を 1 回で済ませるため。いちばん粗い分母に合わせる)。
  // sunDirection: 太陽へ向かう向き(正規化は内部で行う)、time: 雲が風で流れるアニメーション用の秒。
  // 3D ノイズの生成に失敗しているときや、settings.enabled が false のときは何もしない
  void AddVolumeCloud(const VolumeCloudSettings &settings, const Vector3 &sunDirection, float time);
  // ボリューム雲の品質の自動調整(実測の GPU 時間に合わせて、解像度とステップ数を上げ下げする。全ての雲に同じ段を適用する)。
  // AddVolumeCloud に渡した設定は「基準」で、描くときだけ、今の段に合わせて軽くした値で上書きされる。
  // 設定や現在の段の確認用(調整 UI が使う)
  VolumeCloudQuality &GetVolumeCloudQuality() { return volumeCloudQuality_; }
  // depthTest: 深度テストあり(書き込みなし)で描く。Alpha ブレンドのみ対応(他では無視)
  // filter: 描くグループの選び方(既定はすべて)
  void DrawParticles(ParticleManager *pm,
                     BlendMode blendMode = BlendMode::Alpha,
                     bool depthTest = false,
                     ParticleDrawFilter filter = ParticleDrawFilter::All);
  // blendMode: GPU パーティクルの描画に使うブレンドモード（デフォルト Alpha）
  void DrawGPUParticles(BlendMode blendMode = BlendMode::Alpha);
  void DrawLine(const Vector3 &start, const Vector3 &end, const Vector4 &color);
  void DrawGrid(float size, int divisions, const Vector4 &color);
  void RenderPrimitives();
  // エフェクト用描画メソッド（中身はDrawModelとほぼ同じだがパイプラインが違う）
  // writeDepth: true のとき、深度バッファへ書き込む(アルファが 0.5 以下の画素は捨てるので、文字や模様の形のまま書かれる)。
  //             ボリューム雲のように、深度を見て後から描かれるものが、このモデルの手前だけに描かれるようになる。
  //             半透明のエフェクトの後ろを透けさせたいときは false のまま使う
  void DrawEffectModel(ModelInstance *model, bool writeDepth = false);
  void DrawRing(Ring *ring, D3D12_GPU_DESCRIPTOR_HANDLE textureHandle);
  void DrawCylinder(Cylinder *cylinder, D3D12_GPU_DESCRIPTOR_HANDLE textureHandle);
  // 軌跡(リボン)を、カメラに面を向けた帯として描く。シーンの Draw の中で、描きたい軌跡の数だけ呼ぶ
  void DrawRibbon(const RibbonTrail &trail);

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
  // Vignette(画面の周囲を、色に寄せる)。周囲ほど暗く(= 色に近く)なる。色は黒、強さは 1 の既定(従来どおりの暗転)
  void SetVignetteParam(float scale, float powValue);
  // 色と強さ付き。周囲を color へ寄せる(黒なら暗転、赤なら赤い枠)。strength は 0(効果なし)〜 1(最大)
  void SetVignetteParam(float scale, float powValue, const Vector3& color, float strength);
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

  // --- 画面遷移の覆い ---
  // ポストエフェクトのあと(画面へ出す直前)に、覆い具合 cover(0 = 透明、1 = 完全に覆う)で style の効果をかける。
  // 毎フレーム、シーンの Draw の前に呼ぶ。cover が 0 以下のフレームは何もしない
  // time: 遷移を始めてからの経過(秒)。覆い具合と違い、晴れる間も増え続ける(雲の流れ、水滴の流れに使う)
  // duration: 遷移全体の長さ(秒)。雲を抜ける遷移が、time / duration で層をどこまで昇ったかを決める
  void SetTransition(const TransitionStyle &style, float cover, float time, float duration);
  void ClearTransition() { transitionCover_ = 0.0f; }
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
  std::unique_ptr<UnifiedPipeline> transitionPipeline_;
  std::unique_ptr<UnifiedPipeline> lensDropsPipeline_; // 画面遷移の 2 つ目のパス(レンズの水滴)

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
  std::unique_ptr<UnifiedPipeline> effectDepthPipeline_; // エフェクト用(深度を書き込む版。DrawEffectModel の writeDepth)
  std::unique_ptr<UnifiedPipeline> ringPipeline_;
  std::unique_ptr<UnifiedPipeline> ribbonPipeline_;
  std::unique_ptr<RibbonDrawer> ribbonDrawer_;
  ComPtr<ID3D12Resource> ribbonTransformCB_;
  TransformCB *ribbonTransformMapped_ = nullptr;
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
    float tint[4]; // 周囲を寄せる色(rgb)と、効果の強さ(w)
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

  // Transition.PS.hlsl の cbuffer TransitionParam と同じ並び(16 バイト区切り)にすること
  struct alignas(16) TransitionParam {
    Matrix4x4 invViewProj;
    Matrix4x4 viewProj;
    float cameraPosition[3];
    float slabBottom;
    float sunDirection[3];
    float slabTop;
    float color[3];
    float cover;
    float kind;
    float softness;
    float noiseScale;
    float blurStrength;
    float center[2];
    float aspect;
    float time;
    float duration;
    float padding[3];
    float mistFog[4];  // 霧: (彩度の落とし量, ぼかしの最大半径, 覆い具合 1 の視程, 覆い具合 0 の視程)
    float mistFog2[4]; // 霧: (深度の無い背景の距離, 最後の覆いを始める覆い具合, 太陽側の明るさ, デバッグ表示の番号)
    float mistDrop[4]; // 水滴: (濡れ具合, 霧の倍率, 水滴が付き始めてからの秒, レンズ水滴のパスを使うか 0 / 1)
    float mistPlane[4]; // 霧: (深度の無い背景の水平面の高さ, 水平面を使うか 0 / 1, 未使用, 未使用)
  };
  ComPtr<ID3D12Resource> transitionParamCB_;
  TransitionParam* transitionParamMapped_ = nullptr;
  TransitionStyle transitionStyle_{};
  float transitionCover_ = 0.0f;
  float transitionTime_ = 0.0f;
  float transitionDuration_ = 1.0f;

  // 画面遷移の覆いを、ポストエフェクト後のテクスチャ(postProcessTexture_)へ上書きでかける
  void ApplyTransitionPass_();

  // SeaSky.PS.hlsl の cbuffer SeaSkyParam と同じ並び(16 バイト区切り)にすること
  struct alignas(16) SeaSkyParam {
    Matrix4x4 invViewProj;
    Vector3 cameraPosition;
    float floorHeight;
    Vector3 sunDirection;
    float gapDarkness;
  };
  std::unique_ptr<UnifiedPipeline> seaSkyPipeline_;
  ComPtr<ID3D12Resource> seaSkyParamCB_;
  SeaSkyParam *seaSkyParamMapped_ = nullptr;

  // VolumeCloud.hlsli の cbuffer VolumeCloudParam と同じ並び(16 バイト区切り)にすること
  struct alignas(16) VolumeCloudParam {
    Matrix4x4 invViewProj;
    Vector3 cameraPosition;
    float time;
    Vector3 sunDirection;
    float extinction;
    Vector3 boxCenter;
    float noiseScale;
    Vector3 boxHalfSize;
    float detailScale;
    Vector3 windVelocity;
    float threshold;
    float edgeFade;
    float erosion;
    float steps;
    float lightSteps;
    float lightStepLength;
    float phaseG;
    float sunIntensity;
    float ambientIntensity;
    float fullSize[2];  // シーンの描画先の大きさ(画素)
    float lowSize[2];   // 雲を描く低解像度の範囲の大きさ(画素)
    float divisor;      // 解像度の分母
    float shapeMode;    // 外形の種類(0 = 箱、1 = 層)
    float layerBottom;  // 層: 下面の高さ
    float layerTop;     // 層: 上面の高さ
    float maxDistance;  // 層: 描く距離の上限
    float fogDensity;   // 層: 霞の強さ
    float highlightKnee; // 明るい部分の圧縮を始める値
    float shadowLift;   // 影の持ち上げ
    // 着地の反応(層のみ)。VolumeCloudImpact と同じ意味
    float impactCenter[2];
    float impactAge;
    float impactStrength;
    float impactCraterRadius;
    float impactCraterDepth;
    float impactRingWidth;
    float impactRingHeight;
    float impactRingSpeed;
    float impactRefillSeconds;
    float impactRingFadeSeconds;
    float seaFill;
  };
  std::unique_ptr<UnifiedPipeline> volumeCloudDepthPipeline_;     // パス 1: 深度を低解像度へ縮める
  std::unique_ptr<UnifiedPipeline> volumeCloudPipeline_;          // パス 2: 雲を低解像度で描く
  std::unique_ptr<UnifiedPipeline> volumeCloudCompositePipeline_; // パス 3: 拡大してシーンへ重ねる
  // 低解像度の描画先。解像度の分母を実行中に変えられるよう、シーンと同じ大きさで作り、左上の一部だけを使う
  std::unique_ptr<RenderTexture> volumeCloudDistanceLow_; // R32_FLOAT: 物体までの距離
  std::unique_ptr<RenderTexture> volumeCloudColorLow_;    // R16G16B16A16_FLOAT: 雲の光と alpha
  uint32_t sceneWidth_ = 0;  // シーンの描画先の大きさ(InitializePostProcess で決まる)
  uint32_t sceneHeight_ = 0;
  // このフレームに依頼されたボリューム雲(AddVolumeCloud の順)。描いたら空にする
  struct VolumeCloudRequest {
    VolumeCloudSettings settings;
    Vector3 sunDirection;
    float time;
  };
  std::vector<VolumeCloudRequest> volumeCloudRequests_;
  // 定数バッファは、雲 1 つにつき 1 つ(同じフレームに複数の雲を描くとき、設定が上書きされないように)
  ComPtr<ID3D12Resource> volumeCloudParamCBs_[kMaxVolumeClouds];
  VolumeCloudParam *volumeCloudParamMapped_[kMaxVolumeClouds] = {};
  VolumeCloudQuality volumeCloudQuality_;
  std::chrono::steady_clock::time_point volumeCloudLastTime_{}; // 品質の自動調整を前回更新した時刻(経過時間を渡すため)
  bool volumeCloudHasLastTime_ = false;

  // ボリューム雲の全画面パス。シーンの色へプリマルチプライドでブレンドする(依頼がなければ何もしない)
  void DrawVolumeCloudPass_();
  // 雲 1 つ分の定数バッファの中身を埋める。divisor: 解像度の分母(同じフレームの雲で共通)
  static void FillVolumeCloudParam_(VolumeCloudParam &param, const VolumeCloudSettings &settings, const Vector3 &sunDirection,
                                    float time, const Matrix4x4 &invViewProj, const Vector3 &cameraPosition,
                                    uint32_t sceneWidth, uint32_t sceneHeight, int32_t divisor);

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
  std::unique_ptr<CloudNoiseTextures> cloudNoise_; // ボリューム雲用の 3D ノイズ
  std::unique_ptr<RenderTexture> renderTexture_;
  std::unique_ptr<DepthTexture> depthTexture_;
  std::unique_ptr<RenderTexture> postProcessTexture_;
  std::unique_ptr<RenderTexture> gaussianTempTexture_;
};