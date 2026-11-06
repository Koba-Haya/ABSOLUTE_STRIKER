#pragma once
#include "Vector.h"
#include <cstdint>
#include <vector>

// 鱗粉(熾火から灰へ冷めていく粒)の、発生源と動きを受け持つクラス。GPU パーティクル(GPUParticleManager)の上に載る。
//
// 分けた理由: 鱗粉は、死亡の崩壊(翅が燃え尽きる)だけでなく、GameOver の雲の中で漂い、1 か所へ集まり、復活で蝶へ集まる動きにも使い回す。
// そのため、「どこから、どれだけ出すか」(発生源)と「どう動くか」(漂う、重力、風、中心への引き寄せ)を、死亡演出に固定せず、
// 場所と状態を外から渡す形にした。死亡演出に固有の合図(破片の解放、ディゾルブの進み)は PlayerDeathDust が受け持ち、このクラスは何も知らない。
//
// 使い方:
//   Initialize()                      常駐のエミッタを借りる(シーンの開始時)
//   毎フレーム: Burst / SetAmbient / SetGather を、必要なものだけ呼ぶ -> Update()
//   Shutdown()                        エミッタを返し、フィールドを解除する(シーンの終わり)
// 粒の更新と描画は Renderer::DrawGPUParticlesAfterClouds が行う(このクラスは、粒を発生させる指定と、動かす力の指定だけをする)。
// 注意: 粒のプールと力(フィールド)は GPUParticleManager で共有される。同時に使う ScaleDust は 1 つだけにすること。

namespace ScaleDustDefaults {
// --- 色(熾火の橙から、灰へ) ---
constexpr float kEmberColorR = 1.00f;
constexpr float kEmberColorG = 0.45f;
constexpr float kEmberColorB = 0.10f;
constexpr float kAshColorR = 0.40f;
constexpr float kAshColorG = 0.38f;
constexpr float kAshColorB = 0.36f;
constexpr float kCoolRatio = 0.55f;   // 色が灰へ変わりきる、寿命の割合
constexpr float kFadePower = 1.6f;    // 大きいほど、終わり際まで残ってから消える

// --- 寿命・大きさ・発生時の速さ(控えめな既定値から始める) ---
constexpr float kLifeMin = 1.6f;      // 秒
constexpr float kLifeMax = 3.2f;
constexpr float kSizeMin = 0.05f;     // ワールド単位
constexpr float kSizeMax = 0.12f;
constexpr float kSpeedMin = 0.1f;     // 発生時の速さ(向きはランダム)
constexpr float kSpeedMax = 0.9f;
constexpr float kAmountScale = 1.0f;  // 量の倍率(バーストの個数と、常時の発生量に掛ける)

// --- 動き ---
constexpr float kGravity = 0.6f;              // 下向きの加速度(鱗粉は軽いので、ゆっくり落ちる)
constexpr float kWindX = 0.0f;                // 風の加速度
constexpr float kWindY = 0.0f;
constexpr float kWindZ = 0.0f;
constexpr float kDragPerSecond = 0.9f;        // 空気抵抗(1 秒あたりの速度の減衰率)
constexpr float kTurbulenceStrength = 0.8f;   // ゆらぎの力(漂って見える動き)
constexpr float kTurbulenceFrequency = 0.6f;  // ゆらぎの空間の細かさ
constexpr float kTurbulenceSpeed = 0.7f;      // ゆらぎが変わる速さ
constexpr float kInheritVelocityScale = 1.0f; // 発生源の速度を、粒が引き継ぐ割合

// --- 発生源の制御 ---
constexpr float kAmbientIntervalSeconds = 0.05f; // 常時の発生を、まとめて出す間隔(毎フレームだと、粒の数が不安定になる)
constexpr float kMinAmbientRate = 0.01f;         // これ以下の発生量(個 / 秒)は、止めているものとして扱う
constexpr size_t kMaxPendingBursts = 64;         // 1 フレームに出せない分の持ち越しの上限(超えた分は捨てる)
} // namespace ScaleDustDefaults

struct ScaleDustSettings {
  Vector3 emberColor{ScaleDustDefaults::kEmberColorR, ScaleDustDefaults::kEmberColorG, ScaleDustDefaults::kEmberColorB};
  Vector3 ashColor{ScaleDustDefaults::kAshColorR, ScaleDustDefaults::kAshColorG, ScaleDustDefaults::kAshColorB};
  float coolRatio = ScaleDustDefaults::kCoolRatio;
  float fadePower = ScaleDustDefaults::kFadePower;
  float lifeMin = ScaleDustDefaults::kLifeMin;
  float lifeMax = ScaleDustDefaults::kLifeMax;
  float sizeMin = ScaleDustDefaults::kSizeMin;
  float sizeMax = ScaleDustDefaults::kSizeMax;
  float speedMin = ScaleDustDefaults::kSpeedMin;
  float speedMax = ScaleDustDefaults::kSpeedMax;
  float amountScale = ScaleDustDefaults::kAmountScale;
  bool glowOwnColor = false; // true なら、発光パスで、熱さの色ではなく、粒自身の色(emberColor から ashColor)で光る(復活の再生の色など)

