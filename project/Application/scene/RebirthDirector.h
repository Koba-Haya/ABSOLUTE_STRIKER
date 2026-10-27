#pragma once
#include "Vector.h"

// 復活の演出(ゲームオーバー画面でリトライを選んだとき)の、時間の進行と、各部への指示を決めるクラス。
// 不死鳥が灰から復活する場面を参考にした演出:
//   Gather : 灰が一気に巣へ集まり、核が強く光る。見出しと案内は消える
//   Appear : 核の中から、蝶が、ディゾルブの逆再生(熾火の縁)で現れる。翅は半分開いて、上から面が見える(アニメ revive の前半)
//   Charge : 溜め。翅が背中の上へ閉じていき、体が沈み、震えが最大になる。灰が胴体へ吸い込まれ、核が小さく強く光る。最後の 0.2 秒は静止
//   Burst  : 翅が一気に全開になる。ヒットストップ、鱗粉の大バースト、翅の閃光、地面の吹き飛び、カメラの小さな寄り
//   Settle : 落ち着き。蝶が少し浮かび、鱗粉がゆっくり落ちる。羽ばたきへの準備
//   Fly    : 羽ばたいて、上へ、前方(+z)へ飛び立つ。カメラが蝶を追って上を向き、雲の層を抜ける。
//            抜ける間に、ゲームシーンへの切り替えを要求する(雲抜けの遷移が、これをつなぐ)
// PlayerDeathDirector と同じ作り: 何も直接は動かさない。シーンが Update で時間を進め、GetState の値を各部(灰、蝶、カメラ、見出し)へ反映する。
// クラスを分けた理由: タイムライン(いつ、何を、どれだけ)だけを持ち、描画やモデルの扱いは RebirthButterfly と GameOverBackdrop に任せるため。

enum class RebirthPhase {
  Idle,
  Gather,
  Appear,
  Charge,
  Burst,
  Settle,
  Fly,
  Done,
};

