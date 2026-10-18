#pragma once
#include "graphics/VolumeCloudQuality.h"
#include "graphics/VolumeCloudSettings.h"

// ボリューム雲のパラメータを ImGui のスライダーで調整する開発用の UI。
// Release(USE_IMGUI なし)では何も描かず、常に false を返す(設定値は既定値のまま使われる)。
// 描画の仕組みは Renderer::SetVolumeCloud と shaders/VolumeCloud.hlsli を参照。
namespace VolumeCloudDebugUi {

// 呼び出し側が開いている ImGui のウィンドウの中に、スライダー群を出す。
// settings を直接書き換える。戻り値: 値が変わったら true
// 「Copy values」は、現在の値を C++ の初期化子の形でクリップボードへ入れる(VolumeCloudSettings の既定値へ書き戻す用)
bool Draw(VolumeCloudSettings &settings);

// 品質の自動調整の設定と、今の状態(段、GPU 時間の移動平均、実際に描いている値)を出す。
// base: 基準の設定(実際に描く値を求めて表示するために使う。書き換えない)
void DrawQuality(VolumeCloudQuality &quality, const VolumeCloudSettings &base);

} // namespace VolumeCloudDebugUi
