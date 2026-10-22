#pragma once
#include "Method.h"
#include "ModelInstance.h"
#include <cstdint>
#include <memory>
#include <vector>

class ModelResource;
class Renderer;
class TextureResource;
struct VertexData;

// 雲の文字の、壊れたあとの霧(柔らかいパーティクル)のグループ名と、最大数。
// グループは、シーンが作る(ParticleManager::CreateParticleGroup の最後の引数 softAlpha を true にして、パフの画像で作る)
inline constexpr const char *kStageCloudMistGroup = "stageCloudMist";
inline constexpr uint32_t kStageCloudMistMaxInstances = 8000;

// 雲の文字(ステージ名を、雲のパフの集まりで作る)の設定。ステージごとに変えられるよう、データとして持つ。
// 長さの単位はワールド(ユニット)
namespace StageCloudDefaults {
// 文字の中に置くパフの間隔。小さいほど、パフが増えて、文字がなめらかになる(その分、重くなる)
constexpr float kPuffSpacing = 0.06f;
// パフの見える円の半径の範囲。文字の線の太さ(約 0.3)より十分小さくして、「A」や「G」の穴がつぶれないようにする
constexpr float kMinPuffRadius = 0.09f;
constexpr float kMaxPuffRadius = 0.15f;
// 画像のどの範囲を雲にするか(画像の上端を 0、下端を 1)。メイン文字は上の部分、サブタイトルは下の部分
constexpr float kMaskTopV = 0.0f;
constexpr float kMaskBottomV = 0.58f;
// パフが、文字の縁からはみ出さないよう、パフの中心から、半径のこの割合だけ離れた上下左右の 4 点も、文字の中に入るパフだけを置く。
// 0 ではみ出し放題(文字が太って、穴がつぶれる)、大きいほど、文字の内側にだけ置く(大きすぎると、パフが減って、文字が欠ける)
constexpr float kEdgeInset = 0.6f;
// サブタイトルの、パフの間隔・大きさ・縁からはみ出さない度合い(線が細い文字なので、メイン文字より小さくする)
constexpr float kSubtitleSpacing = 0.035f;
constexpr float kSubtitleMinRadius = 0.042f;
constexpr float kSubtitleMaxRadius = 0.07f;
constexpr float kSubtitleEdgeInset = 0.2f;
// 光の筋の幅(文字の幅に対する割合)。筋の近くのパフが、暖かい白に光る
constexpr float kBandWidth = 0.18f;
// 筋が通ってから、パフが育ち切るまでの長さ(出現の進行度に対する割合)
constexpr float kGrowSpan = 0.12f;
// 出現のとき、パフが最初にずれている距離の上限(ユニット。板の面に沿って)と、奥へずれている距離(ユニット)
constexpr float kStartOffset = 1.2f;
constexpr float kStartDepth = 1.0f;
// 突き破られた瞬間、パフ本体が、少し膨らんで消えるまでの時間(秒)と、膨らむ倍率の増え分。
// 消えるのと入れ替わりに、柔らかい霧のパーティクルが広がる
constexpr float kBurstSeconds = 0.18f;
constexpr float kBurstGrow = 0.4f;
// 霧: パフ 1 つにつき、何個の粒を出すか(2 個目は、小さくて速い、細かいちぎれ雲になる)
constexpr int kMistWispsPerPuff = 2;
// 霧: パフの位置から、衝撃点を中心に外へ広がる速さ(衝撃点からの距離 x この値 = 基本の速さ。ユニット / 秒)
constexpr float kMistExpandSpeed = 2.0f;
// 粒ごとの、速さの倍率の範囲(基本の速さに掛ける。粒によって、飛ぶ速さを大きく変えて、散り散りにする)
constexpr float kMistSpeedScaleMin = 0.4f;
constexpr float kMistSpeedScaleMax = 1.8f;
// 衝撃点のまわりを回る速さの割合(衝撃点からの距離 x 割合。向きは粒ごとにランダム。渦を巻くように散る)
constexpr float kMistSwirl = 0.8f;
// 霧が上へ浮く速さ(ユニット / 秒)
constexpr float kMistRise = 0.4f;
// 霧が、カメラ側へ寄ってくる速さの範囲(ユニット / 秒)
constexpr float kMistToCameraMin = 1.0f;
constexpr float kMistToCameraMax = 8.0f;
// 霧の速さの、ばらつき(ユニット / 秒。上下左右へ、この範囲でランダムにずらす)
constexpr float kMistJitter = 2.0f;
// 霧の粒の、最初の大きさ(パフの半径に対する倍率の範囲)と、消える頃の大きさ(最初の大きさに対する倍率の範囲)
constexpr float kMistStartScaleMin = 3.0f;
constexpr float kMistStartScaleMax = 6.0f;
constexpr float kMistEndScaleMin = 2.0f;
constexpr float kMistEndScaleMax = 5.0f;
// 霧の寿命の範囲(秒)と、最初の濃さ(0 〜 1。寿命の間に、0 へ薄れる)
constexpr float kMistLifetimeMin = 0.7f;
constexpr float kMistLifetimeMax = 2.0f;
constexpr float kMistAlpha = 0.8f;
} // namespace StageCloudDefaults