// 演出の既定値(秒、距離、強さ)。値は ImGui(Rebirth)で調整でき、気に入った値をここへ写す
namespace RebirthDefaults {
// 各段階の長さ(秒)。合計は 9.7 秒。飛び立ちは、切り替えを要求したあとの雲抜けの覆い(1.7 秒)が終わるまでを含む
constexpr float kGatherSeconds = 1.5f;
constexpr float kAppearSeconds = 2.2f;
constexpr float kChargeSeconds = 1.3f;
constexpr float kBurstSeconds = 0.4f;
constexpr float kSettleSeconds = 0.8f;
constexpr float kFlySeconds = 3.5f;
// 飛び立つ段階の、この割合まで進んだらゲームへの切り替えを要求する(以後、雲抜けの遷移が 1.7 秒かけて画面を覆い切り、入れ替わる)
constexpr float kSwitchRatio = 0.5f;

// 蝶のアニメ revive の時間割(秒。アニメの中の時刻)。各段階が、この区間を再生する:
//   Appear = 0.00〜kReviveAppearEndSec(現れ)、Charge = 〜kReviveChargeEndSec(溜め)、Burst = 〜kReviveBurstEndSec(バースト)、Settle = 〜kReviveLengthSec(落ち着き)
// 完成した revive の表(引き継ぎ書 9 章)の値に合わせる。revive が無いときは、代役の emerge を、この時間割を長さの割合に直して再生する
constexpr float kReviveAppearEndSec = 1.00f;
constexpr float kReviveChargeEndSec = 2.00f;
constexpr float kReviveBurstEndSec = 2.25f;
constexpr float kReviveLengthSec = 3.00f;
// アニメの中の、特別な時刻(秒)。revive の表(gen_butterfly.py の REVIVE_*)に合わせる:
//   kReviveStillStartSec : 溜めの最後の静止が始まる時刻(1.8 から 2.0 は、ほぼ静止。震えが消える)
//   kReviveOvershootSec  : バーストで、翅が開きすぎる(最大に開く)時刻。閃光のピークとバースト(鱗粉、地面)を、ここに合わせる
constexpr float kReviveStillStartSec = 1.80f;
constexpr float kReviveOvershootSec = 2.10f;

// 灰の集まり(Gather): 集まる速さの倍率と、核を強める量のピーク。見出しと案内は、この段階の前半で消える
constexpr float kGatherSpeedScale = 4.0f;
constexpr float kBoostPeak = 1.5f;
constexpr float kUiFadeRatio = 0.6f;         // 見出しと案内が消えきる、Gather の割合
// 核の強さ(足す量): 蝶が現れ始めたら素早く落とす値(Appear の最初の kCoreFadeRatio の間)。蝶が光に覆われないようにする
constexpr float kBoostAfterAppear = -0.4f;
constexpr float kCoreFadeRatio = 0.35f;
// 核が巣から蝶の胴体へ移る時間(秒。Appear の始まりから)
constexpr float kCoreMoveSeconds = 0.3f;
// 溜め(Charge): 核を小さく強く(圧縮)し、灰を胴体へ強く吸い込む。震えに合わせて脈打ち、アニメが静止する区間(kReviveStillStartSec から溜めの終わり)は脈打ちも止める
constexpr float kChargeBoost = 0.9f;          // 溜めの終わりの核の強さ(足す量)
constexpr float kChargeCoreSizeScale = 0.45f; // 溜めの終わりの核の大きさの倍率(1 で通常)
constexpr float kChargeConvergeScale = 3.0f;  // 溜めの終わりの灰の収束の強さの倍率
constexpr float kChargePulseHz = 7.0f;
constexpr float kChargePulseDepth = 0.25f;
constexpr float kBoostExtinguished = -1.0f;   // バーストで、核が使い切られて消える値

// ディゾルブの逆再生: 胴体(マスク 0.95)が先、翅(0.1〜0.8)が遅れる。翅が見えるようになったので、遅らせる量は弱めにする
constexpr float kWingDelayRatio = 0.10f;
constexpr float kBodyThreshold = 0.85f;
constexpr float kDissolveEdgeRange = 0.12f;

// 蝶の大きさ(翼幅 3.7 に対する倍率)と位置(核からの高さ)
constexpr float kStartScale = 0.50f;         // 現れ始めの大きさ
constexpr float kAppearEndScale = 0.80f;     // 現れ終わり(溜めの間も保つ)の大きさ
constexpr float kFullScale = 1.5f;           // バーストで広がりきった大きさ(翼幅 3.7 x 1.5 = 約 5.6)
constexpr float kAppearLift = 2.0f;          // 現れる高さ(核の上)
constexpr float kHoverHeight = 3.5f;         // バーストのあとの高さ(核の上)
constexpr float kSettleRise = 0.4f;          // 落ち着きの間に、さらに浮かぶ量

// バースト
constexpr float kHitStopSeconds = 0.12f;     // ヒットストップ(灰、島、粒を、ほぼ止める時間)
constexpr float kHitStopScale = 0.05f;       // ヒットストップ中の時間の速さ
constexpr float kZoomAmount = 0.04f;         // 画面の寄り(画角を狭める割合)
constexpr float kZoomInSeconds = 0.10f;
constexpr float kZoomOutSeconds = 0.35f;
constexpr float kFlashSeconds = 0.30f;       // 翅の閃光
constexpr float kFlashGain = 3.0f;
constexpr float kFlashColorR = 0.55f;
constexpr float kFlashColorG = 1.00f;
constexpr float kFlashColorB = 0.95f;
// 鱗粉の大バースト(金から青緑へ冷める。粒ごとの色で光る)。粒のプールは 1024 なので、灰と合わせて収まる数にする
constexpr float kBurstCount = 400.0f;
constexpr float kBurstRadius = 0.5f;
constexpr float kBurstSpeedMin = 6.0f;
constexpr float kBurstSpeedMax = 16.0f;
constexpr float kBurstUpward = 3.0f;         // 上向きの速さを足す
constexpr float kBurstLifeMin = 2.5f;
constexpr float kBurstLifeMax = 4.5f;
constexpr float kBurstSizeMin = 0.08f;
constexpr float kBurstSizeMax = 0.18f;
constexpr float kBurstGoldR = 1.00f;
constexpr float kBurstGoldG = 0.82f;
constexpr float kBurstGoldB = 0.30f;
constexpr float kBurstTealR = 0.20f;
constexpr float kBurstTealG = 0.75f;
constexpr float kBurstTealB = 1.00f;
// 地面の吹き飛び(巣の地面から、灰と土の粒が、外側へ低い角度で)
constexpr float kGroundCount = 140.0f;
constexpr float kGroundSpeedMin = 4.0f;
constexpr float kGroundSpeedMax = 8.0f;
constexpr float kGroundUpward = 1.0f;

// 飛び立ち: 上と前方(+z)への距離(加速しながら進む)と、機首を上げる角度(ラジアン)
constexpr float kFlyUp = 30.0f;       // 雲の層の上面(巣から約 18 上)を突き抜けて、その上へ出る高さ
constexpr float kFlyForward = 25.0f;
constexpr float kFlyEasePower = 1.6f; // 飛び立ちの加速の指数(1 で等速、2 で 2 次の加速)
constexpr float kNoseUpRadians = 0.5f;

// カメラ: 降下の終点から、巣の近くへ寄る(Appear から Charge にかけて)。寄せた先の、巣の中心からの高さと後ろの距離。
// 飛び立つ段階で、蝶の動きを追う割合(1 で蝶と同じだけ動く)
constexpr float kApproachHeight = 6.0f;
constexpr float kApproachBack = 8.0f;
constexpr float kFollowRatio = 0.9f;

// 雲が画面を満たす(Fly の進みの割合。この範囲で、0 から 1 へ)。層の上面を抜ける前に、雲を濃く(ゲームの雲海と同じに)して、抜けたあとは、日の当たる雲海の上の景色にする
constexpr float kFillStart = 0.05f;
constexpr float kFillEnd = 0.50f;
} // namespace RebirthDefaults

