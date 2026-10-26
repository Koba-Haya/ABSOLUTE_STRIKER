#include "ModelInstance.h"
#include "ModelResource.h"
#include "Renderer.h"
#include <cassert>
#include <d3d12.h>
#include <wrl/client.h>
#include "SkinCluster.h"
#include "Animation.h"
#include "ModelUtils.h"
#include "TextureResource.h"

namespace {
static constexpr UINT Align256_(UINT n) { return (n + 255u) & ~255u; }
} // namespace

struct ModelInstance::Impl {
  Microsoft::WRL::ComPtr<ID3D12Resource> cbMaterial;
  Microsoft::WRL::ComPtr<ID3D12Resource> cbTransform;
  MaterialCB *cbMatMapped = nullptr;
  TransformCB *cbTransMapped = nullptr;
  // 平面反射(鏡像)用の変換 CB。必要になるまで作らない
  Microsoft::WRL::ComPtr<ID3D12Resource> cbMirrorTransform;
  TransformCB *cbMirrorTransMapped = nullptr;
  std::shared_ptr<ModelResource> resource;
  
  std::shared_ptr<Animation> currentAnimation;
  float animationTime = 0.0f;
  bool animationLoop = true;

  // アニメーション補間（クロスフェード）用の状態。
  // 遷移開始時点のジョイントローカルポーズをスナップショットし、blendDuration秒かけて
  // 新アニメーションのポーズへLerp/Slerpでブレンドする。
  bool isBlending = false;
  float blendTimer = 0.0f;
  float blendDuration = 0.0f;
  std::vector<QuaternionTransform> blendFromPose;

  std::unique_ptr<Skeleton> skeleton;
  std::unique_ptr<SkinCluster> skinCluster;
  std::shared_ptr<TextureResource> overrideTexture;
  std::shared_ptr<TextureResource> dissolveMaskTexture; // このインスタンス専用のディゾルブのマスク(null なら共有)
};

ModelInstance::ModelInstance() : pImpl_(std::make_unique<Impl>()) {}
ModelInstance::~ModelInstance() = default;

ModelInstance::ModelInstance(ModelInstance &&) noexcept = default;
ModelInstance &ModelInstance::operator=(ModelInstance &&) noexcept = default;

bool ModelInstance::Initialize(const CreateInfo &ci) {
  auto *renderer = Renderer::GetInstance();
  assert(renderer && ci.resource);
  pImpl_->resource = ci.resource;

  world_ = MakeIdentity4x4();

  pImpl_->cbMaterial =
      renderer->CreateUploadBuffer(Align256_(sizeof(MaterialCB)));
  pImpl_->cbMaterial->Map(0, nullptr,
                          reinterpret_cast<void **>(&pImpl_->cbMatMapped));

  pImpl_->cbTransform =
      renderer->CreateUploadBuffer(Align256_(sizeof(TransformCB)));
  pImpl_->cbTransform->Map(0, nullptr,
                           reinterpret_cast<void **>(&pImpl_->cbTransMapped));

  *pImpl_->cbMatMapped = {};
  pImpl_->cbMatMapped->color = ci.baseColor;
  pImpl_->cbMatMapped->enableLighting = ci.lightingMode;
  pImpl_->cbMatMapped->specularColor = ci.specularColor;
  pImpl_->cbMatMapped->uvTransform = MakeIdentity4x4();
  pImpl_->cbMatMapped->shininess = ci.shininess;
  pImpl_->cbMatMapped->environmentCoefficient = ci.environmentCoefficient;

  pImpl_->cbMatMapped->enableDissolve = 0;
  pImpl_->cbMatMapped->dissolveThreshold = 0.0f;
  pImpl_->cbMatMapped->dissolveEdgeRange = 0.03f;
  pImpl_->cbMatMapped->dissolveEdgeColor = {1.0f, 0.4f, 0.3f};
  pImpl_->cbMatMapped->dissolveMaskColor = {1.0f, 1.0f, 1.0f};

  SetWorld(MakeIdentity4x4());

  if (pImpl_->resource->HasBones()) {
    pImpl_->skeleton = std::make_unique<Skeleton>(pImpl_->resource->GetModelData()->skeleton);
    pImpl_->skinCluster = std::make_unique<SkinCluster>();
    pImpl_->skinCluster->Initialize(renderer->GetDX(), static_cast<uint32_t>(pImpl_->skeleton->joints.size()), pImpl_->resource->GetVertexCount());
  }

  return true;
}

void ModelInstance::SetWorld(const Matrix4x4 &world) {
  world_ = world;
  if (pImpl_->cbTransMapped) {
    pImpl_->cbTransMapped->World = world_;
    pImpl_->cbTransMapped->WorldInverseTranspose = Transpose(Inverse(world_));
  }
}

