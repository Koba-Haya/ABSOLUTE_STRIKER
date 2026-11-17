#pragma once
#include <string>

// ボスの種類。ステージごとに 1 つ決める(stages.json の "boss" の文字列と対応する。対応表は BossModelSpec.cpp)。
// ボスを増やすときは、ここに足し、BossModelSpec.cpp の表(名前と見た目の定義)に足す
enum class BossKind {
  Beetle, // 甲虫(クワガタ型)
  Eagle,  // 鷲
};

// ボスのモデルのファイルに入っているアニメーションの名前。
// 甲虫と鷲で、同じ名前にそろえてある(モデルを作るスクリプト: tools/boss_beetle, tools/boss_eagle)。
// 文字列を呼び出し側に直接書かず、ここだけで管理する(綴りの間違いを避け、名前を変えるときの修正箇所を 1 つにする)
namespace BossAnimation {
constexpr const char *kHover = "Hover";   // 待機の滞空(ループ)
constexpr const char *kAttack = "Attack"; // 攻撃の予告から発射まで(ループしない。終わったら待機へ戻す)
constexpr const char *kHit = "Hit";       // 被弾のよろめき(ループしない。終わったら待機へ戻す)
} // namespace BossAnimation

// 撃破演出の、ボロノイのひび割れの形(ボスのモデルごとに違う値)。座標と長さは、モデルを作ったときの単位(拡大前)
struct BossCrackShape {
  // ひびが広がり始める位置(モデルの中心あたり。コアの位置)
  float originX;
  float originY;
  float originZ;
  // origin から、ひびが最後に届く所までの距離(モデルの端まで届くように決める)
  float spreadRadius;
  // ボロノイのセルの大きさ。小さいほど、細かく割れる。0 以下のときは、ひび割れなし(従来のディゾルブで消える)
  float cellSize;
};

// 1 種類のボスの見た目の定義(データ)。責務は「ボスの種類ごとに違う、モデルと大きさを持つこと」だけ。
// ボスの動きと攻撃は、種類によらず BossComponent が行う。
// 分けた理由: ボスの追加を、コードの分岐ではなく、表への 1 行の追加で済ませるため
struct BossModelSpec {
  // モデルのパス(exe 基準)
  const char *modelPath;
  // モデルの拡大率。1.0 で、モデルを作ったときの大きさ(甲虫は幅 約 9、鷲は翼幅 約 12)
  float scale;
  // 当たり判定の球の半径(ワールド単位)。ロックオンの的の大きさにもなる
  float colliderRadius;

  // --- 撃破演出(ひびと剥がれ)に使う資産 ---
  // 内側(コア)のモデルのパス。外殻が剥がれたあとに、金色に光って見える体。
  // 外殻のモデルと、ボーン・アニメーションが同一であること(同じ時刻に同じポーズで動かす)。nullptr のときは、コアなし
  const char *coreModelPath;
  // ディゾルブのマスク画像のパス(外殻の UV の上のボロノイ。tools の gen_dissolve_mask.py が作る)。
  // nullptr のときは、エンジンが共有で持つノイズのマスクで、全体を一様に消す
  const char *dissolveMaskPath;
  // マスクの「ひび割れの段階の最後の値」(0 〜 1)。しきい値が 0 からこの値までがひび、この値から 1 までが剥離。
  // マスクを作るスクリプトの CRACK_MAX と同じ値にする。0 のときは、ひびの段階なし(最初から全体が消えていく)
  float crackEndThreshold;

  // --- 撃破演出(割れて飛び散る)に使う、欠片のモデル ---
  // 欠片のモデルのパス。1 つのスキンメッシュで、1 つの欠片に 1 本の骨が付いている(tools の gen_shards.py が作る)。
  // nullptr のときは、欠片なし(撃破演出は、外殻のディゾルブで消す)
  const char *shardsModelPath;
  // 欠片の付属データ(JSON)のパス。shardsModelPath があるときだけ使う
  const char *shardsInfoPath;

  // --- 撃破演出(ボロノイのひび割れ)の形 ---
  BossCrackShape crackShape;
};

// ボスの種類から、見た目の定義を取り出す。戻り値は、プログラムの終わりまで有効
const BossModelSpec &GetBossModelSpec(BossKind kind);

// 名前(stages.json の "boss" の文字列)を BossKind にする。表に無い名前のときは false を返す
bool ParseBossKind(const std::string &name, BossKind &out);
