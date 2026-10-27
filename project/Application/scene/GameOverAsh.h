#pragma once
#include "EmberCore.h"
#include "ScaleDust.h"
#include "Vector.h"
#include "graphics/particle/GPUParticle.h"

// ゲームオーバー画面の灰(鱗粉)の流れ: 雲の中を漂い、巣の島へ集まり、橙の小さな光(熾火の核)が灯る。
// 不死鳥が灰から復活する場面を参考にした演出で、死亡の崩壊(PlayerDeathDust)が散らした灰の続き。
//
// 状態(GameOverAshPhase):
//   Drifting  : 漂い。カメラの前方の広い範囲に、灰がゆっくり漂う
//   Gathering : 集まり。巣へ向かう収束(ばね)と渦で、灰が巣へ集まり、核が少しずつ強くなる
//   Glowing   : 灯り。灰は巣のまわりを回り、核がゆっくり脈打つ
// 状態は、ゲームオーバー画面の時間(降下が始まってからの秒)で自動で進む。外から SetPhase で指示してもよい
// (リトライの復活の演出が、この状態を進める)。
//
// クラスを分けた理由: ScaleDust は「粒の発生源と動き」だけ、EmberCore は「核の光」だけを持つ。このクラスは、
// ゲームオーバー画面の台本(いつ、どこへ、どれだけ)を決めて、その 2 つへ指示を出す役目。画面(GameOverScene)には何も書き込まない。
// 描画は GPU パーティクルで、雲の合成の前に深度を書いて描く(Renderer::DrawGPUParticles(blend, true))。
// 雲の合成が深度を見るので、灰も光も、手前の雲の濃さで自然にかすみ、島とも前後が正しく重なる。

// 復活の瞬間の鱗粉のバースト(再生の色。金から青緑へ変わる粒が、中心から放射状に広がる)。ReleaseBurst に渡す
struct GameOverAshBurst {
  uint32_t count = 0;
  float radius = 0.5f;                       // 粒が出る球の半径
  float speedMin = 1.2f;                     // 放射状に広がる速さ(向きはランダム)
  float speedMax = 3.6f;
  float lifeMin = 2.0f;
  float lifeMax = 4.0f;
  float sizeMin = 0.08f;
  float sizeMax = 0.18f;
  Vector3 startColor{1.0f, 0.8f, 0.3f};      // 出たての色
  Vector3 endColor{0.25f, 0.85f, 0.95f};     // 冷めたあとの色
  float upward = 0.0f;                       // 全ての粒に足す上向きの速さ
  bool glowOwnColor = true;                  // 発光パスでも、粒自身の色(startColor から endColor)で光らせる(false で熱さの橙・黄)
};

// 地面の吹き飛び(巣の地面から、外側へ低い角度で飛ぶ灰と土の粒)。ReleaseGroundBlast に渡す
struct GameOverAshGroundBlast {
  uint32_t count = 0;
  float radius = 0.3f;
  float speedMin = 4.0f;                     // 外側へ飛ぶ速さ
  float speedMax = 8.0f;
  float upward = 1.0f;                       // 上向きの速さ(低い角度にするため小さく)
  float lifeMin = 1.5f;
  float lifeMax = 3.0f;
  float sizeMin = 0.10f;
  float sizeMax = 0.25f;
  Vector3 startColor{0.50f, 0.38f, 0.26f};   // 土の色
  Vector3 endColor{0.40f, 0.38f, 0.36f};     // 灰の色
};

enum class GameOverAshPhase {
  Drifting,
  Gathering,
  Glowing,
};

