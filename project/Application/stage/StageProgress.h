#pragma once
#include <map>
#include <string>

#include "StageDefinition.h"
#include "../result/BestRecord.h"

// ステージの進行(どのステージが解放されたか、解放の演出を見せたか)を保存して、問い合わせに答えるクラス。
//
// 分けた理由: StageCatalog は、stages.json の「定義」(変わらないデータ)を持つだけにしておく。
// プレイで変わる進行は、別のクラスに分け、定義のファイルを書き換えずに、保存先のファイルへ書く。
// ロックの判定は、定義の unlocked(最初から遊べるか)と、ここの保存(クリアで解放した)の、どちらかが true なら、遊べる。
//
// 保存先: exe の横の savedata/progress.json(exe 基準。日本語を含むフォルダでも読み書きできる)。
//   書き込めない場所などで保存に失敗しても、ゲームは続ける(ログに出す。その回のあいだは、メモリ上の進行で動く)。
//   形式: {"stages": {"<ステージ ID>": {"unlocked": true, "unlockShown": false,
//                     "bestScore": 5100, "bestPoints": 84.2, "bestRank": "A"}, ...}}
//   (ベスト記録の 3 つの項目は、そのステージをクリアしたあとだけ入る。無いものは、記録なしとして読む)
//
// 解放の演出を見せたか(unlockShown): ステージ選択の「鍵が外れる」演出を、解放のたびに 1 回だけ出すための印。
//   どのステージでも同じ扱い(特定のステージだけの処理にしない)。
// ベスト記録: ステージごとに、ベストスコア、ベストの評価の点数、最高ランクを、それぞれ別に、最高の値で保存する(RecordRun)。
// 使い方: ボス撃破で次のステージを解放するとき Unlock。ステージ選択は IsUnlocked でロックを判定し、FindPendingUnlock で演出する
//   ステージを探して、演出を見せたら MarkUnlockShown を呼ぶ。

class StageProgress {
public:
  // 保存ファイルを読む(持ち主の GameData が、アプリの初期化で呼ぶ)。無い、壊れているときは、空の進行で始める。
  // 以前は Singleton で、最初の GetInstance のときに読んでいた
  void Load();

  // そのステージを遊べるか(定義の unlocked が true、または、保存で解放済み)
  bool IsUnlocked(const StageDefinition &stage) const;

  // ステージを解放して、保存する。すでに解放済みなら何もしない(演出の印を、また立てない)。
  // 解放したときは、「演出をまだ見せていない」印が付く
  void Unlock(const std::string &stageId);

  // 解放されたが、鍵が外れる演出をまだ見せていないステージの ID を返す。無ければ空文字
  std::string FindPendingUnlock() const;

  // 鍵が外れる演出を見せたことを記録して、保存する
  void MarkUnlockShown(const std::string &stageId);

  // 今回の結果を、そのステージのベスト記録と比べて、最高の値で更新し、保存する。
  // 戻り値: 更新する前のベスト記録と、何を更新したか(結果画面が「ベスト更新」を出すために使う)
  BestUpdate RecordRun(const std::string &stageId, const RunResult &result);

  // 進行をすべて消す(ロック、演出の印、ベスト記録を、最初の状態へ戻す)。仮の状態の間は、保存しない
  void Reset();

  // 仮の状態に入る(展示モード用)。いまの進行を退避して、空の進行から始める。仮の状態の間は、保存ファイルを書き込まない。
  // すでに仮の状態のときは、何もしない
  void BeginVolatile();
  // 仮の状態を終えて、退避した進行へ戻す(保存ファイルは、そのまま)。仮の状態でないときは、何もしない
  void EndVolatile();

private:
  // 保存する。失敗したときは、ログに出して、続ける
  void Save_() const;

  struct Entry {
    bool unlocked = false;
    bool unlockShown = false;
    BestRecord best; // ベスト記録(クリアしたあと)
  };
  std::map<std::string, Entry> entries_;
  bool volatile_ = false;                   // 仮の状態か(true の間は、保存しない)
  std::map<std::string, Entry> backup_;     // 仮の状態に入る前の進行(EndVolatile で戻す)
};
