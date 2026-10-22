#pragma once
#include "Method.h"
#include "ModelInstance.h"

class Renderer;

// 突き破った瞬間の衝撃の演出(白いフラッシュ)の設定。ステージごとに変えられるよう、データとして持つ。
namespace StageImpactDefaults {
// 白いフラッシュが、最も濃い状態から消えるまでの時間(秒)
constexpr float kFlashSeconds = 0.22f;
// 白いフラッシュの、最大の濃さ(0 〜 1)。弱めにして、突き破った瞬間を、まぶしすぎず、軽く光らせる
constexpr float kFlashPeakAlpha = 0.6f;
} // namespace StageImpactDefaults

struct StageImpactSettings {
  float flashSeconds = StageImpactDefaults::kFlashSeconds;
  float flashPeakAlpha = StageImpactDefaults::kFlashPeakAlpha;
};

// 突き破った瞬間の衝撃の演出(白いフラッシュ)を扱うクラス。
// 衝撃の時刻を受け取って、フラッシュを始め、時間を進めるだけ。いつ衝撃が起きるかは、呼ぶ側が決める。
// 描画の時点のカメラに合わせるので、カメラを描画の間だけずらす演出と組み合わせても、位置がずれない
class StageImpactEffects {
public:
  // フラッシュの白い板のモデルを読み込む。失敗したら、フラッシュが出ないだけで、ほかの処理は動く
  // flashModelPath: 白い板のモデル
  void Initialize(const char *flashModelPath);

  // 衝撃を起こす(フラッシュを始める)
  void Trigger(const StageImpactSettings &settings);

  // 時間を進める(衝撃が起きていないときは何もしない)
  void Update(float deltaTime);

  // 白いフラッシュを描く。カメラのすぐ前に、画面全体を覆う白い板を出す。光っていないときは何もしない
  // eye / target: 描画するカメラの位置と注視点
  void DrawFlash(Renderer *renderer, const Vector3 &eye, const Vector3 &target);

private:
  ModelInstance flashModel_;
  StageImpactSettings settings_;
  float elapsed_ = 0.0f;    // 衝撃からの経過(秒)
  bool triggered_ = false;  // 衝撃が起きたあと(経過を数えている間)は true
  bool flashReady_ = false; // フラッシュのモデルを読み込めたら true
};
