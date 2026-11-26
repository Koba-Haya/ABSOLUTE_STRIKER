#pragma once
#include <cstdint>

// ボスの攻撃の演出(口の前の輪、弾)が使う、粒のグループの名前と画像。
//
// 分けた理由: 粒のグループは、攻撃の演出(BossAttackEffects)と、弾の見た目(BossBulletVisualComponent)の 2 か所が使う。
// どちらかが持つと、もう一方がそれに依存してしまうので、名前と「無ければ作る」処理を、ここに集める。
// 画像は、アルファ(透明度)つきの専用の画像(tools/particle_textures/gen_particle_textures.py が作る)。
// 既存の circle.png は、黒い背景の画像でアルファが無く、アルファ合成では黒い四角が見えるので使わない。
// グループは、どれもアルファ合成の側(softAlpha)。明るい空や雲の上でも、色が白く飛ばない。
namespace BossEffectParticles {
constexpr const char* kGlowGroupName = "BossAttackGlow";       // 柔らかい光の玉: 吸い込まれる粒、閃光、火の粉
constexpr const char* kGlowTexturePath = "resources/app/particle/boss_glow.png";
constexpr const char* kFlareGroupName = "BossAttackFlare";     // 星形の閃光: 発射の火花
constexpr const char* kFlareTexturePath = "resources/app/particle/boss_flare.png";
constexpr const char* kStreakGroupName = "BossBulletStreak";   // 細長い火花の線: 弾の尾
constexpr const char* kStreakTexturePath = "resources/app/particle/boss_streak.png";
constexpr const char* kRingGroupName = "BossBulletRing";       // 細い光の輪: 弾が後ろに残す衝撃波の輪
constexpr const char* kRingTexturePath = "resources/app/particle/boss_ring.png";

constexpr uint32_t kGlowMaxInstances = 500;
constexpr uint32_t kFlareMaxInstances = 500;
constexpr uint32_t kStreakMaxInstances = 1500;
constexpr uint32_t kRingMaxInstances = 800;

// 4 つのグループを、無いものだけ作る(あるものは触らない。CreateParticleGroup は、同名があると中身を消すため)。
// どれも「雲のあとに描く」グループ(雲に隠れないよう、シーンが、雲の合成のあとに AfterCloudsOnly で描く)
void EnsureGroups();
} // namespace BossEffectParticles
