#pragma once
#include <memory>
#include <string>

#include "ISceneFactory.h"

class AppServices;

class SceneFactory final : public ISceneFactory {
public:
  // appServices: シーンへ渡す、アプリの寿命を持つ部品の置き場(所有しない。このファクトリより長く生きること)
  explicit SceneFactory(AppServices &appServices) : appServices_(appServices) {}

  std::unique_ptr<BaseScene> Create(const std::string &sceneId,
                                    const SceneServices &services) override;

private:
  AppServices &appServices_;
};
