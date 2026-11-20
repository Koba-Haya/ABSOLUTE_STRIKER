#include "BossModelSpec.h"

#include <cstddef>

namespace {
// ボスの種類ごとの見た目の定義。BossKind の並びと同じ順にする(添字で引く)
constexpr BossModelSpec kSpecs[] = {
    // Beetle: 幅 約 9、全長 約 14(あごを含む)。当たり判定は、従来の仮のボス(立方体)と同じ半径。
    // 欠片のモデル(boss_beetle_shards.*)は、ボロノイのひび割れ(crackShape)の確認が済むまで、使わずに残してある
    // (使うときは、shardsModelPath と shardsInfoPath に、"resources/app/boss_beetle/boss_beetle_shards.gltf" と ".json" を入れる)。
    // ひびの形: モデルの範囲は x -4.3 〜 4.3、y -2.7 〜 1.2、z -4.6 〜 9.5(あごの先)。中心は胴のあたり、端まで 約 9.5
    {"resources/app/boss_beetle/boss_beetle.gltf", 1.0f, 5.0f, "resources/app/boss_beetle/beetle_dissolve_mask.png", 0.30f,
     nullptr, nullptr, {0.0f, -0.7f, 1.0f, 9.5f, 1.8f}},
    // Eagle: 翼幅 約 12。当たり判定は、翼の先まで含めず、胴と翼の付け根を覆う半径。
    // マスク、欠片、ひびの形は、まだ作っていない(撃破演出は、全体が一様に消える。甲虫の確認のあとに作る)
    {"resources/app/boss_eagle/boss_eagle.gltf", 1.0f, 5.0f, nullptr, 0.0f, nullptr, nullptr, {0.0f, 0.0f, 0.0f, 0.0f, 0.0f}},
};

// stages.json の文字列と BossKind の対応表。ボスを増やすときは、ここにも足す
struct KindName {
  const char *name;
  BossKind kind;
};
constexpr KindName kKindNames[] = {
    {"beetle", BossKind::Beetle},
    {"eagle", BossKind::Eagle},
};

// 表の数と BossKind の数が食い違っていないか(足し忘れを、コンパイル時に見つける)
static_assert(sizeof(kSpecs) / sizeof(kSpecs[0]) == sizeof(kKindNames) / sizeof(kKindNames[0]),
              "kSpecs and kKindNames must have one entry per BossKind");
} // namespace

const BossModelSpec &GetBossModelSpec(BossKind kind) {
  return kSpecs[static_cast<std::size_t>(kind)];
}

bool ParseBossKind(const std::string &name, BossKind &out) {
  for (const KindName &entry : kKindNames) {
    if (name == entry.name) {
      out = entry.kind;
      return true;
    }
  }
  return false;
}