struct StageCloudSettings {
  float puffSpacing = StageCloudDefaults::kPuffSpacing;
  float minPuffRadius = StageCloudDefaults::kMinPuffRadius;
  float maxPuffRadius = StageCloudDefaults::kMaxPuffRadius;
  float edgeInset = StageCloudDefaults::kEdgeInset;
  float maskTopV = StageCloudDefaults::kMaskTopV;
  float maskBottomV = StageCloudDefaults::kMaskBottomV;
  float bandWidth = StageCloudDefaults::kBandWidth;
  float growSpan = StageCloudDefaults::kGrowSpan;
  float startOffset = StageCloudDefaults::kStartOffset;
  float startDepth = StageCloudDefaults::kStartDepth;
  float burstSeconds = StageCloudDefaults::kBurstSeconds;
  float burstGrow = StageCloudDefaults::kBurstGrow;
  int mistWispsPerPuff = StageCloudDefaults::kMistWispsPerPuff;
  float mistExpandSpeed = StageCloudDefaults::kMistExpandSpeed;
  float mistSpeedScaleMin = StageCloudDefaults::kMistSpeedScaleMin;
  float mistSpeedScaleMax = StageCloudDefaults::kMistSpeedScaleMax;
  float mistSwirl = StageCloudDefaults::kMistSwirl;
  float mistRise = StageCloudDefaults::kMistRise;
  float mistToCameraMin = StageCloudDefaults::kMistToCameraMin;
  float mistToCameraMax = StageCloudDefaults::kMistToCameraMax;
  float mistJitter = StageCloudDefaults::kMistJitter;
  float mistStartScaleMin = StageCloudDefaults::kMistStartScaleMin;
  float mistStartScaleMax = StageCloudDefaults::kMistStartScaleMax;
  float mistEndScaleMin = StageCloudDefaults::kMistEndScaleMin;
  float mistEndScaleMax = StageCloudDefaults::kMistEndScaleMax;
  float mistLifetimeMin = StageCloudDefaults::kMistLifetimeMin;
  float mistLifetimeMax = StageCloudDefaults::kMistLifetimeMax;
  float mistAlpha = StageCloudDefaults::kMistAlpha;
};

// サブタイトル用の設定(画像の下の部分を、メイン文字より小さいパフで雲にする。ほかの値は、メイン文字と同じ)
inline StageCloudSettings MakeSubtitleCloudSettings() {
  StageCloudSettings settings;
  settings.puffSpacing = StageCloudDefaults::kSubtitleSpacing;
  settings.minPuffRadius = StageCloudDefaults::kSubtitleMinRadius;
  settings.maxPuffRadius = StageCloudDefaults::kSubtitleMaxRadius;
  settings.edgeInset = StageCloudDefaults::kSubtitleEdgeInset;
  settings.maskTopV = StageCloudDefaults::kMaskBottomV;
  settings.maskBottomV = 1.0f;
  return settings;
}

// ステージ名の文字を、雲のパフの集まりで作って、出現させ、壊すクラス。
// 文字の形: ステージ名の画像の不透明な所を、等間隔に拾って、そこにパフを置く。
// 出現: 光の筋が左から右へ通ると、通った所のパフが、ずれた位置から育って組み上がる(通った順にできる)。筋の近くのパフは光る。
// 壊れ方: 突き破られた瞬間、パフ本体は少し膨らんで消え、入れ替わりに、衝撃点を中心に外へ広がる柔らかい霧(パーティクル)が出る。
// 描画: 全パフの、画面に向く四角形を、毎フレーム 1 つの頂点バッファへ作り直して描く。影・本体・光の 3 回に分ける。
// 板の位置や、いつ割るかは知らない(呼ぶ側が決める)。座標は、板の向き(右 = x、上 = y、前 = z)の座標で、長さはワールドの単位
class StageNameClouds {
public:
  StageNameClouds();
  ~StageNameClouds();

