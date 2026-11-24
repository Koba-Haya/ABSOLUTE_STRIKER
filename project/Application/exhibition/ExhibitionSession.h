#pragma once
#include "../result/RatingSettings.h"
#include "../result/RunResult.h"
#include "ExhibitionResultMaker.h"

/// <summary>
/// 展示モードの状態を持つクラス。展示モードに入っているかと、展示用の結果の見本づくりをまとめて持つ。
/// 分けた理由: 以前は、「入っているか」を関数内 static の bool(グローバルな状態)に、「直前に出したランク」をファイル内の
/// グローバル変数に持っていた。どちらも「展示」の状態なので、1 つのクラスにまとめて、持ち主(AppServices)を決めた。
/// シーンは遷移のたびに作り直されるが、「直前と同じランクを出さない」状態は周回をまたいで残す必要があるので、
/// シーンではなく、アプリの寿命を持つ側が持つ。
/// 書き換え(SetEnabled)は、ExhibitionController だけが行う。シーンは IsEnabled で読むだけ(const の参照で受け取れる)。
/// </summary>
class ExhibitionSession {
public:
  /// <summary>展示モードに入っているか</summary>
  bool IsEnabled() const { return enabled_; }

  /// <summary>展示モードに入る / 出る。呼ぶのは ExhibitionController だけ</summary>
  void SetEnabled(bool enabled) { enabled_ = enabled; }

  /// <summary>展示用の固定の結果を作る(直前と同じランクは避ける)。引数は ExhibitionResultMaker::Make と同じ</summary>
  RunResult MakeResult(int scoreTarget, int playerMaxHp, const RatingSettings &settings) {
    return resultMaker_.Make(scoreTarget, playerMaxHp, settings);
  }

private:
  bool enabled_ = false; // 展示モードに入っているか(起動時は、通常のプレイ)
  ExhibitionResultMaker resultMaker_;
};
