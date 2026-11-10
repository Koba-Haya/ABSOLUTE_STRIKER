#pragma once

class Input;

/// <summary>
/// メニュー操作の入力(上、下、決定)を読み取るクラス。
/// キーボード(上下 / W S、Space / Enter)とパッド(十字キーの上下、A ボタン)の両方を、1 つの「命令」にまとめて返す。
/// 分けた理由: 「どのキー、どのボタンがメニュー操作か」を 1 か所にして、ステージ選択、クリア、ゲームオーバー、ポーズが
/// Input クラスを直接読まなくて済むようにするため(キー割り当ての変更が、1 か所で済む)。
/// 決定は 1 つのボタンだけ(Space / Enter / A は、どれも同じ「決定」)。
/// </summary>
class UiMenuInput {
public:
  /// 1 フレームで読み取るメニュー命令
  enum class Command {
    None,   // 何も押していない
    Up,     // 1 つ上へ
    Down,   // 1 つ下へ
    Decide, // 決定
  };

  /// <summary>
  /// このフレームに押された(押した瞬間の)メニュー命令を返す。押しっぱなしでは繰り返さない。
  /// 同時に複数押されたときは、決定、上、下の順で優先する。
  /// </summary>
  /// <param name="input">入力(nullptr のときは None)</param>
  static Command Poll(const Input *input);
};
