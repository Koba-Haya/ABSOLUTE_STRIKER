#pragma once
#include "graphics/3d/model/ModelInstance.h"
#include "Vector.h"
#include <memory>

class ModelResource;
class Renderer;

// ステージの浮島 1 つ。モデルと、置き場所(位置、大きさ、向き)だけを持つ。揺れは無い(置いたまま動かない)。
// 責務は「島 1 つの見た目の持ち物」だけ。どこに何個置くか、いつ描くかは StageIslands が決める。
// クラスを分けた理由: 島 1 つの持ち物(モデルの実体と行列)を、レールに沿った配置の計算から独立させるため。
// 将来、島の上へタレットを置くとき(島の位置と大きさを問い合わせる)も、この部品に聞けばよい
class StageIsland {
public:
  // resource: 島のモデル(共有。呼び出し側が 1 度だけ読み込んで、島ごとに渡す)。失敗したら false
  bool Initialize(const std::shared_ptr<ModelResource> &resource);

  // 置き場所を決める。position は島の「上面の中心」(モデルの原点)。scale は一様な拡大。
  // rotate は、オイラー角(ラジアン。x: 傾き、y: 向き、z: 傾き)
  void SetPlacement(const Vector3 &position, float scale, const Vector3 &rotate);

  // 描く。大きさは、置いたときの値のまま(距離で大きさを変えない。小さく見えるのは、遠近だけによる)
  void Draw(Renderer *renderer);

  // 島の上面の中心(ワールド)。タレットの配置に使う
  const Vector3 &GetPosition() const { return position_; }
  float GetScale() const { return scale_; }

private:
  ModelInstance model_;
  Vector3 position_{0.0f, 0.0f, 0.0f};
  float scale_ = 1.0f;
  Vector3 rotate_{0.0f, 0.0f, 0.0f};
};
