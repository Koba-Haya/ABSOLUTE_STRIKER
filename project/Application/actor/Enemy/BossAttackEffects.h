#pragma once
#include "AbsoluteEngine/graphics/primitive/ArcRing.h"
#include "BossActions.h"
#include "BossAttackPlan.h"
#include "Type/Vector.h"
#include <cstdint>
#include <random>

// ボスの攻撃の演出(予告・溜め・発射)を受け持つクラス。口の前の「切れ目のある赤い輪」(ArcRing)と、粒(吸い込み、火花)。
//
// 分けた理由: 攻撃の流れ(BossAttackState)と体(BossComponent)に、見た目の数値を混ぜないため。
// 攻撃の状態は、段が変わったことを伝える(IBossActions::SetAttackStage)だけで、見た目は、ここで決める。
//
// 動き(charge.gif を参考にした、「予兆 -> 溜め -> 解放 -> 残り」の型):
//   予告(Telegraph): 口の前に、切れ目の大きな輪が、回りながら現れる(濃くなる)。少し縮む
//   溜め(Charge)   : 輪が口へ縮み(溜めの終わりの少し前に口へ着く)、切れ目が閉じ、速く回り、明るくなる。
//                    輪のあたりから、粒が口へ吸い込まれる(だんだん増える)
//   発射(Fire)     : 輪が弾けるように広がって消える。光の閃光と、放射状の火花がはじけ、火の粉が少し残る
// 弧の数: 単発弾は固定、扇状弾は、弾の数と同じにする(1 本の弧が、1 発の弾に当たる)。扇状弾は、輪も大きくする。
// 粒は、CPU のパーティクル(ParticleManager)の、専用のグループで出す(アルファ合成。明るい空の上でも、色が白く飛ばない)。
// 使い方: OnStageChanged(段が変わるとき) / Update(毎フレーム) / Draw(毎フレーム。シーンの描画の中)
namespace BossAttackEffectDefaults {
// --- 輪 ---
// 板の大きさ(板の半分の長さ。ワールド。ボスは幅 約 9)。輪の半径は、これに 0.7 前後と、下の倍率を掛けた値
constexpr float kQuadWorldRadius = 5.5f;
// 輪の太さ(板の半分の大きさを 1 とした割合)
constexpr float kRingInnerRadius = 0.70f;
constexpr float kRingOuterRadius = 0.78f;
// 輪の大きさの倍率(板に対する)。予告の始め -> 予告の終わり(少し縮む)-> 溜めの終わり(口へ着く)-> 発射の終わり(広がる)
constexpr float kScaleTelegraphStart = 1.00f;
constexpr float kScaleTelegraphEnd = 0.80f;
constexpr float kScaleChargeEnd = 0.20f;
constexpr float kScaleFireEnd = 1.80f;
// 溜めのうち、輪が口へ着くまでの割合(0 〜 1)。残りは、口に着いたまま、強く光って、発射を待つ
constexpr float kChargeArriveShare = 0.80f;
// 扇状弾のとき、輪の大きさに掛ける倍率
constexpr float kSpreadScaleMultiplier = 1.25f;
// 弧の数(単発弾)
constexpr uint32_t kAimedArcCount = 4;
// 切れ目の割合: 予告の始め -> 溜めの終わり(閉じていく)-> 発射の終わり(ばらける)
constexpr float kGapTelegraph = 0.55f;
constexpr float kGapChargeEnd = 0.12f;
constexpr float kGapFireEnd = 0.70f;
// 回る速さ(ラジアン / 秒): 予告の速さ -> 溜めの終わりの速さ。発射では、溜めの終わりの速さのまま
constexpr float kSpinTelegraph = 2.5f;
constexpr float kSpinChargeEnd = 9.0f;
// 明るさ: 予告 -> 溜めの終わり
constexpr float kIntensityTelegraph = 1.0f;
constexpr float kIntensityChargeEnd = 2.2f;
// 予告のうち、輪が濃くなりきるまでの割合(0 〜 1)
constexpr float kTelegraphFadeInShare = 0.5f;
// 発射の、輪が広がって消えるまでの秒数(発射のあとの隙より短くする)
constexpr float kFireSeconds = 0.30f;
// 輪の色(縁、芯)。ボスの弾(ピンク寄りの赤)に合わせた、赤
inline const Vector4 kEdgeColor{ 1.00f, 0.12f, 0.12f, 1.0f };
inline const Vector4 kCoreColor{ 1.00f, 0.78f, 0.70f, 1.0f };

// --- 粒 ---
// 粒のグループの名前と画像は、BossEffectParticles.h にある(弾の見た目と共有するため)
// 吸い込まれる粒(溜め): 1 秒あたりに出す数(溜めの始め -> 終わり)
constexpr float kConvergeRateStart = 50.0f;
constexpr float kConvergeRateEnd = 180.0f;
// 出す位置: 輪の外側の半径に、この倍率(範囲)を掛けた距離(輪のあたりから出る)
constexpr float kConvergeSpawnRadiusMin = 0.95f;
constexpr float kConvergeSpawnRadiusMax = 1.35f;
// 寿命(秒)。口へ向かう速さは「距離 / 寿命」で決まる(寿命の終わりに、口へ着く)
constexpr float kConvergeLifeMin = 0.22f;
constexpr float kConvergeLifeMax = 0.34f;
// 大きさ(板の一辺。ワールド)。始め -> 終わり(口へ近づくと小さくなる)
constexpr float kConvergeSizeStart = 0.30f;
constexpr float kConvergeSizeEnd = 0.06f;
constexpr float kConvergeSizeJitter = 0.25f; // 大きさのばらつき(割合)
inline const Vector4 kConvergeColor{ 1.00f, 0.30f, 0.18f, 0.95f };
// 発射の火花(放射状。輪の面に沿って、四方へ飛ぶ)
constexpr int kFireSparkCount = 48;
constexpr float kFireSparkSpeedMin = 9.0f;
constexpr float kFireSparkSpeedMax = 20.0f;
constexpr float kFireSparkLifeMin = 0.25f;
constexpr float kFireSparkLifeMax = 0.50f;
constexpr float kFireSparkSizeStart = 1.00f; // 星形の画像は、光条が細いので、板を大きめにする
constexpr float kFireSparkSizeEnd = 0.12f;
inline const Vector4 kFireSparkColor{ 1.00f, 0.55f, 0.35f, 1.0f };
// 発射の閃光(口で、一瞬ふくらんで消える大きな光)
constexpr float kFlashSizeStart = 2.0f;
constexpr float kFlashSizeEnd = 6.0f;
constexpr float kFlashLife = 0.16f;
inline const Vector4 kFlashColor{ 1.00f, 0.72f, 0.62f, 0.9f };
// 火の粉(発射のあと、口のまわりにゆっくり漂って消える)
constexpr int kEmberCount = 14;
constexpr float kEmberSpeedMin = 1.0f;
constexpr float kEmberSpeedMax = 3.0f;
constexpr float kEmberLifeMin = 0.5f;
constexpr float kEmberLifeMax = 0.9f;
constexpr float kEmberSizeStart = 0.20f;
constexpr float kEmberSizeEnd = 0.02f;
inline const Vector4 kEmberColor{ 1.00f, 0.30f, 0.15f, 0.9f };
} // namespace BossAttackEffectDefaults

