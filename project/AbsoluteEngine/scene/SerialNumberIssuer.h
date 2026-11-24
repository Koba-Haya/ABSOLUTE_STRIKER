#pragma once

namespace AbsoluteEngine {

// 通し番号(1, 2, 3, ...)を、呼ばれるたびに 1 つずつ発行するクラス。
//
// 作った理由: 「編隊を見分ける番号」や「敵ごとに変える乱数の種」を、ファイル内の static 変数(グローバルな状態)で
// 数えていた。番号は、持ち主(シーンの世界)を 1 つ決めて、そこが発行する形にした。
// 発行した番号は、同じ発行役の中で重ならない。シーンが作り直されると、発行役も作り直され、1 から数え直す。
// メインスレッドからだけ呼ぶこと。
class SerialNumberIssuer {
public:
  // 次の番号を発行する(最初は 1)
  int Issue() { return next_++; }

private:
  int next_ = 1; // 次に発行する番号
};

} // namespace AbsoluteEngine
