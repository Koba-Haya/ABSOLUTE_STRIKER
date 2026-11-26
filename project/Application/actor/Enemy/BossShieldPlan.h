#pragma once

// ボスのシールドの、数値と、外へ見せる状態(データ)。
//
// 分けた理由: シールドの状態クラス(BossShieldStageState と、その 4 つの段階)に数値を埋め込まず、調整を、ここの定数の変更だけで済ませるため。
// また、シールドの見た目(今後作るドームや、時間切れの警告)が、状態クラスを直接知らずに、このデータだけを読めるようにする。
//
// シールドの流れ:
//   Deploy(展開) : ボスの HP が、次のフェーズの境目を切った瞬間に始まる。攻撃を中断して、シールドを張る。この間は、攻撃を受けても何も起きない
//   Hold(維持)   : 制限時間の間、ボスは力を溜める。自機の攻撃は、ボスの HP ではなく、シールドの耐久を減らす。耐久が 0 になれば成功
//   Break(成功)  : シールドが砕ける。ボスはひるみ、少しの間、攻撃しない。そのあと、次のフェーズへ進む
//   Burst(失敗)  : 制限時間が来た。全画面が光る爆発が起きて、自機がダメージを受ける(回避の無敵の間は受けない)。そのあと、次のフェーズへ進む
enum class BossShieldStage {
    None,   // シールドの最中ではない
    Deploy,
    Hold,
    Break,
    Burst,
};

// 外から読める、シールドの今の状態(毎フレーム、状態クラスが更新する)
struct BossShieldView {
    BossShieldStage stage = BossShieldStage::None;
    float hpRatio = 0.0f;       // シールドの耐久の残り(1 = 満タン、0 = 壊れた)
    float timeRatio = 0.0f;     // Hold の残り時間の割合(1 = 始まり、0 = 時間切れ)
    float stageProgress = 0.0f; // いまの段の中の進み具合(0 〜 1)
    bool warning = false;       // 時間切れの直前か(爆発を避ける回避を促す、警告の間)
};

namespace BossShieldDefaults {
constexpr int kShieldHp = 10;                // シールドの耐久(自機の 1 発が 1 ダメージ)
constexpr float kDeploySeconds = 1.2f;       // 展開の時間(この間は無敵)
constexpr float kTimeLimitSeconds = 8.0f;    // 維持の制限時間
constexpr float kWarningSeconds = 1.5f;      // 時間切れの何秒前から、警告を出すか
constexpr float kBurstSeconds = 1.0f;        // 失敗の爆発の段の長さ(爆発は、段の始まりに起きる)
constexpr float kBreakStaggerSeconds = 1.5f; // 成功のあとの、ボスがひるむ時間(自機の攻撃のチャンス)
constexpr int kBlastDamage = 2;              // 時間切れの爆発で、自機が受けるダメージ(回避の無敵の間は 0)
} // namespace BossShieldDefaults
