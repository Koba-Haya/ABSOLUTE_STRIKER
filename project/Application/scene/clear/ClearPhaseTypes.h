#pragma once

// ボス撃破の演出の、設定と段階の名札。ClearDirector(時間の進行)、段階の状態クラス(clear/)、見た目のクラス(BossExplosion など)が共通に使う。
// 分けた理由: 状態クラスが ClearDirector.h を読むと、ClearDirector(状態を持つ側)と状態クラスが、お互いのヘッダを読む形になるため、
// 両方が使う値だけを、このヘッダに置く。

// 演出の長さ・動きの設定。ステージ(ボス)ごとに変わりうる値なので、コードに埋め込まず、まとめて持つ
namespace ClearDefaults {
// 各サブフェーズの長さ(秒)。Cover は、遷移の覆いが担うので、ここでは長さを持たない。
// 全体: 各サブフェーズの合計は、約 9.2 秒(ユーザーの了承済み。光線が出そろうまでと、崩れる様子を見せる時間を優先した。以前の目安は 6 秒以内)
constexpr float kHitStopSeconds = 0.15f;
// Crack と Peel は、長くしてある(以前は 0.8 秒と 1.0 秒)。理由は 2 つ:
//   Crack: 光線(BossLightBeams)が、Crack の間に、約 0.25 秒おきに 1 本ずつ出て、出そろうまで。短いと、まとまって出て、「徐々に増えていく」見え方にならない
//   Peel: ボスが崩れていく様子(セルが 1 つずつ消える)と、爆破の球(BossExplosion)が広がる様子を、ゆっくり見せるため。短いと、すぐに崩れ切ってしまう
constexpr float kCrackSeconds = 4.0f;
constexpr float kPeelSeconds = 3.0f;
constexpr float kBurstSeconds = 0.5f;
constexpr float kFlowSeconds = 1.5f;
// 次の島を解放するときだけ、Flow を、この長さへ延ばし(粒が島へ飛び、カメラが追う)、Release を足す(島が解放される)
constexpr float kReleaseFlowSeconds = 2.5f;
constexpr float kReleaseSeconds = 2.0f;

// ヒットストップのあとの、世界の時間の速さ(1 で通常、0 で停止)と、そこへ向かう時間(秒)
constexpr float kSlowMotionScale = 0.3f;
constexpr float kSlowMotionRampSeconds = 0.2f;
// Flow へ入ってから、世界の時間が通常の速さ(1)へ戻るまでの時間(秒)
constexpr float kRecoverRampSeconds = 0.5f;

// スキップを受け付け始めるまでの猶予(秒)。0 で、すぐ受け付ける
constexpr float kSkipGuardSeconds = 0.5f;
} // namespace ClearDefaults

struct ClearSettings {
  float hitStopSeconds = ClearDefaults::kHitStopSeconds;
  float crackSeconds = ClearDefaults::kCrackSeconds;
  float peelSeconds = ClearDefaults::kPeelSeconds;
  float burstSeconds = ClearDefaults::kBurstSeconds;
  float flowSeconds = ClearDefaults::kFlowSeconds;
  float releaseSeconds = 0.0f; // 0 のときは Release を飛ばして Cover へ進む(次の島を解放するときだけ、シーンが kReleaseSeconds を入れる)
  float slowMotionScale = ClearDefaults::kSlowMotionScale;
  float slowMotionRampSeconds = ClearDefaults::kSlowMotionRampSeconds;
  float recoverRampSeconds = ClearDefaults::kRecoverRampSeconds;
  float skipGuardSeconds = ClearDefaults::kSkipGuardSeconds;
};

enum class ClearPhase {
  Idle, // 始まっていない
  HitStop,
  Crack,
  Peel,
  Burst,
  Flow,
  Release,
  Cover
};
