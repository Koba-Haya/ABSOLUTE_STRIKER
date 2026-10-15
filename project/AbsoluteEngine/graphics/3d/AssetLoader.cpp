#include "AssetLoader.h"
#include <algorithm>
#include <cassert>
#include <filesystem>
#include <Windows.h>
#include "../../base/AssetPath.h"
static std::string ToLower_(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return (char)std::tolower(c); });
  return s;
}

static std::string GetExtLower_(const std::string &filename) {
  auto pos = filename.find_last_of('.');
  if (pos == std::string::npos)
    return {};
  return ToLower_(filename.substr(pos + 1));
}

// exe 基準の実パスに解決する。ビルドしたPCのフォルダへはフォールバックしない。
// Assimp は char* を UTF-8 として扱うため、UTF-8 で返す(日本語を含むフォルダでも開けるように)。
// 存在しなくてもそのまま返し、呼び出し側(Assimp)のエラーに実パスが表示されるようにする
static std::string ResolvePath_(const std::string& path) {
  return AbsoluteEngine::AssetPath::ResolveUtf8(path);
}

std::shared_ptr<const ModelData>
AssetLoader::LoadModel(const std::string &directoryPath,
                       const std::string &filename) {
  const std::string key = MakeKey_(directoryPath, filename);
  if (auto it = cache_.find(key); it != cache_.end()) {
    return it->second;
  }

  Assimp::Importer importer;
  const std::string filePath = directoryPath + "/" + filename;
  const std::string resolvedPath = ResolvePath_(filePath);

  const unsigned flags = aiProcess_Triangulate | aiProcess_MakeLeftHanded |
                         aiProcess_FlipWindingOrder |
                         aiProcess_GenSmoothNormals |
                         aiProcess_JoinIdenticalVertices;

  const aiScene *scene = importer.ReadFile(resolvedPath.c_str(), flags);
  if (!scene || !scene->mRootNode) {
    // MessageBoxA は ANSI で表示するため、表示用のパスは ANSI で作る(resolvedPath は UTF-8)
    const std::string displayPath = AbsoluteEngine::AssetPath::Resolve(filePath).string();
    std::string errorMsg = "Failed to load model:\nPath: " + displayPath + "\nError: " + importer.GetErrorString();
    MessageBoxA(nullptr, errorMsg.c_str(), "AssetLoader Error", MB_OK | MB_ICONERROR);
    return nullptr;
  }

  const std::string ext = GetExtLower_(filename);

  UVFixupOptions uvOpt{};
  // このエンジンでは一律でV反転が必要
  uvOpt.flipV = true;
  uvOpt.flipU = false;

  auto modelData = std::make_shared<ModelData>();
  std::map<uint32_t, int32_t> meshToJointMap;

  // Build Skeleton from ALL nodes first
  BuildSkeleton_(scene->mRootNode, modelData->skeleton, -1, meshToJointMap);

  // Find root joint
  if (!modelData->skeleton.joints.empty()) {
    modelData->skeleton.rootJointIndex = 0;
  }

  // Materials
  modelData->materials.resize(scene->mNumMaterials);
  for (uint32_t i = 0; i < scene->mNumMaterials; ++i) {
    aiMaterial *mat = scene->mMaterials[i];
    MaterialData md{};

    if (mat && mat->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
      aiString tex;
      mat->GetTexture(aiTextureType_DIFFUSE, 0, &tex);
      md.textureFilePath = directoryPath + "/" + tex.C_Str();
    }
    modelData->materials[i] = std::move(md);
  }

  // モデル全体にボーン（スキニングデータ）が存在するかを事前チェック
  bool modelHasBones = false;
  for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
    if (scene->mMeshes[meshIndex]->HasBones()) {
      modelHasBones = true;
      break;
    }
  }

  // Meshes
  modelData->meshes.resize(scene->mNumMeshes);

  for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
    const aiMesh *mesh = scene->mMeshes[meshIndex];
    assert(mesh);

    MeshData meshData;
    meshData.materialIndex = (mesh->mMaterialIndex < scene->mNumMaterials)
                                 ? static_cast<int>(mesh->mMaterialIndex)
                                 : -1;

    std::map<uint32_t, std::vector<std::pair<int32_t, float>>> vertexWeightMap;

    bool hasBones = mesh->HasBones();
    if (hasBones) {
      for (uint32_t i = 0; i < mesh->mNumBones; ++i) {
        aiBone* bone = mesh->mBones[i];
        std::string boneName = bone->mName.C_Str();
        
        if (modelData->skeleton.jointMap.count(boneName)) {
          int32_t jointIndex = modelData->skeleton.jointMap[boneName];
          modelData->skeleton.joints[jointIndex].inverseBindPoseMatrix = ConvertAssimpMatrix(bone->mOffsetMatrix);

          for (uint32_t j = 0; j < bone->mNumWeights; ++j) {
            const aiVertexWeight& weight = bone->mWeights[j];
            vertexWeightMap[weight.mVertexId].push_back({jointIndex, weight.mWeight});
          }
        }
      }
    }

    // Default joint for node-based animation (if mesh has no bones)
    int32_t defaultJointIndex = 0;
    if (meshToJointMap.count(meshIndex)) {
      defaultJointIndex = meshToJointMap[meshIndex];
    }

    // Load Vertices
    meshData.vertices.reserve(mesh->mNumVertices);
    if (modelHasBones) {
      meshData.skinningData.reserve(mesh->mNumVertices);
    }
    for (uint32_t vi = 0; vi < mesh->mNumVertices; ++vi) {
      const aiVector3D &p = mesh->mVertices[vi];
      const aiVector3D &n =
          mesh->HasNormals() ? mesh->mNormals[vi] : aiVector3D(0, 1, 0);
      const aiVector3D &uv = (mesh->HasTextureCoords(0))
                                 ? mesh->mTextureCoords[0][vi]
                                 : aiVector3D(0, 0, 0);

      VertexData v{};
      v.position = {p.x, p.y, p.z, 1.0f};
      v.normal = {n.x, n.y, n.z};
      v.texcoord = {uv.x, uv.y};
      meshData.vertices.push_back(FixupVertex_AssimpToEngine(v, uvOpt));

      if (modelHasBones) {
        // Skinning data
        VertexBoneData vbd{};
        auto it = vertexWeightMap.find(vi);
        if (it != vertexWeightMap.end()) {
          auto &weights = it->second;
          std::sort(weights.begin(), weights.end(),
                    [](const auto &a, const auto &b) {
                      return a.second > b.second;
                    });
          float totalWeight = 0.0f;
          for (size_t k = 0; k < 4 && k < weights.size(); ++k) {
            vbd.boneIDs[k] = weights[k].first;
            vbd.weights[k] = weights[k].second;
            totalWeight += weights[k].second;
          }
          if (totalWeight > 0.0f) {
            for (size_t k = 0; k < 4; ++k) {
              vbd.weights[k] /= totalWeight;
            }
          }
        } else {
          vbd.boneIDs[0] = defaultJointIndex;
          vbd.weights[0] = 1.0f;
        }
        meshData.skinningData.push_back(vbd);
      }
    }

    // Load Indices
    meshData.indices.reserve(mesh->mNumFaces * 3);
    for (uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex) {
      const aiFace &face = mesh->mFaces[faceIndex];
      if (face.mNumIndices == 3) {
        for (uint32_t e = 0; e < 3; ++e) {
          meshData.indices.push_back(face.mIndices[e]);
        }
      }
    }

    modelData->meshes[meshIndex] = std::move(meshData);
  }

  modelData->rootNode = ReadNode_(scene->mRootNode);

  cache_[key] = modelData;
  return modelData;
}

