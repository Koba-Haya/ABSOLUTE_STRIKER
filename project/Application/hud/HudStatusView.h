#pragma once
#include "../ui/UiPanel.h"
#include "BitmapFont.h"
#include "HudLockOnMarkers.h"
#include "HudLockOnSlots.h"
#include "HudProgressBar.h"
#include <vector>

/// <summary>いまのステージの目的(HUD に短い日本語で出す)。ゲームの流れ(GamePhase)と 1 対 1 に対応する</summary>
enum class HudObjective {
  Advance,    // レールを進む間: 敵を倒しながら進む
  DefeatBoss, // ボス戦: ボスを倒す
};

/// <summary>上の中央のバーが表すもの。ゲームの流れ(GamePhase)に合わせて切り替える</summary>
enum class HudGauge {
  Progress, // ステージの進行度(レールを進む間)
  BossHp,   // ボスの体力(ボス戦)
};

/// <summary>HUD の文字に出す値。GameScene が毎フレーム作って渡す(この部品は、ゲームの内部状態を直接読まない)</summary>
struct HudStatus {
  int score = 0;                                 // スコア
  int kills = 0;                                 // 撃破数
  HudObjective objective = HudObjective::Advance; // いまの目的
  // --- ロックオン(照準まわりと、右下の枠) ---
  bool lockingMode = false;                      // ロックオンモード(長押し中)か。輪とカッコは、この間だけ出す
  Vector2 cursorPos{0.0f, 0.0f};                 // カーソルの画面座標(輪の中心)
  float lockRadius = 0.0f;                       // 範囲の輪の半径(画面ピクセル。ロックオンの判定の半径と同じ値を渡す)
  std::vector<Vector2> lockedPositions;          // 捕まえている敵の画面座標(カッコの中心)
  int lockedCount = 0;                           // いま捕まえている数
  int lockMax = 0;                               // ロックオンできる上限(右下の枠の数)
  HudGauge gauge = HudGauge::Progress;           // バーが表すもの
  float hpRatio = 1.0f;                          // 自機の HP の割合(現在 / 最大。0 から 1)
  float gaugeRatio = 0.0f;                       // バーの割合(0 から 1。進行度、またはボスの残り体力の割合)
};

/// <summary>
/// ゲーム中の HUD の文字まわり(目的、スコアと撃破数、進行バー)をまとめて描くクラス。
/// 画面の配置: 左上に目的、右上にスコア、上の中央にバー(進行度、ボス戦ではボスの体力)、左下に自機の HP のバー、右下にロックオン数の枠。ロックオンモードの間は、カーソルの周りに輪、捕まえた敵にカッコ。画面の占有を抑えるため小さく作り、
/// レティクル(中央)には重ならない。
/// 分けた理由: レティクルや HP を持つ GameHUD(スプライトで描く)に文字の描画を足すと責務が混ざるため、
/// 「文字と数値の表示」を別クラスにした。GameHUD と同じく、シーンは Update で値を渡し、Draw を呼ぶだけにする。
/// 最初に Draw されたとき(HUD が初めて出るとき)に出現の動きを始める(開始演出の間に動きが終わってしまわないため)。
/// </summary>
class HudStatusView {
public:
  HudStatusView() = default;
  ~HudStatusView() = default;

  /// <summary>文字の資源とパネルを用意する。ファイルが無いときは、探した場所を表示して終了する</summary>
  void Initialize();

  /// <summary>表示する値を更新する(毎フレーム呼んでよい)</summary>
  void SetStatus(const HudStatus &status) { status_ = status; }

  /// <summary>描く。GameHUD の描画のあと(雲の合成のあと)に呼ぶ</summary>
  void Draw();

private:
  /// 目的の文字列を返す(HudObjective の全種類を扱うこと)
  static const char *GetObjectiveText_(HudObjective objective);
  void DrawObjective_();
  void DrawScore_();
  /// 上の中央のバーを描く(進行か、ボスの体力か)
  void DrawGauge_();
  /// 左下の HP のバーを描く
  void DrawHp_();
  /// ロックオンの表示(右下の枠、輪、カッコ)を描く
  void DrawLockOn_();

  BitmapFont font_;           // 文字(日本語可)
  UiPanel objectivePanel_;    // 左上: 目的
  UiPanel scorePanel_;        // 右上: スコアと撃破数
  HudProgressBar gaugeBar_;    // 上の中央: 進行バー / ボスの体力バー
  HudProgressBar hpBar_;       // 左下: 自機の HP のバー
  HudLockOnSlots lockSlots_;   // 右下: ロックオン数の枠
  HudLockOnMarkers lockMarkers_; // カーソルまわりの輪と、捕まえた敵のカッコ(ロックオンモードの間だけ)
  HudStatus status_;
  bool opened_ = false;       // 出現の動きを始めたか
};
