#pragma once
#include "AbsoluteEngine/graphics/primitive/ArcRing.h"
#include "AbsoluteEngine/graphics/primitive/ShieldDome.h"
#include "BossShieldPlan.h"
#include "Type/Vector.h"
#include <random>
#include <vector>

// ボスのシールドの見た目(三角形の格子のドーム、被弾の衝撃、砕ける破片、失敗の白いフラッシュ)を受け持つクラス。
//
// 分けた理由: シールドの仕組み(BossShieldStageState と、その 4 つの段階の状態クラス)に、見た目の数値を混ぜないため。状態は、外へ見せる状態(BossShieldView)を毎フレーム渡すだけで、
// 見た目は、ここで決める。描き方そのもの(球のメッシュとシェーダー)は、エンジンの ShieldDome が持つ。
//
// 段ごとの見た目:
//   展開(Deploy): ドームが、小さな球から広がりながら、面が 1 枚ずつ現れる
//   維持(Hold)  : ゆっくり明滅する。耐久が減るほど、傷んだ面(赤)が増える。時間切れの直前は、赤く脈打つ
//   成功(Break) : 面が、乱数の小さいものから順に、縮みながら外へ飛んで消える(破片)。同時に、破片の火花と、弱い白いフラッシュが出る
//   失敗(Burst) : 画面全体が白く光り(フラッシュ)、ドームが一気に大きく広がって消える(爆発の衝撃)
// 被弾(AddHit): 自機の弾が、ドームの表面を貫く点(カメラから、被弾した位置へ向かう線と、ドームの交点)から、
//   波紋が表面を広がる(通った面と線が、青白く光る)。交点では、光の玉と、四方へ飛ぶ火花が出る。
// 色は、ドームと破片が敵の色(紫)、被弾の波紋と火花が自機の色(青白い光)。
// 描画は、雲の合成のあと(DrawAfterClouds から呼ぶ)。弾の演出と同じ理由で、雲の裏に回って見えなくならないようにする。
// 使い方: Update(毎フレーム) / AddHit(被弾したとき) / Draw(ドーム。毎フレーム) / DrawFlash(フラッシュ。最後に描く)
namespace BossShieldEffectDefaults {
constexpr float kDomeRadius = 7.5f;           // ドームの半径(ワールド。ボスの体と、あごの先までを包む)
constexpr float kDomeCenterShift = 2.5f;      // ドームの中心を、ボスの位置から、カメラ側(ボスの正面。あごの側)へずらす量
constexpr float kDeployStartScale = 0.45f;    // 展開の始めの、半径の倍率
constexpr float kHoldPulseAmount = 0.012f;    // 維持の間の、半径のゆらぎ(割合)
constexpr float kHoldPulseSpeed = 2.2f;       // 同、速さ(ラジアン / 秒)
constexpr float kBreakExpandScale = 1.1f;     // 砕けるときの、中心の広がり(破片は、これと別に、外へ飛ぶ)
constexpr float kBurstEndScale = 2.2f;        // 失敗の爆発で、広がる倍率
constexpr float kRippleLife = 1.2f;           // 被弾の波紋の寿命(秒)

// --- 被弾の衝撃(交点で出す光の玉と火花) ---
constexpr float kImpactFlashSize = 2.8f;      // 光の玉の大きさ(ワールド)。始め -> 終わり
constexpr float kImpactFlashEndSize = 0.5f;
constexpr float kImpactFlashLife = 0.22f;
constexpr int kImpactSparkCount = 16;
constexpr float kImpactSparkNormalSpeedMin = 1.0f; // 火花が、ドームの外向きへ飛ぶ速さ
constexpr float kImpactSparkNormalSpeedMax = 4.5f;
constexpr float kImpactSparkSideSpeedMax = 9.0f;   // 表面に沿って散る速さ
constexpr float kImpactSparkLifeMin = 0.30f;
constexpr float kImpactSparkLifeMax = 0.60f;
constexpr float kImpactSparkSize = 1.0f;           // 火花の板の大きさ。始め -> 終わり
constexpr float kImpactSparkEndSize = 0.1f;
inline const Vector4 kImpactColor{ 0.65f, 0.88f, 1.00f, 1.0f };   // 衝撃の色(自機の弾の青白い光)

// --- 砕ける破片(火花) ---
constexpr int kBreakSparkCount = 90;
constexpr float kBreakSparkSpeedMin = 5.0f;
constexpr float kBreakSparkSpeedMax = 14.0f;
constexpr float kBreakSparkLifeMin = 0.5f;
constexpr float kBreakSparkLifeMax = 0.95f;
constexpr float kBreakSparkSize = 1.2f;
constexpr float kBreakSparkEndSize = 0.1f;
constexpr float kBreakFlashSize = 9.0f;            // 砕けた瞬間の、中心の光の玉(ワールド)
constexpr float kBreakFlashEndSize = 14.0f;
constexpr float kBreakFlashLife = 0.35f;
inline const Vector4 kBreakSparkColorViolet{ 0.70f, 0.40f, 1.00f, 1.0f };
inline const Vector4 kBreakSparkColorWhite{ 0.95f, 0.88f, 1.00f, 1.0f };
inline const Vector4 kBreakFlashColor{ 0.80f, 0.60f, 1.00f, 0.85f };

// --- 白いフラッシュ(画面全体) ---
constexpr float kFlashDistance = 3.0f;        // カメラの前に出す板までの距離(ワールド)
constexpr float kFlashHalfSize = 80.0f;       // 板の半分の長さ(ワールド。距離に対して、画面全体を覆える大きさ)
constexpr float kBurstFlashPower = 2.0f;      // 失敗のフラッシュの薄れ方(大きいほど、すぐ薄れる)
constexpr float kBreakFlashAlpha = 0.35f;     // 成功のフラッシュの濃さ(失敗より弱い)
constexpr float kBreakFlashShare = 0.3f;      // 成功のフラッシュが続く、段の長さの割合
} // namespace BossShieldEffectDefaults

