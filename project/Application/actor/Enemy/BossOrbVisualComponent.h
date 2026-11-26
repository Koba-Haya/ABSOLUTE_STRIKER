#pragma once
#include "AbsoluteEngine/graphics/primitive/ArcRing.h"
#include "AbsoluteEngine/graphics/primitive/EnergyOrb.h"
#include "AbsoluteEngine/graphics/primitive/EnergyRibbon.h"
#include "AbsoluteEngine/graphics/primitive/RibbonTrail.h"
#include "AbsoluteEngine/scene/Component.h"
#include "Type/Vector.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// ボスの誘導弾の「玉」の見た目。中心の火の玉(3D 陰影の球 + 炎と火花)と、それをまとうリボン(光の帯)と、現れるときの輪。
//
// 分けた理由: 玉の動きと状態(BossOrbComponent)と、見た目を分ける(弾の BulletComponent / BossBulletVisualComponent と同じ分け方)。
// 状態は BossOrbComponent の GetLook() から読むだけ(依存は、見た目 -> 動き の一方向)。
//
// 構成:
//   火の玉     : EnergyOrb(専用シェーダー)。板 1 枚だが、球の法線で陰影をつけるので、立体に見える。自転せず、模様が流れる。
//   まとうリボン: 玉のまわりの、傾いた 3D の軌道を回る帯(RibbonTrail + 専用シェーダー EnergyRibbon)。軌道ごとに、傾きと速さが違う。
//                 帯の通る位置は 3D(ワールド)の曲線で、カメラの動きや玉の動きで、視差が出る。
//                 玉の奥を通る部分と、手前を通る部分に分けて描く(奥 -> 火の玉 -> 手前)。玉の後ろへ回ると、玉に隠れて見える。
//   現れる輪   : ArcRing。現れるときだけ、玉の位置で広がって消える。
// 色: 黒紫の本体と、明るい紫の光(自機の青、ほかの攻撃の赤と、区別できる色)。アルファ合成。
// 描画は、雲の合成のあと(DrawAfterClouds)。玉が雲の裏に回って見えなくならないようにするため。このため、深度は見ない。
namespace BossOrbVisualDefaults {
// --- 火の玉 ---
// 板の大きさ(板の半分の長さ。ワールド)と、球の半径(板の半分の大きさを 1 とした割合)。球の実際の半径 = 板 x 割合
constexpr float kOrbQuadWorldRadius = 2.8f;
constexpr float kOrbRadius = 0.30f;
// 明るさの倍率と、外へにじむ光の強さ
constexpr float kOrbIntensity = 1.0f;
constexpr float kOrbHaloStrength = 1.0f;
// 炎と模様が流れる向き(画面の上。炎は、上へ立ち上る)
inline const Vector2 kOrbFlowDirection{ 0.0f, 1.0f };
// 色(本体: 黒紫、熱い所・炎・光: 明るい紫、最も熱い所・ハイライト・火花: 白に近い薄紫、外側の暗い輪郭)
inline const Vector4 kOrbBodyColor{ 0.06f, 0.00f, 0.16f, 1.0f };
inline const Vector4 kOrbGlowColor{ 0.42f, 0.12f, 0.95f, 1.0f };
inline const Vector4 kOrbCoreColor{ 0.96f, 0.90f, 1.00f, 1.0f };
inline const Vector4 kOrbOutlineColor{ 0.03f, 0.00f, 0.08f, 1.0f };

// --- まとうリボン ---
constexpr std::size_t kRibbonCount = 5;          // 本数(軌道の数)
constexpr std::size_t kRibbonPointCount = 32;    // 1 本の帯を作る点の数(多いほど、なめらかな曲線)
constexpr float kRibbonArcRadians = 4.2f;        // 1 本の帯の長さ(軌道上の角度。約 240 度)
constexpr float kRibbonOrbitRadius = 1.25f;      // 軌道の半径(ワールド。火の玉の半径の、約 1.5 倍)
constexpr float kRibbonOrbitWobble = 0.30f;      // 軌道の半径のゆらぎ(半径に対する割合)と、その細かさ(軌道 1 周あたりの山の数)
constexpr float kRibbonOrbitWobbleWaves = 2.0f;
constexpr float kRibbonOutOfPlaneWobble = 0.30f; // 軌道の面から、手前や奥へ波打つ量(半径に対する割合)と、その細かさ。帯が、平らな輪ではなく、立体にうねる
constexpr float kRibbonOutOfPlaneWaves = 3.0f;
constexpr float kRibbonWidth = 0.50f;            // 先端の幅(ワールド)
constexpr float kRibbonTailWidthRatio = 0.15f;   // 尾の幅の割合(先端に対して)
constexpr float kRibbonBackWidthRatio = 0.75f;   // 玉の奥を通る部分の、幅の割合(遠いので、少し細い)
constexpr float kRibbonBackAlpha = 0.60f;        // 玉の奥を通る部分の、濃さ(手前は 1)
constexpr float kRibbonSpeedMin = 1.7f;          // 回る速さ(ラジアン / 秒)。帯ごとに、最小から最大まで、順に変える
constexpr float kRibbonSpeedMax = 2.7f;
constexpr float kRibbonChargeSpeedBoost = 1.0f;  // 溜まりきったときの、速さの増え方(割合)
constexpr float kRibbonSpeedWobble = 0.35f;      // 速さが、時間でゆれる量(ラジアン)と、その速さ(ラジアン / 秒)
constexpr float kRibbonSpeedWobbleRate = 1.3f;
constexpr float kRibbonFlowSpeed = 1.5f;         // 帯の中の模様が流れる速さ
constexpr float kRibbonChargeIntensity = 0.6f;   // 溜まりきったときの、光の増え方(割合)
constexpr float kRibbonSeedStep = 0.37f;         // 帯ごとの模様のずれ
// 軌道の面の傾き。カメラの視線を 0 とした角度(0 = 正面から見た円、1.57 = 真横から見た線)と、その方位(ラジアン)。帯ごとに違う
constexpr std::array<float, kRibbonCount> kRibbonTiltAngles{ 0.55f, 0.95f, 1.25f, 0.75f, 1.10f };
constexpr std::array<float, kRibbonCount> kRibbonTiltAzimuths{ 0.30f, 1.90f, 3.60f, 5.00f, 2.70f };
// 1 本の帯を、玉の手前側と奥側で分けたときの、最大の数(軌道の一部が玉の後ろへ回る回数は、多くても 2 回)
constexpr std::size_t kMaxRunsPerRibbon = 4;

// --- 飛んでいるときの尾(エネルギーのリボン。玉の後ろへ伸びる長い帯) ---
constexpr float kTrailMinPointDistance = 0.25f;  // 前の点から、この距離以上離れたら、点を記録する(ワールド)
constexpr float kTrailLifetime = 1.00f;          // 点の寿命(秒)。尾の長さ = 玉の速さ x これ
constexpr std::size_t kTrailMaxPoints = 160;
constexpr float kTrailWidth = 1.50f;             // 先端の幅(ワールド)。尾の先は、細く尖る
constexpr float kTrailFlowSpeed = 2.5f;          // 尾の中の模様が流れる速さ
constexpr float kTrailIntensity = 1.6f;          // 尾の光の明るさの倍率

// --- 飛んでいる間の、火の玉の炎が流れる向き ---
// 進む向きの逆(画面の上での)へ、炎が流れる。進む向きが、ほぼ視線に沿っているとき(画面の上で、向きが決まらないとき)は、上向きのまま
constexpr float kFlowMinLength = 0.2f;

// --- 構え(発射の直前)と、飛行の見た目 ---
// 構え: リボンの軌道が縮み(割合)、回る速さが増え(溜まり具合に足す量)、火の玉が明るくなる(明るさの増え方)
constexpr float kAimOrbitScale = 0.55f;
constexpr float kAimSpeedBoost = 1.5f;
constexpr float kAimIntensityBoost = 0.5f;
// 飛行: リボンの軌道の大きさの割合(玉に締まって付いてくる)と、濃さの割合。火の玉の引き伸ばし(進む向きへ、画面の上での速さに応じて)
constexpr float kFlightOrbitScale = 0.65f;
constexpr float kFlightRibbonAlpha = 0.70f;
constexpr float kFlightStretch = 0.80f;
// 飛行中の火花(後ろへ飛ぶ細長い線)を出す、1 秒あたりの数
constexpr float kFlightSparkRate = 90.0f;

// --- 現れるときの輪 ---
// 輪の板の大きさ(板の半分の長さ。ワールド)
constexpr float kFlashQuadWorldRadius = 2.8f;
// 輪の半径(板の半分の大きさを 1 とした割合): 現れ始め -> 現れきったとき(広がる)
constexpr float kFlashRadiusStart = 0.15f;
constexpr float kFlashRadiusEnd = 0.85f;
// 輪の太さ(板の半分の大きさを 1 とした割合)
constexpr float kFlashThickness = 0.05f;
// 輪の弧の数と切れ目の割合、回る速さ(ラジアン / 秒)、明るさ
constexpr uint32_t kFlashArcCount = 5;
constexpr float kFlashGapRatio = 0.40f;
constexpr float kFlashSpinSpeed = 4.0f;
constexpr float kFlashIntensity = 1.6f;
// 輪が薄れていくときの、濃さの減り方の指数(大きいほど、早く薄れる)
constexpr float kFlashFadePower = 1.5f;
} // namespace BossOrbVisualDefaults