void ModelInstance::SetColor(const Vector4 &c) {
  if (pImpl_->cbMatMapped)
    pImpl_->cbMatMapped->color = c;
}
void ModelInstance::SetLightingMode(int32_t m) {
  if (pImpl_->cbMatMapped)
    pImpl_->cbMatMapped->enableLighting = m;
}
void ModelInstance::SetUVTransform(const Matrix4x4 &uv) {
  if (pImpl_->cbMatMapped)
    pImpl_->cbMatMapped->uvTransform = uv;
}
void ModelInstance::SetSpecularColor(const Vector3 &c) {
  if (pImpl_->cbMatMapped)
    pImpl_->cbMatMapped->specularColor = c;
}
void ModelInstance::SetShininess(float s) {
  if (pImpl_->cbMatMapped)
    pImpl_->cbMatMapped->shininess = s;
}
void ModelInstance::SetEnvironmentCoefficient(float c) {
  if (pImpl_->cbMatMapped)
    pImpl_->cbMatMapped->environmentCoefficient = c;
}

void ModelInstance::SetDissolveParam(bool enable, float threshold, float edgeRange, 
                                     const Vector3& edgeColor, const Vector3& maskColor) {
  if (pImpl_->cbMatMapped) {
    pImpl_->cbMatMapped->enableDissolve = enable ? 1 : 0;
    pImpl_->cbMatMapped->dissolveThreshold = threshold;
    pImpl_->cbMatMapped->dissolveEdgeRange = edgeRange;
    pImpl_->cbMatMapped->dissolveEdgeColor = edgeColor;
    pImpl_->cbMatMapped->dissolveMaskColor = maskColor;
  }
}

void ModelInstance::SetOverrideTexture(std::shared_ptr<TextureResource> tex) {
  pImpl_->overrideTexture = tex;
}

void ModelInstance::SetDissolveMaskTexture(std::shared_ptr<TextureResource> tex) {
  pImpl_->dissolveMaskTexture = tex;
}

TextureResource* ModelInstance::GetDissolveMaskTexture() const {
  return pImpl_->dissolveMaskTexture.get();
}

TextureResource* ModelInstance::GetOverrideTexture() const {
  return pImpl_->overrideTexture.get();
}

void ModelInstance::Draw() {
  if (pImpl_->skinCluster) {
    Renderer::GetInstance()->DispatchSkinning(this);
  }
  Renderer::GetInstance()->DrawModel(this);
}

void ModelInstance::DrawSkeleton() {
  if (!pImpl_->skeleton) return;
  auto* renderer = Renderer::GetInstance();
  for (const auto& joint : pImpl_->skeleton->joints) {
    // 自分自身の位置 (平行移動成分を抽出)
    Vector3 start = { joint.skeletonSpaceMatrix.m[3][0], joint.skeletonSpaceMatrix.m[3][1], joint.skeletonSpaceMatrix.m[3][2] };
    start = TransformPoint(start, world_);
    
    // 子への線を描く
    for (int32_t childIndex : joint.children) {
      const auto& child = pImpl_->skeleton->joints[childIndex];
      Vector3 end = { child.skeletonSpaceMatrix.m[3][0], child.skeletonSpaceMatrix.m[3][1], child.skeletonSpaceMatrix.m[3][2] };
      end = TransformPoint(end, world_);
      
      renderer->DrawLine(start, end, {1.0f, 1.0f, 1.0f, 1.0f});
    }
  }
}

unsigned long long ModelInstance::GetMaterialCBAddress() const {
  return pImpl_->cbMaterial->GetGPUVirtualAddress();
}

unsigned long long ModelInstance::GetTransformCBAddress() const {
  return pImpl_->cbTransform->GetGPUVirtualAddress();
}

ModelResource *ModelInstance::GetResource() const {
  return pImpl_->resource.get();
}

ModelInstance::TransformCB *ModelInstance::GetTransformMapped() {
  return pImpl_->cbTransMapped;
}

ModelInstance::TransformCB *ModelInstance::GetMirrorTransformMapped() {
  if (!pImpl_->cbMirrorTransform) {
    pImpl_->cbMirrorTransform = Renderer::GetInstance()->CreateUploadBuffer(Align256_(sizeof(TransformCB)));
    pImpl_->cbMirrorTransform->Map(0, nullptr, reinterpret_cast<void **>(&pImpl_->cbMirrorTransMapped));
    *pImpl_->cbMirrorTransMapped = {};
  }
  return pImpl_->cbMirrorTransMapped;
}

unsigned long long ModelInstance::GetMirrorTransformCBAddress() {
  GetMirrorTransformMapped(); // 未作成なら作る
  return pImpl_->cbMirrorTransform->GetGPUVirtualAddress();
}

SkinCluster* ModelInstance::GetSkinCluster() const {
  return pImpl_->skinCluster.get();
}

Skeleton* ModelInstance::GetSkeleton() const {
  return pImpl_->skeleton.get();
}

Matrix4x4 ModelInstance::GetBoneWorldMatrix(const std::string &boneName) const {
  if (!pImpl_->skeleton) return world_;
  auto it = pImpl_->skeleton->jointMap.find(boneName);
  if (it == pImpl_->skeleton->jointMap.end()) return world_;
  const Joint &joint = pImpl_->skeleton->joints[it->second];
  return Multiply(joint.skeletonSpaceMatrix, world_);
}