  float gravity = ScaleDustDefaults::kGravity;
  Vector3 wind{ScaleDustDefaults::kWindX, ScaleDustDefaults::kWindY, ScaleDustDefaults::kWindZ};
  float dragPerSecond = ScaleDustDefaults::kDragPerSecond;
  float turbulenceStrength = ScaleDustDefaults::kTurbulenceStrength;
  float turbulenceFrequency = ScaleDustDefaults::kTurbulenceFrequency;
  float turbulenceSpeed = ScaleDustDefaults::kTurbulenceSpeed;
  float inheritVelocityScale = ScaleDustDefaults::kInheritVelocityScale;
};

class ScaleDust {
public:
  // 常駐のエミッタを借りる。すでに借りていれば何もしない。粒も全て消す(前の演出の残りが出ないように)
  void Initialize(const ScaleDustSettings& settings = {});
  // エミッタを返し、フィールドを解除する(シーンの終わり)。呼ばないと、次のシーンまでエミッタとフィールドが残る
  void Shutdown();
  bool IsInitialized() const { return emitterIndex_ != kNoEmitter; }

  // --- 発生源(場所と量を、外から渡す) ---

  // その場で、粒をまとめて出す(1 回)。position: 中心、count: 個数(量の倍率が掛かる)、radius: 散らばる球の半径、
  // sourceVelocity: 発生源の速度(粒が引き継ぐ。引き継ぐ割合は設定)。
  // 1 フレームに出せる数(GPUParticleManager::GetBurstPoolSize())を超えた分は、次のフレーム以降へ持ち越す
  void Burst(const Vector3& position, uint32_t count, float radius, const Vector3& sourceVelocity = {0.0f, 0.0f, 0.0f});

  // 続けて出す。毎フレーム呼んでいる間だけ出る(呼ばなくなれば止まる)。
  // center: 中心、radius: 散らばる球の半径、ratePerSecond: 1 秒に出す個数(量の倍率が掛かる)、sourceVelocity: 発生源の速度
  void SetAmbient(const Vector3& center, float radius, float ratePerSecond, const Vector3& sourceVelocity = {0.0f, 0.0f, 0.0f});

  // --- 動き(状態を、外から渡す) ---

  // 1 か所へ引き寄せる。毎フレーム呼んでいる間だけ効く(呼ばなくなれば解除)。point: 集まる場所、strength: 引き寄せる加速度
  void SetGather(const Vector3& point, float strength);

  // 1 か所へ収束させる。毎フレーム呼んでいる間だけ効く。SetGather と違い、距離に比例した力(ばね)で引くので、
  // 遠い粒ほど強く引かれ、近づくと弱まる。空気抵抗(dragPerSecond)と組むと、どの距離の粒も、同じような時間で point に集まる。
  // stiffness: ばね定数(1 / 秒^2)。dragPerSecond が 2 * sqrt(stiffness) なら、振動せずに収束する(小さいと、point のまわりを回る)。
  // SetGather とは同時に使えない(後から呼んだ方が効く)
  void SetConverge(const Vector3& point, float stiffness);

  // point を通る axis の軸まわりに、粒を渦のように回す。毎フレーム呼んでいる間だけ効く。strength: 回す加速度。
  // 収束と組むと、粒が渦を巻きながら集まる
  void SetSwirl(const Vector3& point, const Vector3& axis, float strength);

  // このフレームの指定を反映する(持ち越しのバーストを出し、常時の発生と、力を GPU パーティクルへ渡す)。毎フレーム 1 回、描画の前に呼ぶ
  void Update();

  ScaleDustSettings& GetSettings() { return settings_; }
  const ScaleDustSettings& GetSettings() const { return settings_; }

private:
  static constexpr uint32_t kNoEmitter = 0xFFFFFFFFu;

  // 力(フィールド)のスロット。GPUParticleManager::SetField の slot に対応する
  static constexpr int kFieldWind = 0;       // 重力と風
  static constexpr int kFieldTurbulence = 1; // ゆらぎ
  static constexpr int kFieldDrag = 2;       // 空気抵抗
  static constexpr int kFieldGather = 3;     // 引き寄せ(SetGather)または収束(SetConverge)
  static constexpr int kFieldSwirl = 4;      // 渦

  struct PendingBurst {
    Vector3 position{0.0f, 0.0f, 0.0f};
    uint32_t count = 0;
    float radius = 0.0f;
    Vector3 velocity{0.0f, 0.0f, 0.0f};
  };

  // 力(フィールド)を全て解除する
  void ClearFields_();

  ScaleDustSettings settings_;
  uint32_t emitterIndex_ = kNoEmitter;
  std::vector<PendingBurst> pendingBursts_;

  // このフレームの指定(Update で反映して、リセットする)
  bool ambientRequested_ = false;
  Vector3 ambientCenter_{0.0f, 0.0f, 0.0f};
  float ambientRadius_ = 0.0f;
  float ambientRate_ = 0.0f;
  Vector3 ambientVelocity_{0.0f, 0.0f, 0.0f};
  bool gatherRequested_ = false;
  bool gatherIsSpring_ = false; // true なら距離に比例した力(SetConverge)、false なら一定の力(SetGather)
  bool swirlRequested_ = false;
  Vector3 swirlPoint_{0.0f, 0.0f, 0.0f};
  Vector3 swirlAxis_{0.0f, 1.0f, 0.0f};
  float swirlStrength_ = 0.0f;
  Vector3 gatherPoint_{0.0f, 0.0f, 0.0f};
  float gatherStrength_ = 0.0f;
  bool ambientWasOn_ = false; // 前のフレームに常時の発生が有効だったか(止めるときに、エミッタを無効にするため)
};
