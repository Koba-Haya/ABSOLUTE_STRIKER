#include "GameData.h"

void GameData::Load() {
  stageCatalog_.Load();
  itemCatalog_.Load();
  ratingCatalog_.Load();
  bulletCatalog_.Load();
  stageProgress_.Load();
  inheritanceState_.Load();
}
