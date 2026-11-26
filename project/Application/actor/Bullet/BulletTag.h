#pragma once

// 弾の GameObject に付けるタグ(GameObject::SetTag の値)。当たったときの判定(どちらの陣営の弾か)に使う。
// 分けた理由: 組み立て(BulletBuilder)と撃つ部品が、同じ文字を使うため、1 か所に置く。
namespace BulletTag {
constexpr const char *kEnemy = "EnemyBullet";   // 敵の弾
constexpr const char *kPlayer = "PlayerBullet"; // 自機の弾
} // namespace BulletTag
