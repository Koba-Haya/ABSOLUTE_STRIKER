#pragma once
#include <optional>
#include <string>

#include "Type/Vector.h"

// 弾 1 種類の定義(resources/bullet/bullets.json の "bullets" の 1 要素)。データだけを持つ。
//
// 分けた理由: 弾の見た目と当たり判定の大きさは、弾の種類(敵の種類ごと、自機)で変わるが、同じ種類の弾どうしでは共通。
// 共通の値はこの定義に 1 つだけ置き、1 発ごとの弾は、位置と速度など自分だけの値を持つ(Flyweight の考え方)。
// 定義の読み込み(BulletCatalog)、組み立て(BulletBuilder)、撃つ部品が、共通で参照する型をこのヘッダに置く。
//
// 速さは、ここに置かない。撃つ部品の設定(先読みの計算などに使う、撃ち方の一部)のため。
struct BulletDefinition {
  std::string id;                // 識別子(撃つ部品の JSON の "bulletId" から指す)
  std::string modelPath;         // モデル(exe 基準のパス)。空なら、モデルを付けない(見た目を別の部品が描く弾)
  float scale = 1.0f;            // モデルの拡大率(3 軸とも同じ)
  float radius = 0.5f;           // 当たり判定の球の半径(拡大率は掛からない。この値がそのまま当たりの大きさ)
  std::optional<Vector4> color;  // モデルの色。省略したら、モデルの既定の色
  bool canBeShotDown = true;     // 自機の通常弾で撃ち落とせるか(敵弾にだけ意味がある)
  int prewarmCount = 0;          // 使い回しの置き場所へ、前もって作って入れておく数(最初の連射で、作る処理が重なって止まるのを防ぐ)
};