bool ModelInstance::TryGetBoneModelMatrix(const std::string &boneName, Matrix4x4 &out) const {
  if (!pImpl_->skeleton) return false;
  auto it = pImpl_->skeleton->jointMap.find(boneName);
  if (it == pImpl_->skeleton->jointMap.end()) return false;
  out = pImpl_->skeleton->joints[it->second].skeletonSpaceMatrix;
  return true;
}

template<typename T>
T CalculateValue(const std::vector<Keyframe<T>>& keyframes, float time) {
  assert(!keyframes.empty());
  if (keyframes.size() == 1 || time <= keyframes[0].time) {
    return keyframes[0].value;
  }
  for (size_t index = 0; index < keyframes.size() - 1; ++index) {
    size_t nextIndex = index + 1;
    if (keyframes[index].time <= time && time <= keyframes[nextIndex].time) {
      float t = (time - keyframes[index].time) / (keyframes[nextIndex].time - keyframes[index].time);
      if constexpr (std::is_same_v<T, Vector3>) {
        return Lerp(keyframes[index].value, keyframes[nextIndex].value, t);
      } else if constexpr (std::is_same_v<T, Quaternion>) {
        return Slerp(keyframes[index].value, keyframes[nextIndex].value, t);
      }
    }
  }
  return (*keyframes.rbegin()).value;
}

void ModelInstance::PlayAnimation(std::shared_ptr<Animation> animation, bool loop, float blendDuration) {
  // 既に同じアニメーションを再生中なら何もしない（ブレンドの再トリガーを防ぐ）
  if (pImpl_->currentAnimation == animation) {
    pImpl_->animationLoop = loop;
    return;
  }

  if (blendDuration > 0.0f && pImpl_->currentAnimation && pImpl_->skeleton) {
    // 現在のジョイントローカルポーズをスナップショットしてブレンド元にする
    pImpl_->blendFromPose.resize(pImpl_->skeleton->joints.size());
    for (const auto& joint : pImpl_->skeleton->joints) {
      pImpl_->blendFromPose[joint.index] = joint.transform;
    }
    pImpl_->isBlending = true;
    pImpl_->blendTimer = 0.0f;
    pImpl_->blendDuration = blendDuration;
  } else {
    pImpl_->isBlending = false;
  }

  pImpl_->currentAnimation = animation;
  pImpl_->animationTime = 0.0f;
  pImpl_->animationLoop = loop;
}

bool ModelInstance::IsAnimationFinished() const {
  const auto &animation = pImpl_->currentAnimation;
  return animation && !pImpl_->animationLoop && pImpl_->animationTime >= animation->duration;
}

void ModelInstance::UpdateAnimation(float deltaTime) {
  if (!pImpl_->currentAnimation || !pImpl_->skeleton) return;

  pImpl_->animationTime += deltaTime;
  if (pImpl_->animationLoop) {
    pImpl_->animationTime = std::fmod(pImpl_->animationTime, pImpl_->currentAnimation->duration);
  } else {
    pImpl_->animationTime = std::min(pImpl_->animationTime, pImpl_->currentAnimation->duration);
  }

  // ブレンド係数 t（0=旧ポーズ, 1=新アニメーションのみ）。t(B) + (1-t)(A) の式に対応する。
  float t = 1.0f;
  if (pImpl_->isBlending) {
    pImpl_->blendTimer += deltaTime;
    t = std::min(pImpl_->blendTimer / pImpl_->blendDuration, 1.0f);
  }

  for (auto& joint : pImpl_->skeleton->joints) {
    QuaternionTransform target = joint.transform; // カーブが無いジョイントは現在のポーズを保持
    auto it = pImpl_->currentAnimation->nodeAnimations.find(joint.name);
    if (it != pImpl_->currentAnimation->nodeAnimations.end()) {
      const auto& nodeAnim = it->second;
      if (!nodeAnim.translate.keyframes.empty()) target.translate = CalculateValue(nodeAnim.translate.keyframes, pImpl_->animationTime);
      if (!nodeAnim.rotate.keyframes.empty()) target.rotate = CalculateValue(nodeAnim.rotate.keyframes, pImpl_->animationTime);
      if (!nodeAnim.scale.keyframes.empty()) target.scale = CalculateValue(nodeAnim.scale.keyframes, pImpl_->animationTime);
    }

    if (pImpl_->isBlending && static_cast<size_t>(joint.index) < pImpl_->blendFromPose.size()) {
      const QuaternionTransform& from = pImpl_->blendFromPose[joint.index];
      joint.transform.translate = Lerp(from.translate, target.translate, t);
      joint.transform.rotate = Slerp(from.rotate, target.rotate, t);
      joint.transform.scale = Lerp(from.scale, target.scale, t);
    } else {
      joint.transform = target;
    }
  }

  if (pImpl_->isBlending && t >= 1.0f) {
    pImpl_->isBlending = false;
  }

  UpdateSkeleton(*pImpl_->skeleton);

  if (pImpl_->skinCluster) {
    pImpl_->skinCluster->Update(*pImpl_->skeleton);
  }
}