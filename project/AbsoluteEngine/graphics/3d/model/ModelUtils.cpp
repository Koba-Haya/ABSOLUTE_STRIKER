#include "ModelUtils.h"
#include <utility>
#include <optional>

Matrix4x4 ConvertAssimpMatrix(const aiMatrix4x4 &a) {
  Matrix4x4 m{};
  // Assimp is row-major with pre-multiplication (translation in last column).
  // Engine is row-major with post-multiplication (translation in last row).
  // Thus we need to transpose.
  m.m[0][0] = a[0][0];
  m.m[0][1] = a[1][0];
  m.m[0][2] = a[2][0];
  m.m[0][3] = a[3][0];
  m.m[1][0] = a[0][1];
  m.m[1][1] = a[1][1];
  m.m[1][2] = a[2][1];
  m.m[1][3] = a[3][1];
  m.m[2][0] = a[0][2];
  m.m[2][1] = a[1][2];
  m.m[2][2] = a[2][2];
  m.m[2][3] = a[3][2];
  m.m[3][0] = a[0][3];
  m.m[3][1] = a[1][3];
  m.m[3][2] = a[2][3];
  m.m[3][3] = a[3][3];
  return m;
}

Matrix4x4 ConvertAssimpMatrixTransposed(const aiMatrix4x4 &a) {
  // We already transpose in ConvertAssimpMatrix, so this would be "original" Assimp layout.
  // But usually we just need ConvertAssimpMatrix to work correctly.
  return Transpose(ConvertAssimpMatrix(a));
}

VertexData FixupVertex_AssimpToEngine(const VertexData &v,
                                      const UVFixupOptions &opt) {
  VertexData r = v;

  if (opt.flipU) {
    r.texcoord.x = 1.0f - r.texcoord.x;
  }
  if (opt.flipV) {
    r.texcoord.y = 1.0f - r.texcoord.y;
  }

  return r;
}

void FlipTriangleWinding(VertexData &a, VertexData &b, VertexData &c) {
  std::swap(b, c);
}

std::vector<VertexData> FlattenVertices(const ModelData &model) {
  std::vector<VertexData> out;
  size_t total = 0;
  for (const auto &m : model.meshes) {
    total += m.vertices.size();
  }
  out.reserve(total);

  for (const auto &m : model.meshes) {
    out.insert(out.end(), m.vertices.begin(), m.vertices.end());
  }

  // pad に、モデル本来の頂点位置(スキニング前)を入れておく。
  // スキニングの計算シェーダーは、pad をそのまま出力へ渡すので、描画側(Object3D.VS)は、動いても表面の同じ点は同じ値になる
  // 「元の位置」を読める(撃破演出のひび割れの柄が、体の動きに付いてくるために使う)
  for (VertexData &vertex : out) {
    vertex.pad[0] = vertex.position.x;
    vertex.pad[1] = vertex.position.y;
    vertex.pad[2] = vertex.position.z;
  }
  return out;
}

std::vector<uint32_t> FlattenIndices(const ModelData &model) {
  std::vector<uint32_t> out;
  size_t totalIndices = 0;
  for (const auto &m : model.meshes) {
    totalIndices += m.indices.size();
  }
  out.reserve(totalIndices);

  uint32_t vertexOffset = 0;
  for (const auto &m : model.meshes) {
    for (uint32_t index : m.indices) {
      out.push_back(index + vertexOffset);
    }
    vertexOffset += static_cast<uint32_t>(m.vertices.size());
  }
  return out;
}

std::vector<VertexBoneData> FlattenSkinningData(const ModelData &model) {
  std::vector<VertexBoneData> out;
  size_t total = 0;
  for (const auto &m : model.meshes) {
    total += m.skinningData.size();
  }
  
  if (total == 0) return out;
  
  out.reserve(total);
  for (const auto &m : model.meshes) {
    out.insert(out.end(), m.skinningData.begin(), m.skinningData.end());
  }
  return out;
}

std::vector<SubMeshRange> ComputeSubMeshRanges(const ModelData &model) {
  std::vector<SubMeshRange> out;
  out.reserve(model.meshes.size());

  uint32_t indexStart = 0;
  for (const auto &m : model.meshes) {
    SubMeshRange range;
    range.indexStart = indexStart;
    range.indexCount = static_cast<uint32_t>(m.indices.size());
    range.materialIndex = m.materialIndex;
    out.push_back(range);
    indexStart += range.indexCount;
  }
  return out;
}

std::string PickDiffuseTexturePath(const ModelData &model) {
  for (const auto &mat : model.materials) {
    if (!mat.textureFilePath.empty()) {
      return mat.textureFilePath;
    }
  }
  return {};
}

static int32_t CreateJoint(const Node& node, const std::optional<int32_t>& parent, std::vector<Joint>& joints) {
  Joint joint;
  joint.name = node.name;
  joint.transform = node.transform;
  joint.localMatrix = node.localMatrix;
  joint.skeletonSpaceMatrix = MakeIdentity4x4();
  joint.index = static_cast<int32_t>(joints.size());
  joint.inverseBindPoseMatrix = MakeIdentity4x4();
  
  joints.push_back(joint);
  int32_t currentIndex = joint.index;
  
  for (const auto& childNode : node.children) {
    int32_t childIndex = CreateJoint(childNode, currentIndex, joints);
    joints[currentIndex].children.push_back(childIndex);
  }
  
  return currentIndex;
}

Skeleton CreateSkeleton(const Node& rootNode) {
  Skeleton skeleton;
  skeleton.rootJointIndex = CreateJoint(rootNode, std::nullopt, skeleton.joints);
  for (const auto& joint : skeleton.joints) {
    skeleton.jointMap[joint.name] = joint.index;
  }
  return skeleton;
}

void UpdateSkeleton(Skeleton& skeleton) {
  auto updateJoint = [&](auto& self, int32_t jointIndex, const Matrix4x4& parentMatrix) -> void {
    Joint& joint = skeleton.joints[jointIndex];
    joint.localMatrix = MakeAffineMatrix(joint.transform.scale, joint.transform.rotate, joint.transform.translate);
    
    if (jointIndex == skeleton.rootJointIndex) {
      joint.skeletonSpaceMatrix = joint.localMatrix;
    } else {
      joint.skeletonSpaceMatrix = Multiply(joint.localMatrix, parentMatrix);
    }
    
    for (int32_t childIndex : joint.children) {
      self(self, childIndex, joint.skeletonSpaceMatrix);
    }
  };
  
  if (skeleton.rootJointIndex != -1) {
    updateJoint(updateJoint, skeleton.rootJointIndex, MakeIdentity4x4());
  }
}