class BossAttackEffects {
public:
    // 攻撃の段が変わったとき(BossComponent が、状態からの通知を受けて呼ぶ)。spec: いまの攻撃の定義(段の長さと種類を読む)
    void OnStageChanged(BossAttackStage stage, const BossAttackSpec& spec);

    // 毎フレーム。muzzle: 口の前(出口)のワールド位置。nullptr なら、演出を出さない(モデルに出口のボーンが無いとき)
    void Update(float deltaTime, const Vector3* muzzle);

    // 輪を描く。シーンの描画の中で、物体を描いたあとに呼ぶ(粒は、シーンの粒の描画が描く)
    void Draw();

    // 演出を止めて、輪を消す(ボスの撃破など)
    void Stop();

private:
    // 輪の板と、粒のグループを、初めて使うときに作る。作れなければ false(演出が出ないだけで、ゲームは続ける)
    bool EnsureInitialized_();
    // 輪の見た目(ArcRing のパラメータと、大きさ)を、いまの段と進み具合から決める
    void ApplyLook_(float progress);
    // 溜めの間、輪のあたりから口へ吸い込まれる粒を出す(progress: 溜めの進み具合)
    void EmitConverge_(float deltaTime, float progress);
    // 発射の瞬間に、閃光と、放射状の火花と、火の粉を出す
    void EmitFireBurst_();
    // 範囲 [lo, hi] の乱数
    float Random_(float lo, float hi);

    ArcRing ring_;
    bool ringReady_ = false;
    bool initializeFailed_ = false; // 作るのに失敗した(毎フレーム作り直さない)

    BossAttackStage stage_ = BossAttackStage::None;
    BossAttackSpec spec_{};
    float stageElapsed_ = 0.0f; // いまの段に入ってからの時間
    float stageSeconds_ = 0.0f; // いまの段の長さ
    float spin_ = 0.0f;         // 輪の回転角(ラジアン)
    bool fireBurstPending_ = false; // 発射の段に入った(次の Update で、火花を出す)
    // 描くときの状態(Update が決める)
    bool visible_ = false;
    Vector3 position_{ 0.0f, 0.0f, 0.0f };
    float worldRadius_ = 0.0f;
    // 口の動き(粒の目標を、動きの先へ置くために使う)と、粒を出すための値
    Vector3 previousMuzzle_{ 0.0f, 0.0f, 0.0f };
    bool hasPreviousMuzzle_ = false;
    Vector3 muzzleVelocity_{ 0.0f, 0.0f, 0.0f };
    Vector3 cameraRight_{ 1.0f, 0.0f, 0.0f }; // 粒を、画面に平行な面へ出すための、カメラの右と上
    Vector3 cameraUp_{ 0.0f, 1.0f, 0.0f };
    float emitCarry_ = 0.0f; // 1 フレームで出しきれなかった粒の端数
    std::mt19937 rng_{ 20261125u };
};
