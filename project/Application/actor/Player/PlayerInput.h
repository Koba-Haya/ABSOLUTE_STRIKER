#pragma once
#include "Type/Vector.h"

class Input;

// 1フレーム分のプレイヤー操作（アクション単位）。
// キー/パッド/マウスの生の判定は PlayerInput::Poll に集約し、
// PlayerComponent / LockOnSystem はこの構造体だけを見る。
struct PlayerInputState {
  // キーボード(WASD・矢印)とパッド左スティックによる移動方向。各軸 -1〜1、+Yが画面下。
  Vector2 moveDir{ 0.0f, 0.0f };
  // マウス移動によるカーソルの移動量（ピクセル。感度適用済み）
  Vector2 mouseDelta{ 0.0f, 0.0f };

  bool fireHeld     = false; // 発射/ロックボタンを押している間
  bool fireReleased = false; // 発射/ロックボタンを離した瞬間
  bool dodgePressed = false; // 回避ボタンを押した瞬間
};

class PlayerInput {
public:
  // 現在の入力状態を読み取る。
  // mouseAimEnabled が false の間は、マウス移動をカーソルに反映しない
  // （エディット中に ImGui 操作でレティクルが動くのを防ぐ）。
  static PlayerInputState Poll(const Input& input, bool mouseAimEnabled);
};

// Playモード中の OS マウスカーソルの非表示化 / ウィンドウ内への閉じ込め。
// Win32 のカーソル表示カウンタを崩さないよう、状態を内部で管理する。
namespace MouseCapture {
// want が true の間カーソルを隠して閉じ込める。アプリが非アクティブな間は自動で解除する。
// 毎フレーム呼んでよい（ウィンドウ移動にも追従する）。
void Apply(bool want);
// 現在カーソルを閉じ込め中か（Playモードのマウス照準を有効にするかの判定に使う）
bool IsActive();
} // namespace MouseCapture
