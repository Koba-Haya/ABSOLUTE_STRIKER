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
    float pad2 = 0.0f;
    Vector3 dissolveEdgeColor = {1.0f, 0.4f, 0.3f};
    float pad3 = 0.0f;
    Vector3 dissolveMaskColor = {1.0f, 1.0f, 1.0f};
    float pad4 = 0.0f;
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
  void SetDissolveParam(bool enable, float threshold = 0.0f, float edgeRange = 0.03f, 
                        const Vector3& edgeColor = {1.0f, 0.4f, 0.3f}, const Vector3& maskColor = {1.0f, 1.0f, 1.0f});

  void SetWireframe(bool wireframe) { isWireframe_ = wireframe; }
  bool IsWireframe() const { return isWireframe_; }

  void SetOverrideTexture(std::shared_ptr<TextureResource> tex);
  TextureResource* GetOverrideTexture() const;

  void Draw();
  void DrawSkeleton();

  void UpdateAnimation(float deltaTime);
  // blendDuration > 0 の場合、現在再生中のポーズから新しいアニメーションへ
  // blendDuration秒かけてクロスフェード（Lerp/Slerp補間）しながら遷移する。
  void PlayAnimation(std::shared_ptr<Animation> animation, bool loop = true, float blendDuration = 0.0f);
  // 再生中のアニメーションが最後まで再生されたか。ループしないアニメーションが終わったときだけ true
  // (ループ再生中や、何も再生していないときは false)。次のアニメーションへ切り替える合図に使う
  bool IsAnimationFinished() const;
  
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