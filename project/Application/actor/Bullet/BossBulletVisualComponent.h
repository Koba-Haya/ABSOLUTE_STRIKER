#pragma once
#include "AbsoluteEngine/graphics/primitive/ArcRing.h"
#include "AbsoluteEngine/graphics/primitive/RibbonTrail.h"
#include "AbsoluteEngine/scene/Component.h"
#include "Type/Vector.h"
#include <cstdint>
#include <random>
#include <string>

// ボスの弾の見た目(charge.gif の見た目): 白く光る芯と、赤い光のにじみ、長く引く帯の尾、後ろへ散る細長い火花、後ろに残る衝撃波の輪。
//
// 分けた理由: 弾の動きと当たり判定(BulletComponent)と、見た目(ここ)を分ける。弾のモデル(球)を付けない弾に、これを足す。
// 構成(弾 1 発ごと):
//   芯           : ArcRing(板 1 枚 + シェーダー)で描く、中心が白く、赤い光がにじむ光の玉(中心を原点にした帯で、円盤にしている)
//   帯の尾       : RibbonTrail。先端が白く太く、後ろへ細く薄くなる。アルファ合成で描く
//   火花         : 細長い線の粒。弾の後ろへ飛び、飛ぶ向きに伸びて見える(粒の板を横長にして、向きを合わせる)
//   光の尾の滲み : 弾の位置に、進む向きへ長く伸びた光を毎フレーム置く(動きの残像のように、芯が進む向きへ伸びて見える)
//   衝撃波の輪   : 一定の間隔で、弾の位置に置いて、その場に残す。輪の面は、弾の進む向きに垂直(画面では、進む向きに平たい楕円に見える)。
//                  広がりながら消える。明るい空の上でも形が見えるよう、白・赤・黒(暗い色)を順に出す(参考映像の黒い輪郭)
// 描画は、雲の合成のあと(DrawAfterClouds)。弾が雲の裏に回って見えなくならないようにするため。
// このため、深度は見ない(ボスの体や島の手前に、常に見える)。島に当たった弾は、BulletComponent が消す
// 当たり判定は弾のコライダー(小さな球)のままで、見た目のほうが大きい(判定の位置は、芯の中心)。
// 使い方: 弾の GameObject に追加し、撃つたびに Restart(速度) を呼ぶ(弾は使い回されるので、前回の尾や輪の進みを捨てる)
namespace BossBulletVisualDefaults {
// --- 芯(光の玉) ---
constexpr float kCoreQuadWorldRadius = 3.0f;   // 板の半分の長さ(ワールド。にじむ光が届く範囲)
constexpr float kCoreRadius = 0.32f;           // 光の玉の半径(板の半分を 1 とした割合。中心から、この半径へ向けて薄れる)
constexpr float kCoreHaloStrength = 1.00f;     // 玉の外へにじむ赤い光の強さ
constexpr float kCoreHaloWidth = 0.28f;        // にじむ光の幅(板の半分を 1 とした割合)
constexpr float kCoreCorePower = 0.55f;        // 白い芯の広がり(小さいほど、白い範囲が広い)
constexpr float kCoreIntensity = 2.0f;
constexpr float kCorePulseAmount = 0.25f;      // 明るさの脈打ち(振れ幅)と速さ(ラジアン / 秒)
constexpr float kCorePulseSpeed = 20.0f;
inline const Vector4 kCoreEdgeColor{ 1.00f, 0.10f, 0.08f, 1.0f }; // にじむ光の色(赤)
inline const Vector4 kCoreCoreColor{ 1.00f, 0.95f, 0.90f, 1.0f }; // 芯の色(白に近い)

// --- 帯の尾 ---
constexpr float kTrailWidth = 1.30f;           // 先端の幅(ワールド)
constexpr float kTrailLifetime = 0.45f;        // 点の寿命(秒)。尾の長さ = 弾速 x これ
constexpr float kTrailMinPointDistance = 0.35f; // 前の点から、この距離以上離れたら、点を記録する
constexpr std::size_t kTrailMaxPoints = 64;
inline const Vector4 kTrailColor{ 1.00f, 0.15f, 0.10f, 0.95f };

// --- 火花(細長い線の粒) ---
constexpr float kStreakRate = 110.0f;          // 1 秒あたりに出す数
constexpr float kStreakLifeMin = 0.22f;
constexpr float kStreakLifeMax = 0.40f;
constexpr float kStreakBackSpeedMin = 6.0f;    // 弾の後ろへ飛ぶ速さ
constexpr float kStreakBackSpeedMax = 16.0f;
constexpr float kStreakSideSpeedMax = 2.5f;    // 横へ散る速さ(画面に平行な面の上)
constexpr float kStreakSpawnSpread = 0.30f;    // 出す位置の、弾の中心からのずれ(横方向の速さの割合)
constexpr float kStreakLengthMin = 1.8f;       // 粒の板の長さ(進む向き)と太さ(ワールド)
constexpr float kStreakLengthMax = 3.4f;
constexpr float kStreakThicknessMin = 0.16f;
constexpr float kStreakThicknessMax = 0.26f;
constexpr float kStreakEndLengthRatio = 0.30f; // 消えるときの長さと太さの割合
constexpr float kStreakEndThicknessRatio = 0.60f;
inline const Vector4 kStreakColorHot{ 1.00f, 0.96f, 0.92f, 1.0f }; // 白に近い火花と、赤い火花を、混ぜる
inline const Vector4 kStreakColorRed{ 1.00f, 0.22f, 0.16f, 1.0f };

// --- 光の尾の滲み(弾の位置に、毎フレーム置く、進む向きへ長い光) ---
constexpr float kSmearLength = 5.0f;           // 長さ(進む向き。ワールド)
constexpr float kSmearThickness = 1.3f;        // 太さ
constexpr float kSmearLife = 0.05f;            // 寿命(秒)。短くして、毎フレーム置き直す
inline const Vector4 kSmearColor{ 1.00f, 0.55f, 0.45f, 0.85f };

// --- 衝撃波の輪 ---
constexpr float kRingInterval = 0.06f;         // 置く間隔(秒)
constexpr float kRingLife = 0.55f;
constexpr float kRingSizeStart = 1.6f;         // 輪の直径(ワールド)。始め -> 終わり(広がる)
constexpr float kRingSizeEnd = 4.2f;
constexpr float kRingMinorRatioMin = 0.12f;    // 画面で、進む向きに平たくなる割合の下限(弾が画面に平行に飛ぶとき、輪が線になりすぎないように)
constexpr float kRingBackSpeed = 2.5f;         // 置いたあと、後ろへゆっくり流れる速さ
constexpr int kRingKinds = 3;                  // 色の種類(白、赤、暗い色)を、順に出す
inline const Vector4 kRingColorWhite{ 1.00f, 0.92f, 0.88f, 1.00f };
inline const Vector4 kRingColorRed{ 1.00f, 0.30f, 0.22f, 1.00f };
inline const Vector4 kRingColorDark{ 0.10f, 0.00f, 0.02f, 0.90f };
} // namespace BossBulletVisualDefaults

