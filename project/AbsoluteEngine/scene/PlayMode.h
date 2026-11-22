#pragma once

// シーンの再生状態。ゲームのコンポーネントも読むため、BaseScene とエディタ(EditorSession)の
// 両方から参照できるよう、独立したヘッダに置いている。
enum class PlayMode {
  Edit,  // 編集中。世界は更新されない
  Play,  // 再生中
  Pause  // 一時停止中
};
