#pragma once
#include "Type/Vector.h"
#include "Matrix.h"
#include "ModelInstance.h"
#include "ModelUtils.h"
#include <memory>
#include <vector>

class ModelResource;
class TextureResource;
struct PlayerDeathDissolve;

namespace AbsoluteEngine {
class ModelComponent;
}

// 蝶の翅が裂けて、破片になって飛び散る演出(死亡演出の崩壊のうち、シャードの部分)。
//
// 仕組み:
//   1. 初期化で、翅のメッシュの三角形を、表と裏のペアにして(同じ位置に重なっているので、ペアで 1 枚の板として扱う)、
//      ボロノイ分割(種の近くの三角形を 1 つのかたまりにする)で、かたまり(破片)に分ける。
//   2. 崩壊の進み具合に応じて、翅の外側のかたまりから順に「解放」する。解放の瞬間に、いまの蝶のポーズ(CPU でスキニングして求める)の
//      ワールド座標を記録し、そのかたまりを、翅の描画から消す(三角形の番号を潰して、描かれなくする)。
//   3. 解放したかたまりは、1 つの剛体として動く(蝶の速度を引き継ぎ、外へ飛び、回転し、重力で落ちる)。破片は、専用の 1 つの
//      モデル(全かたまりの頂点をまとめたもの)として、毎フレーム頂点を書き換えて描く。蝶と同じテクスチャ・マスク・しきい値で描くので、
//      解放の前後で見た目が変わらず、飛んでいる間も縁から朽ちる。
// スキニングは GPU(compute)で行われ、CPU からは結果を読めないので、CPU 側で、同じ式(ボーン行列の重み付き和)で再現する。
// 破片の解放の合図は、鱗粉(3c)のために GetReleasesThisFrame で渡す。

// 破片の動きと分け方の設定。名前付きの定数で、実機で見て調整する
namespace WingShatterDefaults {
constexpr int kClustersPerSide = 14;            // 片側の翅を分けるかたまりの数(左右で、合計 約 28 個)
constexpr float kReleaseStartProgress = 0.10f;  // 最初のかたまりを解放する、崩壊の進み具合(0 〜 1)
constexpr float kReleaseEndProgress = 0.80f;    // 最後のかたまりを解放する、崩壊の進み具合
constexpr float kReleaseJitter = 0.06f;         // 解放の時刻のばらつき(進み具合の単位)
constexpr float kOutwardSpeed = 2.5f;           // 翅の外側へ飛ぶ速さ(ユニット / 秒)
constexpr float kUpwardSpeed = 1.0f;            // 上へ飛ぶ速さ(ふわっと舞い上がる)
constexpr float kSpeedJitter = 0.5f;            // 速さのばらつきの割合(0 〜 1)
constexpr float kInheritVelocityScale = 1.0f;   // 蝶の速度を引き継ぐ割合
constexpr float kSpinSpeed = 7.0f;              // 回転の速さ(ラジアン / 秒。ばらつきあり)
constexpr float kGravity = 6.0f;                // 落ちる加速度(ユニット / 秒^2。蝶の落下よりゆるやかで、ひらひら落ちる)
constexpr float kDragPerSecond = 1.4f;          // 空気抵抗(1 秒あたりの速度の減衰率)
constexpr uint32_t kRandomSeed = 20261026;      // 乱数の種(同じ分け方・飛び方にそろえる)
} // namespace WingShatterDefaults

struct WingShatterSettings {
  int clustersPerSide = WingShatterDefaults::kClustersPerSide;
  float releaseStartProgress = WingShatterDefaults::kReleaseStartProgress;
  float releaseEndProgress = WingShatterDefaults::kReleaseEndProgress;
  float releaseJitter = WingShatterDefaults::kReleaseJitter;
  float outwardSpeed = WingShatterDefaults::kOutwardSpeed;
  float upwardSpeed = WingShatterDefaults::kUpwardSpeed;
  float speedJitter = WingShatterDefaults::kSpeedJitter;
  float inheritVelocityScale = WingShatterDefaults::kInheritVelocityScale;
  float spinSpeed = WingShatterDefaults::kSpinSpeed;
  float gravity = WingShatterDefaults::kGravity;
  float dragPerSecond = WingShatterDefaults::kDragPerSecond;
  uint32_t randomSeed = WingShatterDefaults::kRandomSeed;
};

// 破片を解放した瞬間の情報(鱗粉などの、ほかの演出が使う合図)
struct WingShardRelease {
  Vector3 position{0.0f, 0.0f, 0.0f};  // 解放した破片の中心(ワールド)
  Vector3 direction{0.0f, 1.0f, 0.0f}; // 飛んでいく向き(正規化済み)
  uint32_t triangleCount = 0;          // 破片の三角形の数(大きさの目安)
};

