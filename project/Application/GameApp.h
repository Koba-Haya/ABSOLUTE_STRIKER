#pragma once
#include "FrameWork.h"
#include <memory>

#include "AppServices.h"

class SceneManager;

class GameApp final : public AbsoluteFrameWork {
public:
  GameApp();
  ~GameApp() override;

protected:
  void Initialize() override;
  void Finalize() override;
  void Update() override;
  void Draw() override;

private:
  // アプリの寿命を持つ部品の置き場。シーンの工場(SceneManager が持つ)より長く生きるよう、宣言を先にする
  AppServices appServices_;
  std::unique_ptr<SceneManager> sceneManager_;

  // DockBuilder による初期レイアウト構築済みフラグ
  // true になると以降はフレームごとに再構築しない
  bool dockBuilt_ = false;

  // デバッグメニュー(ImGui)の状態。以前は関数内 static(グローバルな状態)に持っていたので、アプリのメンバにした
  bool useTransition_ = true;      // オフにすると、遷移の表を使わず即座に切り替える(開発の速度を落とさないため)
  bool mistDebugDistance_ = false; // 霧の遷移の確認: 距離を色で表示する
  bool mistDropletsOnly_ = false;  // 霧の遷移の確認: 水滴だけを出す
};
