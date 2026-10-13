#include "PlayerInput.h"
#include "Input.h"
#include <algorithm>
#include <cmath>

namespace {
constexpr float kStickDeadzone    = 0.2f;      // パッドスティックのデッドゾーン（-1〜1換算）
constexpr float kStickMax         = 32767.0f;  // XInput スティック最大値
constexpr float kMouseSensitivity = 1.0f;      // マウス1カウントあたりのカーソル移動量（ピクセル）
constexpr int   kMouseLeftButton  = 0;

bool s_cursorHidden = false; // ShowCursor(FALSE) 済みか（表示カウンタを崩さないための状態）
} // namespace

PlayerInputState PlayerInput::Poll(const Input& input, bool mouseAimEnabled) {
  PlayerInputState state;

  // キーボード（WASD / 矢印）
  if (input.PressKey(DIK_W) || input.PressKey(DIK_UP))    state.moveDir.y -= 1.0f;
  if (input.PressKey(DIK_S) || input.PressKey(DIK_DOWN))  state.moveDir.y += 1.0f;
  if (input.PressKey(DIK_A) || input.PressKey(DIK_LEFT))  state.moveDir.x -= 1.0f;
  if (input.PressKey(DIK_D) || input.PressKey(DIK_RIGHT)) state.moveDir.x += 1.0f;

  // ゲームパッド左スティック（デッドゾーンを超えた軸はキー入力より優先）
  if (input.IsGamepadConnected()) {
    const auto pad = input.GetGamepad();
    const float padX =  pad.lx / kStickMax;
    const float padY = -pad.ly / kStickMax; // スティック上が +Y なので反転して画面座標系に合わせる
    if (std::abs(padX) > kStickDeadzone) state.moveDir.x = padX;
    if (std::abs(padY) > kStickDeadzone) state.moveDir.y = padY;
  }

  // マウス（相対移動量をカーソル移動量として使う）
  if (mouseAimEnabled) {
    const auto mouse = input.GetMouse();
    state.mouseDelta.x = static_cast<float>(mouse.dx) * kMouseSensitivity;
    state.mouseDelta.y = static_cast<float>(mouse.dy) * kMouseSensitivity;
  }

  // 発射 / ロックボタン（Space・パッドA・マウス左）
  state.fireHeld = input.PressKey(DIK_SPACE)
                || input.IsPadDown(XINPUT_GAMEPAD_A)
                || input.IsMouseDown(kMouseLeftButton);
  state.fireReleased = input.ReleaseKey(DIK_SPACE)
                    || input.WasPadReleased(XINPUT_GAMEPAD_A)
                    || input.WasMouseReleased(kMouseLeftButton);

  // 回避（左Shift・パッドB）
  state.dodgePressed = input.TriggerKey(DIK_LSHIFT)
                    || input.WasPadPressed(XINPUT_GAMEPAD_B);

  return state;
}

void MouseCapture::Apply(bool want) {
  // GetActiveWindow はこのスレッドのアクティブウィンドウ。アプリが非アクティブなら nullptr。
  HWND hwnd = GetActiveWindow();

  if (want && hwnd) {
    RECT rc{};
    GetClientRect(hwnd, &rc);
    POINT topLeft{ rc.left, rc.top };
    POINT bottomRight{ rc.right, rc.bottom };
    ClientToScreen(hwnd, &topLeft);
    ClientToScreen(hwnd, &bottomRight);
    const RECT clip{ topLeft.x, topLeft.y, bottomRight.x, bottomRight.y };
    ClipCursor(&clip);

    if (!s_cursorHidden) {
      ShowCursor(FALSE);
      s_cursorHidden = true;
    }
  } else if (s_cursorHidden) {
    ClipCursor(nullptr);
    ShowCursor(TRUE);
    s_cursorHidden = false;
  }
}

bool MouseCapture::IsActive() { return s_cursorHidden; }
