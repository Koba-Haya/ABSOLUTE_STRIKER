#include "SceneSerializer.h"
#include <fstream>
#include <iostream>
#include "CpuTimer.h"
#include "../../externals/nlohmann/json.hpp"
#include "ComponentFactory.h"
#include "ColliderComponent.h"
#include "LightNodeComponent.h"
#include "ModelComponent.h"
#include "DissolveComponent.h"


using json = nlohmann::json;

namespace AbsoluteEngine {

// Vector3 を JSON に変換するヘルパー
static json Vector3ToJson(const Vector3& v) {
  return json{ {"x", v.x}, {"y", v.y}, {"z", v.z} };
}

static Vector3 JsonToVector3(const json& j) {
  return { j.value("x", 0.0f), j.value("y", 0.0f), j.value("z", 0.0f) };
}

// GameObject 1つを JSON にシリアライズ
static json SerializeGameObject(const std::shared_ptr<GameObject>& obj, bool forceFullSerialize = false) {
  if (!obj) return json();

  json j;

  if (!forceFullSerialize && obj->IsPrefabInstance()) {
    j["prefabPath"] = obj->GetPrefabPath();
    const Transform& t = obj->GetTransform();
    j["transform"] = {
      {"translate", Vector3ToJson(t.translate)},
      {"rotate", Vector3ToJson(t.rotate)},
      {"scale", Vector3ToJson(t.scale)}
    };

    // プレハブのインスタンスでもコンポーネントの追加・変更（オーバーライド）を保存する
    json componentsJson = json::array();
    for (const auto& comp : obj->GetComponents()) {
      json cJson;
      cJson["type"] = comp->GetTypeName();
      comp->Serialize(cJson);
      componentsJson.push_back(cJson);
    }
    if (!componentsJson.empty()) {
      j["components"] = componentsJson;
    }
    
    return j;
  }

  j["name"] = obj->GetName();
  j["tag"] = obj->GetTag();
  
  const Transform& t = obj->GetTransform();
  j["transform"] = {
    {"translate", Vector3ToJson(t.translate)},
    {"rotate", Vector3ToJson(t.rotate)},
    {"scale", Vector3ToJson(t.scale)}
  };

  json componentsJson = json::array();
  for (const auto& comp : obj->GetComponents()) {
    json cJson;
    cJson["type"] = comp->GetTypeName();
    comp->Serialize(cJson);
    componentsJson.push_back(cJson);
  }
  j["components"] = componentsJson;

  json childrenJson = json::array();
  for (const auto& child : obj->GetChildren()) {
    childrenJson.push_back(SerializeGameObject(child, forceFullSerialize));
  }
  j["children"] = childrenJson;

  return j;
}

// JSON から GameObject をデシリアライズ
static std::shared_ptr<GameObject> DeserializeGameObject(const json& j) {
  if (j.contains("prefabPath")) {
    std::string prefabPath = j["prefabPath"];
    auto obj = SceneSerializer::LoadPrefab(prefabPath);
    if (obj) {
      if (j.contains("transform")) {
        const auto& tJson = j["transform"];
        Transform& t = obj->GetTransform();
        t.translate = JsonToVector3(tJson["translate"]);
        t.rotate = JsonToVector3(tJson["rotate"]);
        t.scale = JsonToVector3(tJson["scale"]);
      }
      
      // シーンファイル側のコンポーネント（オーバーライド）で上書きまたは追加
      if (j.contains("components") && j["components"].is_array()) {
        for (const auto& cJson : j["components"]) {
          std::string typeName = cJson.value("type", "");
          
          // 既にプレハブ由来で持っているコンポーネントか探す
          IComponent* existingComp = nullptr;
          for (const auto& comp : obj->GetComponents()) {
              if (comp->GetTypeName() == typeName) {
                  existingComp = comp.get();
                  break;
              }
          }
          
          if (existingComp) {
              existingComp->Deserialize(cJson); // 既存なら設定を上書き（オーバーライド）
          } else {
              auto comp = ComponentFactory::GetInstance().Create(typeName);
              if (comp) {
                  comp->Deserialize(cJson);
                  obj->AddComponent(std::move(comp)); // 無ければ追加
              }
          }
        }
      }
      
      return obj;
    } else {
      std::cerr << "Failed to load prefab: " << prefabPath << std::endl;
      auto missingObj = std::make_shared<GameObject>("Missing Prefab");
      return missingObj;
    }
  }

  std::shared_ptr<GameObject> obj = std::make_shared<GameObject>(j.value("name", "GameObject"));
  obj->SetTag(j.value("tag", "Untagged"));

  if (j.contains("transform")) {
    const auto& tJson = j["transform"];
    Transform& t = obj->GetTransform();
    t.translate = JsonToVector3(tJson["translate"]);
    t.rotate = JsonToVector3(tJson["rotate"]);
    t.scale = JsonToVector3(tJson["scale"]);
  }

  if (j.contains("components") && j["components"].is_array()) {
    for (const auto& cJson : j["components"]) {
      std::string typeName = cJson.value("type", "");
      auto comp = ComponentFactory::GetInstance().Create(typeName);
      if (comp) {
        comp->Deserialize(cJson);
        obj->AddComponent(std::move(comp));
      }
    }
  }

  if (j.contains("children") && j["children"].is_array()) {
    for (const auto& childJson : j["children"]) {
      auto childObj = DeserializeGameObject(childJson);
      if (childObj) {
        obj->AddChild(childObj);
      }
    }
  }

  return obj;
}

bool SceneSerializer::Serialize(const std::string& filepath, const std::vector<std::shared_ptr<GameObject>>& rootObjects, bool forceFullSerialize) {
  std::string jsonString = SerializeToString(rootObjects, forceFullSerialize);
  if (jsonString.empty()) return false;

  std::ofstream ofs(filepath);
  if (!ofs.is_open()) return false;
  ofs << jsonString;
  return true;
}

std::string SceneSerializer::SerializeToString(const std::vector<std::shared_ptr<GameObject>>& rootObjects, bool forceFullSerialize) {
  json j;
  json rootArray = json::array();
  for (const auto& obj : rootObjects) {
    // タイムラインプレビューオブジェクトは保存対象から除外する（タスク16）
    if (obj && obj->IsTimelinePreview()) continue;
    rootArray.push_back(SerializeGameObject(obj, forceFullSerialize));
  }
  j["rootObjects"] = rootArray;

  return j.dump(4);
}

bool SceneSerializer::Deserialize(const std::string& filepath, std::vector<std::shared_ptr<GameObject>>& outRootObjects) {
  std::ifstream file(filepath);
  if (!file.is_open()) return false;

  json j;
  try {
    file >> j;
  } catch (const json::parse_error& e) {
    std::cerr << "JSON parse error: " << e.what() << std::endl;
    return false;
  }

  outRootObjects.clear();
  if (j.contains("rootObjects") && j["rootObjects"].is_array()) {
    for (const auto& childJson : j["rootObjects"]) {
      auto obj = DeserializeGameObject(childJson);
      if (obj) {
        outRootObjects.push_back(obj);
      }
    }
  }

  return true;
}

std::shared_ptr<GameObject> SceneSerializer::CopyGameObject(std::shared_ptr<GameObject> src) {
  if (!src) return nullptr;
  json j = SerializeGameObject(src, true); // コピー時はフルシリアライズ
  return DeserializeGameObject(j);
}

bool SceneSerializer::DeserializeFromString(const std::string& jsonString, std::vector<std::shared_ptr<GameObject>>& outRootObjects) {
  if (jsonString.empty()) return false;
  try {
    json j = json::parse(jsonString);
    if (j.contains("rootObjects") && j["rootObjects"].is_array()) {
      for (const auto& objJson : j["rootObjects"]) {
        auto obj = DeserializeGameObject(objJson);
        if (obj) {
          outRootObjects.push_back(obj);
        }
      }
    }
  } catch (const std::exception& e) {
    std::cerr << "Failed to parse json string: " << e.what() << std::endl;
    return false;
  }
  return true;
}

bool SceneSerializer::SavePrefab(const std::string& filepath, std::shared_ptr<GameObject> obj) {
  if (!obj) return false;
  
  json j = SerializeGameObject(obj, true);

  std::ofstream file(filepath);
  if (file.is_open()) {
    file << j.dump(4);
    return true;
  }
  return false;
}

std::shared_ptr<GameObject> SceneSerializer::LoadPrefab(const std::string& filepath) {
  // 停止の調査用: プレハブ 1 件の読み込み全体(ファイル読み込み・解析・オブジェクト生成)の合計を測る
  CpuTimer::AccumScope prefabScope("LoadPrefab(all)");
  std::ifstream file(filepath);
  if (!file.is_open()) return nullptr;

  json j;
  try {
    CpuTimer::AccumScope parseScope("LoadPrefab.fileParse");
    file >> j;
  } catch (const json::parse_error& e) {
    std::cerr << "JSON parse error: " << e.what() << std::endl;
    return nullptr;
  }

  auto obj = DeserializeGameObject(j);
  if (obj) {
    obj->SetPrefabPath(filepath);
  }
  return obj;
}

} // namespace AbsoluteEngine
