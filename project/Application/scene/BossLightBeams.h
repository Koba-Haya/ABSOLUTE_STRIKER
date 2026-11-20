#pragma once
#include <cstdint>
#include <memory>
#include <random>
#include <vector>

#include "AbsoluteEngine/graphics/3d/model/CrackPointPicker.h"
#include "AbsoluteEngine/scene/GameObject.h"
#include "../actor/Enemy/BossModelSpec.h"
#include "ClearDirector.h"
#include "AbsoluteEngine/graphics/primitive/RibbonTrail.h"
#include "Type/Vector.h"

// ボス撃破の演出の、光線(体から放射状に伸びる、太くて先の尖った光の帯)を受け持つクラス。
//
// 分けた理由: 光線は、「出る点の選び方」「伸びる時刻と長さ」「描き方(リボン)」という、まとまった責務を持つ。
// 外殻のひび割れ(BossDefeatVisual)とは別の部品にして、BossDefeatVisual は、サブフェーズと進み具合を渡すだけにする。
//
// 仕組み:
//   1. Begin: ボロノイのひびの線の上にある頂点と、その頂点のセルが光って消える時刻を、GPU に選ばせる(CrackPointPicker)。
//      ひびが光る場所と、選ばれる点が一致する。時刻は、描画と同じ計算なので、セルが実際に光って消える瞬間と一致する。
//   2. 結果が読めたら(1 フレーム以降)、選ばれた点から、方向がばらけるように、光線の根元を数本選ぶ。
//   3. 毎フレーム: 選んだ頂点を、スキニングした後のワールド位置へ移す(CPU で骨の行列を使う)。体が動いても、向きが付いてくる。
//      光線は、体の中心(ひびの広がり始める位置)を根元にして、選んだ頂点を通る向き(放射状)に伸びる。
//      体の内側にある部分は、殻に隠れる(深度テスト)。そのため、ひびの線の上の点から、光が突き出して見える。セルが消えると、中の光線が見える。
//   4. 光線は、Crack の間に、一定の間隔で 1 本ずつ増えて、Crack の終わりまでに出そろう(出そろってから、Peel で崩壊が始まる)。
//      伸びるときは、勢いよく伸びて、残る。
//   5. Peel(崩壊)では、各光線が、通っている点のセルが消える瞬間に合わせて、先端から根元へ縮んで消える。
//      「光線は、ひびから出ているので、そのひびが崩れたら消える」。殻が消えたあとに、光線だけが宙に残らない。Burst の時点では、全部消えている。
// 描画は、エンジンのリボン(RibbonTrail と Renderer::DrawRibbon)に任せる。
// 画面を暗くしないので、明るい空や雲の上でも見えるよう、通常のアルファ合成で描き、濃い色の縁と白い芯にする(加算だと、白く飛ぶ)。
// 注意: ボスのモデルは、骨(スキニング)付きであること。そうでなければ、光線は出ない。

namespace BossLightBeamDefaults {
// --- 数と出る点 ---
constexpr uint32_t kBeamCount = 14;        // 光線の数(参考映像は、数本から十数本)。間隔は、演出の時間(ClearDefaults)を長くして確保する
constexpr float kPickChance = 0.5f;        // ひびの線の上の頂点のうち、候補にする割合(候補が多いほど、方向をばらけさせやすい)
constexpr float kPickSeedRange = 1000.0f;  // 候補を選ぶ乱数の種の範囲(依頼ごとに変える)
// --- 時間(Crack の長さを 1 としたときの、0 〜 1)。光線は、Crack の間に出そろう ---
// 間隔 = (kLastStart - kFirstStart) x Crack の長さ(ClearDefaults::kCrackSeconds) / (本数 - 1)。約 0.25 秒になるよう、Crack を長くしてある
constexpr float kFirstStart = 0.12f;       // 最初の光線が伸び始める時刻(ひびが、ある程度広がってから)
constexpr float kLastStart = 0.90f;        // 最後の光線が伸び始める時刻(Crack が終わるまでに、伸び切る余裕を残す)
constexpr float kStartCurve = 1.0f;        // 1 で、一定の間隔。1 より大きいと、始めはまばらで、後半に増える
// 伸び切るまでにかかる時間(勢いよく伸びる)。光線が出る間隔(約 0.25 秒)より短くして、1 本ずつ出て見えるようにする
constexpr float kGrowSpan = 0.08f;
// --- 形 ---
constexpr float kMaxLength = 24.0f;        // 光線の長さ(ワールド単位。体の中心からの長さ。ボスは幅 約 9)
constexpr float kLengthJitter = 0.35f;     // 長さのばらつき(0 〜 1。大きいほど、短い光線が混ざる)
constexpr float kWidth = 1.1f;             // 根元の幅(ワールド単位)
constexpr float kWidthJitter = 0.3f;       // 幅のばらつき(0 〜 1)
constexpr float kMidPosition = 0.8f;       // 先端の手前の中間点の位置(光線の長さに対する割合)。ここまでは太さと明るさを保つ
constexpr float kMidAgeRatio = 0.3f;       // 中間点の、リボンの年齢の割合(0 で最も太く明るい、1 で最も細く薄い)。小さいほど、先端の近くまで太い
constexpr float kRibbonLifetime = 1.0f;    // リボンの寿命(秒)。年齢の割合から age を作るための基準で、見た目には影響しない
// --- 色(濃い縁と、白い芯。明るい空や雲の上でも見えるよう、空の青の補色の赤橙) ---
inline const Vector4 kColor{1.0f, 0.18f, 0.04f, 1.0f};
} // namespace BossLightBeamDefaults

