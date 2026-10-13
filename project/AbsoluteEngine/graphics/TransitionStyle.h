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
};
