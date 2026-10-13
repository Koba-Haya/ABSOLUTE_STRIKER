#pragma once
#include <memory>
#include <optional>
#include <string>
#include "Type/Vector.h"

namespace AbsoluteEngine {
class GameObject;
}

// 直進弾の生成パラメータ
struct BulletSpec {
  Vector3 position{ 0.0f, 0.0f, 0.0f }; // 発射位置
  Vector3 velocity{ 0.0f, 0.0f, 0.0f }; // 速度（ワールド空間, m/秒）
  float radius = 0.5f;                  // コライダー半径
  std::optional<Vector4> color;         // 弾モデルの色（未指定ならモデル既定）
  std::string name;                     // GameObject名（空ならファクトリ既定）
};

// ホーミング弾（ベジェ曲線で追尾）の生成パラメータ
struct HomingBulletSpec {
  Vector3 startPos{ 0.0f, 0.0f, 0.0f };   // 始点（P0）
  Vector3 controlPos{ 0.0f, 0.0f, 0.0f }; // 制御点（P1）
  float duration = 0.6f;                  // 着弾までの時間（秒）
  float radius = 1.0f;                    // コライダー半径
  std::weak_ptr<AbsoluteEngine::GameObject> target; // 追尾対象
};

// 弾の GameObject（モデル・コライダー・弾コンポーネント）の組み立てを1箇所に集約する。
// 戻り値のオブジェクトは呼び出し側が scene->AddRootObject() で登録すること。
namespace BulletFactory {

std::shared_ptr<AbsoluteEngine::GameObject> CreatePlayerNormal(const BulletSpec& spec);
std::shared_ptr<AbsoluteEngine::GameObject> CreatePlayerHoming(const HomingBulletSpec& spec);
std::shared_ptr<AbsoluteEngine::GameObject> CreateEnemy(const BulletSpec& spec);

} // namespace BulletFactory
