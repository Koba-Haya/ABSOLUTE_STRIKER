#pragma once
#include <memory>

#include "AbsoluteEngine/scene/GameObject.h"
#include "../actor/Enemy/BossModelSpec.h"
#include "BossDustHold.h"
#include "BossExplosion.h"
#include "BossLightBeams.h"
#include "BossShardInfo.h"
#include "BossShatter.h"
#include "ClearDirector.h"
#include "Type/Vector.h"

// ボス撃破の演出の、ボスの見た目(外殻の割れ方、消えたセルから出る粒)を決めるクラス。
// ClearDirector が時間(サブフェーズと、その中の進み具合)を決め、このクラスが、その値を、ボスのオブジェクトへ反映する。
// 分けた理由: 時間の枠(ClearDirector)と、見た目の中身(ここ)を分けて、見た目の調整で時間の管理を触らないため。
//
// 仕組み: ボスは、外殻(shell)のモデルだけで描く。割れ方は、3 通りある。
//   ボロノイ: 外殻の表面に、3D ボロノイのひびが光って広がり、セルが光って消えていく。消えるセルの頂点から、光の粒が出る。
//             体の中心から、光線(BossLightBeams)が放射状に伸びる(Crack の間に、1 本ずつ増えて、出そろう)。
//             Peel で崩壊が始まると同時に、爆破の球(BossExplosion)が、中心から広がる
//   欠片があるボス: 撃破の瞬間に、外殻を消して、あらかじめ割ってある欠片のモデルへ差し替える。欠片の動きは BossShatter が決める
//   どちらもないボス: ディゾルブで消える。マスク画像が、外殻の UV の上のボロノイで、「溝(ひび)が開く -> セルが縁から消える(剥がれる)」の順に消える
// サブフェーズごとの動き:
//   HitStop : 何もしない(外殻、または組み立てた欠片のまま)
//   Crack   : ひびが広がり、光線が 1 本ずつ増える(欠片は、継ぎ目が開く。ディゾルブは、溝が開く)
//   Peel    : 崩壊。セルが光って消え、粒が出る。爆破の球が広がる(欠片は、外側から解放されて飛ぶ。ディゾルブは、外殻が剥がれ落ちる)
//   Burst   : 外殻はすべて消えている(欠片は、残りが弾け飛ぶ)
//   Flow    : 外殻はすべて消えている(欠片も縮んで消える)
//   Cover   : すべて消えた状態(スキップで、直接ここへ来ても、見た目が揃うようにする)

// 見た目の設定(ステージ(ボス)ごとに変わりうる値なので、コードに埋め込まず、まとめて持つ)
namespace BossDefeatDefaults {
// ひびと剥がれの縁の光
inline const Vector3 kEdgeColor{1.0f, 0.78f, 0.30f}; // 縁の色(金)
constexpr float kEdgeRange = 0.10f;                    // 縁の幅(マスクの値の幅。大きいほど、溝の縁の光が太い)
constexpr float kEmberAmount = 0.0f;                   // 熾火(灰 -> 赤 -> 橙)にせず、金色の縁にする(0 で、縁の色へ混ぜるだけ)
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
// 粒の寿命 = 「爆散で解き放たれるまで(球の中に溜まっている間)」+ 下の「解き放たれたあとに残る寿命」。
// 粒は、溜めている間は消えないよう、出した時点で、解放までの残り時間を足した寿命にする(BossDustHold が、溜めて、解き放つ)
constexpr float kDustLifeAfterReleaseMin = 1.2f;       // 解き放たれたあとに残る寿命(秒)の範囲
constexpr float kDustLifeAfterReleaseMax = 2.4f;
// 次の島へ飛ばすとき: 粒の寿命 = 解放までの残り + Burst + Flow + この余裕(目的地へ着いたあと、少し残って消える)
constexpr float kFlightLifeMarginMin = 0.2f;
constexpr float kFlightLifeMarginMax = 0.9f;
constexpr float kDustSizeMin = 0.12f;                  // 大きさ(ワールド単位。ボスは幅 約 9)
constexpr float kDustSizeMax = 0.32f;
constexpr float kDustSpeedMin = 0.2f;                  // 生まれたときの、向きがランダムな速さ
constexpr float kDustSpeedMax = 1.2f;
constexpr float kDustOutwardSpeed = 2.5f;              // ひびの広がり始める位置から、外向きに足す速さ(ワールド単位 / 秒)
} // namespace BossDefeatDefaults

