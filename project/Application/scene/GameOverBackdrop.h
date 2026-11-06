#pragma once
#include "GameOverIslands.h"
#include "graphics/SeaSkySettings.h"
#include "graphics/VolumeCloudSettings.h"
#include <memory>

class GameCamera;
class Sprite;

// ゲームオーバー画面の見た目の既定値。蝶は雲の「中」へ落ちて沈んだので、この画面は雲の層の中から見た景色にする。
// 値は ImGui(Game Over Cloud)で調整でき、気に入った値をここへ写す
namespace GameOverCloudDefaults {
// カメラの置き場所: 雲の層の下面から上面までの割合(0 = 下面、1 = 上面)。中ほどは雲が最も濃い所で、上面の近くは薄い。
// 切り替わった直後は、落下カメラが沈み込んだ高さ(上面の少し下)から始めて、晴れる間に、層の深い所へ沈む
constexpr float kStartCameraLayerRatio = 0.85f;
constexpr float kCameraLayerRatio = 0.70f;

// カメラの向き(水平からの角度。度。負で下向き)。
// 落下から切り替わった直後は、落下カメラの最後と同じ「真下の雲を見下ろす」向きで、遷移が晴れる間に、水平に近い向きへ戻る
// (真下ちょうどは、カメラの上方向が定まらないので避ける)
constexpr float kStartPitchDeg = -85.0f;
constexpr float kSettledPitchDeg = -12.0f;

// 雲の濃さ(消光係数。大きいほど見通しが短い)。切り替わった直後は、落下の最後の雲(ゲームの雲海と同じ濃さ。画面を覆っている)から
// 始めて、晴れる間に薄くして、見通しを開く。薄くした先が「雲の中の見え方」で、遠くのもの(のちの島)がもやに溶けて見える距離を決める
constexpr float kSettledDensity = 0.06f;
// 光の強さの倍率(ゲームの雲海の設定に掛ける)。切り替わった直後は 1(落下の最後と同じ明るさ)から、晴れる間に落ち着いた値へ
constexpr float kSettledSunScale = 0.60f;
constexpr float kSettledAmbientScale = 0.90f;

// 降下: 遷移が晴れて落ち着いたあと、カメラが雲の中をゆっくり降りて、巣の島へ寄り、止まる(以後の灰の集まりと復活は、この終点の姿勢が前提)。
// 終点: 巣のくぼみの中心の上 kEndHeightAboveNest、見下ろす角度 kEndPitchDeg(巣から手前へ、高さ / tan(角度) だけ離れた位置)
constexpr float kDescentSeconds = 4.5f;
constexpr float kEndHeightAboveNest = 10.0f;
constexpr float kEndPitchDeg = -38.0f;

// 空(雲の層の上の抜け)を塗りつぶす膜。雲より先に描かれるので、雲の薄い所から見える空だけを、雲の中の灰白にする
constexpr float kSkyVeilR = 0.70f;
constexpr float kSkyVeilG = 0.74f;
constexpr float kSkyVeilB = 0.80f;
constexpr float kSkyVeilAlpha = 0.90f;

// 雲を描いたあとに全体へ重ねる色かぶり(雲の中の空気の色)。切り替わった直後は 0 から、晴れる間に落ち着いた値へ
constexpr float kTintR = 0.55f;
constexpr float kTintG = 0.62f;
constexpr float kTintB = 0.75f;
constexpr float kSettledTintAlpha = 0.12f;

// 降下が進むにつれて、霞を弱める(近づいた島の緑と土の茶を、はっきり見せるため。遠くの島は、雲の濃さが残るので霞んだまま)。
// 終点(降下の進み 1)での、色かぶりの弱まり(1 で完全に消える)と、雲の濃さの倍率(落ち着いた濃さに掛ける)
constexpr float kEndTintRelief = 1.0f;
constexpr float kEndDensityScale = 0.5f;

// 復活の飛び立ちで、雲が画面を満たすときの白かぶり(満たし 1 での色かぶりの不透明度。色は白)。
// 雲の濃さと光は、落下の最後(ゲームの雲海と同じ)へ戻して、ゲームシーンのスタート演出(雲の中から昇る)とそろえる
constexpr float kFillWhiteAlpha = 0.0f;
} // namespace GameOverCloudDefaults

