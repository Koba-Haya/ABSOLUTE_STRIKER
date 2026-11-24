#pragma once
#include "../Type/Vector.h"

namespace AbsoluteEngine {

// メインカメラの位置と向きを、書き換えるための窓口(インターフェース)。
//
// IScreenView(読み取り専用)と分けた理由: カメラを動かすのは、レールに沿って進めるレールカメラの部品だけでよい。
// 敵や自機など、見るだけの部品に、書き換える機能まで見せないようにする(インターフェース分離の原則)。
// 実装は MainCameraView(IScreenView と同じクラス。持ち主は 1 つで、窓口が 2 つ)。
class ICameraRig {
public:
  virtual ~ICameraRig() = default;

  // メインカメラがあるか。false の間は、SetPose は何もしない
  virtual bool HasCamera() const = 0;

  // メインカメラの位置(eye)、注視点(target)、上方向(up)を、同時に設定する。
  // 3 つを必ず一緒に設定するので、1 つの関数にした。カメラが無いときは何もしない
  virtual void SetPose(const Vector3& eye, const Vector3& target, const Vector3& up) = 0;
};

} // namespace AbsoluteEngine