struct RebirthSettings {
  float gatherSeconds = RebirthDefaults::kGatherSeconds;
  float appearSeconds = RebirthDefaults::kAppearSeconds;
  float chargeSeconds = RebirthDefaults::kChargeSeconds;
  float burstSeconds = RebirthDefaults::kBurstSeconds;
  float settleSeconds = RebirthDefaults::kSettleSeconds;
  float flySeconds = RebirthDefaults::kFlySeconds;
  float switchRatio = RebirthDefaults::kSwitchRatio;
  float reviveAppearEndSec = RebirthDefaults::kReviveAppearEndSec;
  float reviveChargeEndSec = RebirthDefaults::kReviveChargeEndSec;
  float reviveBurstEndSec = RebirthDefaults::kReviveBurstEndSec;
  float reviveLengthSec = RebirthDefaults::kReviveLengthSec;
  float reviveStillStartSec = RebirthDefaults::kReviveStillStartSec;
  float reviveOvershootSec = RebirthDefaults::kReviveOvershootSec;
  float gatherSpeedScale = RebirthDefaults::kGatherSpeedScale;
  float boostPeak = RebirthDefaults::kBoostPeak;
  float uiFadeRatio = RebirthDefaults::kUiFadeRatio;
  float boostAfterAppear = RebirthDefaults::kBoostAfterAppear;
  float coreFadeRatio = RebirthDefaults::kCoreFadeRatio;
  float coreMoveSeconds = RebirthDefaults::kCoreMoveSeconds;
  float chargeBoost = RebirthDefaults::kChargeBoost;
  float chargeCoreSizeScale = RebirthDefaults::kChargeCoreSizeScale;
  float chargeConvergeScale = RebirthDefaults::kChargeConvergeScale;
  float chargePulseHz = RebirthDefaults::kChargePulseHz;
  float chargePulseDepth = RebirthDefaults::kChargePulseDepth;
  float boostExtinguished = RebirthDefaults::kBoostExtinguished;
  float wingDelayRatio = RebirthDefaults::kWingDelayRatio;
  float bodyThreshold = RebirthDefaults::kBodyThreshold;
  float dissolveEdgeRange = RebirthDefaults::kDissolveEdgeRange;
  float startScale = RebirthDefaults::kStartScale;
  float appearEndScale = RebirthDefaults::kAppearEndScale;
  float fullScale = RebirthDefaults::kFullScale;
  float appearLift = RebirthDefaults::kAppearLift;
  float hoverHeight = RebirthDefaults::kHoverHeight;
  float settleRise = RebirthDefaults::kSettleRise;
  float hitStopSeconds = RebirthDefaults::kHitStopSeconds;
  float hitStopScale = RebirthDefaults::kHitStopScale;
  float zoomAmount = RebirthDefaults::kZoomAmount;
  float zoomInSeconds = RebirthDefaults::kZoomInSeconds;
  float zoomOutSeconds = RebirthDefaults::kZoomOutSeconds;
  float flashSeconds = RebirthDefaults::kFlashSeconds;
  float flashGain = RebirthDefaults::kFlashGain;
  Vector3 flashColor{RebirthDefaults::kFlashColorR, RebirthDefaults::kFlashColorG, RebirthDefaults::kFlashColorB};
  float burstCount = RebirthDefaults::kBurstCount;
  float burstRadius = RebirthDefaults::kBurstRadius;
  float burstSpeedMin = RebirthDefaults::kBurstSpeedMin;
  float burstSpeedMax = RebirthDefaults::kBurstSpeedMax;
  float burstUpward = RebirthDefaults::kBurstUpward;
  float burstLifeMin = RebirthDefaults::kBurstLifeMin;
  float burstLifeMax = RebirthDefaults::kBurstLifeMax;
  float burstSizeMin = RebirthDefaults::kBurstSizeMin;
  float burstSizeMax = RebirthDefaults::kBurstSizeMax;
  Vector3 burstGold{RebirthDefaults::kBurstGoldR, RebirthDefaults::kBurstGoldG, RebirthDefaults::kBurstGoldB};
  Vector3 burstTeal{RebirthDefaults::kBurstTealR, RebirthDefaults::kBurstTealG, RebirthDefaults::kBurstTealB};
  float groundCount = RebirthDefaults::kGroundCount;
  float groundSpeedMin = RebirthDefaults::kGroundSpeedMin;
  float groundSpeedMax = RebirthDefaults::kGroundSpeedMax;
  float groundUpward = RebirthDefaults::kGroundUpward;
  float flyUp = RebirthDefaults::kFlyUp;
  float flyForward = RebirthDefaults::kFlyForward;
  float flyEasePower = RebirthDefaults::kFlyEasePower;
  float noseUpRadians = RebirthDefaults::kNoseUpRadians;
  float approachHeight = RebirthDefaults::kApproachHeight;
  float approachBack = RebirthDefaults::kApproachBack;
  float followRatio = RebirthDefaults::kFollowRatio;
  float fillStart = RebirthDefaults::kFillStart;
  float fillEnd = RebirthDefaults::kFillEnd;
};

