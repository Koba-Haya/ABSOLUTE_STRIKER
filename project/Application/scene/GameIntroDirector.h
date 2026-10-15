#pragma once

// ゲーム開始時のイントロ(画面遷移の覆いが晴れる間の、見た目だけの助走)の演出。
// 進行度から、カメラと自機をどれだけ後ろへ戻すか、画角をどれだけ広げるかを求めるだけで、状態を持たない。
// 実際のレール進行や敵の更新は、イントロが終わるまで止まっているので、ゲームの進行度はずれない。
struct GameIntroPose {
  float backOffset = 0.0f; // 開始位置から、進行方向の後ろへ戻す距離(ユニット)。0 でスタート位置
  float riseOffset = 0.0f; // 開始位置から、下へ下げる距離(ユニット)。雲の層の中から昇り始める。0 でスタート位置
  float pitchUp = 0.0f;    // 自機の機首の上げ角(ラジアン)。昇りながら水平へ戻る。0 で水平
  float fovScale = 1.0f;   // 画角の倍率(1 で通常の画角)
};

class GameIntroDirector {
public:
  // revealProgress: 覆いが晴れる進み具合(0 = 晴れ始める前、1 = 完全に晴れた。時間に比例)
  // railSpeed: レールの進む速さ(ユニット / 秒)。助走の速さをこれに合わせると、イントロの終わりで
  //            ゲームの実際の進行へ、速度が途切れずにつながる
  // revealSeconds: 覆いが晴れる時間(秒)。助走の距離 = railSpeed * revealSeconds
  static GameIntroPose Evaluate(float revealProgress, float railSpeed, float revealSeconds);
};
