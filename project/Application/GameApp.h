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
};