class WingShatter {
public:
  WingShatter();
  ~WingShatter();

  // 蝶のモデルから、破片への分け方を求め、破片の描画用のモデルを作る(シーンの開始時に 1 回。重いのでここで済ませる)。
  // 翅の描画から消した三角形が、前の実行から残っていても、ここで元に戻す。蝶のモデルが無い・翅が見つからないときは false
  bool Initialize(AbsoluteEngine::ModelComponent* playerModel, const std::shared_ptr<TextureResource>& atlasTexture,
                  const std::shared_ptr<TextureResource>& dissolveMask, const WingShatterSettings& settings = {});

  // 解放の状態をすべて元に戻し、翅の描画も元に戻す(演出の前、シーンの終わり)
  void Reset();

  // 毎フレーム(実時間)進める。
  // crumbleProgress: 崩壊の進み具合(0 〜 1)。bodyVelocity: 蝶の今の速度(ワールド。破片が引き継ぐ)
  void Update(float deltaTime, float crumbleProgress, const Vector3& bodyVelocity);

  // 破片を描く。dissolve は、蝶と同じディゾルブの状態(同じしきい値で朽ちる)
  void Draw(const PlayerDeathDissolve& dissolve);

  // 解放が 1 つでも始まっているか(描く必要があるか)
  bool IsActive() const { return releasedCount_ > 0; }

  // このフレームに解放された破片(鱗粉の発生などに使う)。次の Update で入れ替わる
  const std::vector<WingShardRelease>& GetReleasesThisFrame() const { return releasesThisFrame_; }

private:
  // 同じ位置で重なっている、翅の表の三角形と裏の三角形のペア(同じ動きをする 1 枚の板)
  struct TrianglePair {
    uint32_t triangles[2] = {0, 0};
    uint32_t count = 0; // 1 または 2(裏が見つからなければ 1)
  };

  struct Cluster {
    std::vector<uint32_t> triangles; // 翅のメッシュの三角形の番号(表と裏の両方)
    Vector3 bindCentroid{0.0f, 0.0f, 0.0f};
    float releaseProgress = 0.0f;
    uint32_t firstVertex = 0;        // 破片のモデルの頂点バッファの中の、先頭(三角形 1 つにつき 3 頂点)
    // 解放後
    bool released = false;
    Vector3 position{0.0f, 0.0f, 0.0f};
    Vector3 velocity{0.0f, 0.0f, 0.0f};
    Vector3 angle{0.0f, 0.0f, 0.0f};
    Vector3 angularVelocity{0.0f, 0.0f, 0.0f};
  };

  void BuildClusters_();
  void Release_(Cluster& cluster, const Vector3& bodyVelocity);
  // 翅のメッシュの頂点 1 つの、いまのポーズでのワールド座標と法線(CPU のスキニング)
  void SkinVertex_(uint32_t vertex, Vector3& position, Vector3& normal) const;
  void ApplyHiddenTriangles_();

  AbsoluteEngine::ModelComponent* playerModel_ = nullptr;
  ModelResource* modelResourceRaw_ = nullptr; // 蝶のモデル(翅の描画の三角形の番号を書き換える対象。持ち主は蝶の ModelInstance)
  const ModelData* modelData_ = nullptr;
  WingShatterSettings settings_;

  uint32_t wingMeshIndex_ = 0;
  uint32_t wingVertexOffset_ = 0; // 全メッシュをまとめた頂点の並びの中での、翅のメッシュの先頭
  uint32_t wingIndexStart_ = 0;   // 同じく、三角形の番号の並びの中での先頭
  uint32_t triangleCount_ = 0;

  std::vector<uint32_t> originalIndices_; // 全メッシュをまとめた、元の三角形の番号
  std::vector<uint32_t> currentIndices_;  // 解放した三角形を潰したもの
  std::vector<Cluster> clusters_;

  // 破片の描画用のモデル(全かたまりの頂点をまとめたもの。解放前のかたまりは、面積 0 にして描かない)
  std::shared_ptr<ModelResource> shardResource_;
  ModelInstance shardInstance_;
  std::vector<VertexData> localVertices_; // 解放の瞬間の、かたまりの中心からの相対の頂点(回転の前)
  std::vector<VertexData> worldVertices_;

  std::vector<WingShardRelease> releasesThisFrame_;
  uint32_t releasedCount_ = 0;
  bool built_ = false;
  bool indicesDirty_ = false;
};
