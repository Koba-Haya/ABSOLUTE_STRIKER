#pragma once
#include "Method.h"
#include "ModelInstance.h"
#include "StageNameClouds.h"
#include "StageNameFragments.h"
#include "VoronoiFracture.h"
#include <memory>
#include <string>
#include <vector>

class Renderer;
class TextureResource;

// ステージ名の板の配置の設定。ステージごとに変わる値なので、データとして持つ。
namespace StageNameBoardDefaults {
// 板の横幅(ユニット)
constexpr float kWidth = 12.0f;
// 自機の中心から、機首の先までの距離(ユニット)のおおよその値。
// 自機の「中心」が板に着く前に、機首の先が板に触れるので、その分だけ板を前へずらして、触れた瞬間に割れるようにする
constexpr float kContactOffset = 2.0f;
// 俯瞰の間、カメラの正面に置くときの、カメラからの距離(ユニット)。近いほど大きく見える
constexpr float kHoldDistance = 20.0f;
// 俯瞰の間の位置を、視線より上へずらす角度(ラジアン)。画面の上寄りに置いて、画面の下にいる竜と重ならないようにする
constexpr float kHoldAngle = 0.12f;
// 竜を飛び越えるときの、弧の高さ(ユニット)
constexpr float kArcHeight = 8.0f;
// 板が、俯瞰の位置から、竜の前へ着くまでの時間(秒)。「戻る」段階の始まりから数える。竜が板に触れる時刻(breakSeconds)より短くする
constexpr float kArcSeconds = 0.8f;
// 板の上端が、画面の上端のどこまで届いてよいか(画面の半分の高さに対する割合。1 で上端)。
// 弧の高さは、この範囲に収まるよう、毎フレーム抑える(カメラが下がったり、画角が変わったりしても、板が画面の外へ出ない)
constexpr float kScreenLimit = 0.85f;
} // namespace StageNameBoardDefaults

struct StageNameBoardSettings {
  float width = StageNameBoardDefaults::kWidth;
  float contactOffset = StageNameBoardDefaults::kContactOffset;
  float holdDistance = StageNameBoardDefaults::kHoldDistance;
  float holdAngle = StageNameBoardDefaults::kHoldAngle;
  float arcHeight = StageNameBoardDefaults::kArcHeight;
  float arcSeconds = StageNameBoardDefaults::kArcSeconds;
  float screenLimit = StageNameBoardDefaults::kScreenLimit;
};

// ステージ名の見せ方(素材)。ステージごとに選ぶ
enum class StageNameStyle {
  Shards, // 画像を貼った板が、ボロノイの破片になって割れる(石・氷・ガラスのイメージ)
  Clouds  // 文字が、雲のパフの集まりで、光の筋が通ると組み上がり、突き破られると霧になって広がる(空のイメージ)
};

// ステージ名を、ワールドに置いて描くクラス。
// 板の位置・向き・出現の進行度を受け取って描くだけで、いつ出すか(演出の進行)は知らない。出す時間はシーンが決める。
// 板はカメラの進行方向の逆を向く(上下は傾けない)ので、後ろのカメラから、文字が正しい向きで読める。
// 見せ方は StageNameStyle で切り替える(板と破片、または雲)。
class StageNameBoard {
public:
  // 板のモデルと、ステージ名の画像を読み込む。
  // modelPath: 板のモデル(四角形 1 枚)のパス。texturePath: ステージ名の画像(透明背景の PNG)のパス
  // puffTexturePath: 雲のパフの画像のパス(雲の見せ方を使わないなら nullptr でよい)
  // 戻り値: 読み込みに成功したら true(失敗したら描かない)
  bool Initialize(const char *modelPath, const char *texturePath, const char *puffTexturePath = nullptr);

  // 見せ方を選ぶ(BuildFracture より前に呼ぶ)。
  // titleCloud: 雲の見せ方での、メイン文字の設定。subtitleCloud: サブタイトルの設定(MakeSubtitleCloudSettings)。
  // 板と破片の見せ方では、どちらも使わない
  void SetStyle(StageNameStyle style, const StageCloudSettings &titleCloud, const StageCloudSettings &subtitleCloud);

