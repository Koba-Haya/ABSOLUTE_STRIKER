#include "AnimationManager.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <Windows.h>
#include "../../../base/AssetPath.h"

namespace {

// Assimp のアニメーション 1 つを、エンジンの Animation に変換する。
std::shared_ptr<Animation> ConvertAnimation_(const aiAnimation* animationAssimp) {
  auto animation = std::make_shared<Animation>();
  animation->name = animationAssimp->mName.C_Str();

  float ticksPerSecond = static_cast<float>(animationAssimp->mTicksPerSecond);
  if (ticksPerSecond == 0.0f) {
    ticksPerSecond = 1.0f; // GLTF defaults to seconds
  }

  animation->duration = static_cast<float>(animationAssimp->mDuration) / ticksPerSecond;

  for (uint32_t channelIndex = 0; channelIndex < animationAssimp->mNumChannels; ++channelIndex) {
    aiNodeAnim* nodeAnimationAssimp = animationAssimp->mChannels[channelIndex];
    NodeAnimation& nodeAnimation = animation->nodeAnimations[nodeAnimationAssimp->mNodeName.C_Str()];

    for (uint32_t keyIndex = 0; keyIndex < nodeAnimationAssimp->mNumPositionKeys; ++keyIndex) {
      aiVectorKey& keyAssimp = nodeAnimationAssimp->mPositionKeys[keyIndex];
      KeyframeVector3 keyframe;
      keyframe.time = static_cast<float>(keyAssimp.mTime) / ticksPerSecond;
      keyframe.value = {keyAssimp.mValue.x, keyAssimp.mValue.y, keyAssimp.mValue.z};
      nodeAnimation.translate.keyframes.push_back(keyframe);
    }

    for (uint32_t keyIndex = 0; keyIndex < nodeAnimationAssimp->mNumRotationKeys; ++keyIndex) {
      aiQuatKey& keyAssimp = nodeAnimationAssimp->mRotationKeys[keyIndex];
      KeyframeQuaternion keyframe;
      keyframe.time = static_cast<float>(keyAssimp.mTime) / ticksPerSecond;
      keyframe.value = {keyAssimp.mValue.x, keyAssimp.mValue.y, keyAssimp.mValue.z, keyAssimp.mValue.w};
      nodeAnimation.rotate.keyframes.push_back(keyframe);
    }

    for (uint32_t keyIndex = 0; keyIndex < nodeAnimationAssimp->mNumScalingKeys; ++keyIndex) {
      aiVectorKey& keyAssimp = nodeAnimationAssimp->mScalingKeys[keyIndex];
      KeyframeVector3 keyframe;
      keyframe.time = static_cast<float>(keyAssimp.mTime) / ticksPerSecond;
      keyframe.value = {keyAssimp.mValue.x, keyAssimp.mValue.y, keyAssimp.mValue.z};
      nodeAnimation.scale.keyframes.push_back(keyframe);
    }
  }

  return animation;
}

} // namespace

std::shared_ptr<const AnimationSet> AnimationManager::LoadAnimationSet(const std::string& directoryPath, const std::string& filename) {
  const std::string filePath = directoryPath + "/" + filename;

  if (auto it = cache_.find(filePath); it != cache_.end()) {
    return it->second;
  }

  // exe 基準の実パスへ解決する(AssetLoader と同じ。日本語を含むフォルダでも開けるよう UTF-8 で渡す)
  const std::string resolvedPath = AbsoluteEngine::AssetPath::ResolveUtf8(filePath);

  Assimp::Importer importer;
  const unsigned flags = aiProcess_MakeLeftHanded | aiProcess_FlipWindingOrder;
  const aiScene* scene = importer.ReadFile(resolvedPath.c_str(), flags);
  if (!scene) {
    // MessageBoxA は ANSI で表示するため、表示用のパスは ANSI で作る(resolvedPath は UTF-8)
    const std::string displayPath = AbsoluteEngine::AssetPath::Resolve(filePath).string();
    const std::string errorMsg = "Failed to load animation:\nPath: " + displayPath + "\nError: " + importer.GetErrorString();
    MessageBoxA(nullptr, errorMsg.c_str(), "AnimationManager Error", MB_OK | MB_ICONERROR);
    return nullptr;
  }
  if (scene->mNumAnimations == 0) return nullptr;

  auto animationSet = std::make_shared<AnimationSet>();
  for (uint32_t i = 0; i < scene->mNumAnimations; ++i) {
    animationSet->animations.push_back(ConvertAnimation_(scene->mAnimations[i]));
  }

  cache_[filePath] = animationSet;
  return animationSet;
}

std::shared_ptr<Animation> AnimationManager::LoadAnimation(const std::string& directoryPath, const std::string& filename) {
  auto set = LoadAnimationSet(directoryPath, filename);
  return set ? set->First() : nullptr;
}

std::shared_ptr<Animation> AnimationManager::LoadAnimation(const std::string& directoryPath, const std::string& filename,
                                                           const std::string& animationName) {
  auto set = LoadAnimationSet(directoryPath, filename);
  return set ? set->Find(animationName) : nullptr;
}
