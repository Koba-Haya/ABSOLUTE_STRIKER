#pragma once
#include "ui/ResultMenu.h"

// クリア画面の段階の状態クラス(IResultFlowState)から見た、シーンへの窓口。ClearScene が private 継承で実装する。
//
// 分けた理由: ゲームの進行の窓口(IGameFlowActions)と同じ。状態クラスが ClearScene を直接知らないようにし(循環参照を避ける)、
// 状態クラスが使える操作を、ここに書いた最小限に絞る。画面の部品(集計、継承の選択、案内、メニュー)は ClearScene が持ち、
// 状態クラスは「いつ、何を始めるか」と「次へ進むか」だけを決める。
class IResultFlowActions {
public:
  virtual ~IResultFlowActions() = default;

  // --- 入力と画面の様子 ---
  // このフレームに、決定(決定ボタンかクリック)が押されたか。展示中と、画面の切り替えの最中は、常に false
  virtual bool IsDecidePressed() const = 0;
  // 展示モードか(操作を受け付けず、自動で進める)
  virtual bool IsExhibition() const = 0;
  // 画面の切り替えの最中か
  virtual bool IsTransitioning() const = 0;

  // --- 結果の集計 ---
  // 集計が終わったか(最後の行とランクまで出た)
  virtual bool IsTallyFinished() const = 0;
  // 集計の途中を飛ばして、最後まで出す
  virtual void SkipTally() = 0;

  // --- 継承 ---
  // 今回取った強化があるか(継承で選ぶものがあるか)
  virtual bool HasPickCandidates() const = 0;
  // 集計のあと、「決定ボタンで継承へ」の案内を出す / 消す
  virtual void ShowContinuePrompt() = 0;
  virtual void HideContinuePrompt() = 0;
  // 継承の選択を始める(集計は、薄く消え始める)。autoPlay: 展示の自動操作で選ぶか
  virtual void BeginPick(bool autoPlay) = 0;
  // 継承の選択が終わったか
  virtual bool IsPickDone() const = 0;
  // 継承を確定して保存する(1 回だけ呼ぶ)。選択を始めていなければ、選ばずに確定する(世代だけ進む)
  virtual void CommitInheritance() = 0;

  // --- メニューと、次の画面 ---
  // メニューを開く。delaySeconds: 現れ始めるまでの待ち(秒)
  virtual void OpenMenu(float delaySeconds) = 0;
  // メニューの入力を 1 フレーム進め、選ばれた項目を返す(選ばれていなければ None)
  virtual ResultMenu::Choice UpdateMenu() = 0;
  // 選ばれた項目の画面へ切り替える(もう一度 / ステージ選択 / タイトル)
  virtual void RequestSceneFor(ResultMenu::Choice choice) = 0;
};