struct BossDefeatSettings {
  Vector3 edgeColor = BossDefeatDefaults::kEdgeColor;
  float edgeRange = BossDefeatDefaults::kEdgeRange;
  float emberAmount = BossDefeatDefaults::kEmberAmount;
  Vector3 crackColor = BossDefeatDefaults::kCrackColor;
  float crackIntensity = BossDefeatDefaults::kCrackIntensity;
  float crackLineWidth = BossDefeatDefaults::kCrackLineWidth;
  float dustEmitChance = BossDefeatDefaults::kDustEmitChance;
  Vector3 dustColorStart = BossDefeatDefaults::kDustColorStart;
  Vector3 dustColorEnd = BossDefeatDefaults::kDustColorEnd;
  float dustCoolRatio = BossDefeatDefaults::kDustCoolRatio;
  float dustFadePower = BossDefeatDefaults::kDustFadePower;
  float dustLifeAfterReleaseMin = BossDefeatDefaults::kDustLifeAfterReleaseMin;
  float dustLifeAfterReleaseMax = BossDefeatDefaults::kDustLifeAfterReleaseMax;
  // 粒に溜める時間と、飛ぶ時間を求めるための、Peel、Burst、Flow の長さ(秒)。ClearSettings(撃破演出の長さ)と、同じ値にすること(GameScene が、演出を始める前に合わせる)
  float peelSeconds = ClearDefaults::kPeelSeconds;
  float burstSeconds = ClearDefaults::kBurstSeconds;
  float flowSeconds = ClearDefaults::kFlowSeconds;
  // 次の島へ飛ばすときの、粒の寿命の余裕(Flow の終わりを過ぎて、残る時間。秒の範囲)。目的地へ着いたあと、少し残って、消える
  float flightLifeMarginMin = BossDefeatDefaults::kFlightLifeMarginMin;
  float flightLifeMarginMax = BossDefeatDefaults::kFlightLifeMarginMax;
  float dustSizeMin = BossDefeatDefaults::kDustSizeMin;
  float dustSizeMax = BossDefeatDefaults::kDustSizeMax;
  float dustSpeedMin = BossDefeatDefaults::kDustSpeedMin;
  float dustSpeedMax = BossDefeatDefaults::kDustSpeedMax;
  float dustOutwardSpeed = BossDefeatDefaults::kDustOutwardSpeed;
  BossLightBeamSettings beams; // 光線(体から放射状に伸びる光の帯)
  BossExplosionSettings explosion; // 爆破の球(崩壊と同時に、体の中心から広がる)
  BossDustHoldSettings dustHold;   // 粒を球の中に溜めて、爆散で解き放つ力
  BossShatterSettings shatter; // 欠片の動き(欠片があるボスだけが使う)
};

class BossDefeatVisual {
public:
  // 演出を始める。shell: 外殻のオブジェクト(ModelComponent と DissolveComponent を持つ)。
  // crackEndThreshold: マスクの、ひびの段階の最後の値(BossModelSpec::crackEndThreshold)。
  // shards: 欠片のモデルのオブジェクト(なければ nullptr。あれば、外殻を消して、欠片へ差し替える。位置・向き・大きさは、ボスと同じにしておく)。
  // shardInfo: 欠片の付属データ(shards があるときだけ使う)
  // crackShape: ボロノイのひび割れの形(BossModelSpec::crackShape)。cellSize が 0 以下なら、ひび割れは使わず、ディゾルブだけで消える
  void Begin(std::shared_ptr<AbsoluteEngine::GameObject> shell, std::shared_ptr<AbsoluteEngine::GameObject> shards,
             const BossShardInfo &shardInfo, float crackEndThreshold, const BossCrackShape &crackShape,
             const BossDefeatSettings &settings);

