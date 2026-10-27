#pragma once
#include "graphics/3d/model/ModelInstance.h"
#include "Vector.h"
#include <array>
#include <cstdint>

// ゲームオーバー画面の浮島の群れ。雲の層の「中」にあり、距離に応じて雲のもやに溶けて見える。
// 責務は「島の配置(固定の乱数)、ゆっくりした揺れ、描画」だけ。雲の濃さやカメラは GameOverBackdrop が持つ。
// 島は雲を合成する前(GameOverBackdrop::Begin の中)に描く。雲の合成が深度を見るので、手前の雲の濃さで自然にかすむ。
// クラスを分けた理由: 島の配置と動きは、背景の雲やカメラと独立した持ち物で、のちに数や種類を増やしやすいため

// 島の既定値。値は ImGui(Game Over Islands)で調整でき、気に入った値をここへ写す
namespace GameOverIslandDefaults {
constexpr int32_t kMaxCount = 16;      // 用意するモデルの数の上限(ImGui の数の上限)
constexpr int32_t kCount = 12;         // 置く数
constexpr int32_t kSeed = 20261028;    // 配置の乱数のシード(固定。毎回同じ配置になる)
constexpr float kMinDistance = 15.0f;  // カメラからの水平距離の範囲(ユニット)
constexpr float kMaxDistance = 110.0f;
constexpr float kSpreadDeg = 42.0f;    // 正面(+z)からの左右の広がり(度)。画面の横の画角は約 81 度なので、縁の外の島も少し混ぜる
// 島の上面の高さ: 雲の層の下面から上面までの割合の範囲(島の底は 3〜4 ユニット下に伸びる。上面より上へ出ると雲の外になる)
constexpr float kMinLayerRatio = 0.15f;
constexpr float kMaxLayerRatio = 0.55f;
constexpr float kScaleMin = 1.4f;      // 大きさの倍率の範囲(モデルの上面の半径は 1.5〜3.2)
constexpr float kScaleMax = 2.4f;
constexpr float kBobAmplitude = 0.35f; // 上下の揺れの大きさ(ユニット)
constexpr float kBobSpeed = 0.35f;     // 揺れの速さ(ラジアン / 秒。島ごとに 0.7〜1.3 倍)
constexpr float kYawSpinSpeed = 0.04f; // ゆっくりした回転(ラジアン / 秒。島ごとに向きが違う)
constexpr float kLightIntensity = 1.0f; // 島に当てる太陽の強さ
// 巣の島(特別枠。復活の灰が集まる所)。カメラの降下の終点で、巣のくぼみの中心(モデルの原点)が画面の中央に来る。
// 位置は固定: 正面(+z)の水平距離と、雲の層の下面から上面までの割合(終点のカメラは、巣の少し上に置くので、低めにする)
constexpr float kNestDistance = 30.0f;
constexpr float kNestLayerRatio = 0.10f;
constexpr float kNestScale = 1.8f;
// 乱数配置の島を巣に近づけない半径(水平。ユニット)と、カメラの通り道(原点から巣までの線)の両側の空け幅。島の大きさ(倍率)ぶんは、別に足す
constexpr float kNestClearRadius = 22.0f;
constexpr float kPathClearWidth = 7.0f;
constexpr float kClearPerScale = 3.0f;  // 島の大きさ 1 倍あたりに足す空き(上面の半径の目安)
constexpr int32_t kAvoidRetries = 30;   // 避けられない島を、置き直す回数の上限
} // namespace GameOverIslandDefaults

class GameOverIslands {
public:
  // layerBottom / layerTop: 雲の層の下面と上面の高さ(島の高さを、この間に散らす)
  void Initialize(float layerBottom, float layerTop);
  void Update(float deltaTime);
  // sunDirection: 太陽へ向かう向き(雲の光の向きと合わせる)。雲の合成の前に呼ぶ
  void Draw(const Vector3 &sunDirection);

  // 巣の島のくぼみの中心(ワールド。揺れない)。カメラの降下の終点と、灰の集まる先が使う
  const Vector3 &GetNestCenter() const { return nestPosition_; }

#ifdef USE_IMGUI
  void DrawDebugUi();
#endif

private:
  struct Island {
    ModelInstance model;
    Vector3 position{0.0f, 0.0f, 0.0f}; // 基準の位置(揺れの中心)
    float scale = 1.0f;
    float yaw = 0.0f;
    float tiltX = 0.0f;
    float tiltZ = 0.0f;
    float bobPhase = 0.0f;
    float bobSpeedRatio = 1.0f;
    float spinSign = 1.0f;
  };

  // 巣の島の位置と向きを反映する(揺れない)
  void ApplyNestWorld_();
  // 乱数のシードから、全島の配置を決め直す(固定の乱数なので、設定が同じなら毎回同じ配置)
  void Layout_();
  // 揺れを含めたワールド行列を、全島へ反映する
  void ApplyWorlds_();

  std::array<Island, GameOverIslandDefaults::kMaxCount> islands_;
  ModelInstance nestModel_;              // 巣の島(特別枠。乱数配置には含めない)
  bool nestLoaded_ = false;
  Vector3 nestPosition_{0.0f, 0.0f, 0.0f};
  bool loaded_ = false;
  float layerBottom_ = 0.0f;
  float layerTop_ = 0.0f;
  float time_ = 0.0f;

  int32_t count_ = GameOverIslandDefaults::kCount;
  int32_t seed_ = GameOverIslandDefaults::kSeed;
  float minDistance_ = GameOverIslandDefaults::kMinDistance;
  float maxDistance_ = GameOverIslandDefaults::kMaxDistance;
  float spreadDeg_ = GameOverIslandDefaults::kSpreadDeg;
  float minLayerRatio_ = GameOverIslandDefaults::kMinLayerRatio;
  float maxLayerRatio_ = GameOverIslandDefaults::kMaxLayerRatio;
  float scaleMin_ = GameOverIslandDefaults::kScaleMin;
  float scaleMax_ = GameOverIslandDefaults::kScaleMax;
  float bobAmplitude_ = GameOverIslandDefaults::kBobAmplitude;
  float bobSpeed_ = GameOverIslandDefaults::kBobSpeed;
  float yawSpinSpeed_ = GameOverIslandDefaults::kYawSpinSpeed;
  float lightIntensity_ = GameOverIslandDefaults::kLightIntensity;
  float nestDistance_ = GameOverIslandDefaults::kNestDistance;
  float nestLayerRatio_ = GameOverIslandDefaults::kNestLayerRatio;
  float nestScale_ = GameOverIslandDefaults::kNestScale;
  float nestClearRadius_ = GameOverIslandDefaults::kNestClearRadius;
  float pathClearWidth_ = GameOverIslandDefaults::kPathClearWidth;
};