namespace GameOverAshDefaults {
// --- 台本(降下が始まってからの秒。降下の長さ 4.5 秒に合わせる) ---
constexpr float kGatherStartSeconds = 0.0f;  // 集まりが始まる時刻(降下の始まり = カメラが降り始める頃)
constexpr float kGatherSeconds = 4.5f;       // 集まりの長さ(降下が終わる頃に、集まりきって灯りになる)

// --- 発生の範囲(カメラの前方。近景から遠景まで) ---
constexpr float kSpawnAheadDistance = 26.0f; // カメラから前方へ、発生の球の中心までの距離
constexpr float kSpawnRadius = 26.0f;        // 発生の球の半径(中心が前方なので、近景から遠景までを覆う)
constexpr float kSpawnRate = 90.0f;          // 1 秒に出す個数(粒のプール 1024 を超えないよう、寿命と合わせて決める)
constexpr float kGlowSpawnRateScale = 0.35f; // 灯りの間の発生量の倍率(灰の山が膨らみすぎないよう、減らす)

// --- 灰の見た目と漂い(控えめ。ScaleDustSettings の既定を、この画面用に上書きする) ---
constexpr float kLifeMin = 4.5f;
constexpr float kLifeMax = 8.0f;
constexpr float kSizeMin = 0.08f;
constexpr float kSizeMax = 0.22f;
constexpr float kSpeedMin = 0.05f;
constexpr float kSpeedMax = 0.35f;
constexpr float kDriftGravity = 0.03f;       // ほとんど落ちない(雲の中で浮かぶ)
constexpr float kDriftDrag = 0.5f;
constexpr float kDriftTurbulenceStrength = 0.5f;
constexpr float kDriftTurbulenceFrequency = 0.35f;
constexpr float kDriftTurbulenceSpeed = 0.4f;

// --- 集まりの力 ---
constexpr float kConvergeStiffness = 0.9f;   // ばね定数(1 / 秒^2)。集まりの最後の値
constexpr float kGatherDrag = 1.9f;          // 集まりの最後の空気抵抗(2 * sqrt(ばね定数) に近いと、振動せずに収束する)
constexpr float kSwirlStrength = 1.0f;       // 渦の強さ。収束と組むと、巣のまわりを回りながら集まる
constexpr float kGlowSwirlScale = 0.6f;      // 灯りの間の渦の倍率

// --- 核の強さ ---
constexpr float kCoreIntensity = 1.0f;       // 集まりきったときの核の強さ
constexpr float kCoreEaseStart = 0.10f;      // 集まりの進みが、この割合を超えてから核が灯り始める

// --- 発光パス(雲のあとに加算で描く層) ---
constexpr float kHotRadius = 5.0f;           // 巣からこの距離までの粒が、熱を帯びる
constexpr float kHotBoostMax = 0.8f;         // 集まりきったとき、巣に近い粒の熱さに足す量

constexpr float kMinGatherSeconds = 0.01f;   // 0 で割らないための下限
} // namespace GameOverAshDefaults

struct GameOverAshSettings {
  float gatherStartSeconds = GameOverAshDefaults::kGatherStartSeconds;
  float gatherSeconds = GameOverAshDefaults::kGatherSeconds;
  float spawnAheadDistance = GameOverAshDefaults::kSpawnAheadDistance;
  float spawnRadius = GameOverAshDefaults::kSpawnRadius;
  float spawnRate = GameOverAshDefaults::kSpawnRate;
  float glowSpawnRateScale = GameOverAshDefaults::kGlowSpawnRateScale;
  float convergeStiffness = GameOverAshDefaults::kConvergeStiffness;
  float gatherDrag = GameOverAshDefaults::kGatherDrag;
  float swirlStrength = GameOverAshDefaults::kSwirlStrength;
  float glowSwirlScale = GameOverAshDefaults::kGlowSwirlScale;
  float coreIntensity = GameOverAshDefaults::kCoreIntensity;
  float coreEaseStart = GameOverAshDefaults::kCoreEaseStart;
  GPUGlowParams glow;     // 発光パスの調整値(尾の長さ、ちらつき、芯・光輪の鋭さ、熱さの色の傾き、明るさ)
  float hotRadius = GameOverAshDefaults::kHotRadius;
  float hotBoostMax = GameOverAshDefaults::kHotBoostMax;
  ScaleDustSettings dust; // 灰の見た目と漂い(コンストラクタでこの画面用の既定にする)
  EmberCoreSettings core;

  GameOverAshSettings();
};

class GameOverAsh {
public:
  // 灰と核を使い始める(画面の開始時)。nestCenter: 巣の中心(ワールド)
  void Initialize(const Vector3& nestCenter);
  // エミッタ・力・点光源を返す(画面の終わり)
  void Shutdown();

  // 発光パスを描く(雲の合成のあと・見出しの前に、毎フレーム 1 回呼ぶ)。熱を帯びる中心と時刻を反映して、Renderer へ渡す
  void DrawGlow();

  // 毎フレーム 1 回、描画の前に呼ぶ。settledSeconds: 降下が始まってからの秒(晴れる前は 0)。
  // 外から状態を指示していない間は、この秒で状態が自動で進む
  void Update(float deltaTime, float settledSeconds);

  // --- 外から状態を進めるための口(復活の演出が使う) ---

