#pragma once
#include <cstdint>

namespace DirectX {
class ScratchImage;
} // namespace DirectX

// 雲(ボリューム雲)用の 3D ノイズを CPU で生成する。D3D12 には依存しない(画像データを作るだけ)。
// どの画像も「タイル可能」: 端と端がつながるので、並べて繰り返し使っても継ぎ目が出ない。
// 画素は R8G8B8A8_UNORM(リニア)。ミップマップなし。失敗したときは空の画像(GetImageCount() == 0)を返す。
// 各チャンネルの使い道は、雲のシェーダー側で決める(ここでは材料を作るだけ)。
namespace CloudNoiseGenerator {

// 雲の「基本の形」用。size は 1 辺の画素数。
//   R: Perlin-Worley(なめらかな大きなうねりに、丸い塊の形を足したもの。雲の大まかな形)
//   G, B, A: Worley(反転)を 3 つの細かさで。値が大きいほど塊の中心に近い。細かさは G < B < A の順に細かい
DirectX::ScratchImage GenerateShape(uint32_t size);

// 形を削る細かい模様用。size は 1 辺の画素数。
//   R, G, B: Worley(反転)を 3 つの細かさで(基本の形より細かい)
//   A: 常に 1
DirectX::ScratchImage GenerateDetail(uint32_t size);

} // namespace CloudNoiseGenerator
