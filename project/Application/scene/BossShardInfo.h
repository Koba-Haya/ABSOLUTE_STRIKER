#pragma once
#include <string>
#include <vector>

// ボスの欠片モデル(boss_beetle_shards.gltf など)に付属する、欠片ごとの情報(JSON)の 1 件分。
// JSON は、モデルを作るスクリプト(tools/boss_beetle の gen_shards.py)が作る。
struct BossShardEntry {
  std::string name;    // 骨の名前(Shard_00 など。欠片のモデルの骨と同じ)
  std::string kind;    // 部位の分類(body, head, wing, leg, horn, antenna)
  float volume = 0.0f; // 欠片の体積(欠片全体の重心を求めるときの重み)
  float radius = 0.0f; // 欠片の重心から、最も遠い頂点までの距離
};

// 欠片モデルの付属データを読み込んで、持つクラス。責務は「JSON を読み、欠片の情報を引けるようにすること」だけ。
// 欠片の動きは BossShatter が担当する。
// 分けた理由: ファイルの形式(JSON の項目名など)を、動きの計算から切り離し、形式が変わっても、動きのクラスを直さずに済むため。
class BossShardInfo {
public:
  // JSON を読む(exe 基準のパス)。読めないときは false を返し、中身を空にする
  bool Load(const std::string &path);

  const std::vector<BossShardEntry> &GetEntries() const { return entries_; }

private:
  std::vector<BossShardEntry> entries_;
};
