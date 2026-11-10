#pragma once
#include "StageIsland.h"
#include <cstdint>
#include <memory>
#include <vector>

class RailCameraComponent;
class Renderer;

// ステージの浮島の群れの既定値。見た目の調整は、ここの値を変えて再ビルドする
namespace StageIslandDefaults {
constexpr int32_t kSeed = 20261109;       // 配置の乱数のシード(固定。毎回同じ配置になる)
// --- レールに沿った並べ方 ---
constexpr float kFirstDistance = 40.0f;   // 最初の島を置く、レール上の距離(始点から。ユニット)
constexpr float kSpacing = 32.0f;         // 島どうしの間隔(レールに沿った距離。ユニット)
constexpr float kSpacingJitter = 0.35f;   // 間隔のばらつき(+-。間隔に対する割合)
// --- 横のずれ(レールから、水平に離す) ---
constexpr float kCorridorHalfWidth = 12.0f; // レールの通り道として空ける幅(片側。自機と敵が動く範囲)
constexpr float kLateralRange = 40.0f;      // 通り道の外へ、さらに離す幅の範囲(乱数)
constexpr float kTopRadiusPerScale = 3.2f;  // 島の大きさ 1 倍あたりの、上面の半径の目安(モデルは 1.5 から 3.2)。大きい島ほど外へ離す
// --- 高さ: 雲の上面から見た、島の原点(上面の中心)の高さの範囲(負は雲の上面より下)。
// 島のモデルは、原点から下へ 3.6 から 6 倍(大きさの倍率ぶん)伸びる。雲の面付近に置くと、島の下側は雲に埋まり、雲海に浮かぶ岩に見える。
// 雲の上面より大きく上に置くと、島の下側が雲の外に見えて、空に浮かぶ島になってしまう(画面で確認済み) ---
constexpr float kHeightAboveCloudMin = -6.0f;
constexpr float kHeightAboveCloudMax = 2.0f;
// --- 大きさ。ステージのカメラは視野が狭い(縦 約 26 度)ので、GameOver より大きくする ---
constexpr float kScaleMin = 3.0f;
constexpr float kScaleMax = 6.0f;
constexpr float kMaxTiltRad = 0.10f;        // 島の傾きの最大(ラジアン。約 6 度)
// --- 描く範囲(自機のいるレール上の距離からの差) ---
constexpr float kDrawBehind = 40.0f;        // 後ろへ、これより離れた島は描かない(カメラは振り返らない)
constexpr float kDrawAhead = 950.0f;        // 前へ、これより遠い島は描かない(ゲームカメラの遠方クリップ 1000 より少し手前)
// --- タレットの島(手で位置を指定する。タレットは rock の島だけに乗り、飾りの島は rock を使わない) ---
// rock は灰色の岩肌で木が無い。緑の飾りの島の中で、色で見分けがつき、木にタレットが隠れない。
// lateral: レールからの横のずれ(正は右、負は左。ユニット)。distance: レール上の距離。scale: 島の大きさの倍率
struct TurretIslandSpec {
  float distance;
  float lateral;
  float scale;
};
constexpr TurretIslandSpec kTurretIslands[] = {
    {360.0f, -28.0f, 3.5f},
    {640.0f, 28.0f, 3.5f},
    {920.0f, -28.0f, 3.5f},
};
constexpr float kTurretIslandHeightAboveCloud = 2.0f; // 島の原点の、雲の上面からの高さ(丘の頂点は、これより 0.52 x 倍率 高い)
constexpr float kRockTopHeight = 0.52f;               // rock のモデルの、原点から頂点までの高さ(倍率 1 のとき。island_rock.gltf の最大の y)
constexpr float kTurretClearDistance = 40.0f;         // タレットの島の前後で、飾りの島を置かない距離(レールに沿った距離)
} // namespace StageIslandDefaults

// ステージの浮島の群れ。レールに沿って、通り道の両側に、雲の上へ頭を出す島を置く。
// 責務は「レール基準の配置(固定の乱数)と、描く範囲の絞り込み」だけ。島 1 つの持ち物は StageIsland。
// 島は雲を合成する前(雲の依頼のあと、物体の描画の前)に描く。雲の合成が深度を見るので、島の下側は雲に埋まる。
// 光は設定しない(シーンの光をそのまま使う)。Renderer の光の定数バッファは 1 つで、同じフレームの最後に書いた光が全モデルに
// 効くため、ここで光を書くと敵と自機の照明が変わってしまう。
// GameOverIslands とは独立している(GameOver は、カメラ基準の配置と巣の島を持つ別の作り)
class StageIslands {
public:
  // rail: 島を並べるレール(読み取りだけ)。cloudLayerTop: 雲海の上面の高さ(島の高さの基準)
  // 戻り値: モデルを 1 つも読み込めなければ false(その場合 Draw は何も描かない)
  bool Initialize(const RailCameraComponent &rail, float cloudLayerTop);

  // 描く。railDistance: 自機(カメラ)のいる、レール上の距離(始点から。ユニット)。雲の依頼のあと、物体の描画の前に呼ぶ
  void Draw(Renderer *renderer, float railDistance);

  // タレットを置く島の 1 つ(位置と大きさは island に聞く)
  struct TurretSlot {
    const StageIsland *island = nullptr; // StageIslands が持つ島(StageIslands が生きている間、有効)
    float distance = 0.0f;               // 島の、レール上の距離
  };
  const std::vector<TurretSlot> &GetTurretSlots() const { return turretSlots_; }

private:
  struct Entry {
    std::unique_ptr<StageIsland> island; // ModelInstance は動かせないので、ポインタで持つ
    float distance = 0.0f;               // 島の、レール上の距離(昇順に並べる)
  };
  std::vector<Entry> entries_;
  std::vector<TurretSlot> turretSlots_;
};