  // いまのサブフェーズと、その中の進み具合(0 〜 1。ClearDirector が決める)を、ボスの見た目へ反映する。毎フレーム呼ぶ。
  // deltaTime: 1 フレームの実時間(秒。欠片の飛び方の積分に使う)
  void Update(ClearPhase phase, float phaseProgress, float deltaTime);

  // シーンの開始時に 1 回呼ぶ。爆破の球のモデルを読み込む(演出の最中に、読み込みで画面が止まらないようにする)
  void Initialize();

  // 光線と爆破の球を描く。シーンの Draw の中で、物体を描いたあとに呼ぶ
  void Draw();

  // 爆散の白いフラッシュを描く。カメラのすぐ前の板なので、シーンの Draw の最後に、描画のカメラの位置と注視点を渡して呼ぶ
  void DrawFlash(const Vector3 &eye, const Vector3 &target);

  // 演出の最中か(Begin のあと)
  bool IsActive() const { return shell_ != nullptr; }

  // 粒を飛ばす目的地(次の島。ワールド)を設定する。Begin のあと、粒が出る Peel の前に呼ぶ(Begin が、前の値を消す)。
  // 呼ばなければ、粒は、解き放たれたあと、ゆるく漂うだけ
  void SetFlightTarget(const Vector3 &target);
  // 粒を飛ばす目的地を取り出す。設定されていなければ false
  bool GetFlightTarget(Vector3 &target) const;

  // 撃破演出のカメラが回る軸(爆破の球の中心。体の動きに付いてくる)を求める。ボロノイのひびを使わないボス(軸が決まらない)は false
  bool GetBurstCenter(Vector3 &center) const;

  // 粒に掛けた力(溜める力、解き放つ力)を外す。シーンの終わりに呼ぶ(力は、GPU パーティクルの共有の枠なので、次のシーンに残さない)
  void Shutdown();

  // 演出を、始まる前の状態へ戻す(IsActive が false になり、光線・爆破の球は描かれなくなる)。粒に掛けた力も外す。
  // Begin の始めと、エディタの Stop(プレイの状態を最初へ戻すとき)に呼ぶ。Initialize で読み込んだモデルは残す
  void Reset();

private:
  // 爆破の球の中心(体の中心のワールド位置。体の動きに付いてくる)を求める。求められなければ false
  bool ComputeBurstCenter_(Vector3 &center) const;
  // 外殻のディゾルブを設定する(enable が false なら、元の見た目)。欠片へ差し替えているときは、指定によらず、外殻を全て消す
  void ApplyShell_(bool enable, float threshold);
  // 外殻のボロノイのひびの、広がった割合(reveal)と、セルが消えた割合(breakAmount)を、0 〜 1 で設定する。
  // ひび割れを使わないボスでは、何もしない
  void ApplyCrack_(float reveal, float breakAmount = 0.0f);
  // ボロノイのひび割れを使うか(形が決まっていて、欠片へ差し替えていないとき)
  bool UsesVoronoiCrack_() const;
  // セルが消えた割合が、previousBreak から currentBreak へ進む間に消えるセルの頂点から、粒を出す(GPU パーティクルへ依頼する)
  // secondsToRelease: 爆散で解き放たれるまでの残り時間(秒)。粒の寿命に足して、溜まっている間に消えないようにする
  void EmitCellDust_(float previousBreak, float currentBreak, float secondsToRelease);

  std::shared_ptr<AbsoluteEngine::GameObject> shell_;
  BossLightBeams beams_; // 光線(体の中心から、ひびの線の上の点を通って、放射状に伸びる)
  BossExplosion explosion_; // 爆破の球(崩壊と同時に、体の中心から広がる)
  BossDustHold dustHold_;   // 粒を球の中に溜めて、爆散で解き放つ力
  bool hasFlightTarget_ = false; // 粒を飛ばす目的地が設定されているか
  Vector3 flightTarget_{0.0f, 0.0f, 0.0f};
  BossShatter shatter_; // 欠片の動き(欠片があるボスだけ。IsActive が true なら、欠片へ差し替え済み)
  float crackEnd_ = 0.0f;
  BossCrackShape crackShape_{};
  float lastCellBreak_ = 0.0f; // 前のフレームまでに、粒を出し終えた、セルが消えた割合(次の依頼の始まりの値)
  BossDefeatSettings settings_;
};
