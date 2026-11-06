#pragma once
#include "Method.h"
#include <cstdint>

// 画面遷移の「見た目」の種類。Renderer の最終パス(Transition.PS.hlsl)が、この値で描き分ける。
// シェーダー側の kKind* 定数と同じ並びにすること。
enum class TransitionKind : int32_t {
  Fade = 0,      // 単色へ溶かす(白 / 黒など)
  NoiseWipe = 1, // ノイズの形に沿って、単色で画面を覆う(雲のワイプ)
  ZoomBlur = 2,  // 画面中心へ向かう放射状のぼかしを強め、最後に単色へ溶かす(突入感)
  Ripple = 3,    // 中心から外へ広がる水の波紋で画面を歪ませ、波の内側を単色で覆う(着水)
  Droplets = 4,  // レンズに水滴が付いて屈折し、くもって単色へ溶ける
  CloudDive = 5, // 雲の層(ボリュームの雲)を下から上へ昇って抜ける。層の中で水滴が付き、抜けたあとも残る
  MistLens = 6,  // 霧とレンズの水滴(LensMist.hlsli)。場面を決め打ちせず、MistLensStyle のパラメータで見た目を決める
};

// 水滴を画面の縁へ寄せる設定(レンズや風防の水滴は縁に集まり、中央は開く)。遷移ごとに持てる。
// 中心からの距離(画面の端を 1 とする楕円の距離)に、縁のノイズでむらにした帯のマスクを掛ける:
//   距離が innerEdge 以下は 0(中央)、outerEdge 以上は 1(縁)。間はなめらか。
// マスクは、水滴の出やすさ(セルごとに 1 つ)と、縁だけのぼかしの両方に使う。
struct LensEdgeStyle {
  bool enabled = true;           // false なら、従来どおり画面全体に水滴を付ける(ぼかしも無し)
  float innerEdge = 0.55f;       // 縁の内側の位置(これより内側は、水滴が出ない)
  float outerEdge = 0.95f;       // 縁の外側の位置(これより外側は、水滴が最も出やすい)
  float jitter = 0.12f;          // 縁の輪郭の揺らぎの幅(距離の単位)。0 で整った楕円
  float jitterScale = 3.0f;      // 揺らぎの細かさ(画面の高さあたりの波の数)
  float centerKeep = 0.0f;       // 中央にも残す水滴の割合(0 = 完全に開ける、1 = 縁と同じ)
  float largeBias = 2.0f;        // 大粒のマスクのべき(1 より大きいほど、大粒は縁寄りにだけ出る)
  float blurRadius = 0.006f;     // 縁のぼかしの最大半径(画面の高さを 1 とする)。0 でぼかし無し
  // 水滴の数と見た目。暗さでなく、屈折・明るい縁・青みの影で水滴らしさを出す(背景が明るい雲でも灰色の点にならない)
  float smallDensity = 0.5f;     // 小粒の数の倍率(1 = 縁寄せだけのときの数)
  float largeDensity = 0.8f;     // 流れる大粒の数の倍率
  float refractScale = 1.8f;     // 屈折の強さの倍率(水滴の中の像のずれ)
  float highlightScale = 1.0f;   // 明るい縁と小さなハイライトの強さの倍率
  float bodyDarken = 0.96f;      // 水滴の中の明るさ(1 = 暗くしない)
  float rimDarken = 0.35f;       // 影の側の細い縁の暗さ
  float shadowTint = 0.5f;       // 影の側の、青みの強さ(0〜1)
};

