#include "SceneFactory.h"

#include "GameScene.h"
#include "DevScene.h"
#include "SceneIds.h"
#include "TitleScene.h"
#include "StageSelectScene.h"
#include "ClearScene.h"
#include "GameOverScene.h"

std::unique_ptr<BaseScene> SceneFactory::Create(const std::string &sceneId,
                                                const SceneServices &services) {
  (void)services;

  if (sceneId == SceneId::Title) {
    return std::make_unique<TitleScene>();
  }
  if (sceneId == SceneId::StageSelect) {
    return std::make_unique<StageSelectScene>();
  }
  if (sceneId == SceneId::Game) {
    return std::make_unique<GameScene>();
  }
  if (sceneId == SceneId::Dev) {
    return std::make_unique<DevScene>();
  }
  if (sceneId == SceneId::Clear) {
    return std::make_unique<ClearScene>();
  }
  if (sceneId == SceneId::GameOver) {
    return std::make_unique<GameOverScene>();
  }
  return nullptr;
}
