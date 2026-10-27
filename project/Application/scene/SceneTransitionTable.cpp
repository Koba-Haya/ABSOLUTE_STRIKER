#include "SceneTransitionTable.h"

#include "SceneIds.h"

namespace SceneTransitionTable {
namespace {

// 覆う色(リニア値)
const Vector3 kWhite{1.0f, 1.0f, 1.0f};
const Vector3 kBlack{0.0f, 0.0f, 0.0f};
const Vector3 kDeepWater{0.01f, 0.04f, 0.10f}; // 着水して沈む、深い青
const Vector3 kMistGrey{0.52f, 0.52f, 0.60f};  // 雲の中の霧(明るい灰紫。リニア値)

// 時間(秒)
// 雲を抜ける: 入れ替えは、全体の約半分(層の中心。雲が最も濃い瞬間)で行う
constexpr float kCloudDiveOut = 1.7f;  // 雲の層へ入るまで
constexpr float kCloudDiveHold = 0.3f; // 層の中心(入れ替えの瞬間)
constexpr float kCloudDiveIn = 1.6f;   // 層を抜けるまで
// 雲の層の高さ(それぞれのシーンの世界の y)。助走の上昇量(GameIntroDirector)と、タイトルの上昇量(TitleDirector)と合わせる
constexpr float kTitleSlabBottom = 45.0f;
constexpr float kTitleSlabTop = 105.0f;
constexpr float kGameSlabBottom = -95.0f;
constexpr float kGameSlabTop = -25.0f;
constexpr float kDropletsOut = 2.0f;   // レンズの水滴: 水滴が増えてくもる時間(見る間もなく終わらないよう、長めにする)
constexpr float kDropletsIn = 2.2f;    // レンズの水滴: くもりが晴れて水滴が流れ落ちる時間
// ゲームオーバー: 蝶が雲海へ落ちたあと、霧と水滴で覆い、GameOver へ入れ替わって、霧が晴れながら水滴が乾く
constexpr float kMistOut = 1.5f;
constexpr float kMistHold = 0.4f;
constexpr float kMistIn = 1.8f;
constexpr float kGameSeaPlaneY = -35.5f; // ゲームの雲海の平均の高さ(霧の距離を、水平の層との交点から求める)
constexpr float kShortOut = 0.5f;
constexpr float kMediumOut = 0.7f;
constexpr float kLongOut = 0.9f;
constexpr float kShortHold = 0.1f;
constexpr float kMediumHold = 0.15f;
constexpr float kShortIn = 0.5f;
constexpr float kMediumIn = 0.7f;
constexpr float kLongIn = 0.9f;

// ノイズワイプ(雲の形で覆う)の見た目
constexpr float kWipeSoftness = 0.3f;
constexpr float kWipeNoiseScale = 3.0f;

// ズームブラー(突入感)の強さ
constexpr float kZoomBlurStrength = 0.4f;

TransitionSpec MakeSpec(TransitionKind kind, const Vector3 &color, float out, float hold, float in) {
  TransitionSpec spec;
  spec.style.kind = kind;
  spec.style.color = color;
  spec.outSeconds = out;
  spec.holdSeconds = hold;
  spec.inSeconds = in;
  return spec;
}

TransitionSpec MakeFade(const Vector3 &color, float out, float hold, float in) {
  return MakeSpec(TransitionKind::Fade, color, out, hold, in);
}

TransitionSpec MakeNoiseWipe(const Vector3 &color, float out, float hold, float in) {
  TransitionSpec spec = MakeSpec(TransitionKind::NoiseWipe, color, out, hold, in);
  spec.style.softness = kWipeSoftness;
  spec.style.noiseScale = kWipeNoiseScale;
  return spec;
}

TransitionSpec MakeZoomBlur(const Vector3 &color, float out, float hold, float in) {
  TransitionSpec spec = MakeSpec(TransitionKind::ZoomBlur, color, out, hold, in);
  spec.style.blurStrength = kZoomBlurStrength;
  return spec;
}

TransitionSpec MakeRipple(const Vector3 &color, float out, float hold, float in) {
  return MakeSpec(TransitionKind::Ripple, color, out, hold, in);
}

TransitionSpec MakeDroplets(const Vector3 &color, float out, float hold, float in) {
  return MakeSpec(TransitionKind::Droplets, color, out, hold, in);
}

TransitionSpec MakeCloudDive(const Vector3 &color, float out, float hold, float in) {
  TransitionSpec spec = MakeSpec(TransitionKind::CloudDive, color, out, hold, in);
  // 入れ替え前(タイトルの世界: 水面が y = 0)の、雲の層。スタート演出の上昇が、層の中心(約 75)へ届く
  spec.style.slabBottom = kTitleSlabBottom;
  spec.style.slabTop = kTitleSlabTop;
  // 入れ替え後(ゲームの世界: 雲海の平均の高さが y = -35.5、カメラの開始位置が y = 0)の、雲の層。
  // ゲームの助走は、層の中(下寄り)から始まり、雲海の上面を抜けてスタート位置へ昇る
  spec.slabBottomAfter = kGameSlabBottom;
  spec.slabTopAfter = kGameSlabTop;
  // 雲を抜けるときの水の膜は、霧の上にのる水滴(2 つ目のパス)で描く
  spec.style.mist.lensDrops = true;
  return spec;
}

// 霧と水滴で覆う(ゲームの雲海へ落ちたあと)。霧の距離は、雲海の面との交点から求める
TransitionSpec MakeMistLens(const Vector3 &color, float out, float hold, float in) {
  TransitionSpec spec = MakeSpec(TransitionKind::MistLens, color, out, hold, in);
  spec.style.mist.lensDrops = true;
  spec.style.mist.backgroundPlane = true;
  spec.style.mist.backgroundPlaneY = kGameSeaPlaneY;
  return spec;
}

} // namespace

std::optional<TransitionSpec> Resolve(const std::string &fromSceneId, const std::string &toSceneId) {
  // タイトル -> ゲーム: スタート演出で加速している間に、外周から雲が流れ込んで覆う(雲突入)。
  // 覆い切ったところでゲームへ入れ替わり、雲の中を飛び続けながら晴れて、ゲームの空へ出る
  if (fromSceneId == SceneId::Title && toSceneId == SceneId::Game) {
    return MakeCloudDive(kWhite, kCloudDiveOut, kCloudDiveHold, kCloudDiveIn);
  }
  // ゲーム -> クリア: 太陽へ向かって加速し、白へ溶ける(達成感)
  if (fromSceneId == SceneId::Game && toSceneId == SceneId::Clear) {
    return MakeZoomBlur(kWhite, kLongOut, kMediumHold, kMediumIn);
  }
  // ゲーム -> ゲームオーバー: 雲海へ落ちて、霧と水滴で覆われ、晴れながら水滴が乾く
  if (fromSceneId == SceneId::Game && toSceneId == SceneId::GameOver) {
    return MakeMistLens(kMistGrey, kMistOut, kMistHold, kMistIn);
  }
  // クリア -> タイトル: レンズに水滴が付き、空の世界へ戻る
  if (fromSceneId == SceneId::Clear && toSceneId == SceneId::Title) {
    return MakeDroplets(kWhite, kDropletsOut, kShortHold, kDropletsIn);
  }
  // ゲームオーバー -> タイトル: 雲の形のワイプ
  if (fromSceneId == SceneId::GameOver && toSceneId == SceneId::Title) {
    return MakeNoiseWipe(kWhite, kLongOut, kShortHold, kLongIn);
  }
  // クリア / ゲームオーバー -> ゲーム(リトライ): 雲の形のワイプ(白)で覆う
  if ((fromSceneId == SceneId::Clear || fromSceneId == SceneId::GameOver) && toSceneId == SceneId::Game) {
    return MakeNoiseWipe(kWhite, kLongOut, kShortHold, kLongIn);
  }
  return std::nullopt;
}

} // namespace SceneTransitionTable
