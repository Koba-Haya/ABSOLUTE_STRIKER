#include "StageNameBoard.h"

#include "BasisMatrix.h"

#include "AbsoluteEngine/resources/AssetManager.h"
#include "ModelResource.h"
#include "Renderer.h"
#include "TextureResource.h"

#include <algorithm>

namespace {
// ライティングモード 0: ライトの影響を受けず、テクスチャの色をそのまま出す(タイトルのロゴと同じ)
constexpr int32_t kUnlitMode = 0;
// 板のモデル(四角形)の横幅。モデルは x が -1 〜 1 なので 2。スケールを求めるときの基準にする
constexpr float kBoardModelWidth = 2.0f;
// 板のモデル(stage_name_board.obj)の大きさ。x が -1 〜 1、y が -0.25 〜 0.25 の四角形(画像は 4:1)
constexpr float kBoardModelHalfHeight = 0.25f;
// 確認用の線を、板の手前(カメラ側)へ浮かせる距離。板と同じ深さだと、線が板に埋もれて見えにくい
constexpr float kDebugLineLift = 0.02f;
// 確認用の線の色(黄色)
constexpr Vector4 kDebugLineColor{1.0f, 0.9f, 0.1f, 1.0f};
// 進行方向の水平成分が、これより小さいときは向きが決められないので、ワールドの前(+z)を使う
constexpr float kMinHorizontalLength = 1e-4f;

} // namespace

bool StageNameBoard::Initialize(const char *modelPath, const char *texturePath, const char *puffTexturePath) {
  auto *assets = AbsoluteEngine::AssetManager::GetInstance();

  auto resource = assets->Load<ModelResource>(modelPath);
  if (!resource) {
    return false;
  }
  ModelInstance::CreateInfo info{};
  info.resource = resource;
  info.lightingMode = kUnlitMode;
  if (!model_.Initialize(info)) {
    return false;
  }

  // 板のモデルが指す画像ではなく、ステージごとの画像で差し替える
  auto texture = assets->Load<TextureResource>(texturePath);
  if (!texture) {
    return false;
  }
  model_.SetOverrideTexture(texture);
  texture_ = texture;
  texturePath_ = texturePath;

  // 雲のパフの画像(雲の見せ方を使わないときは、読み込まない)
  if (puffTexturePath != nullptr) {
    puffTexture_ = assets->Load<TextureResource>(puffTexturePath);
  }

  ready_ = true;
  return true;
}

void StageNameBoard::SetStyle(StageNameStyle style, const StageCloudSettings &titleCloud, const StageCloudSettings &subtitleCloud) {
  style_ = style;
  titleCloudSettings_ = titleCloud;
  subtitleCloudSettings_ = subtitleCloud;
}

void StageNameBoard::Place(const Vector3 &anchor, const Vector3 &forward, const StageNameBoardSettings &settings) {
  if (!ready_) {
    return;
  }

  // 板は垂直に立てる。進行方向から上下の成分を取り除き(水平にし)、向きだけを使う
  Vector3 flatForward{forward.x, 0.0f, forward.z};
  flatForward = Length(flatForward) > kMinHorizontalLength ? Normalize(flatForward) : Vector3{0.0f, 0.0f, 1.0f};

  const Vector3 worldUp{0.0f, 1.0f, 0.0f};
  const Vector3 right = Normalize(Cross(worldUp, flatForward)); // 左手系: 上 x 前 = 右
  const Vector3 up = Cross(flatForward, right);

  // 横幅が settings.width になるよう、モデルの半幅(1.0)を拡大する。縦は横と同じ比率で拡大される
  const float halfWidth = settings.width / kBoardModelWidth;
  // 衝撃点(板の中の座標)が anchor に重なるよう、板の中心を、衝撃点の分だけ右と上へ戻す。
  // 板の外へ広がる向き(右・上)の長さは、ワールドでは板の中の座標 x halfWidth
  const Vector3 position{anchor.x - right.x * impact_.x * halfWidth - up.x * impact_.y * halfWidth,
                         anchor.y - right.y * impact_.x * halfWidth - up.y * impact_.y * halfWidth,
                         anchor.z - right.z * impact_.x * halfWidth - up.z * impact_.y * halfWidth};
  model_.SetWorld(MakeBasisMatrix({right.x * halfWidth, right.y * halfWidth, right.z * halfWidth},
                                  {up.x * halfWidth, up.y * halfWidth, up.z * halfWidth}, flatForward, position));
  // 破片・雲は、板の中の座標をワールドの長さにそろえて持っている(拡大は破片・雲の側で済んでいる)ので、軸は単位ベクトルにする
  frame_ = MakeBasisMatrix(right, up, flatForward, position);
}

