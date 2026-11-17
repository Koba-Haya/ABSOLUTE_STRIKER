#pragma once
#include <memory>

#include "AbsoluteEngine/scene/GameObject.h"
#include "../actor/Enemy/BossModelSpec.h"
#include "BossShardInfo.h"
#include "BossShatter.h"
#include "ClearDirector.h"
#include "Type/Vector.h"

// ボス撃破の演出の、ボスの見た目(外殻の割れ方、内側(コア)の光)を決めるクラス。
// ClearDirector が時間(サブフェーズと、その中の進み具合)を決め、このクラスが、その値を、ボスのオブジェクトへ反映する。
// 分けた理由: 時間の枠(ClearDirector)と、見た目の中身(ここ)を分けて、見た目の調整で時間の管理を触らないため。
//
// 仕組み: ボスは、2 つのモデルの重ね合わせで描く。
//   外殻(shell): 割れ方は、2 通りある。
//                欠片があるボス: 撃破の瞬間に、外殻を消して、あらかじめ割ってある欠片のモデルへ差し替える。欠片の動きは BossShatter が決める。
//                欠片がないボス: ディゾルブで消す。マスク画像が、外殻の UV の上のボロノイで、「溝(ひび)が開く -> セルが縁から消える(剥がれる)」の順に消える
//   内側(core) : 照明なしの金色で描く(発光)。外殻が消えたところ(欠片の継ぎ目や、剥がれたあと)から見える
// サブフェーズごとの動き(コアの光は、どちらのボスも同じ):
//   HitStop : 何もしない(外殻、または組み立てた欠片のまま)
//   Crack   : 欠片は、継ぎ目が開く(ディゾルブは、溝が開く)。中の光が漏れる。コアの明るさが上がる
//   Peel    : 欠片は、外側から解放されて飛ぶ(ディゾルブは、外殻が剥がれ落ちる)。コアがさらに明るくなる
//   Burst   : 外殻はすべて消えている(欠片は、残りが弾け飛ぶ)。コアが最も明るい(光の破裂は、別のクラスが担当する)
//   Flow    : コアが縮んで、光に溶ける(欠片も縮んで消える)
//   Cover   : すべて消えた状態(スキップで、直接ここへ来ても、見た目が揃うようにする)

// 見た目の設定(ステージ(ボス)ごとに変わりうる値なので、コードに埋め込まず、まとめて持つ)
namespace BossDefeatDefaults {
// ひびと剥がれの縁の光
inline const Vector3 kEdgeColor{1.0f, 0.78f, 0.30f}; // 縁の色(金)
constexpr float kEdgeRange = 0.10f;                    // 縁の幅(マスクの値の幅。大きいほど、溝の縁の光が太い)
constexpr float kEmberAmount = 0.0f;                   // 熾火(灰 -> 赤 -> 橙)にせず、金色の縁にする(0 で、縁の色へ混ぜるだけ)
// コア(内側)の発光。色に掛ける倍率(照明なしなので、この値が、そのまま明るさになる)
inline const Vector3 kCoreColor{1.0f, 0.62f, 0.10f};
constexpr float kCoreIntensityIdle = 1.0f;  // ひびが入る前
constexpr float kCoreIntensityCrack = 1.6f; // ひびが開き切ったとき
constexpr float kCoreIntensityPeak = 2.4f;  // 剥がれ切ったとき(破裂の瞬間)
// ボロノイのひび割れ(外殻の表面で光る線)
inline const Vector3 kCrackColor{1.0f, 0.80f, 0.35f}; // ひびの光の色(金)
constexpr float kCrackIntensity = 1.8f;                // ひびの光の強さ(色に掛ける。出力は 1 で頭打ちなので、白に近づく)
constexpr float kCrackLineWidth = 0.14f;               // ひびが広がり切ったときの線の太さ(セルの大きさに対する割合)
// セルが消える場所から出る粒(殻の欠片が、光の粒になって散る)
constexpr float kDustEmitChance = 0.4f;                // 消えるセルの頂点のうち、粒を出す割合(ボスの頂点は 約 4700 個。総数 = 頂点数 x この値)
inline const Vector3 kDustColorStart{1.0f, 0.85f, 0.40f}; // 粒の、生まれたときの色(明るい金)
inline const Vector3 kDustColorEnd{1.0f, 0.45f, 0.10f};   // 冷めたときの色(橙)
constexpr float kDustCoolRatio = 0.6f;                 // 色が変わりきる、寿命の割合
constexpr float kDustFadePower = 2.0f;                 // 大きいほど、終わり際まで残ってから消える
constexpr float kDustLifeMin = 1.0f;                   // 寿命(秒)
constexpr float kDustLifeMax = 2.2f;
constexpr float kDustSizeMin = 0.12f;                  // 大きさ(ワールド単位。ボスは幅 約 9)
constexpr float kDustSizeMax = 0.32f;
constexpr float kDustSpeedMin = 0.2f;                  // 生まれたときの、向きがランダムな速さ
constexpr float kDustSpeedMax = 1.2f;
constexpr float kDustOutwardSpeed = 2.5f;              // コアの位置から、外向きに足す速さ(ワールド単位 / 秒)
} // namespace BossDefeatDefaults