// ゲームオーバー画面の背景。死亡演出で蝶が沈んだ雲の層の「中」から、周りの雲を見る(ゲームの背景と同じ空と雲の層)。
// 落下からの切り替えは、遷移の霧ではなく、この雲そのもので隠す:
//   落下カメラが雲へ沈んで画面が雲で満たされた所で、このシーンへ入れ替わる。このシーンも雲の中にいて、
//   最初は落下の最後と同じ濃さ・向き(真下を見下ろす)で始まり、遷移が晴れる間に、雲が薄くなって見通しが開き、カメラが水平へ戻る。
// ResultBackdrop(クリア画面の空)と同じ使い方(Begin と End の間に、見出しなどのスプライトを描く)。
// クラスを分けた理由: ResultBackdrop は「空だけの背景」で、こちらは「雲の中 + 晴れる間の変化 + 調整 UI」と持ち物が違うため
class GameOverBackdrop {
public:
  GameOverBackdrop();
  ~GameOverBackdrop(); // Sprite を前方宣言で持つので、実装は .cpp へ

  void Initialize();
  // deltaTime: 経過(秒)。revealProgress: 遷移の覆いが晴れる進み具合(0〜1。遷移が無ければ 1)。
  // 雲の濃さ・光・カメラの向きは、この進み具合で「落下の最後の状態」から「落ち着いた状態」へ移る
  void Update(float deltaTime, float revealProgress);
  // 降下の長さ(秒)。展示で、GameOver に留まる時間を決めるのに使う
  float GetDescentSeconds() const { return descentSeconds_; }
  // 遷移が晴れきってからの経過(秒。晴れきるまでは 0)。灰の集まりの台本が、降下の始まりに合わせるのに使う
  float GetSettledSeconds() const { return settledTime_; }
  // 巣の島のくぼみの中心(ワールド)。灰の集まる先
  const Vector3 &GetNestCenter() const { return islands_.GetNestCenter(); }
  // --- 復活の演出が使う窓口 ---
  // 今のカメラの目の位置と注視点(降下の終点の姿勢を、復活の始まりの姿勢として受け取る)
  const Vector3 &GetCameraEye() const { return cameraEye_; }
  const Vector3 &GetCameraTarget() const { return cameraTarget_; }
  // カメラを上書きする(次の Update から反映。ClearCameraOverride で通常に戻る)
  void SetCameraOverride(const Vector3 &eye, const Vector3 &target);
  void ClearCameraOverride() { cameraOverride_ = false; }
  // 画角の倍率(1 で通常。1 より小さいと、画面が寄る)。復活のバーストの寄りに使う
  void SetFovScale(float scale) { fovScale_ = scale; }
  // 雲が画面を満たす度合い(0 〜 1)。上げるほど、雲が濃く、明るくなり、白くかぶさる
  void SetCloudFill(float fill) { cloudFill_ = fill; }
  // 空を描き始め、雲を依頼する
  void Begin();
  // 雲を描いてシーンへ合成する。見出しなどのスプライトは、これを呼んだあとに Draw する
  // (雲は深度を見て重なり、深度を持たないスプライトは、雲より前に描くと雲に隠れてしまうため)
  void ResolveClouds();
  // シーンの描画を終える(遷移の最終パスもここで動く)。ResolveClouds のあとで呼ぶ
  void End();

#ifdef USE_IMGUI
  // 調整用の窓(Debug / Development のみ)。雲の濃さ、見える距離、光の向き、色をスライダーで変える
  void DrawDebugUi();
#endif

private:
  // 進み具合から、今の雲・カメラ・膜の状態を決めて反映する
  void ApplyLook_(float ease);
  // カメラの目の位置と向きを決める。ease: 遷移が晴れる進み(なめらか)。descent: 降下の進み(0〜1、なめらか)
  void ApplyCamera_(float ease, float descent);