  // 板を置く。板の「衝撃点」(BuildFracture で渡した点)が、anchor に重なるように置く。
  // 自機が anchor を通れば、ちょうど衝撃点を突き破る。BuildFracture のあとに呼ぶこと(衝撃点を使うため)
  // anchor: 自機が板に触れる位置(ワールド)。forward: 進行方向(長さ 1 でなくてもよい。水平な向きだけを使う)
  void Place(const Vector3 &anchor, const Vector3 &forward, const StageNameBoardSettings &settings);

  // 壊れるときの準備をする(まだ割らない)。重いので、シーンを作るときに呼ぶ。
  // 板と破片の見せ方: 衝撃点を中心にボロノイ分割して、領域と破片のメッシュを作る。
  // 雲の見せ方: 画像から、文字の形にパフを置く(fracture と fragment は使わない)。
  // impact: 衝撃点。板の中の座標(板の中心が (0, 0)、横が -1 〜 1、縦が -0.25 〜 0.25)
  // board: 板の大きさ(破片・パフの大きさを板と合わせるために使う)。fracture: 割れ方。fragment: 破片の厚みや動き
  // 戻り値: 準備できたら true
  bool BuildFracture(const Vector2 &impact, const StageNameBoardSettings &board, const FractureSettings &fracture,
                     const FragmentSettings &fragment);

  // 板を割る。板は破片に入れ替わり、衝撃点から外へ、進行方向へ飛んでいく(BuildFracture のあと。1 回だけ)
  void Break();

  // 時間を進める(破片の動き)。割れていないときは何もしない。毎フレーム呼ぶ
  void Update(float deltaTime);

  // 割れていたら true
  bool IsBroken() const { return broken_; }

  // 衝撃点から、板の上端までの高さ(ワールドのユニット)。板が画面の上端を超えないよう、弧の高さを抑えるのに使う。
  // BuildFracture のあとに呼ぶこと(衝撃点を使うため)
  float HeightAboveImpact(const StageNameBoardSettings &settings) const;

  // 分割の領域の輪郭を、板の上に線で描く(分割の確認用。透明度が 0 のときと、割れたあとは描かない)
  void DrawDebugCells(Renderer *renderer) const;

  // 出現の度合い(0 = 見えない、1 = 出切った)。
  // 板の見せ方では、板の透明度。雲の見せ方では、光の筋が左から右へ通る進行度(筋が通った所から、雲が組み上がる)
  void SetAlpha(float alpha) { alpha_ = alpha; }

  // 描く。読み込みに失敗している、または出現の度合いが 0 以下のときは何もしない
  void Draw(Renderer *renderer);

private:
  ModelInstance model_;
  std::shared_ptr<TextureResource> texture_;    // ステージ名の画像(破片にも同じものを貼る)
  std::shared_ptr<TextureResource> puffTexture_; // 雲のパフの画像
  std::string texturePath_;                      // ステージ名の画像のパス(雲の文字の形を読み取るのに使う)
  StageNameStyle style_ = StageNameStyle::Shards;
  StageCloudSettings titleCloudSettings_;        // 雲の見せ方での、メイン文字の設定
  StageCloudSettings subtitleCloudSettings_;     // 雲の見せ方での、サブタイトルの設定
  std::vector<VoronoiCell> cells_;               // ボロノイ分割の領域(板の中の座標)
  StageNameFragments fragments_;                 // 割れたあとの破片
  StageNameClouds clouds_;                       // 雲の文字(メイン文字)
  StageNameClouds subtitleClouds_;               // 雲の文字(サブタイトル)
  Vector2 impact_{0.0f, 0.0f};                   // 衝撃点(板の中の座標)
  Matrix4x4 frame_ = MakeIdentity4x4();          // 板の向きの座標をワールドへ変える行列(単位ベクトルの軸。破片・雲の描画に使う)
  bool broken_ = false;
  bool ready_ = false;
  float alpha_ = 0.0f;
};
