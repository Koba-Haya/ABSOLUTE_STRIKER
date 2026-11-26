#pragma once
#include "IOpeningState.h"

// 開幕の演出の状態: イントロ(画面遷移の覆いが晴れる間の、見た目だけの助走)。
//
// 覆いが晴れる進み具合に合わせて、カメラと自機を、進行方向の後ろ・雲の層の中から進める(反映は GameScene)。世界は止まっている。
// 次へ進む条件: 覆いが晴れ終わった -> イントロを終えて、PullBackState(俯瞰へ引く)
// カメラを実際に動かすので、描画の間の俯瞰のずれは重ねない(AppliesViewOffset が false)。
class IntroState : public IOpeningState {
public:
  // settings: ステージ開始演出の設定(次の段階へ渡すために持つ)
  explicit IntroState(const StageStartSettings &settings) : settings_(settings) {}

  std::unique_ptr<IOpeningState> Update(IOpeningActions &actions, float deltaTime) override;

  bool AppliesViewOffset() const override { return false; }

private:
  StageStartSettings settings_;
};
