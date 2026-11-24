#include "GameData.h"

void GameData::Load() {
  stageCatalog_.Load();
  itemCatalog_.Load();
  ratingCatalog_.Load();
  stageProgress_.Load();
  inheritanceState_.Load();
}