  // 状態を指示する。以後、自動で漂い -> 集まり へは進まない(集まりが終われば灯りへは進む)。
  // Gathering は集まりの最初から、Glowing は集まりきった状態から始まる
  void SetPhase(GameOverAshPhase phase);
  // 自動の進行へ戻す(指示を解く)
  void ClearPhaseOverride();
  GameOverAshPhase GetPhase() const { return phase_; }
  // 集まりの進み(0 〜 1。Glowing は 1)
  float GetGatherProgress() const;
  // 集まりが進む速さの倍率(1 で通常。上げると、早く集まりきる)
  void SetGatherSpeedScale(float scale) { gatherSpeedScale_ = scale; }
  float GetGatherSpeedScale() const { return gatherSpeedScale_; }
  // 核の光を強める量(今の強さに足す。0 で足さない)
  void SetCoreBoost(float boost) { coreBoost_ = boost; }
  float GetCoreBoost() const { return coreBoost_; }
  // 中心から、再生の色の粒を放射状に放つ(1 回。次の Update で出る)。出る粒だけ、色・速さ・寿命・大きさを burst の値にする
  void ReleaseBurst(const Vector3& center, const GameOverAshBurst& burst) {
    burstCenter_ = center;
    burst_ = burst;
    burstPending_ = burst.count > 0;
  }
  // 巣の地面から、外側へ低い角度で飛ぶ灰と土の粒を放つ(1 回。バーストの 1 フレーム後に出る。同じフレームには 1 種類の粒の設定しか使えないため)
  void ReleaseGroundBlast(const Vector3& center, const GameOverAshGroundBlast& blast) {
    groundCenter_ = center;
    ground_ = blast;
    groundStage_ = blast.count > 0 ? 1 : 0;
  }
  // 核の大きさの倍率(1 で通常。溜めで小さくすると、光が圧縮されて見える)と、灰の収束の強さの倍率(1 で通常)
  void SetCoreSizeScale(float scale) { coreSizeScale_ = scale; }
  void SetConvergeScale(float scale) { convergeScale_ = scale; }
  // 核の位置から、粒を上へ放つ(1 回)。count: 個数、speed: 上向きの速さ
  void ReleaseUpward(uint32_t count, float speed);
  // 核を、外の位置(復活した蝶の胴体)へ追従させる。position: 核の光の中心にしたい位置(ワールド)、blend: 0 で巣のまま、1 で position
  // (0 と 1 の間は、巣から position への直線上。巣から胴体へ飛ぶ移りに使う)。灰の集まる先(収束と渦の目標)と、巣の島への点光源も、核に付いて動く
  void SetCoreFollow(const Vector3& position, float blend) {
    followPosition_ = position;
    followBlend_ = blend;
  }
  // 灰の発生量の倍率(1 で通常、0 で新しい灰を出さない)。復活で、灰が胴体へ吸い込まれて消えていくときに下げる
  void SetSpawnScale(float scale) { spawnScale_ = scale; }
  // 核の位置(ワールド)。復活した蝶が現れる所
  Vector3 GetCorePosition() const { return core_.GetPosition(); }

  GameOverAshSettings& GetSettings() { return settings_; }

private:
  // 今のカメラ(Renderer が持つ)から、目の位置と前方の向きを得る
  void GetCamera_(Vector3& eye, Vector3& forward) const;
  void EnterPhase_(GameOverAshPhase phase);

  GameOverAshSettings settings_;
  ScaleDust dust_;
  EmberCore core_;
  Vector3 nestCenter_{0.0f, 0.0f, 0.0f};
  GameOverAshPhase phase_ = GameOverAshPhase::Drifting;
  float gatherClock_ = 0.0f;        // 集まりが始まってからの秒(速さの倍率が掛かる)
  float gatherSpeedScale_ = 1.0f;
  float coreBoost_ = 0.0f;
  Vector3 followPosition_{0.0f, 0.0f, 0.0f}; // 核の追従先(SetCoreFollow)
  float followBlend_ = 0.0f;                  // 0 で巣、1 で追従先
  float spawnScale_ = 1.0f;                   // 灰の発生量の倍率
  Vector3 burstCenter_{0.0f, 0.0f, 0.0f};     // 再生の色のバースト(ReleaseBurst)
  GameOverAshBurst burst_;
  bool burstPending_ = false;
  Vector3 groundCenter_{0.0f, 0.0f, 0.0f};    // 地面の吹き飛び(ReleaseGroundBlast)
  GameOverAshGroundBlast ground_;
  int groundStage_ = 0;                       // 0 = なし、1 = 次の Update を待つ、2 = 次の Update で出す
  float coreSizeScale_ = 1.0f;                // 核の大きさの倍率
  float convergeScale_ = 1.0f;                // 灰の収束の強さの倍率
  bool phaseOverridden_ = false;    // 外から状態を指示されている間 true(自動で漂いから集まりへ進まない)
  float glowTime_ = 0.0f;           // ちらつきの時刻(秒)
  float gatherEase_ = 0.0f;         // 集まりの進み(なめらか。0 〜 1)。熱を帯びる量に使う
  uint32_t frameCount_ = 0;         // Initialize からのフレーム数(最初の数フレームの粒の全消去に使う)
  bool initialized_ = false;
};
