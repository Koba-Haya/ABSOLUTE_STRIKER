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

// パーティクルを描くとき、どのグループを描くか。
// 柔らかい縁で重なって見せたいグループ(霧・雲など)は、加算ではなく、アルファブレンドで描きたい。
// そのため、グループに「柔らかいアルファ」の印を付けて(ParticleManager::CreateParticleGroup)、描く側が選べるようにする
enum class ParticleDrawFilter {
  All,           // すべてのグループ(従来どおり)
  NormalOnly,    // 「柔らかいアルファ」の印がないグループだけ
  SoftAlphaOnly, // 「柔らかいアルファ」の印があるグループだけ
};
