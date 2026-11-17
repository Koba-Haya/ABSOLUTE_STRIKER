#pragma once
#include "BlendMode.h"
#include "Method.h"
#include <memory>
#include <string>

// 前方宣言
class ModelResource;
struct Animation;
struct Skeleton;
struct SkinCluster;
class TextureResource;

// 照明モード(ModelInstance::CreateInfo::lightingMode / SetLightingMode。Object3d.PS.hlsl の enableLighting と対応)
namespace ModelLightingMode {
constexpr int32_t kUnlit = 0;        // 照明なし(テクスチャの色をそのまま出す)
constexpr int32_t kLambert = 1;      // Lambert(光が当たらない側は真っ黒)
constexpr int32_t kHalfLambert = 2;  // Half-Lambert
constexpr int32_t kThinSurface = 3;  // 薄い面(Half-Lambert + 環境光。蝶の翅など、両面が見えるもの)
// 光る球の殻(爆破の球など)。照明は使わず、視線に斜めの面(縁)ほど濃く、正対する面(中心)ほど透ける。裏面も同じ扱い(abs)。
// 半透明なので、アルファ合成のエフェクト用パイプライン(Renderer::DrawEffectModel)で描くこと。
// 色の指定: 縁の色 = SetColor(アルファは全体の濃さ)、中心の色 = SetSpecularColor、縁から中心へ変わる鋭さ = SetShininess(rim = (1 - 正対の度合い) ^ この値。大きいほど、縁の細い輪だけが濃くなり、中が透ける)
constexpr int32_t kRimGlow = 4;
// 爆散のプラズマ球。照明は使わず、中心は白く飛び、縁は縁の色へ変わり、球面を流れる斑が縁の色へ寄る。不透明に近い球(半透明にもできる)。
// 色の指定: 縁の色 = SetColor(アルファは全体の濃さ)、縁の鋭さ = SetShininess(rim = (1 - 正対の度合い) ^ この値)、
//           斑の量 = SetEnvironmentCoefficient(0 〜 1。0 で斑なし)、ノイズが流れる位相(秒) = SetSpecularColor の x
// 半透明で重ねるので、エフェクト用パイプライン(Renderer::DrawEffectModel)で描くこと。
constexpr int32_t kPlasma = 5;
// ふわっとした光の輪。球の中心ほど濃く、輪郭へ向けてなめらかに透ける(プラズマ球より大きい球に使うと、外側へ光がにじむ)。
// 色の指定: 色 = SetColor(アルファは最大の濃さ)、中心から輪郭への落ち方 = SetShininess(濃さ = 正対の度合い ^ この値。大きいほど、中心だけが濃い)
constexpr int32_t kSoftGlow = 6;
} // namespace ModelLightingMode

class ModelInstance {
public:
  struct CreateInfo {
    std::shared_ptr<ModelResource> resource;
    Vector4 baseColor = {1, 1, 1, 1};
    int32_t lightingMode = 1;
    Vector3 specularColor = {1.0f, 1.0f, 1.0f};
    float shininess = 32.0f;
    float environmentCoefficient = 0.0f; // デフォルトを 0.0f (OFF) に
  };

  // HLSL 定数バッファ構造体 (Renderer が書き込むために公開)
  struct MaterialCB {
    Vector4 color{1, 1, 1, 1};
    int32_t enableLighting = 1;
    Vector3 specularColor{1.0f, 1.0f, 1.0f};
    Matrix4x4 uvTransform = MakeIdentity4x4();
    float shininess = 32.0f;
    float environmentCoefficient = 0.0f;
    float pad[2] = {0, 0};
    
    // --- Dissolve ---
    int32_t enableDissolve = 0;
    float dissolveThreshold = 0.0f;
    float dissolveEdgeRange = 0.03f;
    float dissolveEmber = 0.0f; // 縁を熾火で光らせる強さ(0 で従来どおり。HLSL の dissolveEmber と同じ位置)
    Vector3 dissolveEdgeColor = {1.0f, 0.4f, 0.3f};
    float pad3 = 0.0f;
    Vector3 dissolveMaskColor = {1.0f, 1.0f, 1.0f};
    float pad4 = 0.0f;

    // --- Crack (ボロノイのひび割れ。HLSL の同名の項目と、同じ並び・同じ位置にする) ---
    int32_t enableCrack = 0;
    float crackCellScale = 0.5f;
    float crackLineWidth = 0.1f;
    float crackReveal = 0.0f;
    Vector3 crackColor = {1.0f, 0.78f, 0.30f};
    float crackIntensity = 1.0f;
    Vector3 crackOrigin = {0.0f, 0.0f, 0.0f};
    float crackSpreadRadius = 1.0f;
    float crackBreak = 0.0f;
    float crackPad[3] = {0.0f, 0.0f, 0.0f};
  };

  // ボロノイのひび割れの設定(SetCrackParam に渡す)
  struct CrackParam {
    bool enable = false;
    float cellSize = 2.0f;      // セルの大きさ(モデル本来の単位)
    float lineWidth = 0.1f;     // ひびの線の太さの最大値(セルの大きさに対する割合)
    float reveal = 0.0f;        // ひびが広がった割合(0 〜 1)
    Vector3 color = {1.0f, 0.78f, 0.30f};
    float intensity = 1.0f;     // ひびの光の強さ
    Vector3 origin = {0.0f, 0.0f, 0.0f}; // ひびが広がり始める位置(モデル本来の座標)
    float spreadRadius = 1.0f;  // origin から、ひびが最後に届く所までの距離
    float breakAmount = 0.0f;   // セルが消えた割合(0 〜 1)
  };

