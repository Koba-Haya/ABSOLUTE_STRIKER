#pragma once
#include "UiPanel.h"
#include "Vector.h"

class BitmapFont;
struct StageDefinition;

/// <summary>
/// ステージ選択の「カード」。選んだステージの番号(STAGE 01)、名前、世界観の説明文を、UiPanel の中に描く。
/// ロック中のステージは、名前と説明文を伏せて「???」とロックの表示にする(中身を先に見せないため)。
/// 分けた理由: 「ステージ 1 本の紹介を描く」は、ステージ選択の他の処理(入力、一覧、背景)と別の責務で、
/// 将来ほかの画面(出撃前の確認など)でも使えるようにするため。シーンは SetStage で表示するステージを渡し、Draw を呼ぶだけ。
/// </summary>
class StageCard {
public:
  StageCard();
  ~StageCard();

  /// <summary>font は文字を描くために使う(所有しない。カードより長く生きていること)</summary>
  void Initialize(BitmapFont *font);

  /// <summary>カードの位置と大きさ(画面ピクセル座標。position は左上)</summary>
  void SetBounds(const Vector2 &position, const Vector2 &size);

  /// <summary>出現の動きを始める(待ち秒の意味は UiPanel::Open と同じ)</summary>
  void Open(float delaySeconds = 0.0f);

  /// <summary>表示するステージを決める。stage は、呼び出し側が持ち続けること(StageCatalog のもの)。nullptr なら中身を描かない</summary>
  /// <param name="stage">表示するステージ</param>
  /// <param name="number">ステージの番号(1 から。一覧の並びで決まる)</param>
  void SetStage(const StageDefinition *stage, int number);

  /// <summary>カード(外枠と中身)を描く</summary>
  void Draw();

  /// <summary>中身の薄さ(UiPanel::GetContentAlpha と同じ。カードの外の文字を一緒に出すときに使う)</summary>
  float GetContentAlpha() const { return panel_.GetContentAlpha(); }

private:
  /// 中身(番号、名前、説明文、ロック表示)を描く
  void DrawContent_(float alpha);

  BitmapFont *font_ = nullptr;
  UiPanel panel_;
  UiRectRenderer rects_; // 名前の下の区切り線用
  const StageDefinition *stage_ = nullptr;
  int number_ = 0;
};