// 霧とレンズの水滴(TransitionKind::MistLens)の見た目のパラメータ。
// 霧の量は、物体までの距離と視程から画素ごとに決める(1 - exp(-距離 / 視程))。視程は、覆い具合に応じて遠い値から近い値へ縮む。
// 霧の色は TransitionStyle::color を使う。
struct MistLensStyle {
  float desaturate = 0.8f;          // 霧が濃い所の、彩度の落とし量(0〜1)
  float blurRadius = 0.010f;        // 霧が濃い所のぼかしの最大半径(画面の高さを 1 とする)
  float visibilityNear = 30.0f;     // 覆い具合 1 のときの視程(世界の距離。この距離の物体は約 63% 霞む)
  float visibilityFar = 100000.0f;  // 覆い具合 0 のときの視程(ほぼ霧なし)
  float backgroundDistance = 400.0f; // 深度の無い背景(空、雲海)を、この距離にあるものとして扱う
  float wrapStart = 0.8f;           // この覆い具合を超えたら、最後の覆いを足して入れ替えを隠す
  float sunGlow = 0.8f;             // 太陽の側で、霧を明るく暖かくする強さ(0〜1)
  bool debugDistance = false;       // 距離を白黒で表示する(深度の確認用)
  float fogScale = 1.0f;            // 霧の量の倍率(0 で霧なし。水滴だけを確認するとき)
  // 水滴の濡れ具合(0〜1)の時間の曲線。覆う区間の進み具合 / 晴れる区間の進み具合で決める。
  // 覆う間は、wetRiseStart から wetRiseEnd にかけて付く。保つ間は 1。晴れる間は、wetDrainStart から wetDrainEnd にかけて乾く
  float wetRiseStart = 0.55f;
  float wetRiseEnd = 0.9f;
  float wetDrainStart = 0.05f;
  float wetDrainEnd = 1.0f;
  float wetness = 0.0f;             // 今の濡れ具合(SceneTransition が、上の曲線から毎フレーム入れる。設定ではない)
  // 霧の深度の無い背景(空、雲海)に、水平な層(雲海の面、水面)があるとき: 視線との交点までを距離にする
  bool backgroundPlane = false;
  float backgroundPlaneY = 0.0f;    // その層の高さ(入れ替え前のシーンの世界の y)
  // 霧の上に、レンズの水滴(2 つ目のパス)を描くか。霧の遷移と、雲を抜ける遷移で使う
  bool lensDrops = false;
  float lensTime = 0.0f;            // 水滴が付き始めてからの秒(同上。水滴の流れる位置に使う)
  LensEdgeStyle edge;               // 水滴を縁へ寄せる設定
};

// 画面遷移の見た目のパラメータ(覆い具合 cover 以外の、種類ごとの設定)。
// 覆い具合(0 = 透明、1 = 完全に覆う)は、時間の進行を管理する SceneTransition から Renderer へ別に渡す。
struct TransitionStyle {
  TransitionKind kind = TransitionKind::Fade;
  Vector3 color{1.0f, 1.0f, 1.0f}; // 覆う色(リニア値)
  float softness = 0.25f;          // NoiseWipe: 縁のやわらかさ(大きいほどぼやける)
  float noiseScale = 3.0f;         // NoiseWipe: 画面の高さあたりのノイズのマス数(小さいほど大きな塊)
  float blurStrength = 0.35f;      // ZoomBlur: 覆い具合が最大のときの、中心へ引き込む割合(0〜1)
  Vector2 center{0.5f, 0.5f};      // ZoomBlur / Ripple: 中心(画面の UV)
  // CloudDive: 世界に固定された雲の層の、下面・上面の高さ(描画しているシーンの世界の y)。
  // 実際のカメラと自機が、この層の中を通る。層の外では雲は見えない
  float slabBottom = 40.0f;
  float slabTop = 100.0f;
  // CloudDive: 太陽へ向かう向き(世界)。雲の明暗と、レンズフレアの位置に使う
  Vector3 sunDirection{0.30f, 0.22f, 1.0f};
  // MistLens: 霧とレンズの水滴の設定
  MistLensStyle mist;
};

// ImGui での調整用: 有効にすると、すべての遷移の LensEdgeStyle を、この値で上書きする(Release では使わない)。
// 遷移ごとの値を決めるまでの、見た目の調整に使う。気に入った値は SceneTransitionTable へ写す
struct LensEdgeDebugOverride {
  bool enabled = false;
  LensEdgeStyle style;
};
inline LensEdgeDebugOverride &GetLensEdgeDebugOverride() {
  static LensEdgeDebugOverride instance;
  return instance;
}