struct BossDefeatSettings {
  Vector3 edgeColor = BossDefeatDefaults::kEdgeColor;
  float edgeRange = BossDefeatDefaults::kEdgeRange;
  float emberAmount = BossDefeatDefaults::kEmberAmount;
  Vector3 coreColor = BossDefeatDefaults::kCoreColor;
  float coreIntensityIdle = BossDefeatDefaults::kCoreIntensityIdle;
  float coreIntensityCrack = BossDefeatDefaults::kCoreIntensityCrack;
  float coreIntensityPeak = BossDefeatDefaults::kCoreIntensityPeak;
  Vector3 crackColor = BossDefeatDefaults::kCrackColor;
  float crackIntensity = BossDefeatDefaults::kCrackIntensity;
  float crackLineWidth = BossDefeatDefaults::kCrackLineWidth;
  float dustEmitChance = BossDefeatDefaults::kDustEmitChance;
  Vector3 dustColorStart = BossDefeatDefaults::kDustColorStart;
  Vector3 dustColorEnd = BossDefeatDefaults::kDustColorEnd;
  float dustCoolRatio = BossDefeatDefaults::kDustCoolRatio;
  float dustFadePower = BossDefeatDefaults::kDustFadePower;
  float dustLifeMin = BossDefeatDefaults::kDustLifeMin;
  float dustLifeMax = BossDefeatDefaults::kDustLifeMax;
  float dustSizeMin = BossDefeatDefaults::kDustSizeMin;
  float dustSizeMax = BossDefeatDefaults::kDustSizeMax;
  float dustSpeedMin = BossDefeatDefaults::kDustSpeedMin;
  float dustSpeedMax = BossDefeatDefaults::kDustSpeedMax;
  float dustOutwardSpeed = BossDefeatDefaults::kDustOutwardSpeed;
  BossShatterSettings shatter; // 欠片の動き(欠片があるボスだけが使う)
};

class BossDefeatVisual {
public:
  // 演出を始める。shell: 外殻のオブジェクト(ModelComponent と DissolveComponent を持つ)。
  // core: 内側のオブジェクト(なければ nullptr)。crackEndThreshold: マスクの、ひびの段階の最後の値(BossModelSpec::crackEndThreshold)。
  // shards: 欠片のモデルのオブジェクト(なければ nullptr。あれば、外殻を消して、欠片へ差し替える。位置・向き・大きさは、ボスと同じにしておく)。
  // shardInfo: 欠片の付属データ(shards があるときだけ使う)
  // crackShape: ボロノイのひび割れの形(BossModelSpec::crackShape)。cellSize が 0 以下なら、ひび割れは使わず、ディゾルブだけで消える
  void Begin(std::shared_ptr<AbsoluteEngine::GameObject> shell, std::shared_ptr<AbsoluteEngine::GameObject> core,
             std::shared_ptr<AbsoluteEngine::GameObject> shards, const BossShardInfo &shardInfo, float crackEndThreshold,
             const BossCrackShape &crackShape, const BossDefeatSettings &settings);

  // いまのサブフェーズと、その中の進み具合(0 〜 1。ClearDirector が決める)を、ボスの見た目へ反映する。毎フレーム呼ぶ。
  // deltaTime: 1 フレームの実時間(秒。欠片の飛び方の積分に使う)
  void Update(ClearPhase phase, float phaseProgress, float deltaTime);

  // 演出の最中か(Begin のあと)
  bool IsActive() const { return shell_ != nullptr; }

private:
  // 外殻のディゾルブを設定する(enable が false なら、元の見た目)。欠片へ差し替えているときは、指定によらず、外殻を全て消す
  void ApplyShell_(bool enable, float threshold);
  // コアの発光の強さと、大きさ(1 で元の大きさ)を設定する
  void ApplyCore_(float intensity, float scale);
  // 外殻のボロノイのひびの、広がった割合(reveal)と、セルが消えた割合(breakAmount)を、0 〜 1 で設定する。
  // ひび割れを使わないボスでは、何もしない
  void ApplyCrack_(float reveal, float breakAmount = 0.0f);
  // ボロノイのひび割れを使うか(形が決まっていて、欠片へ差し替えていないとき)
  bool UsesVoronoiCrack_() const;
  // セルが消えた割合が、previousBreak から currentBreak へ進む間に消えるセルの頂点から、粒を出す(GPU パーティクルへ依頼する)
  void EmitCellDust_(float previousBreak, float currentBreak);

  std::shared_ptr<AbsoluteEngine::GameObject> shell_;
  std::shared_ptr<AbsoluteEngine::GameObject> core_;
  Vector3 coreBaseScale_{1.0f, 1.0f, 1.0f}; // 演出の開始時の、コアの大きさ(縮める基準)
  BossShatter shatter_; // 欠片の動き(欠片があるボスだけ。IsActive が true なら、欠片へ差し替え済み)
  float crackEnd_ = 0.0f;
  BossCrackShape crackShape_{};
  float lastCellBreak_ = 0.0f; // 前のフレームまでに、粒を出し終えた、セルが消えた割合(次の依頼の始まりの値)
  BossDefeatSettings settings_;
};