// 演出を始めるときの、周りの状態
struct RebirthStart {
  Vector3 corePosition{0.0f, 0.0f, 0.0f};  // 核(蝶が現れる所)のワールド位置
  Vector3 nestCenter{0.0f, 0.0f, 0.0f};    // 巣のくぼみの中心(カメラの注視点)
  Vector3 cameraEye{0.0f, 0.0f, 0.0f};     // 始まりのカメラ(降下の終点)
  Vector3 cameraTarget{0.0f, 0.0f, 1.0f};
};

// 今の瞬間の、各部への指示
struct RebirthState {
  RebirthPhase phase = RebirthPhase::Idle;
  // 灰と核
  float gatherSpeedScale = 1.0f; // 灰の集まる速さの倍率
  float convergeScale = 1.0f;    // 灰の収束の強さの倍率(溜めで上げる)
  float coreBoost = 0.0f;        // 核の強さに足す量
  float coreSizeScale = 1.0f;    // 核の大きさの倍率(溜めで小さくする = 圧縮)
  float coreFollowBlend = 0.0f;  // 核が巣から蝶の胴体へ移った割合(0 で巣、1 で胴体)
  float ashSpawnScale = 1.0f;    // 灰の発生量の倍率(Charge で 0 へ。胴体へ吸い込まれて消える)
  // 世界(灰、島、粒)の時間の速さ(ヒットストップで下げる)と、画角の倍率(バーストの寄り)
  float timeScale = 1.0f;
  float fovScale = 1.0f;
  // 見出しと案内の透明度の倍率(1 で通常、0 で消える)
  float uiOpacity = 1.0f;
  // 蝶
  bool butterflyVisible = false;
  Vector3 butterflyPosition{0.0f, 0.0f, 0.0f};
  Vector3 butterflyRotate{0.0f, 0.0f, 0.0f}; // Transform の回転(x が負で機首上げ)
  float butterflyScale = 1.0f;               // 翼幅 3.7 に対する倍率
  bool dissolveEnabled = false;
  float dissolveThreshold = 0.0f;            // 逆再生: 1(何も見えない)から 0(全部見える)へ
  float dissolveEdgeRange = 0.0f;
  bool reviveActive = false;                 // アニメ revive の時刻を指定する段階か(Appear から Settle)
  float reviveSeconds = 0.0f;                // その段階の、アニメの中の時刻(秒。0〜reviveLengthSec)
  float reviveLengthSeconds = 3.0f;          // アニメ revive の長さの想定(秒。代役のアニメの時刻に直すのに使う)
  bool flapping = false;                     // 羽ばたき(Flap)に切り替える段階か
  float flashAmount = 0.0f;                  // 翅の閃光の強さ(明るさに足す倍率。バーストの瞬間に最大で、すぐ 0 へ)
  Vector3 flashColor{1.0f, 1.0f, 1.0f};      // 閃光の色(玉虫色の青緑寄り)
  // カメラ(true の間、背景の通常のカメラを上書きする)
  bool cameraOverride = false;
  Vector3 cameraEye{0.0f, 0.0f, 0.0f};
  Vector3 cameraTarget{0.0f, 0.0f, 1.0f};
  // 雲が画面を満たす度合い(0 〜 1)
  float cloudFill = 0.0f;
};

