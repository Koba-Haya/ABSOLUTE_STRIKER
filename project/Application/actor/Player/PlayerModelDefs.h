#pragma once

// 自機のモデル(蝶)に関する名前の定義。モデルを作るスクリプト(tools/butterfly/gen_butterfly.py)の名前と揃える。
// 文字列を呼び出し側に直接書かず、ここだけで管理する(綴りの間違いを避け、名前を変えるときの修正箇所を 1 つにする)
namespace PlayerModel {
// 発射位置の印のボーン(頭の先)。弾・リボンの尾・ロックオンのミサイルは、このボーンの位置から出る
constexpr const char* kMuzzleBone = "muzzle";
} // namespace PlayerModel

// 自機のモデルのファイルに入っているアニメーションの名前
namespace PlayerAnimation {
constexpr const char* kFlap = "Flap";     // 羽ばたき(通常。ループ)
constexpr const char* kGlide = "Glide";   // 滑空(ループ)
constexpr const char* kHit = "Hit";       // 被弾でよろめく(ループしない。終わったら通常へ戻す)
constexpr const char* kTumble = "Tumble"; // 墜落のきりもみ(ループ)
} // namespace PlayerAnimation
