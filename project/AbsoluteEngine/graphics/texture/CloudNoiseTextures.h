#pragma once
#include "TextureResource.h"

class DirectXCommon;

// ボリューム雲が共通で使う 3D ノイズテクスチャ 2 枚(基本の形・細かい模様)を持つクラス。
// 起動時に 1 回だけ CPU で生成して GPU へ送り、以降はシェーダーが SRV(読み取り用の窓口)から参照する。
// 生成の中身(ノイズの作り方)は CloudNoiseGenerator.h を参照。このクラスは「作ったものを GPU に置いて持つ」だけを担当する。
// チャンネルの意味は CloudNoiseGenerator.h のコメントのとおり。
// サンプラーは、繰り返し(WRAP)で線形補間のものを使うこと(タイル可能な画像なので)。
class CloudNoiseTextures {
public:
  // 生成して GPU へ送る命令をコマンドリストに積む(実際の転送は、そのフレームの描画の最初に行われる)。
  // 失敗したら false。その場合 IsInitialized() は false のままで、雲は描かない運用にする
  bool Initialize(DirectXCommon *dx);

  bool IsInitialized() const { return initialized_; }

  // t0 などの SRV テーブルに設定するための GPU ハンドル
  D3D12_GPU_DESCRIPTOR_HANDLE GetShapeSrvGpu() const { return shape_.GetSrvGpu(); }
  D3D12_GPU_DESCRIPTOR_HANDLE GetDetailSrvGpu() const { return detail_.GetSrvGpu(); }

private:
  TextureResource shape_;
  TextureResource detail_;
  bool initialized_ = false;
};