class RebirthDirector {
public:
  // 演出を始める(1 回だけ。すでに始まっていれば何もしない)
  void Begin(const RebirthStart &start);
  // 1 フレーム進める(実時間)
  void Update(float deltaTime);

  bool IsActive() const { return state_.phase != RebirthPhase::Idle && state_.phase != RebirthPhase::Done; }
  const RebirthState &GetState() const { return state_; }
  float GetElapsed() const { return elapsed_; }
  float GetTotalSeconds() const;
  // 切り替えを要求する時刻に着いたフレームに、1 回だけ true を返す
  bool ConsumeSceneChange();
  // 翅が開きすぎる瞬間(バーストの始まりから、アニメの時刻 reviveOvershootSec に当たる時刻)に、1 回だけ true を返す(鱗粉の大バーストと地面の吹き飛びを出す合図)
  bool ConsumeBurst();

  RebirthSettings &GetSettings() { return settings_; }

#ifdef USE_IMGUI
  // 調整窓。「Start rebirth」が押されたら true を返す(シーンが Begin を呼ぶ)
  bool DrawDebugUi();
#endif

private:
  void Evaluate_();

  RebirthSettings settings_;
  RebirthStart start_;
  RebirthState state_;
  float elapsed_ = 0.0f;
  bool sceneChangeRequested_ = false;
  bool sceneChangePending_ = false;
  bool burstTriggered_ = false;
  bool burstPending_ = false;
};
