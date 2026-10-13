#pragma once
#include "AbsoluteEngine/base/WinApp.h"

// 画面（クライアント領域）サイズ。ウィンドウ作成時のサイズ(WinApp)を唯一の出典にする。
// プレイヤー入力・ロックオン・HUD など、スクリーン座標を扱う箇所はここを参照すること。
namespace ScreenSize {
constexpr float kWidth  = static_cast<float>(WinApp::kClientWidth);
constexpr float kHeight = static_cast<float>(WinApp::kClientHeight);
constexpr float kCenterX = kWidth  * 0.5f;
constexpr float kCenterY = kHeight * 0.5f;
} // namespace ScreenSize