struct BossLightBeamSettings {
  uint32_t beamCount = BossLightBeamDefaults::kBeamCount;
  float pickChance = BossLightBeamDefaults::kPickChance;
  float firstStart = BossLightBeamDefaults::kFirstStart;
  float lastStart = BossLightBeamDefaults::kLastStart;
  float startCurve = BossLightBeamDefaults::kStartCurve;
  float growSpan = BossLightBeamDefaults::kGrowSpan;
  float maxLength = BossLightBeamDefaults::kMaxLength;
  float lengthJitter = BossLightBeamDefaults::kLengthJitter;
  float width = BossLightBeamDefaults::kWidth;
  float widthJitter = BossLightBeamDefaults::kWidthJitter;
  float midPosition = BossLightBeamDefaults::kMidPosition;
  float midAgeRatio = BossLightBeamDefaults::kMidAgeRatio;
  Vector4 color = BossLightBeamDefaults::kColor;
};

class BossLightBeams {
public:
  // 演出を始める。shell: ボスの外殻のオブジェクト(ModelComponent を持つ)。
  // crackShape: ひび割れの形(外殻の描画に設定したものと同じ値)。crackLineWidth: ひびの線の太さ(外殻の描画に設定したものと同じ値)。
  // crackShape.cellSize が 0 以下なら、光線は出ない
  void Begin(std::shared_ptr<AbsoluteEngine::GameObject> shell, const BossCrackShape &crackShape, float crackLineWidth,
             const BossLightBeamSettings &settings);

  // いまのサブフェーズと、その中の進み具合(0 〜 1)から、光線を伸ばす。毎フレーム呼ぶ(Begin のあと)
  void Update(ClearPhase phase, float phaseProgress);

  // 光線を描く。シーンの Draw の中で、物体を描いたあとに呼ぶ
  void Draw();

  // 演出の最中か(Begin のあと)
  bool IsActive() const { return shell_ != nullptr; }

private:
  // 光線 1 本分
  struct Beam {
    uint32_t vertexIndex = 0;   // 根元の頂点の番号(モデルの頂点バッファの中の番号)
    Vector3 bindPosition{};     // 根元の、モデル本来の位置(スキニング前)
    uint32_t boneIDs[4] = {0, 0, 0, 0}; // 根元の頂点に影響する骨の番号(スキニングの計算に使う。GPU のスキニングと同じ値)
    float weights[4] = {0.0f, 0.0f, 0.0f, 0.0f}; // それぞれの骨の重み(合計 1)
    float startAt = 0.0f;       // 伸び始める時刻(Crack の長さを 1 としたときの、0 〜 1)
    float heatStart = 0.0f;     // 光線の通る点のセルが、光り始める時刻(Peel の長さを 1 としたときの、0 〜 1。この時刻から、縮み始める)
    float vanishAt = 0.0f;      // そのセルが、消える時刻(同じ単位。この時刻に、縮み切って消える)
    float length = 0.0f;        // 伸び切ったときの長さ
    float width = 0.0f;         // 根元の幅
    RibbonTrail trail;          // 描く帯
    bool visible = false;       // 今フレーム、描くか
  };

  // 選ばれた点から、方向がばらけるように光線の根元を選び、光線の一覧を作る
  void BuildBeams_(const std::vector<CrackPointPicker::PickedPoint> &candidates);
  // 光線 1 本の、今のワールドの形(根元から先端)を決めて、リボンへ反映する。
  // timeline: 光線が伸びる時刻(Crack の長さを 1 とした 0 〜 1)。breakAmount: セルが消えた割合(Peel の長さを 1 とした 0 〜 1。外殻の描画と同じ値)
  void UpdateBeam_(Beam &beam, float timeline, float breakAmount);
  // 光線の根元を、スキニングした後のワールド座標へ移す(骨の行列は、GPU のスキニングと同じものを CPU で使う)。行列が取れないときは false
  bool SkinnedWorldPosition_(const Beam &beam, Vector3 &out) const;
  // サブフェーズと進み具合を、Crack を 0 〜 1 とした時刻へ直す(Peel 以降は 1)
  float ToTimeline_(ClearPhase phase, float progress) const;
  // サブフェーズと進み具合を、セルが消えた割合へ直す(Crack までは 0、Peel で 0 から 1 へ、Burst 以降は 1。BossDefeatVisual の cellBreak と同じ)
  float ToBreakAmount_(ClearPhase phase, float progress) const;

  std::shared_ptr<AbsoluteEngine::GameObject> shell_;
  BossCrackShape crackShape_{};
  BossLightBeamSettings settings_;
  std::vector<Beam> beams_;
  bool pickRequested_ = false; // GPU に、候補の選択を依頼したか
  bool built_ = false;         // 光線の一覧を作り終えたか(候補を受け取って、選んだ後)
  std::mt19937 random_;
};
