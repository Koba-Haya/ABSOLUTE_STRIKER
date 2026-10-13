#pragma once
#include "graphics/CloudSeaSettings.h"

// 雲海のパラメータを ImGui のスライダーで調整する開発用の UI。
// Release(USE_IMGUI なし)では何も描かず、常に false を返す(設定値は既定値のまま使われる)。
// 描画の仕組みは Renderer::DrawCloudSea と shaders/CloudSea.hlsli を参照。
namespace CloudSeaDebugUi {

// 呼び出し側が開いている ImGui のウィンドウの中に、スライダー群を出す。
// settings を直接書き換える。戻り値: 値が変わったら true
// 「Copy values」は、現在の値を C++ の初期化子の形でクリップボードへ入れる(CloudSeaSettings の既定値へ書き戻す用)
bool Draw(CloudSeaSettings &settings);

} // namespace CloudSeaDebugUi