bool StageNameBoard::BuildFracture(const Vector2 &impact, const StageNameBoardSettings &board, const FractureSettings &fracture,
                                   const FragmentSettings &fragment) {
  if (!ready_) {
    return false;
  }
  const float halfWidth = kBoardModelWidth * 0.5f;
  const FractureRect rect{{-halfWidth, -kBoardModelHalfHeight}, {halfWidth, kBoardModelHalfHeight}};
  impact_ = impact;
  broken_ = false;

  // 板の中の座標 1 が、ワールドで何ユニットか(横幅 / 板の中の横幅)
  const float worldScale = board.width / kBoardModelWidth;

  if (style_ == StageNameStyle::Clouds) {
    // 雲: 画像から文字の形を読み取り、パフを置く。衝撃点も、ワールドの長さ(板の中の座標 x 倍率)で渡す。
    // メイン文字(画像の上の部分)とサブタイトル(下の部分)を、別々の雲にする。
    // 光の筋の位置は、メイン文字の横の範囲にそろえる(サブタイトルも、筋が通った所からできる)
    cells_.clear();
    const float scaledHalfWidth = halfWidth * worldScale;
    const float scaledHalfHeight = kBoardModelHalfHeight * worldScale;
    const Vector2 scaledImpact{impact.x * worldScale, impact.y * worldScale};
    const bool titleBuilt = clouds_.Build(texturePath_.c_str(), puffTexture_, scaledHalfWidth, scaledHalfHeight, scaledImpact,
                                          titleCloudSettings_);
    const Vector2 sweepRange = clouds_.GetTextRangeX();
    const bool subtitleBuilt = subtitleClouds_.Build(texturePath_.c_str(), puffTexture_, scaledHalfWidth, scaledHalfHeight,
                                                     scaledImpact, subtitleCloudSettings_, &sweepRange);
    return titleBuilt && subtitleBuilt;
  }

  cells_ = VoronoiFracture::Build(rect, VoronoiFracture::GenerateSeeds(rect, impact, fracture));
  return fragments_.Build(cells_, rect, worldScale, texture_, fragment);
}

void StageNameBoard::Break() {
  if (!ready_ || broken_) {
    return;
  }
  if (style_ == StageNameStyle::Clouds) {
    if (!clouds_.IsBuilt()) {
      return;
    }
    clouds_.Break(frame_);
    subtitleClouds_.Break(frame_);
  } else {
    if (!fragments_.IsBuilt()) {
      return;
    }
    fragments_.Launch(impact_);
  }
  broken_ = true;
}

float StageNameBoard::HeightAboveImpact(const StageNameBoardSettings &settings) const {
  // 板の中の座標(板の上端は y = 半分の高さ)から、衝撃点の y を引き、ワールドの長さへ直す
  const float worldScale = settings.width / kBoardModelWidth;
  return (kBoardModelHalfHeight - impact_.y) * worldScale;
}

void StageNameBoard::Update(float deltaTime) {
  // 雲は、組み上がったあとの揺れのために、壊れる前も時間を進める
  clouds_.Update(deltaTime);
  subtitleClouds_.Update(deltaTime);
  if (broken_) {
    fragments_.Update(deltaTime);
  }
}

void StageNameBoard::DrawDebugCells(Renderer *renderer) const {
  if (!ready_ || broken_ || alpha_ <= 0.0f || style_ != StageNameStyle::Shards) {
    return;
  }
  // 板の中の座標 (x, y) を、板の行列で、ワールドの座標へ変える。
  // 行列の行は (右, 上, 前, 位置)。手前(前の逆向き)へ少し浮かせて、板と同じ深さにならないようにする
  const Matrix4x4 &world = model_.GetWorld();
  auto toWorld = [&](const Vector2 &p) -> Vector3 {
    return {world.m[0][0] * p.x + world.m[1][0] * p.y - world.m[2][0] * kDebugLineLift + world.m[3][0],
            world.m[0][1] * p.x + world.m[1][1] * p.y - world.m[2][1] * kDebugLineLift + world.m[3][1],
            world.m[0][2] * p.x + world.m[1][2] * p.y - world.m[2][2] * kDebugLineLift + world.m[3][2]};
  };

  for (const VoronoiCell &cell : cells_) {
    const size_t count = cell.polygon.size();
    for (size_t i = 0; i < count; ++i) {
      renderer->DrawLine(toWorld(cell.polygon[i]), toWorld(cell.polygon[(i + 1) % count]), kDebugLineColor);
    }
  }
}

void StageNameBoard::Draw(Renderer *renderer) {
  if (!ready_) {
    return;
  }

  if (style_ == StageNameStyle::Clouds) {
    // 出現の度合い(alpha_)を、光の筋の進行度として雲へ渡す(メイン文字もサブタイトルも、同じ筋が通る)。
    // 壊れたあと、パフ本体はすぐ消え、霧はパーティクルが描く
    clouds_.SetReveal(alpha_);
    subtitleClouds_.SetReveal(alpha_);
    clouds_.Draw(renderer, frame_);
    subtitleClouds_.Draw(renderer, frame_);
    return;
  }

  // 割れたあとは、板の代わりに破片を描く(破片は、自分の寿命で薄くなって消える)
  if (broken_) {
    fragments_.Draw(renderer, frame_);
    return;
  }
  if (alpha_ <= 0.0f) {
    return;
  }
  model_.SetColor({1.0f, 1.0f, 1.0f, alpha_});
  // 深度を書き込む。雲(深度を見て後から描かれる)が、板の手前にあるときだけ板に重なり、奥にあるときは板に隠れる
  renderer->DrawEffectModel(&model_, true);
}
