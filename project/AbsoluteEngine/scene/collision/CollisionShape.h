#pragma once
#include "../../Type/Vector.h"
#include <functional>

namespace AbsoluteEngine {

class SphereShape;
class AabbShape;

// 2 つの形状の衝突の結果
struct CollisionResult {
  bool isHit = false;
  Vector3 normal{0, 0, 0}; // 相手から自分(先に指定した側)へ向かう法線(自分を押し出す方向)
  float depth = 0.0f;      // めり込みの深さ
};

// 形状を世界のどこに置くか(形状そのものは、大きさだけを持ち、位置を持たない)。
// center: 形状の中心の世界座標, scale: 大きさに掛ける倍率(衝突判定では 1。エディタの表示では、オブジェクトの scale)
struct ShapePlacement {
  Vector3 center{0, 0, 0};
  Vector3 scale{1, 1, 1};
};

// 当たり判定の形状の基底クラス(球や箱など)。
//
// 作った理由: 以前は、衝突判定(CollisionManager)とエディタ(選択のレイ判定、枠の表示)が、
// ColliderComponent の種類(Sphere / AABB)を if で調べて、形ごとの計算を直接書いていた。
// 形ごとの計算を、この基底を継承したクラスへ移し、使う側は基底の参照だけで呼べるようにする(ポリモーフィズム)。
//
// 2 つの形状の組(球と球、球と箱、箱と箱)の判定は、Double Dispatch で選ぶ:
//   1 回目: a.Collide(b) で、a の実際の型が決まる(a の override が呼ばれる)
//   2 回目: その中で b.CollideWith(*this) を呼ぶと、b の実際の型と、引数(= a)の型の組で、関数が決まる
// これにより、型を調べる if / dynamic_cast を使わずに、組ごとの計算へたどり着ける。
// 注意: 形状を増やすときは、全ての形状クラスに、新しい形状との CollideWith を足す必要がある(Double Dispatch の性質)。
class ICollisionShape {
public:
  virtual ~ICollisionShape() = default;

  // 自分(self に置いた)と、相手(other に置いた)の衝突を調べる(1 回目の振り分け)。
  // 結果の法線は、自分を押し出す向き
  virtual CollisionResult Collide(const ShapePlacement& self, const ICollisionShape& other, const ShapePlacement& otherPlacement) const = 0;

  // first(firstPlacement に置いた)と、自分(self に置いた)の衝突を調べる(2 回目の振り分け)。
  // 結果の法線は、first を押し出す向き。Collide の中からだけ呼ぶ
  virtual CollisionResult CollideWith(const ShapePlacement& self, const SphereShape& first, const ShapePlacement& firstPlacement) const = 0;
  virtual CollisionResult CollideWith(const ShapePlacement& self, const AabbShape& first, const ShapePlacement& firstPlacement) const = 0;

  // レイ(origin から dir の向き)が当たるかを調べる。当たったら true を返し、outT にレイの上の距離を入れる(エディタの選択用)
  virtual bool Raycast(const ShapePlacement& self, const Vector3& origin, const Vector3& dir, float& outT) const = 0;

  // 形の輪郭を、世界座標の線分として drawLine へ渡す(エディタの枠の表示用。描き方は呼ぶ側が決める)
  using LineSink = std::function<void(const Vector3& from, const Vector3& to)>;
  virtual void DrawWire(const ShapePlacement& self, const LineSink& drawLine) const = 0;
};

} // namespace AbsoluteEngine
