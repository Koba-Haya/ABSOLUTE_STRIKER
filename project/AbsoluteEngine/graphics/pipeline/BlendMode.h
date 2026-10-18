#pragma once

enum class BlendMode {
  Opaque = 0, // ブレンドなし
  Alpha,      // 通常のアルファブレンド
  Add,        // 加算
  Subtract,   // 減算
  Multiply,   // 乗算
  Screen,     // スクリーン
  PremultipliedAlpha, // プリマルチプライド: 結果 = src.rgb + dst * (1 - src.a)(ボリューム雲など、光を足しつつ背後を減らす描画)
};
