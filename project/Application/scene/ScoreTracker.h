#pragma once
#include <unordered_map>

// スコア・撃破数・編隊（Formation）の残数管理。
// 演出（エフェクト・ポストエフェクト）は持たず、数値の管理だけを担当する。
class ScoreTracker {
public:
  static constexpr int kScorePerKill = 100;           // 1体撃破ごとの基礎点（編隊メンバーも同様に加算される）
  static constexpr int kFormationBonusMultiplier = 2; // 編隊を画面内で全滅させた場合の倍率
  static constexpr int kKillStreakInterval = 5;       // キルストリークが発生する撃破数の間隔

  // 編隊メンバーの生成時に呼ぶ（編隊の初期メンバー数と残数を積む）
  void RegisterFormationMember(int formationId);

  // 敵を1体撃破した時に呼ぶ（基礎点を加算）。キルストリークに到達したら true を返す。
  bool OnEnemyKilled();

  // 編隊メンバーを1体撃破した時に呼ぶ（OnEnemyKilled とは別に呼ぶこと）。
  // 編隊が全滅したら全滅ボーナスを加算して、その編隊の初期メンバー数を返す。それ以外は 0。
  int OnFormationMemberKilled(int formationId);

  int GetScore() const { return score_; }
  int GetKillCount() const { return killCount_; }

private:
  int score_ = 0;
  int killCount_ = 0;

  // 編隊ID -> 残存メンバー数。0になった時点で全滅として扱い、mapから削除する
  std::unordered_map<int, int> formationRemaining_;
  // 編隊ID -> 初期メンバー数（ボーナス計算用。全滅時にformationRemaining_と一緒に削除する）
  std::unordered_map<int, int> formationTotal_;
};