  struct TransformCB {
    Matrix4x4 WVP = MakeIdentity4x4();
    Matrix4x4 World = MakeIdentity4x4();
    Matrix4x4 WorldInverseTranspose = MakeIdentity4x4();
  };

  ModelInstance();
  ~ModelInstance();

  ModelInstance(const ModelInstance &) = delete;
  ModelInstance &operator=(const ModelInstance &) = delete;

  ModelInstance(ModelInstance &&) noexcept;
  ModelInstance &operator=(ModelInstance &&) noexcept;

  bool Initialize(const CreateInfo &ci);

  void SetWorld(const Matrix4x4 &world);
  const Matrix4x4 &GetWorld() const { return world_; }

  void SetColor(const Vector4 &c);
  void SetLightingMode(int32_t m);
  void SetUVTransform(const Matrix4x4 &uv);
  void SetSpecularColor(const Vector3 &c);
  void SetShininess(float s);
  void SetEnvironmentCoefficient(float c);
  // emberAmount: 縁を熾火(外縁は灰、内側へ赤、橙)で光らせる強さ(0 〜 1。0 は縁の色へ混ぜるだけ)
  void SetDissolveParam(bool enable, float threshold = 0.0f, float edgeRange = 0.03f,
                        const Vector3& edgeColor = {1.0f, 0.4f, 0.3f}, const Vector3& maskColor = {1.0f, 1.0f, 1.0f},
                        float emberAmount = 0.0f);
  // ボロノイのひび割れを設定する(enable が false なら、何も描かない)。cellSize が 0 以下のときは、無効として扱う
  void SetCrackParam(const CrackParam &param);

  void SetWireframe(bool wireframe) { isWireframe_ = wireframe; }
  bool IsWireframe() const { return isWireframe_; }

  void SetOverrideTexture(std::shared_ptr<TextureResource> tex);
  TextureResource* GetOverrideTexture() const;

  // このインスタンスだけが使うディゾルブのマスク(null なら、Renderer が共有で持つマスクを使う)。
  // マスクはモデルの UV で引かれるので、モデル専用の朽ち方(例: 翅の外側から内側へ)を、専用のマスクで作れる
  void SetDissolveMaskTexture(std::shared_ptr<TextureResource> tex);
  TextureResource* GetDissolveMaskTexture() const;

  void Draw();
  void DrawSkeleton();

  void UpdateAnimation(float deltaTime);
  // blendDuration > 0 の場合、現在再生中のポーズから新しいアニメーションへ
  // blendDuration秒かけてクロスフェード（Lerp/Slerp補間）しながら遷移する。
  // restart: true なら、すでに同じアニメーションを再生中でも、いまのポーズから頭へ戻して再生し直す(既定は false で、何もしない)
  void PlayAnimation(std::shared_ptr<Animation> animation, bool loop = true, float blendDuration = 0.0f, bool restart = false);
  // 再生中のアニメーションが最後まで再生されたか。ループしないアニメーションが終わったときだけ true
  // (ループ再生中や、何も再生していないときは false)。次のアニメーションへ切り替える合図に使う
  bool IsAnimationFinished() const;
  // 再生中のアニメーションの現在時刻(秒)を直接指定して、その時刻のポーズにする(タイムラインが、時刻を毎フレーム決めるときに使う。
  // UpdateAnimation は、時間を足して進める)。ループしないアニメは、長さで止まる
  void SetAnimationTime(float seconds);
  
  SkinCluster* GetSkinCluster() const;
  Skeleton* GetSkeleton() const;

  // 指定したボーン名の現在のワールド行列を取得する。ボーンが存在しない場合はモデルのワールド行列を返す。
  Matrix4x4 GetBoneWorldMatrix(const std::string &boneName) const;

  // 指定したボーンの、モデル空間(ワールド行列を掛ける前)の行列を取得する。ボーンが存在しなければ false。
  // GetBoneWorldMatrix は「最後に SetWorld した行列」を使うので 1 フレーム遅れるが、
  // こちらは、呼び出し側が「今の」ワールド行列を掛けて使える(ModelComponent::TryGetBoneWorldPosition)
  bool TryGetBoneModelMatrix(const std::string &boneName, Matrix4x4 &out) const;

  unsigned long long GetMaterialCBAddress() const;
  unsigned long long GetTransformCBAddress() const;
  ModelResource *GetResource() const;

  TransformCB *GetTransformMapped();

  // 平面反射(鏡像)の描画専用の変換 CB。
  // 変換 CB は CPU が書き込んだ内容を GPU が実行時に読むので、同じ CB を 1 フレームで 2 回の描画(通常と鏡像)に使うと
  // 両方とも後から書いた内容で描かれてしまう。鏡像側は別の CB に書いて、これを避ける。
  // 初めて呼ばれたときに作る(反射を使わないモデルは確保しない)
  TransformCB *GetMirrorTransformMapped();
  unsigned long long GetMirrorTransformCBAddress();

private:
  struct Impl;
  std::unique_ptr<Impl> pImpl_;

  Matrix4x4 world_ = MakeIdentity4x4();
  bool isWireframe_ = false;
};