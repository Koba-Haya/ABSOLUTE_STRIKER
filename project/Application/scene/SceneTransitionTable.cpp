#include "SceneTransitionTable.h"

#include "SceneIds.h"

namespace SceneTransitionTable {
namespace {

// 覆う色(リニア値)
const Vector3 kWhite{1.0f, 1.0f, 1.0f};
const Vector3 kBlack{0.0f, 0.0f, 0.0f};
const Vector3 kDeepWater{0.01f, 0.04f, 0.10f}; // 着水して沈む、深い青

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
// ゲームオーバー: 蝶が雲海へ落ちて沈む。覆いは霧ではなく実際の雲で、落下カメラ(DeathCameraDirector)が雲の層へ沈んで画面が雲で満たされる間に、
// GameOver へ入れ替わる。水滴だけを、周辺に重ねる。入れ替えは、覆う時間の終わり。
//   覆う時間 = 落下カメラが最後のキーに着く(着地の終わりから kCoverHoldSeconds = 2.0 秒後)少し手前。そこまでにカメラは雲の中へ沈み切る
//   晴れる時間 = GameOver の雲が薄くなって見通しが開き、カメラが水平へ戻り、水滴が乾く時間(GameOverBackdrop が進み具合に合わせて動く)
constexpr float kSinkCoverOut = 1.8f;
constexpr float kSinkCoverHold = 0.4f;
constexpr float kSinkCoverIn = 1.8f;
constexpr float kShortOut = 0.5f;
constexpr float kMediumOut = 0.7f;
constexpr float kLongOut = 0.9f;
constexpr float kShortHold = 0.1f;
constexpr float kMediumHold = 0.15f;
constexpr float kShortIn = 0.5f;
// 復活(ゲームオーバー -> ゲーム): 入れ替え前の雲の層(ゲームオーバーの世界の高さ)。飛び立つ蝶を追うカメラ(高さ約 -30 から -23)が、その中へ入る。
// 入れ替え後は、タイトルからゲームと同じ(ゲームのスタート演出の雲の層)
// 蝶は雲の層(ゲームの雲海。上面 -24)を抜けて、日の当たる雲海の上へ出る。そのあと、さらに上の雲の層へ入る所で覆う(二重の層)
constexpr float kRebirthSlabBottom = -14.0f;
constexpr float kRebirthSlabTop = 14.0f;
// 復活の切り替えの方式: true で雲抜け(CloudDive)、false で短い白のフェード(うまくいかなければ切り替えて試す)
constexpr bool kRebirthUsesCloudDive = true;
constexpr float kRebirthFlashOut = 0.4f;
constexpr float kMediumIn = 0.7f;
constexpr float kLongIn = 0.9f;

// 水滴を縁へ寄せる設定(遷移ごとに持つ。値の意味は LensEdgeStyle を参照)。
// 既定は、画面の外周 2〜3 割に水滴が集まり、中央の 5〜6 割が開く(距離 0.55 から付き始め、0.95 で最も出やすい)。
// 雲を抜ける(CloudDive)と、ゲームオーバーで値を変えられるよう、別の定数にしてある
constexpr float kCloudDiveEdgeInner = 0.55f;
constexpr float kCloudDiveEdgeOuter = 0.95f;
constexpr float kCloudDiveEdgeJitter = 0.12f;
constexpr float kCloudDiveEdgeBlur = 0.006f;
// ゲームオーバーの水滴の濡れ具合の曲線(覆う区間 / 晴れる区間の進み具合 0〜1)。
// 付く: 覆い始めからすぐ付き始め、覆う区間の半分で出そろう(落下して沈む間じゅう、水滴が付いている)。
// 乾く: 晴れ始めからすぐ乾き始め、晴れる区間の約 1/3(約 0.6 秒)で消える(島が浮かび上がる頃には、ほぼ乾いている)
constexpr float kSinkWetRiseStart = 0.0f;
constexpr float kSinkWetRiseEnd = 0.5f;
constexpr float kSinkWetDrainStart = 0.0f;
constexpr float kSinkWetDrainEnd = 0.35f;
constexpr float kSinkEdgeInner = 0.55f;
constexpr float kSinkEdgeOuter = 0.95f;
constexpr float kSinkEdgeJitter = 0.12f;
constexpr float kSinkEdgeBlur = 0.006f;

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

TransitionSpec MakeCloudDive(const Vector3 &color, float out, float hold, float in, float slabBottomBefore, float slabTopBefore) {
  TransitionSpec spec = MakeSpec(TransitionKind::CloudDive, color, out, hold, in);
  // 入れ替え前の、雲の層(呼び出し側が、そのシーンの世界の高さで渡す。タイトルは水面が y = 0 で、スタート演出の上昇が層の中心(約 75)へ届く)
  spec.style.slabBottom = slabBottomBefore;
  spec.style.slabTop = slabTopBefore;
  // 入れ替え後(ゲームの世界: 雲海の平均の高さが y = -35.5、カメラの開始位置が y = 0)の、雲の層。
  // ゲームの助走は、層の中(下寄り)から始まり、雲海の上面を抜けてスタート位置へ昇る
  spec.slabBottomAfter = kGameSlabBottom;
  spec.slabTopAfter = kGameSlabTop;
  // 雲を抜けるときの水の膜は、霧の上にのる水滴(2 つ目のパス)で描く
  spec.style.mist.lensDrops = true;
  spec.style.mist.edge.innerEdge = kCloudDiveEdgeInner;
  spec.style.mist.edge.outerEdge = kCloudDiveEdgeOuter;
  spec.style.mist.edge.jitter = kCloudDiveEdgeJitter;
  spec.style.mist.edge.blurRadius = kCloudDiveEdgeBlur;
  return spec;
}

// 水滴だけを重ねる(ゲームの雲海へ落ちて沈むあと)。覆いは雲そのものが担うので、霧は使わない(fogScale = 0)。
// 画面の覆い具合は、水滴の付く曲線とシーンの入れ替えの時刻にだけ使われる
TransitionSpec MakeLensDropsOnly(float out, float hold, float in) {
  TransitionSpec spec = MakeSpec(TransitionKind::MistLens, kBlack, out, hold, in);
  spec.style.mist.fogScale = 0.0f;
  spec.style.mist.lensDrops = true;
  spec.style.mist.wetRiseStart = kSinkWetRiseStart;
  spec.style.mist.wetRiseEnd = kSinkWetRiseEnd;
  spec.style.mist.wetDrainStart = kSinkWetDrainStart;
  spec.style.mist.wetDrainEnd = kSinkWetDrainEnd;
  spec.style.mist.edge.innerEdge = kSinkEdgeInner;
  spec.style.mist.edge.outerEdge = kSinkEdgeOuter;
  spec.style.mist.edge.jitter = kSinkEdgeJitter;
  spec.style.mist.edge.blurRadius = kSinkEdgeBlur;
  return spec;
}

} // namespace

std::optional<TransitionSpec> Resolve(const std::string &fromSceneId, const std::string &toSceneId) {
  // タイトル -> ゲーム: スタート演出で加速している間に、外周から雲が流れ込んで覆う(雲突入)。
  // 覆い切ったところでゲームへ入れ替わり、雲の中を飛び続けながら晴れて、ゲームの空へ出る
  if (fromSceneId == SceneId::Title && toSceneId == SceneId::Game) {
    return MakeCloudDive(kWhite, kCloudDiveOut, kCloudDiveHold, kCloudDiveIn, kTitleSlabBottom, kTitleSlabTop);
  }
  // タイトル -> ステージ選択: タイトルの発進演出のまま、外周から雲が流れ込んで覆う(雲突入。タイトルの演出を活かす)
  if (fromSceneId == SceneId::Title && toSceneId == SceneId::StageSelect) {
    return MakeCloudDive(kWhite, kCloudDiveOut, kCloudDiveHold, kCloudDiveIn, kTitleSlabBottom, kTitleSlabTop);
  }
  // ステージ選択 -> ゲーム: 雲の形のワイプ(白)で覆う。覆いの中でゲームが始まるので、スタート演出の助走も、覆いが晴れる間に動く
  if (fromSceneId == SceneId::StageSelect && toSceneId == SceneId::Game) {
    return MakeNoiseWipe(kWhite, kLongOut, kShortHold, kLongIn);
  }
  // ゲーム -> クリア: 太陽へ向かって加速し、白へ溶ける(達成感)
  if (fromSceneId == SceneId::Game && toSceneId == SceneId::Clear) {
    return MakeZoomBlur(kWhite, kLongOut, kMediumHold, kMediumIn);
  }
  // ゲーム -> ゲームオーバー: 雲海へ落ちて雲の中へ沈み、画面が雲で満たされる間に入れ替わる(霧は使わない)。周辺に水滴が付き、晴れながら乾く
  if (fromSceneId == SceneId::Game && toSceneId == SceneId::GameOver) {
    return MakeLensDropsOnly(kSinkCoverOut, kSinkCoverHold, kSinkCoverIn);
  }
  // クリア -> タイトル: レンズに水滴が付き、空の世界へ戻る
  if (fromSceneId == SceneId::Clear && toSceneId == SceneId::Title) {
    return MakeDroplets(kWhite, kDropletsOut, kShortHold, kDropletsIn);
  }
  // ゲームオーバー -> タイトル: 雲の形のワイプ
  if (fromSceneId == SceneId::GameOver && toSceneId == SceneId::Title) {
    return MakeNoiseWipe(kWhite, kLongOut, kShortHold, kLongIn);
  }
  // ゲームオーバー -> ゲーム(復活): 飛び立つ蝶を追うカメラが雲の中へ入り、雲抜け(タイトルからゲームと同じ)で覆って、ゲームのスタート演出へ
  if (fromSceneId == SceneId::GameOver && toSceneId == SceneId::Game) {
    if (kRebirthUsesCloudDive) {
      return MakeCloudDive(kWhite, kCloudDiveOut, kCloudDiveHold, kCloudDiveIn, kRebirthSlabBottom, kRebirthSlabTop);
    }
    return MakeFade(kWhite, kRebirthFlashOut, kShortHold, kLongIn);
  }
  // クリア -> ステージ選択: クリア -> タイトルと同じ、レンズの水滴
  if (fromSceneId == SceneId::Clear && toSceneId == SceneId::StageSelect) {
    return MakeDroplets(kWhite, kDropletsOut, kShortHold, kDropletsIn);
  }
  // ゲームオーバー -> ステージ選択: ゲームオーバー -> タイトルと同じ、雲の形のワイプ
  if (fromSceneId == SceneId::GameOver && toSceneId == SceneId::StageSelect) {
    return MakeNoiseWipe(kWhite, kLongOut, kShortHold, kLongIn);
  }
  // クリア -> ゲーム(リトライ): 雲の形のワイプ(白)で覆う
  if (fromSceneId == SceneId::Clear && toSceneId == SceneId::Game) {
    return MakeNoiseWipe(kWhite, kLongOut, kShortHold, kLongIn);
  }
  return std::nullopt;
}

} // namespace SceneTransitionTable
