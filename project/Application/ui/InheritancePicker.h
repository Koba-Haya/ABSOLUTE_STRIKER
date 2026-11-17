#pragma once
#include <chrono>
#include <string>
#include <vector>

#include "BitmapFont.h"
#include "UiPanel.h"
#include "UiPipBar.h"
#include "UiPointer.h"
#include "UiRectRenderer.h"
#include "../inheritance/InheritanceState.h"
#include "../result/RunResult.h"

class Input;

/// <summary>
/// クリア画面の「継承の選択」。今回取った強化のうち、どれを何個、次の世代へ引き継ぐかを選ぶ。
/// 左のパネルに、強化の種類ごとの行を出す。1 行は、強化の色の印、名前、強化の段階(枠の並び。継承済みの分と、
/// これから増える分を色で分ける)、今回取った数(枠の並び。選んだ個数だけ色が付く)。最後に「この内容で継承する」の行。
/// 右のパネルに、ランク、継承の枠(枠の並び。使った分が埋まる)、強化の倍率(レアリティ)、世代の進みを出す。
/// 数字は、枠の並びに置き換えて出さない(分母と分子の不揃い、数字と文字が並ぶ見づらさをなくすため)。
/// 取った強化 1 個が、1 枠を使う。同じ種類を何個選んでもよく、選んだ 1 個ごとに、ランクの倍率が掛かる。
///
/// 分けた理由: 「継承するものを選ばせる」は、結果の集計(ResultTally)やメニュー(ResultMenu)とは別の責務。
/// 枠の並びの描画は UiPipBar に任せ、継承の確定(InheritanceState への書き込み)は持たない
/// (選んだものを GetPicks でシーンへ返すだけ。保存の方針を、選ぶ画面から切り離すため)。
///
/// 操作: 上下で行を動かす。取った強化の行で、右(または決定)で 1 個増やし、左で 1 個減らす。
///       枠がいっぱいのとき、増やそうとすると、断られる。最後の行を決定すると、確定(何も選ばなくても、確定できる)。
/// マウス: 動かすと、下の行が選ばれる。行を左クリックで 1 個増やす(取った数まで選んでいたら 0 へ戻る)、右クリックで 1 個減らす、
///         左右の矢印を左クリックで、その方向へ増減する。最後の行を左クリックで、確定する。
/// 使い方: HasCandidates で候補があるか調べる -> Initialize -> SetVerticalRange -> Begin -> Update / Draw(毎フレーム)
///         -> IsDone になったら、GetPicks と GetRarity を読んで、シーンが継承を確定する
/// </summary>
class InheritancePicker {
public:
  /// <summary>候補になる強化(今回取った強化)が、1 つでもあるか</summary>
  static bool HasCandidates();

  /// <summary>文字の資源とパネルを用意する。ファイルが無いときは、探した場所を表示して終了する</summary>
  void Initialize();

  /// <summary>選択を置く縦の範囲(画面ピクセル)を決める。Begin の前に呼ぶ。集計(ResultTally)と同じ範囲にそろえる</summary>
  void SetVerticalRange(float top, float bottom);

  /// <summary>
  /// 選択を始める。rank は、今回の結果のランク(枠の数と倍率が決まる)。delaySeconds だけ待ってから、パネルが開く。
  /// 候補が無いときは、何もしない(IsActive が false のまま)
  /// </summary>
  void Begin(RunRank rank, float delaySeconds);

  /// <summary>選択を始めたか(Begin を呼んで、候補があった)</summary>
  bool IsActive() const { return active_; }

  /// <summary>確定したか(最後の行が決定された)</summary>
  bool IsDone() const { return done_; }

  /// <summary>選んだ強化(種類と個数。確定のあと、または、確定の前の途中経過)。個数が 0 の種類は含めない</summary>
  std::vector<InheritanceState::Pick> GetPicks() const;

  /// <summary>継承の倍率(ランクで決まる)</summary>
  float GetRarity() const { return rarity_; }

  /// <summary>1 フレームの入力を読んで、選択を動かす / 決定する。acceptInput が false の間(遷移中など)は読まない</summary>
  void Update(const Input *input, bool acceptInput);

  /// <summary>選択を描く。シーンの 3D の描画の最中(雲の合成のあと)に呼ぶ</summary>
  void Draw();

private:
  /// 候補の 1 つ(今回取った強化の種類)
  struct Candidate {
    ItemEffectKind kind = ItemEffectKind::LockOnMax;
    int earned = 0;       // 今回取った数(選べる個数の上限)
    int picked = 0;       // 継承に選んだ個数(0 から earned)
    int maxStacks = 0;    // 強化の上限(枠の並びの長さ)
    float inherited = 0;  // いまの継承した積み数(選ぶ前。小数)
    std::string name;     // 画面に出す名前(数字なし。「最大HP」など)
    Vector4 color{1.0f, 1.0f, 1.0f, 1.0f}; // 強化の色の印
  };

  /// 左のパネルの行の配置(描画と、マウスの当たり判定で、同じ値を使うため、1 か所で計算する)
  struct ListGeometry {
    Vector2 origin;      // 本体領域の左上
    Vector2 body;        // 本体領域の大きさ
    float rowHeight = 0; // 1 行の高さ
    float fontSize = 0;  // 行の文字の大きさ
    float rowsTop = 0;   // 最初の候補の行の上端(見出しの行の下)
  };
  ListGeometry GetListGeometry_() const;
  /// 候補 index の行の、左右の矢印(個数を減らす / 増やす)の当たり判定の長方形
  UiRect GetArrowRect_(const ListGeometry &geometry, size_t index, bool right) const;
  /// マウスの状態を読んで、行の選択、個数の増減、確定をする(キーボードとパッドの入力が無いフレームに呼ぶ)
  void UpdatePointer_(const UiPointer::State &pointer);
  /// 選んでいる個数の合計(使っている枠の数)
  int UsedSlots_() const;
  /// 選んだ個数を、delta だけ変える。範囲外(0 未満、取った数より多い)や、枠がいっぱいのときは、変えない(いっぱいのときは知らせを出す)
  void ChangePicked_(Candidate &candidate, int delta);
  /// 決定が押されたとき、いまの行に対する処理(候補の行は個数を 1 つ増やす。最後まで増やしていたら 0 へ戻す。確定の行は確定する)
  void Decide_();
  /// 選択中の行が点滅する明るさの割合(0 から 1)。時刻から計算する
  float GetBlink_() const;
  /// 左のパネルの中身を描く
  void DrawList_(float alpha);
  /// 右のパネルの中身を描く
  void DrawInfo_(float alpha);

  BitmapFont font_;
  UiPanel listPanel_; // 左: 強化の行
  UiPanel infoPanel_; // 右: ランク、枠、倍率、世代
  UiRectRenderer rects_;
  UiPipBar pips_;
  std::vector<Candidate> candidates_;
  int cursor_ = 0; // 選択中の行(0 から candidates_.size() - 1 が候補、candidates_.size() が確定の行)
  RunRank rank_ = RunRank::C;
  int slots_ = 0;
  float rarity_ = 1.0f;
  bool active_ = false;
  bool done_ = false;
  float top_ = 0.0f;
  float bottom_ = 0.0f;
  std::chrono::steady_clock::time_point moveTime_;    // 選択の行を最後に動かした時刻(点滅の基準)
  std::chrono::steady_clock::time_point messageTime_; // 「枠がいっぱいです」を出した時刻
  bool hasMessage_ = false;
};