class BossBulletVisualComponent : public AbsoluteEngine::IComponent {
public:
    BossBulletVisualComponent();

    // 撃つたびに呼ぶ。velocity: 弾の速度(ワールド。直進弾なので、撃つときに 1 回渡す)。
    // 前回の飛行の状態(尾の点、経過時間、火花の端数、輪の間隔と色の順番)を捨てて、最初からにする。
    // 芯の板(GPU の資源)は、作り直さずに使い回す
    void Restart(const Vector3& velocity);
    ~BossBulletVisualComponent() override = default;

    void Update(float deltaTime) override;
    // 雲の合成のあとに描く(弾が、雲の裏に隠れないようにする)
    void DrawAfterClouds() override;

    std::string GetTypeName() const override { return "BossBulletVisualComponent"; }

private:
    // 火花、光の尾の滲み、衝撃波の輪を出す。cameraRight / cameraUp / cameraAxis: 画面に平行な面の向きと、視線の軸
    void EmitParticles_(float deltaTime, const Vector3& position, const Vector3& cameraRight, const Vector3& cameraUp, const Vector3& cameraAxis);
    float Random_(float lo, float hi);

    Vector3 velocity_{ 0.0f, 0.0f, 0.0f };
    Vector3 direction_{ 0.0f, 0.0f, 1.0f }; // 速度の向き(長さ 1)
    float speed_ = 0.0f;
    RibbonTrail trail_;
    ArcRing core_;
    bool coreReady_ = false;
    bool initializeFailed_ = false; // 作るのに失敗した(毎フレーム作り直さない)
    bool particlesReady_ = false;
    float age_ = 0.0f;              // 出てからの時間(脈打つ明るさに使う)
    float streakCarry_ = 0.0f;      // 1 フレームで出しきれなかった火花の端数
    float ringTimer_ = 0.0f;
    int ringCount_ = 0;
    std::mt19937 rng_;
};
