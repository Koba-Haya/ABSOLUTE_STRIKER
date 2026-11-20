#pragma once
#include "UiRectRenderer.h"
#include "Vector.h"

class Input;

/// <summary>
/// メニューを、マウスで操作するための入力を読み取るクラス(マウスの位置、動いたか、クリックしたか)。
/// 分けた理由: 「どの入力がメニュー操作か」は UiMenuInput がキーボードとパッドについて 1 か所にまとめている。
/// マウスは、位置を画面の座標へ直す処理が要るので、別の部品にした(UiMenuInput は、位置を持たない「命令」だけを返すため)。
/// Input クラスのマウスは、相対移動量だけで位置を持たないので、位置は Win32 の API(カーソルの位置)から、
/// 描画の座標(ScreenSize の大きさ)へ直して得る。
///
/// 注意:
///  - ImGui(開発用の窓)の上にマウスがあるとき、ImGui がマウスを使うときは、無効にする(ImGui のクリックと混ざらないように)。
///  - ウィンドウが非アクティブのとき、マウスが画面の外のときは、無効(valid が false)。
///  - 「動いた」は、そのフレームに、マウスの相対移動量があったか。止まったままのマウスが、キーボードの選択を上書きしないようにするために使う。
/// </summary>
class UiPointer {
public:
  /// 1 フレームのマウスの状態
  struct State {
    bool valid = false;        // 位置が使えるか(窓の中にあり、ImGui が使っていない)
    Vector2 position{0.0f, 0.0f}; // 描画の座標での位置(ScreenSize の大きさ。左上が原点)
    bool moved = false;        // このフレームに、マウスが動いたか
    bool leftClicked = false;  // このフレームに、左ボタンを押したか(押した瞬間だけ)
    bool rightClicked = false; // このフレームに、右ボタンを押したか(押した瞬間だけ)
  };

  /// <summary>このフレームのマウスの状態を読む。input が nullptr のときは、valid が false の状態を返す</summary>
  static State Poll(const Input *input);

  /// <summary>位置 point が、長方形 rect の中にあるか</summary>
  static bool Contains(const UiRect &rect, const Vector2 &point);
};
