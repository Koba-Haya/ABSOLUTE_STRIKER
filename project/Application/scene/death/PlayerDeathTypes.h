#pragma once
#include "Vector.h"

// 自機の死亡演出の、設定と段階の名札。PlayerDeathDirector(時間の進行)と、段階の状態クラス(death/)が共通に使う。
// 分けた理由: 状態クラスが PlayerDeathDirector.h を読むと、持つ側と状態クラスが、お互いのヘッダを読む形になるため、両方が使う値だけを置く。

// 演出の長さ・動きの設定。ステージごとに変わりうる値なので、コードに埋め込まず、まとめて持つ
namespace PlayerDeathDefaults {
// 各サブフェーズの長さ(秒)。Cover は、遷移の覆い(約 2 秒)が担うので、ここでは長さを持たない。
// 全体: HitStop + Crumble + Fall + Impact + 遷移の覆い で、6 秒以内にする
constexpr float kHitStopSeconds = 0.12f;
constexpr float kCrumbleSeconds = 1.2f;
constexpr float kFallSeconds = 1.8f;
constexpr float kImpactSeconds = 0.6f;

// ヒットストップのあとの、世界の時間の速さ(1 で通常、0 で停止)と、そこへ向かう時間(秒)
constexpr float kSlowMotionScale = 0.3f;
constexpr float kSlowMotionRampSeconds = 0.25f;

// 蝶のディゾルブ(朽ちる)。マスク(butterfly_dissolve_mask.png)の値が、しきい値以下の所から消える。
// マスクは、翅の外側ほど小さく、胴が最も大きい(0.95)ので、しきい値を 0 から終わりの値まで上げると、翅の外側から内側へ、最後に胴が朽ちる
// しきい値は、崩壊の始まり(ヒットストップの終わり)から、着地を過ぎて kDissolveTailSeconds 秒後まで、線形に上げる。
// 翅(マスク 0.10〜0.80)は着地の少し手前までに朽ち、胴(マスク 0.95)は着地まで残って、雲に沈みながら消える
constexpr float kDissolveStartThreshold = 0.0f;   // 崩壊の始まり(何も消えない)
constexpr float kDissolveEndThreshold = 1.0f;     // 崩壊の終わり(胴まで、すべて消える)
constexpr float kDissolveTailSeconds = 0.6f;      // 着地のあと、しきい値が終わりの値へ届くまでの時間(秒)
constexpr float kDissolveEdgeRange = 0.12f;       // 縁の光る帯の幅(しきい値の単位。熾火は灰 -> 赤 -> 橙と色が変わるので、やや広く取る)
constexpr float kDissolveEdgeColorR = 1.00f;      // 縁の色(熾火の橙。帯の中で、外縁の灰と暗い赤へつながる)
constexpr float kDissolveEdgeColorG = 0.42f;
constexpr float kDissolveEdgeColorB = 0.08f;
constexpr float kDissolveEmberAmount = 1.0f;      // 縁を熾火で光らせる強さ(0 で従来の「縁の色へ混ぜるだけ」)

// デバッグ用: true の間、死亡の位置と向きを動かさない(蝶は、死んだ場所に留まって朽ちる)。
// 崩壊(ディゾルブと破片)だけを、その場で見たいときに使う(通常は false。落下する)
constexpr bool kDebugHoldPlayerPosition = false;

// スキップを受け付け始めるまでの猶予(秒)。0 で、すぐ受け付ける
constexpr float kSkipGuardSeconds = 0.5f;

// 自機の落下の動き。前へ進む勢い(レールの速さに掛ける割合。時間とともに減る)、着地する高さ、機首が下がる量。
// 落下は、「ヒットストップのあと、崩壊と落下の終わりに、雲の上面(kLandingHeight)へちょうど着く」ように、着く時刻から逆算する
// (落ち始めはゆっくりで、だんだん速くなる放物線)。着いたあとは、その速さのまま、雲の中へ沈みながら減速する
constexpr float kForwardSpeedScale = 0.8f;      // 死亡の瞬間の、前へ進む勢い(レールの速さの割合)
constexpr float kForwardDecayPerSecond = 1.2f;  // 前へ進む勢いが減る速さ(1 秒あたりの指数的な減衰率)
constexpr float kLandingHeight = -24.0f;        // 着地する高さ(ワールド y)。雲の上面。シーンが、雲の設定の値で上書きする
constexpr float kSinkDecayPerSecond = 2.0f;     // 雲へ沈んだあと、沈む速さが減る速さ(1 秒あたりの減衰率)
constexpr float kNoseDropRadians = 0.9f;        // 機首が下がる最大の角度(ラジアン)
constexpr float kNoseDropSeconds = 1.5f;        // 機首が下がりきるまでの時間(秒)
} // namespace PlayerDeathDefaults

struct PlayerDeathSettings {
  float hitStopSeconds = PlayerDeathDefaults::kHitStopSeconds;
  float crumbleSeconds = PlayerDeathDefaults::kCrumbleSeconds;
  float fallSeconds = PlayerDeathDefaults::kFallSeconds;
  float impactSeconds = PlayerDeathDefaults::kImpactSeconds;
  float dissolveStartThreshold = PlayerDeathDefaults::kDissolveStartThreshold;
  float dissolveEndThreshold = PlayerDeathDefaults::kDissolveEndThreshold;
  float dissolveTailSeconds = PlayerDeathDefaults::kDissolveTailSeconds;
  float dissolveEdgeRange = PlayerDeathDefaults::kDissolveEdgeRange;
  Vector3 dissolveEdgeColor{PlayerDeathDefaults::kDissolveEdgeColorR, PlayerDeathDefaults::kDissolveEdgeColorG,
                            PlayerDeathDefaults::kDissolveEdgeColorB};
  float dissolveEmberAmount = PlayerDeathDefaults::kDissolveEmberAmount;
  bool debugHoldPlayerPosition = PlayerDeathDefaults::kDebugHoldPlayerPosition;
  float slowMotionScale = PlayerDeathDefaults::kSlowMotionScale;
  float slowMotionRampSeconds = PlayerDeathDefaults::kSlowMotionRampSeconds;
  float skipGuardSeconds = PlayerDeathDefaults::kSkipGuardSeconds;
  float forwardSpeedScale = PlayerDeathDefaults::kForwardSpeedScale;
  float forwardDecayPerSecond = PlayerDeathDefaults::kForwardDecayPerSecond;
  float landingHeight = PlayerDeathDefaults::kLandingHeight;
  float sinkDecayPerSecond = PlayerDeathDefaults::kSinkDecayPerSecond;
  float noseDropRadians = PlayerDeathDefaults::kNoseDropRadians;
  float noseDropSeconds = PlayerDeathDefaults::kNoseDropSeconds;
};

enum class PlayerDeathPhase {
  Idle,     // 始まっていない
  HitStop,
  Crumble,
  Fall,
  Impact,
  Cover
};
