// ============================================================
// TelegraphComponent.h
// 役割: 敵の攻撃の直前に、「これから何をするか」が見て分かる予備動作(光る、震える)を出す。
//
// なぜ 1 つの部品か: 射撃の 3 部品(EnemyShootComponent、TurretShootComponent、AimedShootComponent)と、
//       突進(ChargeOnApproachComponent)が、同じ光り方の処理を、それぞれに書かないため。
//       攻撃する側は、「いま撃ってよいか」(HoldFire)と「撃った」(End)だけをこの部品に伝える。
//       光る、震えるの見た目は、すべてこの部品の中にある。
//
// 光り方は 2 種類(設定 glowMode で選ぶ)。種類ごとの処理は、光り方のクラス(ITelegraphGlow の派生)が持つ:
//   muzzle : 弾の出口(muzzleOffset の位置)に、光る玉を出し、予備動作の間、大きくする(蜂の針、蜘蛛の口)。MuzzleOrbGlow
//   body   : 体全体の色を、光る色(bodyGlowTint)に替える(鳥の突進、横切りの敵。モデルが立方体の敵)。BodyTintGlow
// この部品は、予備動作の流れ(いつ始めて、いつ終えるか)と震えを持ち、光り方は、区切りで光り方のクラスを呼ぶだけにする。
// 震え(shakeAmplitudeDeg が 0 より大きいとき): 体を前後軸まわりに細かく揺らす(突進の直前)。
//
// 被弾の白いフラッシュ(EnemyComponent)との区別: 光る色は橙か赤。被弾は白。
//
// 使い方(攻撃する側):
//   - 撃つ敵: 毎フレーム HoldFire(次に撃つまでの秒数) を呼び、true なら撃たずに待つ。撃つ瞬間に End() を呼ぶ。
//     予備動作は「撃つ時刻の duration 秒前」に始まり、撃つ時刻にちょうど終わるので、発射の周期は変わらない。
//   - 突進: 突進に入るときに Begin()、動き出すときに End() を呼ぶ(時間は GetDuration() を使う)。
// 島に隠れている間は出さない(IsAllowedToShow_ の中。島の遮蔽の判定は、まだこのブランチに無いので、あとで 1 行足す)。
// ============================================================
#pragma once
#include "AbsoluteEngine/scene/Component.h"
#include "ITelegraphGlow.h"
#include "Type/Vector.h"
#include <memory>
#include <string>

namespace AbsoluteEngine {
class GameObject;
class ISpawner;
class IScreenView;
}

class TelegraphComponent : public AbsoluteEngine::IComponent {
public:
    // spawner: 光る玉を出すときに使う、オブジェクトを出す窓口。screen: 画面内かの判定に使う、画面の窓口。
    // どちらも所有しない。このコンポーネントより長く生きること
    TelegraphComponent(AbsoluteEngine::ISpawner& spawner, AbsoluteEngine::IScreenView& screen);
    ~TelegraphComponent() override;

    void Update(float deltaTime) override;

    std::string GetTypeName() const override { return "TelegraphComponent"; }

    void Serialize(nlohmann::json& j) const override;
    void Deserialize(const nlohmann::json& j) override;

    // 予備動作の長さ(秒)
    float GetDuration() const { return duration_; }
    // 再生中か(Begin から End まで)
    bool IsPlaying() const { return playing_; }

    // 予備動作を始める(再生中なら何もしない)。startElapsed は、途中(経過 startElapsed 秒)から始めるときの値
    void Begin(float startElapsed = 0.0f);
    // 予備動作を終える(見た目を元に戻す)。再生中でなければ何もしない
    void End();

    // 撃つ敵用。次に撃つまでの秒数 secondsUntilFire を渡す(撃つ時刻を過ぎていれば 0 以下)。
    // 戻り値が true の間は、撃たずに待つ。false なら、撃ってよい(予備動作が済んだ、または見せない)。
    //   - 撃つ時刻の duration 秒前になったら、自動で Begin する(撃つ時刻に、ちょうど終わる)
    //   - 予備動作を見せずに撃つ時刻が来たとき(画面に入った直後など)は、そこから始めて、duration 秒待つ
    //   - 画面外、島に隠れているなど、見せられないときは、予備動作なしで false を返す
    bool HoldFire(float secondsUntilFire);

private:
    AbsoluteEngine::ISpawner& spawner_; // 光る玉を出す窓口
    AbsoluteEngine::IScreenView& screen_; // 画面の見え方(カメラの位置、画面内か)を知る窓口

    // 光り方の種類(プレハブの JSON の glowMode。保存するデータとして、数値のまま持つ)
    enum class GlowMode {
        Muzzle, // 弾の出口に光る玉
        Body,   // 体全体の色を替える
    };

    // 予備動作を見せてよいか(画面内か。島に隠れていないか)
    bool IsAllowedToShow_() const;
    // 設定(glowMode と、その光り方の設定値)から、光り方のクラスを作り直す
    // (Factory: 種類の数値を、光り方のクラスへ変換する場所は、ここだけ)
    void RebuildGlow_();
    // いまの予備動作の進み具合(0 〜 1)
    float GetProgress_() const;
    // 体の震えを、いまの経過時間に合わせる
    void UpdateShake_();
    // 持ち主の回転のうち、Z 成分だけを書き換える(X と Y は、そのまま残す)
    void SetOwnerRotateZ_(float rotateZ);

    // ---- 設定(プレハブの JSON で調整できる) ----
    GlowMode glowMode_ = GlowMode::Muzzle;
    float duration_ = 0.4f;                         // 予備動作の長さ(秒)
    Vector3 muzzleOffset_ = { 0.0f, 0.0f, 0.0f };   // 弾の出口の位置(敵のモデルの座標。敵の拡大率を掛けて使う)
    Vector4 glowColor_ = { 1.0f, 0.55f, 0.15f, 1.0f }; // 光る玉の色(橙)
    float orbScaleStart_ = 0.1f;                    // 光る玉の大きさ(予備動作の最初。拡大率 1 の敵のとき)
    float orbScaleEnd_ = 0.45f;                     // 光る玉の大きさ(予備動作の最後)
    Vector4 bodyGlowTint_ = { 2.6f, 0.6f, 0.5f, 1.0f }; // body のとき、体にかける色(赤み。被弾の白と区別する)
    float shakeAmplitudeDeg_ = 0.0f;                // 震えの大きさ(度)。0 なら震えない
    float shakeFrequency_ = 24.0f;                  // 震えの速さ(1 秒の往復の回数)
    float screenMargin_ = 1.0f;                     // 画面の端からの余裕(NDC。1.0 なら、画面内にいる間だけ出す)

    // ---- 状態 ----
    bool playing_ = false;
    float elapsed_ = 0.0f;                          // 予備動作の経過時間
    Vector4 baseColor_ = { 1.0f, 1.0f, 1.0f, 1.0f };
    bool baseColorCaptured_ = false;                // 元の体の色を覚えたか(最初の Update で覚える)
    float baseRotateZ_ = 0.0f;                      // 震える前の傾き
    std::unique_ptr<ITelegraphGlow> glow_;          // 光り方(設定から作る)
};