std::shared_ptr<const ModelData>
AssetLoader::LoadModel(const std::string &path) {
  std::filesystem::path p(path);
  const std::string dir = p.parent_path().string();
  const std::string file = p.filename().string();
  return LoadModel(dir, file);
}

Node AssetLoader::ReadNode_(const aiNode *node) {
  Node result{};

  aiVector3D scale, translate;
  aiQuaternion rotate;
  node->mTransformation.Decompose(scale, rotate, translate);

  result.transform.scale = {scale.x, scale.y, scale.z};
  result.transform.rotate = {rotate.x, -rotate.y, -rotate.z, rotate.w};
  result.transform.translate = {-translate.x, translate.y, translate.z};

  result.localMatrix = MakeAffineMatrix(result.transform.scale,
                                        result.transform.rotate,
                                        result.transform.translate);
  result.name = node->mName.C_Str();

  result.meshIndices.reserve(node->mNumMeshes);
  for (uint32_t i = 0; i < node->mNumMeshes; ++i) {
    result.meshIndices.push_back(node->mMeshes[i]);
  }

  result.children.reserve(node->mNumChildren);
  for (uint32_t i = 0; i < node->mNumChildren; ++i) {
    result.children.push_back(ReadNode_(node->mChildren[i]));
  }

  return result;
}

void AssetLoader::BuildSkeleton_(
    const aiNode *node, Skeleton &skeleton, int32_t parentJointIndex,
    std::map<uint32_t, int32_t> &meshToJointMap) {
  std::string name = node->mName.C_Str();

  int32_t currentIndex = static_cast<int32_t>(skeleton.joints.size());
  skeleton.jointMap[name] = currentIndex;

  Joint joint;
  joint.name = name;
  joint.index = currentIndex;

  aiVector3D scale, translate;
  aiQuaternion rotate;
  node->mTransformation.Decompose(scale, rotate, translate);

  joint.transform.scale = {scale.x, scale.y, scale.z};
  joint.transform.rotate = {rotate.x, -rotate.y, -rotate.z, rotate.w};
  joint.transform.translate = {-translate.x, translate.y, translate.z};

  joint.localMatrix = MakeAffineMatrix(joint.transform.scale,
                                       joint.transform.rotate,
                                       joint.transform.translate);
  joint.skeletonSpaceMatrix = MakeIdentity4x4();
  joint.inverseBindPoseMatrix = MakeIdentity4x4();

  skeleton.joints.push_back(joint);

  if (parentJointIndex != -1) {
    skeleton.joints[parentJointIndex].children.push_back(currentIndex);
  }

  // Register meshes attached to this node
  for (uint32_t i = 0; i < node->mNumMeshes; ++i) {
    meshToJointMap[node->mMeshes[i]] = currentIndex;
  }

  for (uint32_t i = 0; i < node->mNumChildren; ++i) {
    BuildSkeleton_(node->mChildren[i], skeleton, currentIndex, meshToJointMap);
  }
}
