#include "UiPointer.h"
#include "Input.h"
#include "../ScreenSize.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {
constexpr int kMouseLeftButton = 0;  // Input のボタン番号(0 = 左)
constexpr int kMouseRightButton = 1; // 1 = 右
} // namespace

UiPointer::State UiPointer::Poll(const Input *input) {
  State state;
  if (!input) return state;

#ifdef USE_IMGUI
  // ImGui(開発用の窓)がマウスを使うとき(窓の上にあるときなど)は、メニューには渡さない
  if (ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureMouse) return state;
#endif

  // アクティブなウィンドウ(アプリが非アクティブなら nullptr)の、描画領域の中の位置を求める
  HWND hwnd = GetActiveWindow();
  if (!hwnd) return state;
  POINT cursor{};
  RECT client{};
  if (!GetCursorPos(&cursor) || !ScreenToClient(hwnd, &cursor) || !GetClientRect(hwnd, &client)) return state;
  const float clientWidth = static_cast<float>(client.right - client.left);
  const float clientHeight = static_cast<float>(client.bottom - client.top);
  if (clientWidth <= 0.0f || clientHeight <= 0.0f) return state;

  // 描画の座標(ScreenSize の大きさ)へ直す。窓の大きさが変わっても、位置が合うように
  state.position = {static_cast<float>(cursor.x) * ScreenSize::kWidth / clientWidth,
                    static_cast<float>(cursor.y) * ScreenSize::kHeight / clientHeight};
  state.valid = state.position.x >= 0.0f && state.position.x < ScreenSize::kWidth && state.position.y >= 0.0f &&
                state.position.y < ScreenSize::kHeight;

  const Input::MouseState mouse = input->GetMouse();
  state.moved = mouse.dx != 0 || mouse.dy != 0;
  state.leftClicked = input->WasMousePressed(kMouseLeftButton);
  state.rightClicked = input->WasMousePressed(kMouseRightButton);
  return state;
}

bool UiPointer::Contains(const UiRect &rect, const Vector2 &point) {
  return point.x >= rect.x && point.x < rect.x + rect.width && point.y >= rect.y && point.y < rect.y + rect.height;
}
