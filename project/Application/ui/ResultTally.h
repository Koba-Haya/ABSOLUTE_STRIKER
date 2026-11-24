#pragma once
#include "BitmapFont.h"
#include "UiPanel.h"
#include "UiRectRenderer.h"
#include "../result/BestRecord.h"
#include "../result/RunResult.h"

/// <summary>
/// クリア画面の「結果の集計」(評価の内訳が順に現れ、数字が数え上がり、合計とランクが決まる)を描くクラス。
/// 左のパネルに、評価の 6 項目(スコア、撃破率、被ダメージ、編隊の全滅、マルチロック、ボス戦ノーダメージ)を 1 行ずつ出す。
/// 右のパネルに、合計の点数と、ランク(S / A / B / C)を出す。
/// 分けた理由: 「結果を、時間をかけて見せる」という演出は、メニュー(ResultMenu)や見出し(ResultBanner)とは別の責務。
/// ゲームオーバー画面は、この集計を使わない(結果が無い)ので、クリア画面だけが持つ部品にする。
/// 評価の計算(RunEvaluator)は持たない。渡された RunResult を、見せるだけ。
///
/// 演出の流れ(時間は、Begin からの経過):
///   パネルが開く -> 項目が 1 つずつ現れる(割合の棒と点数が、0 から数え上がる)-> 合計が数え上がる -> ランクが、ぽんと押される -> 終わり
/// 終わったら IsFinished が true。待たずに飛ばしたいときは Skip(最後の状態へ、すぐ移る)。
/// 使い方: Initialize(シーンの開始時に 1 回)-> SetVerticalRange -> Begin(結果と待ち時間)-> Update / Draw(毎フレーム)
/// </summary>
class RatingCatalog;

class ResultTally {
public:
  /// <summary>文字の資源とパネルを用意する。ファイルが無いときは、探した場所を表示して終了する</summary>
  /// <param name="rating">評価の基準(各行の配点と、ランクの境目。所有しない。このクラスより長く生きていること)</param>
  void Initialize(const RatingCatalog &rating);

  /// <summary>集計を置く縦の範囲(画面ピクセル。top は上端、bottom は下端)を決める。見出しとメニューの間を、渡す。Begin の前に呼ぶ</summary>
  void SetVerticalRange(float top, float bottom);

  /// <summary>
  /// 集計を始める。delaySeconds だけ待ってから、パネルが開く(見出しが現れるのを待つときなどに使う)。
  /// best: 今回の結果を記録したときの、ベスト記録の更新の内容(「ベスト」と「NEW RECORD」、ランクアップの表示に使う)
  /// </summary>
  void Begin(const RunResult &result, const BestUpdate &best, float delaySeconds);

  /// <summary>結果が渡されて、集計が始まったか(Begin を呼んだか)</summary>
  bool IsActive() const { return active_; }

  /// <summary>集計が、すべて終わったか(ランクが決まったあと。Begin を呼ぶ前は false)</summary>
  bool IsFinished() const { return active_ && time_ >= finishTime_; }

  /// <summary>集計全体(2 つのパネルと中身)の透明度の倍率(1 で通常、0 で消える)。継承の選択へ切り替えるときに、集計を薄く消すのに使う</summary>
  void SetOpacityScale(float scale);

  /// <summary>集計を飛ばして、最後の状態(すべて現れ、ランクが決まった状態)へ移る</summary>
  void Skip();

  /// <summary>時間を進める(1 フレームの長さ。秒)</summary>
  void Update(float deltaTime);

  /// <summary>集計を描く。シーンの 3D の描画の最中(雲の合成のあと)に呼ぶ</summary>
  void Draw();

private:
  const RatingCatalog *rating_ = nullptr; // 評価の基準(所有しない。Initialize で受け取る)

  /// 評価の 1 行(左のパネルに出す)
  struct Row {
    const char *label;       // 項目名
    std::string detail;      // 数字の説明(「42 / 45 体」など)
    const RatingEntry *entry; // 割合と点数
    float maxPoints;         // 満点(重み)
  };

  /// 行を作る(結果から、説明の文字と満点を決める)
  void BuildRows_();
  /// 行 index が現れ始める時刻(Begin からの経過)
  float RowStartTime_(int index) const;
  /// 左のパネルの中身を描く(alpha は、パネルの現れ具合)
  void DrawRows_(float alpha);
  /// 右のパネルの中身を描く(合計とランク)
  void DrawRank_(float alpha);

  BitmapFont font_;
  UiPanel rowsPanel_; // 左: 評価の内訳
  UiPanel rankPanel_; // 右: 合計とランク
  UiRectRenderer rects_;
  RunResult result_;
  BestUpdate best_; // ベスト記録の更新の内容
  std::vector<Row> rows_;
  bool active_ = false;
  float top_ = 0.0f;
  float bottom_ = 0.0f;
  float waitSeconds_ = 0.0f; // パネルが開き始めるまでの、残りの待ち(秒)。待つ間は、時間を進めない
  float time_ = 0.0f;       // 待ちが終わってからの経過(秒)
  float totalStart_ = 0.0f; // 合計の数え上がりが始まる時刻
  float stampStart_ = 0.0f; // ランクが押される時刻
  float finishTime_ = 0.0f; // 集計が終わる時刻
  float totalMaxPoints_ = 0.0f; // 点数の満点(重みの合計)
};
