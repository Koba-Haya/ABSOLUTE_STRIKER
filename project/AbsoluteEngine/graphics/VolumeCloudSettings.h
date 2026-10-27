#pragma once
#include "Method.h"
#include <cstdint>

// ボリューム雲(ワールドに固定した箱 1 つ)の見た目のパラメータ。
// 描画(Renderer::AddVolumeCloud / VolumeCloud.PS.hlsl)とデバッグ UI(VolumeCloudDebugUi)の両方が参照するデータだけの構造体。
// 描画の仕組み(3D ノイズ + レイマーチ)は shaders/VolumeCloud.hlsli を参照。
constexpr int32_t kVolumeCloudMinResolutionDivisor = 1;
constexpr int32_t kVolumeCloudMaxResolutionDivisor = 4;

// 外形の種類
enum class VolumeCloudShape : int32_t {
  Box = 0,   // ワールドに固定した箱(に内接する楕円体の塊)
  Layer = 1, // 水平に無限に続く層(雲海。下面と上面の高さで決まる)
};

// 層の雲への「着地の反応」(物体が雲の面に落ちたときの、くぼみと、外へ広がる波の縁)。
// 層のシェーダー(VolumeCloud.hlsli の ImpactDisplacement)が、密度を引く高さをずらして表現する。strength が 0 なら何も起きない。
// 時間と位置は、これを管理する側(Application の CloudImpact)が毎フレーム書く。
struct VolumeCloudImpact {
  float strength = 0.0f;         // 反応の強さ(0 = 無効)
  float centerX = 0.0f;          // 着地点のワールド座標
  float centerZ = 0.0f;
  float age = 0.0f;              // 着地からの経過秒
  float craterRadius = 3.5f;     // くぼみの半径(ユニット)
  float craterDepth = 7.0f;      // くぼみの深さ(雲の面を押し下げる量。層の厚みは 20)
  float ringWidth = 3.0f;        // 波の縁の幅
  float ringHeight = 5.0f;       // 波の縁の盛り上がり
  float ringSpeed = 10.0f;       // 波の縁が外へ広がる速さ(ユニット / 秒)
  float refillSeconds = 2.5f;    // くぼみが埋まり戻るまでの秒
  float ringFadeSeconds = 2.2f;  // 波の縁が消えるまでの秒
  float seaFill = 0.0f;          // 落下の間、雲海を雲で満たす量(0〜1。CloudImpact が時間で決める。0 = 通常の雲海)
  // 落下の間の雲海の満たし方(CloudImpact が使う)
  float fallFillAmount = 0.8f;   // 満たしきったときの量
  float fallFillStartSeconds = 0.3f; // 死亡から、満たし始めるまでの秒
  float fallFillRampSeconds = 1.2f;  // 満たしきるまでの秒
};

struct VolumeCloudSettings {
  bool enabled = true;
  VolumeCloudShape shape = VolumeCloudShape::Box;

  // --- 箱(ワールド座標) ---
  // 既定は、ゲームのレール(x = 0, y = 0 で、z 方向へまっすぐ進む)の途中に置いた箱。カメラはこの中を通り抜ける
  Vector3 center{0.0f, -2.8f, 267.0f};
  Vector3 halfSize{40.0f, 16.0f, 45.0f}; // 中心から面までの距離(ユニット)

  // --- 層(shape が Layer のとき。箱の位置と大きさは使わない) ---
  float layerBottom = -44.0f; // 下面の高さ(ワールド y)。既定は旧方式の雲海(平均 y = -35.5、厚み 9)に合わせた値
  float layerTop = -24.0f;  // 上面の高さ(旧方式の雲海の起伏の最大に合わせた値)
  float maxDistance = 1000.0f; // 描く距離の上限(カメラから。ゲームカメラの遠方クリップと同じ。その先は霞)
  float fogDensity = 0.0022f;  // 距離に対する霞の強さ(大きいほど、遠くの雲が早く霞に溶ける。小さいと、描く距離の端で雲が急に途切れて見える)

  // --- 形 ---
  float noiseScale = 0.05f; // 形のノイズの細かさ(1 ユニットあたりのタイル数。0.05 なら 20 ユニットで 1 周期)
  float detailScale = 2.0f; // 細かい模様の倍率(形のノイズに対する比)
  float threshold = 0.75f;  // 雲になるしきい値(大きいほど隙間が増える)
  float edgeFade = 0.60f;   // 外形(箱に内接する楕円体)の縁を薄くする幅(0〜1)
  float erosion = 0.50f;    // 細かい模様で削る強さ
  Vector3 wind{2.0f, 0.0f, 0.0f}; // ノイズが流れる速さ(ユニット / 秒)

  // --- 光と濃さ ---
  float density = 0.06f;          // 消光係数(大きいほど濃く、向こうが見えなくなる)
  float sunIntensity = 0.7f;      // 太陽の光の強さ
  float ambientIntensity = 0.7f;  // 空からの光の強さ
  float phaseG = 0.55f;           // 前方散乱の強さ(0〜1)
  float highlightKnee = 0.75f;    // 明るい部分の圧縮を始める値(0〜1)。光が強くても、これを超える分は 1 へ滑らかに圧縮されて、白飛びが減る。1 で圧縮なし
  float shadowLift = 0.15f;       // 影の持ち上げ(0〜1)。大きいほど、影の部分に空からの明るい光が回り、影が浅くなる

  // --- 着地の反応(層のみ) ---
  VolumeCloudImpact impact;

  // --- 負荷 ---
  // 雲を描く解像度の分母。1 = シーンと同じ、2 = 縦横半分(負荷は約 1/4)、4 = 縦横 1/4。
  // 雲はぼやけた形が多いので、半分でも目立ちにくい。拡大のときに、深度を見て物体の縁へのにじみを抑える
  int32_t resolutionDivisor = 2;
  int32_t steps = 32;             // 視線のステップ数(多いほど重く、きれい)
  int32_t lightSteps = 4;         // 太陽方向のステップ数
  float lightStepLength = 8.0f;   // 太陽方向の 1 歩目の長さ(ユニット)
};

// 雲海(下に広がる雲の層)の既定の設定を作る。箱の雲と同じ構造体を、層の外形で使う。
// 値は、旧方式の雲海に見た目を寄せるための出発点(調整 UI で詰める)
inline VolumeCloudSettings MakeSeaLayerSettings() {
  VolumeCloudSettings s;
  s.shape = VolumeCloudShape::Layer;
  s.noiseScale = 0.025f; // 約 40 ユニットで 1 周期(箱の既定と同じ程度。大きな周期の濃淡を別に重ねて、繰り返しを崩している)
  s.detailScale = 3.7f;
  s.threshold = 0.74f;
  s.edgeFade = 0.35f; // 上面側の薄くなる幅。広いと、上面近くが透けて、水平に近い視線で雲が見えなくなる
  s.erosion = 0.60f;
  s.density = 0.4f;     // 上から見ると、表面近くで光が尽きる濃さ
  s.sunIntensity = 0.75f;
  s.ambientIntensity = 1.0f;
  s.resolutionDivisor = 2;
  s.steps = 32;
  s.lightSteps = 4;
  s.lightStepLength = 8.0f;
  return s;
}
