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

// --- 門(手で位置を指定する。道の両側に島を 1 つずつ置いて、間を抜けて進む見た目を作る) ---
// 島には当たり判定が無いので、自機が島に当たることは無い。門は見た目だけの配置。
// 自機は、カメラの前 22 ユニットの平面で、横に約 +-9 まで動く。島の縁と道の中心の間(clearance)は、これより広く取る。
// 広すぎると「間を抜けている」感じが出ないので、通り道の幅(kCorridorHalfWidth)に少し足した程度を目安にする

// 島の種類。ステージで使える飾りの島の種類。Rock は灰色の岩肌(タレットの島と同じモデル)
enum class GateIslandKind {
  Small,
  Medium,
  Forest,
  Rock,
};

// 島の高さの基準。レールを上下させたとき、島とカメラの高さの差を保つかどうかを選ぶ
enum class GateHeightMode {
  CloudTop, // 雲の上面が基準(heightOffset は、雲の上面からの高さ。負は雲の中)。雲と島の位置関係が変わらない
  Rail,     // その距離のレールの高さが基準(heightOffset は、レールからの高さ。負はレールより下)。カメラと島の高さの差が変わらない
};

// 門の片側の島 1 つ
struct GateIslandSpec {
  GateIslandKind kind;  // 島の種類
  float scale;          // 島の大きさの倍率。0 以下なら、この側には島を置かない(片側だけの門にできる)
  float clearance;      // 道の中心から、島の上面の縁までの、水平の距離(ユニット)。島の中心は、これに上面の半径を足した位置になる
  float heightOffset;   // 島の原点(上面の中心)の高さ。基準は、門の heightMode が決める
  float yaw;            // 島の向き(ラジアン。島の見た目に変化を付けるだけ)
};

// 門 1 つ。レール上の距離 distance に、左右の島を向かい合わせに置く
struct GateSpec {
  float distance;           // レール上の距離(始点から。ユニット)
  GateHeightMode heightMode; // 高さの基準
  GateIslandSpec left;      // 道の左側の島
  GateIslandSpec right;     // 道の右側の島
};

constexpr float kGateClearDistance = 40.0f;   // 門の前後で、飾りの島を置かない距離(レールに沿った距離。大きい島が門の間を塞がないように)
// 今のステージは、レールが y=0 の直線で、雲の上面は y=-24。レール基準の高さは「雲の上面基準の高さ - 24」で、同じ位置になる。
// (レール基準の指定は、レールが上下しても、カメラと島の高さの差を保つためのもの。雲の上面の高さを変えたら、ここを直す)
constexpr float kGateHeightBelowRail = -26.0f; // 島の原点を、雲の上面より 2 低い位置に置く、レールからの高さ(今のステージ)
constexpr float kGateHeightAboveCloud = 0.0f;  // 島の原点を、雲の上面と同じ高さに置く、雲の上面からの高さ

// 門の一覧(距離の昇順でなくてもよい)。見た目は、画面で確認して調整する
// clearance 14 は、通り道の幅 12 に 2 足した値(自機の移動範囲 +-9 より、十分広い)
constexpr GateSpec kGates[] = {
    // 距離 150: 同じ大きさで向かい合う、基本の門(雲の上面基準)
    {150.0f, GateHeightMode::CloudTop, {GateIslandKind::Forest, 4.0f, 14.0f, kGateHeightAboveCloud, 0.6f}, {GateIslandKind::Medium, 4.0f, 14.0f, kGateHeightAboveCloud, 2.1f}},
    // 距離 270: 大きさの違う門(左を大きく、右を小さく。レール基準)
    {270.0f, GateHeightMode::Rail, {GateIslandKind::Medium, 5.0f, 15.0f, kGateHeightBelowRail, 4.0f}, {GateIslandKind::Small, 3.5f, 14.0f, kGateHeightBelowRail, 1.2f}},
    // 距離 500: 片側だけ(右の島だけ。左は空ける)。道が片側に開ける変化
    {500.0f, GateHeightMode::CloudTop, {GateIslandKind::Small, 0.0f, 0.0f, 0.0f, 0.0f}, {GateIslandKind::Forest, 5.0f, 14.0f, kGateHeightAboveCloud, 3.3f}},
    // 距離 740: 岩の門(レール基準)
    {740.0f, GateHeightMode::Rail, {GateIslandKind::Rock, 4.0f, 14.0f, kGateHeightBelowRail, 5.0f}, {GateIslandKind::Rock, 4.0f, 14.0f, kGateHeightBelowRail, 0.3f}},
    // 距離 1060: 終盤の、広い門
    {1060.0f, GateHeightMode::CloudTop, {GateIslandKind::Forest, 5.0f, 16.0f, kGateHeightAboveCloud, 1.7f}, {GateIslandKind::Forest, 5.0f, 16.0f, kGateHeightAboveCloud, 4.4f}},
};

// --- カーブの内側の除外 ---
// 島の上面の縁が、道の通り道(kCorridorHalfWidth)に入る島は、置かない。道に食い込む島を防ぐ。
// 判定は、レール上の点(kRailSampleSpacing ごと)との、水平の最短距離で行う。直線のレールでは、どの島も、自分の距離の点から
// lateral(>= 通り道 + 上面の半径)だけ離れているので、1 つも除外されない
constexpr float kRailSampleSpacing = 8.0f;      // 判定に使う、レール上の点の間隔(ユニット)
constexpr float kRailClearanceTolerance = 0.5f; // 浮動小数点の誤差を逃がす余裕(ユニット)。直線のレールで、縁がちょうど通り道の端に来る島を、誤って除外しないため
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
