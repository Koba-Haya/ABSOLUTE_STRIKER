#pragma once
#include "ComponentDeps.h"
#include "GameObject.h"
#include <string>
#include <vector>

namespace AbsoluteEngine {

// JSON とオブジェクトの相互変換。読み込み側の関数は、部品を作るために、窓口の束(ComponentDeps)を受け取る。
// 束は、読み込んだ部品へ渡される(窓口を要する部品のコンストラクタ注入)。束の窓口は、読み込みの間だけ有効であればよい
class SceneSerializer {
public:
  // 現在のシーンツリーを JSON 形式で保存
  static bool Serialize(const std::string& filepath, const std::vector<std::shared_ptr<GameObject>>& rootObjects, bool forceFullSerialize = false);
  static std::string SerializeToString(const std::vector<std::shared_ptr<GameObject>>& rootObjects, bool forceFullSerialize = false);

  // JSON からシーンツリーを読み込み
  static bool Deserialize(const std::string& filepath, std::vector<std::shared_ptr<GameObject>>& outRootObjects, const ComponentDeps& deps);
  static bool DeserializeFromString(const std::string& jsonString, std::vector<std::shared_ptr<GameObject>>& outRootObjects, const ComponentDeps& deps);

  // プレハブの保存と読み込み
  static bool SavePrefab(const std::string& filepath, std::shared_ptr<GameObject> obj);
  static std::shared_ptr<GameObject> LoadPrefab(const std::string& filepath, const ComponentDeps& deps);

  // オブジェクトのディープコピーを作成（シリアライズの仕組みを利用）
  static std::shared_ptr<GameObject> CopyGameObject(std::shared_ptr<GameObject> src, const ComponentDeps& deps);
};

} // namespace AbsoluteEngine