namespace AbsoluteEngine {
class ISpawner;
}

struct BossOrbLook;

class BossOrbVisualComponent : public AbsoluteEngine::IComponent {
public:
    // spawner: 玉が消えるときに、尾を別のオブジェクトとして残すための、オブジェクトを出す窓口(所有しない。シーンが持ち、玉より長く生きる)
    explicit BossOrbVisualComponent(AbsoluteEngine::ISpawner& spawner);
    ~BossOrbVisualComponent() override = default;

    // 飛行中の尾の記録と、玉が終わったときの後始末(着弾の演出、尾の切り離し、GameObject の破棄)を行う
    void Update(float deltaTime) override;
    // 雲の合成のあとに描く(玉が、雲の裏に隠れないようにする)
    void DrawAfterClouds() override;

    std::string GetTypeName() const override { return "BossOrbVisualComponent"; }

private:
    // 1 本の帯の、玉の手前側または奥側の一続きの部分(描くときは、1 つの RibbonTrail にする)
    struct Run {
        std::size_t poolIndex; // runPool_ の何番目の RibbonTrail を使うか
        bool back;             // true なら、玉の奥(カメラから見て、玉の中心より遠い側)
        float seed;            // 帯の模様のずれ
    };

    // 火の玉と、現れる輪の板を、初めて描くときに作る。作れなければ false(見た目が出ないだけで、ゲームは続ける)
    bool EnsureInitialized_();
    // いまの状態から、まとうリボンの形を作り直す(runs_ と runPool_ を更新する)。
    // center: 玉の中心。right / up / axis: カメラの右、上、視線の軸(ワールド)。cameraPosition: カメラの位置(奥と手前を分けるのに使う)
    // orbit: 軌道の大きさの割合(現れるときに、0 から 1 へ広がる)。age / charge: 玉の時間と溜まり具合
    void BuildRibbons_(const Vector3& center, const Vector3& right, const Vector3& up, const Vector3& axis,
                       const Vector3& cameraPosition, float orbit, float age, float charge);
    // runs_ のうち、奥側(back = true)または手前側(back = false)の部分を描く。alpha: 全体の濃さ。time: 玉の時間
    void DrawRuns_(bool back, float alpha, float time, float charge);
    // 飛行中の尾の見た目(色と模様)。time: 玉の時間
    EnergyRibbonStyle MakeTrailStyle_(float time) const;
    // 玉が終わったときの後始末。着弾の演出を出し、尾を切り離して残し、GameObject を壊す。
    // position: 玉の見た目の位置(らせんのずれを含む)。fade: 尾の濃さ
    void Finish_(const BossOrbLook& look, const Vector3& position, float fade);

    AbsoluteEngine::ISpawner& spawner_; // 尾を残すための窓口(所有しない)
    RibbonTrail trail_;                 // 飛行中の尾(玉の見た目の位置を記録する)
    bool trailStarted_ = false;         // 尾の記録を始めたか(飛んでいない玉は、尾を残さない)
    bool launchEffectDone_ = false;     // 発射の演出を出したか(飛び出した最初のフレームに、1 回だけ出す)
    float sparkCarry_ = 0.0f;           // 1 フレームで出しきれなかった、飛行中の火花の端数
    bool finished_ = false;             // 後始末を済ませたか(1 回だけ行う)

    EnergyOrb orb_;
    ArcRing flash_;
    bool ready_ = false;
    bool initializeFailed_ = false; // 作るのに失敗した(毎フレーム作り直さない)

    std::vector<RibbonTrail> runPool_; // 帯の部分の置き場(毎フレーム使い回す)
    std::vector<Run> runs_;            // 今フレームに描く帯の部分
    float seed_ = 0.0f;                // 玉のずれ(BossOrbComponent の look から読む)
};
