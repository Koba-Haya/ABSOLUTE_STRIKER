#pragma once
#include "Method.h"
#include "ModelInstance.h"
#include "VoronoiFracture.h"
#include <cstdint>
#include <memory>
#include <vector>

class ModelResource;
class Renderer;
class TextureResource;
struct VertexData;

// 砕けた破片の見た目と動きの設定。ステージごとに変えられるよう、データとして持つ。
// 長さの単位はワールド(ユニット)。板の横幅は StageNameBoardSettings::width で決まる
namespace FragmentDefaults {
// 破片の厚み(ユニット)
constexpr float kThickness = 0.25f;
// 衝撃点のすぐ近くの破片が、板の面に沿って外へ飛ぶ速さ(ユニット / 秒)
constexpr float kBlastSpeed = 9.0f;
// 衝撃点からの距離による、外へ飛ぶ速さの弱まり方。この距離(ユニット)だけ離れると、速さが半分になる
constexpr float kBlastHalfDistance = 4.0f;
// 進行方向(板の前)へ飛ぶ速さ(ユニット / 秒)。負の値は、進行方向の逆(カメラ側)へ飛ぶ。
// カメラ側へ飛ばすと、自機が突き抜けたあと、破片が自機のまわりを通って、カメラへ向かって飛び出してくるように見える
constexpr float kForwardSpeed = -10.0f;
// 下向きの加速度(ユニット / 秒の 2 乗)
constexpr float kGravity = 6.0f;
// 回転の速さの上限(ラジアン / 秒。軸ごと。衝撃点に近いほど、この値に近づく)
constexpr float kSpinSpeed = 5.0f;
// 破片ごとの、速さのばらつき(0 〜 1。0.3 なら、速さが 70% 〜 130% になる)
constexpr float kSpeedJitter = 0.3f;
// 割れてから、破片が消え終わるまでの時間(秒)
constexpr float kLifetimeSeconds = 1.6f;
// 消え終わる前の、薄くなっていく時間(秒)
constexpr float kFadeSeconds = 0.5f;
// 乱数の種(同じ値なら毎回同じ動きになる)
constexpr uint32_t kRandomSeed = 1;
} // namespace FragmentDefaults

struct FragmentSettings {
  float thickness = FragmentDefaults::kThickness;
  float blastSpeed = FragmentDefaults::kBlastSpeed;
  float blastHalfDistance = FragmentDefaults::kBlastHalfDistance;
  float forwardSpeed = FragmentDefaults::kForwardSpeed;
  float gravity = FragmentDefaults::kGravity;
  float spinSpeed = FragmentDefaults::kSpinSpeed;
  float speedJitter = FragmentDefaults::kSpeedJitter;
  float lifetimeSeconds = FragmentDefaults::kLifetimeSeconds;
  float fadeSeconds = FragmentDefaults::kFadeSeconds;
  uint32_t randomSeed = FragmentDefaults::kRandomSeed;
};

// ボロノイ分割の領域(凸多角形)を、厚みのある破片にして、飛ばして、描くクラス。
// 破片は、前面・背面・側面を持つ角柱。画像(UV)は元の板の位置から決めるので、割る前と同じ絵に見える。
// 板の位置や、いつ割るかは知らない(呼ぶ側が決める)。
//
// 座標: 板の「向き」(右 = x、上 = y、前 = z)の座標で、長さはワールドの単位にそろえる。
//   板を置く位置・向きは、Draw に渡す行列(frame)で、ワールドへ変える。
// 描画: 全破片の頂点を 1 つの頂点バッファにまとめ、毎フレーム CPU で破片ごとに動かして書き換える。1 回の描画で済む。
class StageNameFragments {
public:
  StageNameFragments();
  ~StageNameFragments();

  StageNameFragments(const StageNameFragments &) = delete;
  StageNameFragments &operator=(const StageNameFragments &) = delete;

  // 領域から、破片のメッシュを作る(まだ飛ばさない)。
  // cells: ボロノイ分割の領域(rect と同じ座標)。rect: 分割した範囲(板の中の座標。画像全体に対応する)
  // worldScale: 板の中の座標 1 を、ワールドの長さにする倍率(板の横幅 / rect の横幅)
  // texture: 破片に貼る画像(ステージ名)。settings: 厚みなど
  // 戻り値: 作れたら true
  bool Build(const std::vector<VoronoiCell> &cells, const FractureRect &rect, float worldScale,
             const std::shared_ptr<TextureResource> &texture, const FragmentSettings &settings);

  // 破片を飛ばし始める。impact: 衝撃点(Build に渡した rect と同じ座標)
  // 衝撃点に近い破片ほど、速く外へ飛び、速く回る
  void Launch(const Vector2 &impact);

  // 時間を進める(飛ばしていないときは何もしない)
  void Update(float deltaTime);

  // 描く。飛ばしていない、または消え終わったときは何もしない
  // frame: 板の向きの座標を、ワールドへ変える行列(行 = 右・上・前の単位ベクトルと、板の位置)
  void Draw(Renderer *renderer, const Matrix4x4 &frame);

  bool IsBuilt() const { return built_; }
  // 飛ばし始めて、まだ消え終わっていなければ true
  bool IsActive() const;

private:
  // 1 つの破片の動きの状態(形の頂点は、localVertices_ にまとめて持つ)
  struct Fragment {
    Vector3 centroid{0.0f, 0.0f, 0.0f};        // 破片の中心(板の向きの座標)。形の頂点は、この中心からの相対
    Vector3 offset{0.0f, 0.0f, 0.0f};          // 中心の移動量
    Vector3 velocity{0.0f, 0.0f, 0.0f};        // 移動の速さ
    Vector3 angle{0.0f, 0.0f, 0.0f};           // 回転角(ラジアン。軸ごと)
    Vector3 angularVelocity{0.0f, 0.0f, 0.0f}; // 回転の速さ
    uint32_t firstVertex = 0;                  // 頂点の並びの中での、この破片の最初の頂点の位置
    uint32_t vertexCount = 0;
  };

  // 破片の透明度(消え終わる前に、だんだん薄くなる)
  float CalcAlpha_() const;

  std::vector<Fragment> fragments_;
  std::vector<VertexData> localVertices_; // 破片の中心からの相対の頂点(動かない)
  std::vector<VertexData> worldVertices_; // 毎フレーム、動かしたあとのワールドの頂点を作る作業用
  std::shared_ptr<ModelResource> resource_;
  ModelInstance instance_;
  FragmentSettings settings_;
  float worldScale_ = 1.0f; // 板の中の座標 1 を、ワールドの長さにする倍率(Build で受け取る)
  float elapsed_ = 0.0f;    // 飛ばし始めてからの経過(秒)
  bool built_ = false;
  bool launched_ = false;
};
