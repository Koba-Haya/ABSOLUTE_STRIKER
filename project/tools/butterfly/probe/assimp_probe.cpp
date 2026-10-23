// 蝶の glTF を、エンジンの AssetLoader と同じフラグで Assimp に読ませて、中身を表示する確認用ツール(ゲーム本体には含めない)。
// 使い方: assimp_probe.exe <gltfのパス>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <cstdio>
#include <map>
#include <vector>
#include <algorithm>
#include <functional>

int main(int argc, char** argv) {
  if (argc < 2) return 1;
  Assimp::Importer importer;
  // AssetLoader::LoadModel と同じフラグ
  const unsigned flags = aiProcess_Triangulate | aiProcess_MakeLeftHanded | aiProcess_FlipWindingOrder |
                         aiProcess_GenSmoothNormals | aiProcess_JoinIdenticalVertices;
  const aiScene* scene = importer.ReadFile(argv[1], flags);
  if (!scene) { std::printf("LOAD FAILED: %s\n", importer.GetErrorString()); return 2; }
  std::printf("meshes=%u materials=%u animations=%u\n", scene->mNumMeshes, scene->mNumMaterials, scene->mNumAnimations);
  for (unsigned m = 0; m < scene->mNumMaterials; ++m) {
    aiString tex;
    unsigned n = scene->mMaterials[m]->GetTextureCount(aiTextureType_DIFFUSE);
    if (n) scene->mMaterials[m]->GetTexture(aiTextureType_DIFFUSE, 0, &tex);
    std::printf("material %u: diffuse textures=%u path=%s\n", m, n, n ? tex.C_Str() : "(none)");
  }
  for (unsigned i = 0; i < scene->mNumMeshes; ++i) {
    const aiMesh* mesh = scene->mMeshes[i];
    std::map<unsigned, int> influence;
    for (unsigned b = 0; b < mesh->mNumBones; ++b)
      for (unsigned w = 0; w < mesh->mBones[b]->mNumWeights; ++w) influence[mesh->mBones[b]->mWeights[w].mVertexId]++;
    int maxInf = 0;
    for (auto& kv : influence) if (kv.second > maxInf) maxInf = kv.second;
    std::printf("mesh %u '%s': verts=%u faces=%u bones=%u material=%u uv0=%d weightedVerts=%zu maxInfluences=%d\n",
                i, mesh->mName.C_Str(), mesh->mNumVertices, mesh->mNumFaces, mesh->mNumBones, mesh->mMaterialIndex,
                mesh->HasTextureCoords(0), influence.size(), maxInf);
    // 頂点の範囲(MakeLeftHanded 適用後 = エンジンが見る座標)。頭が + Z 側か - Z 側かを確かめる
    aiVector3D lo = mesh->mVertices[0], hi = mesh->mVertices[0];
    for (unsigned v = 0; v < mesh->mNumVertices; ++v) {
      const aiVector3D& p = mesh->mVertices[v];
      lo.x = std::min(lo.x, p.x); lo.y = std::min(lo.y, p.y); lo.z = std::min(lo.z, p.z);
      hi.x = std::max(hi.x, p.x); hi.y = std::max(hi.y, p.y); hi.z = std::max(hi.z, p.z);
    }
    std::printf("  bounds (engine coords): x[%.3f, %.3f] y[%.3f, %.3f] z[%.3f, %.3f]\n", lo.x, hi.x, lo.y, hi.y, lo.z, hi.z);
    for (unsigned b = 0; b < mesh->mNumBones; ++b) {
      std::string n = mesh->mBones[b]->mName.C_Str();
      if (n == "head" || n == "muzzle" || n == "abdomen_tip" || n == "wing_tip_R") {
        const aiMatrix4x4 inv = mesh->mBones[b]->mOffsetMatrix;  // 逆バインド行列。平行移動 = -(ボーンの位置)
        std::printf("  inverse bind translation of %s: (%.3f, %.3f, %.3f)\n", n.c_str(), inv.a4, inv.b4, inv.c4);
      }
    }
    // AssetLoader と同じく、頂点ごとに重みの大きい順の上位 4 本を取り、合計がどうなるかを調べる
    std::map<unsigned, std::vector<float>> perVertex;
    float placeholderMax = 0.0f;
    for (unsigned b = 0; b < mesh->mNumBones; ++b)
      for (unsigned w = 0; w < mesh->mBones[b]->mNumWeights; ++w) {
        perVertex[mesh->mBones[b]->mWeights[w].mVertexId].push_back(mesh->mBones[b]->mWeights[w].mWeight);
        if (mesh->mBones[b]->mNumWeights == 1) placeholderMax = std::max(placeholderMax, mesh->mBones[b]->mWeights[w].mWeight);
      }
    float minTop4 = 1e9f; int under = 0;
    for (auto& kv : perVertex) {
      std::sort(kv.second.begin(), kv.second.end(), std::greater<float>());
      float sum = 0; for (size_t k = 0; k < 4 && k < kv.second.size(); ++k) sum += kv.second[k];
      minTop4 = std::min(minTop4, sum); if (sum < 0.99f) ++under;
    }
    std::printf("  top4 weight sum: min=%.4f verts(sum<0.99)=%d, single-weight bone entries max weight=%.4f\n", minTop4, under, placeholderMax);
    for (unsigned b = 0; b < mesh->mNumBones; ++b) std::printf("  bone %s (%u weights)\n", mesh->mBones[b]->mName.C_Str(), mesh->mBones[b]->mNumWeights);
  }
  for (unsigned a = 0; a < scene->mNumAnimations; ++a) {
    const aiAnimation* an = scene->mAnimations[a];
    std::printf("animation %u '%s': duration=%.3f ticksPerSecond=%.3f channels=%u\n", a, an->mName.C_Str(), an->mDuration, an->mTicksPerSecond, an->mNumChannels);
  }
  return 0;
}
