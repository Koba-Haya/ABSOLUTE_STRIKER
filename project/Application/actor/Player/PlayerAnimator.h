#pragma once

namespace AbsoluteEngine {
class ModelComponent;
}

// PlayerAnimator の調整値
namespace PlayerAnimatorDefaults {
// 被弾のアニメーション(Hit)へ切り替えるときのクロスフェードの長さ(秒)。被弾は瞬間的な衝撃なので短くする
constexpr float kHitBlendSeconds = 0.06f;
// Hit が終わって通常の羽ばたきへ戻るときのクロスフェードの長さ(秒)。Hit の終わりは滑空の姿勢なので、ゆっくり羽ばたきへ戻す
constexpr float kReturnBlendSeconds = 0.25f;
// 死亡のきりもみ(Tumble)へ切り替えるときのクロスフェードの長さ(秒)
constexpr float kDeathBlendSeconds = 0.1f;
} // namespace PlayerAnimatorDefaults

// 自機の蝶のアニメーションの切り替えだけを担当するクラス(通常は羽ばたきをループ、被弾でよろめき、終わったら羽ばたきへ戻る)。
// 何のアニメーションをいつ再生するかの判断だけを持ち、実際の再生は ModelComponent に任せる。
// 被弾の検知(HP が減る)はしない。HP を減らす PlayerComponent が NotifyHit() で知らせる(検知の場所を 1 か所にして、二重処理を避ける)。
class PlayerAnimator {
public:
  // 状態を最初に戻す(シーンの開始時)。最初の羽ばたきの再生は、モデルを読み込むシーン側が行う
  void Reset();

  // 被弾したことを知らせる。再生の切り替えは、次の Update で行う(衝突の処理の途中でアニメーションを触らないため)
  void NotifyHit() { hitRequested_ = true; }

  // 死んで落ち始めたことを知らせる。次の Update で、きりもみ(Tumble)をループ再生する。
  // 以降、被弾の通知(NotifyHit)は無視する(死亡中に Hit へ戻らないように)
  void NotifyDeath() { deathRequested_ = true; }

  // 毎フレーム呼ぶ。被弾の通知があれば Hit を再生し、Hit が終わったら羽ばたきへ戻す。model が null なら何もしない
  void Update(AbsoluteEngine::ModelComponent* model);

  // いま被弾のアニメーションを再生中か
  bool IsHitPlaying() const { return state_ == State::Hit; }

private:
  enum class State {
    Normal, // 通常(羽ばたき)
    Hit,    // 被弾のよろめき中
    Dying   // 死亡(きりもみで落ちている間。元には戻らない)
  };

  State state_ = State::Normal;
  bool hitRequested_ = false;
  bool deathRequested_ = false;
};
