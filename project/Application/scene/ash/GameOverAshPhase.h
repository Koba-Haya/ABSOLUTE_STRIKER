#pragma once

// ゲームオーバー画面の灰の段階の名札。「今どの段階か」を外へ見せる読み取り専用の値(調整画面の表示、デバッグの段階指定)。
// 灰の進み方(次の段階)は、段階の状態クラス(ash/ の AshDriftingState など)が決める。
// 分けた理由: GameOverAsh(状態を持つ側)と、段階の状態クラスの両方が使うので、どちらにも依存しない小さなヘッダに置く。
enum class GameOverAshPhase {
  Drifting,  // 漂い
  Gathering, // 集まり
  Glowing,   // 灯り
};