  std::shared_ptr<GameCamera> camera_;
  std::unique_ptr<Sprite> skyVeil_; // 空を塗りつぶす膜(画面全体の半透明)
  std::unique_ptr<Sprite> tint_;    // 雲を描いたあとに重ねる色かぶり(画面全体の半透明)
  SeaSkySettings seaSky_;
  GameOverIslands islands_;         // 雲の中の浮島の群れ(雲を合成する前に描く)
  VolumeCloudSettings baseCloud_;   // ゲームの雲海と同じ設定(落下の最後の状態)。濃さと光は、ここから ApplyLook_ が導く
  VolumeCloudSettings cloudLayer_;  // 今フレームに描く雲
  float time_ = 0.0f;

  // 調整できる、落ち着いた状態の値(既定は GameOverCloudDefaults)
  float startCameraLayerRatio_ = GameOverCloudDefaults::kStartCameraLayerRatio;
  float cameraLayerRatio_ = GameOverCloudDefaults::kCameraLayerRatio;
  float startPitchDeg_ = GameOverCloudDefaults::kStartPitchDeg;
  float settledPitchDeg_ = GameOverCloudDefaults::kSettledPitchDeg;
  float settledDensity_ = GameOverCloudDefaults::kSettledDensity;
  float settledSunScale_ = GameOverCloudDefaults::kSettledSunScale;
  float settledAmbientScale_ = GameOverCloudDefaults::kSettledAmbientScale;
  float skyVeilColor_[3] = {GameOverCloudDefaults::kSkyVeilR, GameOverCloudDefaults::kSkyVeilG, GameOverCloudDefaults::kSkyVeilB};
  float skyVeilAlpha_ = GameOverCloudDefaults::kSkyVeilAlpha;
  float tintColor_[3] = {GameOverCloudDefaults::kTintR, GameOverCloudDefaults::kTintG, GameOverCloudDefaults::kTintB};
  float settledTintAlpha_ = GameOverCloudDefaults::kSettledTintAlpha;
  float endTintRelief_ = GameOverCloudDefaults::kEndTintRelief;
  float endDensityScale_ = GameOverCloudDefaults::kEndDensityScale;

  // 復活の演出からの上書き
  bool cameraOverride_ = false;
  Vector3 overrideEye_{0.0f, 0.0f, 0.0f};
  Vector3 overrideTarget_{0.0f, 0.0f, 1.0f};
  float cloudFill_ = 0.0f;
  float fovScale_ = 1.0f;
  Vector3 cameraEye_{0.0f, 0.0f, 0.0f};    // 今フレームのカメラ(ApplyCamera_ が書く)
  Vector3 cameraTarget_{0.0f, 0.0f, 1.0f};

  // 降下(落ち着いたあとの経過で進む)
  float descentSeconds_ = GameOverCloudDefaults::kDescentSeconds;
  float endHeightAboveNest_ = GameOverCloudDefaults::kEndHeightAboveNest;
  float endPitchDeg_ = GameOverCloudDefaults::kEndPitchDeg;
  float settledTime_ = 0.0f;          // 遷移が晴れきってからの経過(秒)
  float descent_ = 0.0f;              // 降下の進み(0〜1、なめらか)
  bool debugDescentOverride_ = false; // 確認用: true の間、経過の代わりに debugDescent_ を使う(終点の姿勢を止めて見られる)
  float debugDescent_ = 0.0f;

  // 確認用: true の間、遷移の進み具合の代わりに debugReveal_ を使う(切り替わった直後の見え方を、止めて見られる)
  bool debugRevealOverride_ = false;
  float debugReveal_ = 0.0f;
};
