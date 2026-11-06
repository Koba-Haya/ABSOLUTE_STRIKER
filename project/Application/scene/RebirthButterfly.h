#pragma once
#include "RebirthDirector.h"
#include "Vector.h"
#include "graphics/3d/model/ModelInstance.h"
#include <memory>

struct Animation;

// 復活の演出で出る蝶(ゲームオーバー画面用の ModelInstance)。ゲームシーンの自機と同じモデル(butterfly.gltf)・同じ大きさ(翼幅 3.7)・
// 同じ向き(頭が +Z、機首上げ 30 度が焼き込み済み)で、RebirthDirector が出す状態(位置、大きさ、ディゾルブ、アニメの段階)を反映して描く。
// 責務は「蝶のモデルの読み込み、アニメの切り替え、描画」だけ。タイムラインは RebirthDirector が持つ。
// クラスを分けた理由: 自機のようにオブジェクト・コンポーネントの仕組みを持たないシーン(GameOverScene)で、蝶だけを出すため
class RebirthButterfly {
public:
  // モデル・ディゾルブのマスク・アニメを読み込む。読み込めなければ false(描かない)
  bool Initialize();
  // 状態を反映し、アニメを進める。deltaTime: 実時間の 1 フレーム
  void Update(float deltaTime, const RebirthState &state);
  // 胴体の中心(胸のあたり。ワールド)。Update のあとに呼ぶ。見えていない、またはボーンが無いときは false
  bool GetBodyPosition(Vector3 &out) const;
  // 雲の合成の前に、深度を書いて描く(島と同じ扱い。雲のもやでかすむ)。見えない間は何もしない
  void Draw();

#ifdef USE_IMGUI
  // 確認用の表示(読み込み、位置、大きさ、ディゾルブ、アニメ、描画した回数)。ImGui の窓の中に出す
  void DrawDebugUi();
#endif

private:
  ModelInstance model_;
  std::shared_ptr<Animation> revive_; // 復活のアニメ(無いときは、代役の emerge)
  bool usingStandIn_ = false;         // 代役を使っている
  std::shared_ptr<Animation> flap_;
  Vector3 fitCenter_{0.0f, 0.0f, 0.0f}; // モデルの中心(翼幅に合わせる拡大の前に原点へ移す)
  float fitScale_ = 1.0f;               // 翼幅を kWingspan に合わせる拡大
  bool loaded_ = false;
  bool visible_ = false;
  bool flapStarted_ = false;
  Matrix4x4 world_ = MakeIdentity4x4(); // 今フレームに設定したワールド行列(胴体のボーンのモデル空間の位置を、ワールドへ直すのに使う)
  float brightness_ = 1.6f; // 蝶の色に掛ける明るさ(低い太陽が後ろから当たる向きなので、翅の表が暗くなりやすい。ImGui で調整)
  const char *loadFailure_ = "not initialized"; // 読み込みに失敗した理由(成功したら "none")
  // 確認用: 直近の状態と、描画した回数
  Vector3 lastPosition_{0.0f, 0.0f, 0.0f};
  float lastScale_ = 0.0f;
  float lastThreshold_ = 0.0f;
  bool lastDissolve_ = false;
  bool lastFlapping_ = false;
  float lastReviveSeconds_ = 0.0f;
  int drawCount_ = 0;
};
