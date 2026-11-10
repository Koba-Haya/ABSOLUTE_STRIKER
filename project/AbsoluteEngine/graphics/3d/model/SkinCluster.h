#pragma once

#include "Matrix.h"
#include <wrl/client.h>
#include <d3d12.h>
#include <vector>

class DirectXCommon;
struct Skeleton;

struct WellForGPU {
  Matrix4x4 skeletonSpaceMatrix;
  Matrix4x4 skeletonSpaceInverseTransposeMatrix;
};

struct SkinCluster {
  Microsoft::WRL::ComPtr<ID3D12Resource> paletteResource;
  WellForGPU* mappedPalette = nullptr;
  uint32_t srvIndex = 0;

  // CS の定数バッファ(b0: 頂点数)。インスタンスごとに持つ。
  // 理由: 共有の 1 個を使うと、1 フレームに複数のスキンモデル(蝶、蜂、鳥など)を処理したとき、
  //       GPU が実行時に読む値が「最後に書いた頂点数」になり、先に処理したモデルの後半の頂点が計算されない。
  Microsoft::WRL::ComPtr<ID3D12Resource> infoResource;
  // CS出力用
  Microsoft::WRL::ComPtr<ID3D12Resource> skinnedVertexBuffer;
  uint32_t uavIndex = 0;
  D3D12_VERTEX_BUFFER_VIEW vbView{};

  bool Initialize(DirectXCommon* dx, uint32_t jointCount, uint32_t vertexCount);
  void Update(const Skeleton& skeleton);
};
