#pragma once
#include "Vector.h"
#include <cstdint>

// 熾火の核: 巣の中心に灯る、小さな橙の光。GPU パーティクルの大きな粒を、毎フレーム短い寿命で出し続けて光に見せる
// (重なって、柔らかい 1 つの光に見える)。発光専用の粒なので、通常の描画では描かれず、発光パス
// (Renderer::DrawGPUParticleGlow。雲の合成のあとに加算で描く)だけで描かれる。1 枚の板の中に、白黄の芯・橙の光輪・大きなやわらかい外光が
// 重なる(発光パスのシェーダーが描く)ので、雲のもやで色が薄まらず、焦点として読める。
// 光の強さ(0 〜)と位置は外から渡す。強さに応じて大きさ・明るさ・色の熱さが変わり、落ち着くと脈打つ。
// 巣の島への光は、同じ強さの橙の点光源で当てる(任意)。
//
// クラスを分けた理由: 灰の流れ(GameOverAsh / ScaleDust)と、核の光は、持ち物(粒の群れ / 1 つの光)も調整する値も違うため。
// 復活(リトライ)の演出が、強さを上げるだけで光を強められるように、強さだけを入口にしている。

namespace EmberCoreDefaults {
constexpr float kSize = 3.2f;              // 光の板の一辺(ワールド)。中に芯・光輪・外光が重なる。見える芯の半径は板の 1〜2 割、外光は 4 割ほど
constexpr float kLift = 0.35f;             // 巣の中心から上へ持ち上げる量(くぼみの底に埋もれて半分隠れないように)
constexpr float kMinSizeRatio = 0.30f;     // 強さ 0 のときの大きさの割合(強さ 1 で 1 倍)
constexpr float kBoostSizeGain = 0.60f;    // 強さが 1 を超えた分に掛けて、大きさに足す割合(復活で光を強めたときに、広がる)
constexpr float kHotColorR = 1.00f;        // 強いときの色(橙)
constexpr float kHotColorG = 0.52f;
constexpr float kHotColorB = 0.14f;
constexpr float kDimColorR = 0.45f;        // 弱いときの色(暗い橙)
constexpr float kDimColorG = 0.15f;
constexpr float kDimColorB = 0.05f;
constexpr float kPulseSpeed = 2.2f;        // 脈打つ速さ(ラジアン / 秒)
constexpr float kPulseDepth = 0.25f;       // 脈打つ深さ(強さに掛ける割合の振れ幅)
constexpr float kParticleLife = 0.08f;     // 光の粒 1 つの寿命(秒)。短いほど、強さの変化に素早く追従する(加算で重なる枚数も減る)
constexpr float kMinHeat = 0.45f;          // 強さ 0 に近いときの熱さ(色の傾きの位置。1 で黄、0.55 前後で橙、低いと赤)
constexpr float kGainPerIntensity = 1.0f;  // 強さ 1 あたりの発光の明るさの倍率
constexpr float kEmitInterval = 1.0f / 60.0f; // 光の粒を出す間隔(秒)
constexpr float kJitterRadius = 0.03f;     // 光の粒の出現位置の散らばり(ちらつきを抑えるため、ごく小さく)
constexpr float kFadePower = 2.0f;         // 粒のフェード(大きいほど、終わり際まで残る)
constexpr float kLightRadius = 14.0f;      // 島に当てる点光源の届く距離
constexpr float kLightIntensity = 2.5f;    // 点光源の強さ(光の強さ 1 のとき)
constexpr float kLightDecay = 2.0f;
constexpr float kMinActiveIntensity = 0.001f; // これ以下の強さは、消えているものとして扱う
} // namespace EmberCoreDefaults

struct EmberCoreSettings {
  float size = EmberCoreDefaults::kSize;
  float lift = EmberCoreDefaults::kLift;
  float minSizeRatio = EmberCoreDefaults::kMinSizeRatio;
  float boostSizeGain = EmberCoreDefaults::kBoostSizeGain;
  // 島への点光源の色(核の光そのものの色は、発光パスが熱さから決める)
  Vector3 hotColor{EmberCoreDefaults::kHotColorR, EmberCoreDefaults::kHotColorG, EmberCoreDefaults::kHotColorB};
  Vector3 dimColor{EmberCoreDefaults::kDimColorR, EmberCoreDefaults::kDimColorG, EmberCoreDefaults::kDimColorB};
  float pulseSpeed = EmberCoreDefaults::kPulseSpeed;
  float pulseDepth = EmberCoreDefaults::kPulseDepth;
  float particleLife = EmberCoreDefaults::kParticleLife;
  float minHeat = EmberCoreDefaults::kMinHeat;
  float gainPerIntensity = EmberCoreDefaults::kGainPerIntensity;
  float fadePower = EmberCoreDefaults::kFadePower;
  bool lightEnabled = true;
  float lightRadius = EmberCoreDefaults::kLightRadius;
  float lightIntensity = EmberCoreDefaults::kLightIntensity;
};

class EmberCore {
public:
  // 光の粒を出す常駐エミッタを借りる
  void Initialize(const EmberCoreSettings& settings = {});
  // エミッタを返し、点光源を消す
  void Shutdown();
  bool IsInitialized() const { return emitterIndex_ != kNoEmitter; }

  // 今の状態を渡す(毎フレーム呼ぶ)。nestCenter: 巣の中心(ワールド)。intensity: 光の強さ(0 で消える、1 で通常、1 超で強い)。
  // pulseAmount: 脈打ちの強さの割合(0 で脈打たない、1 で設定どおり)
  void SetState(const Vector3& nestCenter, float intensity, float pulseAmount);

  // 時間を進め、光の粒の指定と点光源を反映する。毎フレーム 1 回、描画の前に呼ぶ
  void Update(float deltaTime);

  // 脈打ちを含めた、今の光の強さ(確認用)
  float GetCurrentIntensity() const { return currentIntensity_; }
  // 光の中心(巣の中心から持ち上げた位置。ワールド)
  Vector3 GetPosition() const { return {center_.x, center_.y + settings_.lift, center_.z}; }

  EmberCoreSettings& GetSettings() { return settings_; }

private:
  static constexpr uint32_t kNoEmitter = 0xFFFFFFFFu;

  EmberCoreSettings settings_;
  uint32_t emitterIndex_ = kNoEmitter;
  Vector3 center_{0.0f, 0.0f, 0.0f};
  float intensity_ = 0.0f;
  float pulseAmount_ = 0.0f;
  float pulseTime_ = 0.0f;
  float currentIntensity_ = 0.0f;
  bool lightWasOn_ = false;
};