  StageNameClouds(const StageNameClouds &) = delete;
  StageNameClouds &operator=(const StageNameClouds &) = delete;

  // 画像から、パフの位置を決めて、描画の準備をする(読み込みが重いので、シーンを作るときに呼ぶ)。
  // maskImagePath: ステージ名の画像(文字の形を、不透明な所から読み取る。settings の範囲だけを雲にする)
  // puffTexture: パフの画像
  // halfWidth / halfHeight: 画像を貼る板の半分の大きさ(ワールドの長さ)
  // impact: 壊れるとき、広がる中心(板の中心が (0, 0)、ワールドの長さ)
  // sweepRangeX: 光の筋が通る横の範囲(最小 x、最大 x)。複数の雲の文字(メイン文字とサブタイトル)で筋の位置をそろえるときに渡す。
  //              nullptr なら、この文字の横の範囲を使う
  // 戻り値: 作れたら true(失敗したら描かない)
  bool Build(const char *maskImagePath, const std::shared_ptr<TextureResource> &puffTexture, float halfWidth, float halfHeight,
             const Vector2 &impact, const StageCloudSettings &settings, const Vector2 *sweepRangeX = nullptr);

  // この文字の横の範囲(最小 x、最大 x)。別の雲の文字へ、筋の位置をそろえるために渡す
  Vector2 GetTextRangeX() const { return textRangeX_; }

  // 出現の進行度(0 = まだ何もない、1 = 組み上がった)。光の筋は、この進行度に合わせて、左から右へ通る
  void SetReveal(float reveal) { reveal_ = reveal; }
  float GetReveal() const { return reveal_; }

  // 壊す。パフ本体が膨らんで消え始め、霧のパーティクルを出す。1 回だけ
  // frame: 板の向きの座標を、ワールドへ変える行列(霧を、ワールドの位置・向きで出すのに使う)
  void Break(const Matrix4x4 &frame);
  bool IsBroken() const { return broken_; }
  // 壊し始めてからの経過(秒)。壊れていなければ 0
  float GetBreakElapsedSeconds() const { return broken_ ? breakElapsed_ : 0.0f; }

  // 時間を進める
  void Update(float deltaTime);

  // 描く。壊れて、パフ本体が消え終わったあとは、何もしない(霧は、パーティクルが描く)
  // frame: 板の向きの座標を、ワールドへ変える行列(行 = 右・上・前の単位ベクトルと、板の位置)
  void Draw(Renderer *renderer, const Matrix4x4 &frame);

  bool IsBuilt() const { return built_; }

private:
  // 1 つのパフ
  struct Puff {
    Vector2 target{0.0f, 0.0f}; // 文字の中の位置(板の向きの座標)
    Vector3 startOffset{0.0f, 0.0f, 0.0f}; // 出現のとき、最初にずれている向き(育つにつれて 0 へ)
    float radius = 0.2f;        // 見える円の半径
    float xNorm = 0.0f;         // 文字の左端を 0、右端を 1 とした、横の位置(光の筋が通る順)
    float phase = 0.0f;         // 揺れの位相
  };

  // 1 回の描画(影・本体・光のどれか)の頂点バッファ
  struct Layer;

  bool BuildLayer_(Layer &layer, const std::shared_ptr<TextureResource> &puffTexture);

  std::vector<Puff> puffs_;
  std::unique_ptr<Layer> shadow_;
  std::unique_ptr<Layer> body_;
  std::unique_ptr<Layer> glow_;
  StageCloudSettings settings_;
  Vector2 impact_{0.0f, 0.0f};
  Vector2 textRangeX_{0.0f, 0.0f};
  float reveal_ = 0.0f;
  float time_ = 0.0f;         // 揺れ用の経過(秒)
  float breakElapsed_ = 0.0f;
  bool broken_ = false;
  bool built_ = false;
};
