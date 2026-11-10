#pragma once
#include "AbsoluteEngine/scene/Component.h"
#include "ItemDefinition.h"
#include "Type/Vector.h"
#include <functional>
#include <memory>
#include <string>

namespace AbsoluteEngine {
class GameObject;
}

// 落ちているアイテム 1 個の動き(ゆれる、自転する、自機が近づいたら吸い寄せられる、取得する、時間で消える)。
// 分けた理由: 「取得判定」を、アイテムの持ち主(1 個のオブジェクト)の中に閉じ込め、シーンに処理を書かないため。
//             効果そのものは知らない(取得した瞬間に、設定された関数 onCollected を 1 回呼ぶだけ。効果は ItemEffectApplier の担当)。
// 取得は 2 段: pullRadius 以内で吸い寄せ、pickupRadius 以内で取得。取得の瞬間に効果を出し、
// 残りの短い間(collectSeconds)は、膨らんでから縮みつつ自機へ吸い込まれる演出だけを行い、その後に消える。
class ItemPickupComponent : public AbsoluteEngine::IComponent {
public:
  ItemPickupComponent() = default;
  ~ItemPickupComponent() override = default;

  // 出現直後に ItemSpawner が呼ぶ。baseScale は定義の拡大率、onCollected は取得の瞬間に 1 回呼ぶ関数
  void Configure(const ItemPickupSettings &settings, float baseScale, std::function<void()> onCollected);

  void Update(float deltaTime) override;

  std::string GetTypeName() const override { return "ItemPickupComponent"; }

private:
  // 自機のオブジェクトを探す(見つかるまで毎フレーム試す。見つけたら覚える)
  std::shared_ptr<AbsoluteEngine::GameObject> FindPlayer_();
  // 自機へ向かって、speed で dt 秒ぶん近づく(base 位置を動かす)
  void MoveToward_(const Vector3 &target, float speed, float deltaTime);
  // 見た目(位置のゆれ、自転、拡大)を、いまの状態から決めて、持ち主の Transform へ書く
  void ApplyVisual_(float scale);

  ItemPickupSettings settings_;
  float baseScale_ = 1.0f;
  std::function<void()> onCollected_;

  std::weak_ptr<AbsoluteEngine::GameObject> player_;

  Vector3 basePosition_{0.0f, 0.0f, 0.0f}; // ゆれを除いた位置(吸い寄せで動くのは、これ)
  bool initialized_ = false;               // basePosition_ を、最初の Update で持ち主の位置から決めたか
  float age_ = 0.0f;                       // 出現してからの時間(ゆれの位相、寿命の判定に使う)
  float spinAngle_ = 0.0f;                 // 自転の角度
  bool collected_ = false;                 // 取得済み(演出中)か
  float collectTimer_ = 0.0f;              // 取得の演出の経過時間
};
