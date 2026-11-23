#pragma once
#include "../Type/Matrix.h"
#include "../Type/Vector.h"

namespace AbsoluteEngine {

// 「いまの画面(メインカメラ)の見え方」を、読み取り専用で知るための窓口(インターフェース)。
//
// 作った理由: 敵やボス、自機の部品が、画面内か、カメラの位置や向きはどうか、を知るために、
// グローバル(BaseScene::GetActiveScene)から「今のシーン」を探し、さらにカメラの行列から NDC を手計算していた。
// 知りたいのは「画面の見え方」だけなので、その機能だけを窓口にして、使う側が受け取る形にする
// (インターフェース分離の原則、依存性の逆転の原則)。行列を使った「画面内か」の計算も、ここへ集めた。
// 読み取り専用なので、この窓口を受け取った部品は、カメラを書き換えられない(書き換えは ICameraRig)。
// 実装は MainCameraView。使う側は、カメラの型(GameCamera)を知らなくてよい。
class IScreenView {
public:
  virtual ~IScreenView() = default;

  // メインカメラがあるか。false の間は、下の問い合わせは、意味のある値を返さない(位置は 0、行列は単位行列、IsOnScreen は false)
  virtual bool HasCamera() const = 0;

  // 世界座標 worldPos が、画面内にあるか。
  // ndcMargin は、画面の端(NDC で ±1)からどこまでを「画面内」とみなすか(1.0 ちょうど画面内、1.2 なら端の外 20% まで)。
  // カメラの背後にあるとき、カメラが無いときは false
  virtual bool IsOnScreen(const Vector3& worldPos, float ndcMargin) const = 0;

  // カメラの位置と向き(長さ 1)。右と上は、カメラの向きから求めた、実際の右・上
  virtual Vector3 GetEye() const = 0;
  virtual Vector3 GetForward() const = 0;
  virtual Vector3 GetRight() const = 0;
  virtual Vector3 GetActualUp() const = 0;

  // ビュー行列と射影行列(照準の逆投影や、ロックオンの画面位置の計算に使う)
  virtual Matrix4x4 GetViewMatrix() const = 0;
  virtual Matrix4x4 GetProjectionMatrix() const = 0;
};

} // namespace AbsoluteEngine