class BossShieldEffects {
public:
    // 毎フレーム。view: シールドの今の状態。center: ドームの中心(ワールド)。cameraForward: カメラの向き(白いフラッシュの板を置く方向)
    void Update(float deltaTime, const BossShieldView& view, const Vector3& center, const Vector3& cameraForward);

    // 被弾したとき。worldPosition: 被弾した位置(ワールド)。ドームの表面の交点から、波紋と衝撃を出す。
    // ドームがないとき(展開と維持の間でないとき)は、何もしない
    void AddHit(const Vector3& worldPosition);

    // ドームを描く。雲の合成のあとに呼ぶ。見えていないときは、何もしない
    void Draw();

    // 画面全体の白いフラッシュを描く。ほかの演出より手前に出すので、最後に呼ぶ。光っていないときは、何もしない
    void DrawFlash();

    // 演出を止めて、ドームを消す(ボスの撃破など)
    void Stop();

private:
    bool EnsureInitialized_();
    // 乱数(範囲 [lo, hi])
    float Random_(float lo, float hi);
    // 球面上の、ランダムな向き(長さ 1)
    Vector3 RandomDirection_();
    // 砕けた瞬間の、破片の火花と、中心の光の玉を出す
    void EmitBreakBurst_();

    ShieldDome dome_;
    ArcRing flash_;
    bool ready_ = false;
    bool flashReady_ = false;
    bool initializeFailed_ = false; // 作るのに失敗した(毎フレーム作り直さない)

    BossShieldView view_;
    BossShieldStage previousStage_ = BossShieldStage::None;
    Vector3 center_{ 0.0f, 0.0f, 0.0f };
    Vector3 cameraForward_{ 0.0f, 0.0f, 1.0f };
    float time_ = 0.0f;
    std::vector<ShieldDome::Hit> hits_; // 古い波紋から順に並べる
    std::mt19937 rng_{ 20261126u };

    // 描くときの値(Update が決める)
    bool visible_ = false;
    float radius_ = 0.0f;
    float appear_ = 1.0f;
    float breakProgress_ = 0.0f;
    float alpha_ = 1.0f;
    float flashAlpha_ = 0.0f;
};
