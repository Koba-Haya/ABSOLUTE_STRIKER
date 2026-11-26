#pragma once
#include "GameOverAshPhase.h"
#include <memory>

// 灰の段階の状態クラスへ、GameOverAsh が毎フレーム渡す情報
struct AshStateContext {
  float gatherDeltaSeconds = 0.0f; // このフレームに、集まりの時計を進める秒(実時間に、集まりの速さの倍率を掛けたもの)
  float gatherLengthSeconds = 0.0f; // 集まりきるまでの長さ(秒。0 で割らない下限を当てた値)
  float settledSeconds = 0.0f;     // 降下が始まってからの秒(画面の覆いが晴れる前は 0)
  float gatherStartSeconds = 0.0f; // 降下が始まってから、自動で集まり始めるまでの秒
  bool autoAdvance = true;         // 降下の時間で、自動で漂いと集まりを行き来するか(外から集まりを頼まれたあとは false)

  // 自動の進行で、集まり始めてよいか(降下が始まり、開始の時刻を過ぎた)
  bool ShouldStartGathering() const { return autoAdvance && settledSeconds > 0.0f && settledSeconds >= gatherStartSeconds; }
  // 自動の進行で、漂いへ戻すか(降下がまだ始まっていない、または開始の時刻より前へ戻った。降下のやり直しなど)
  bool ShouldReturnToDrifting() const { return autoAdvance && (settledSeconds <= 0.0f || settledSeconds < gatherStartSeconds); }
};

// ゲームオーバー画面の灰の「段階」の共通の形(State パターンの、状態の基底クラス)。
//
// 段階の一覧(遷移):
//   AshDriftingState(漂い)   -> AshGatheringState(自動: 開始の時刻を過ぎた / 外から: 集まりを頼まれた)
//   AshGatheringState(集まり) -> AshGlowingState(集まりきった) / AshDriftingState(自動: 開始の時刻より前へ戻った)
//   AshGlowingState(灯り)    -> AshDriftingState(自動: 開始の時刻より前へ戻った)
// 次の段階は、段階のクラス自身が、Update と OnGatherRequested の戻り値で決める。
// 外(GameOverScene の復活の演出)は、段階を直接指定せず、「集まり始めて」と頼むだけ(受けるかは、今の段階が決める)。
// そのため、ありえない順番(集まる前にいきなり灯る、など)の遷移は起きない。デバッグ用の段階の指定だけは、GameOverAsh に専用の入口がある。
//
// 問い合わせ(IsConverging など)は、GameOverAsh が、灰の動きと核の光を決めるために聞く。既定は「漂い」の答え。
class IAshState {
public:
  virtual ~IAshState() = default;

  // 外へ見せる、今の段階の名札
  virtual GameOverAshPhase GetPhase() const = 0;

  // 毎フレーム呼ばれる。戻り値: 次の段階へ進むときは、次の段階。留まるときは nullptr
  virtual std::unique_ptr<IAshState> Update(const AshStateContext &context) = 0;

  // 外から「集まり始めて」と頼まれたとき(復活の演出)。受けて次の段階へ進むときは、次の段階。受けないときは nullptr
  virtual std::unique_ptr<IAshState> OnGatherRequested() { return nullptr; }

  // 集まりの進み(0 〜 1)。gatherLengthSeconds: 集まりきるまでの長さ(秒)
  virtual float GetGatherProgress(float gatherLengthSeconds) const {
    (void)gatherLengthSeconds;
    return 0.0f;
  }
  // 灰が巣(核)へ向かっているか(収束と渦をかけ、核を灯すか)
  virtual bool IsConverging() const { return false; }
  // 灯っているか(灰の量を減らし、渦を変え、核を脈打たせる)
  virtual bool IsGlowing() const { return false; }
};
