#pragma once
#include <cstdint>

// ゲームシーンが使う重い資産を、タイトル表示中に先に読み込んで保持しておくクラス。
//
// 背景: ModelManager / TextureManager のキャッシュは weak_ptr(持ち主がいなくなると切れる参照)で持つ。
//       そのためタイトルシーンが終わって竜のモデルなどを手放すと、ゲームシーンで同じ資産を
//       ディスクから読み直すことになり、遷移の瞬間に約0.5秒の停止が出ていた。
// 対策: ここで shared_ptr(強い参照)を握り続けて、シーンが替わってもキャッシュを切らさない。
//       遷移時の GameScene::Initialize は、キャッシュに当たって短時間で済む。
// 使い方: タイトルの Initialize で Preload()、アプリ終了時(DirectX の破棄より前)に Release() を呼ぶ。
// 注意: 保持した分のメモリを常時使う。読み込みはメインスレッドで同期的に行う(タイトルの起動が少し伸びる)。
namespace GameAssetPaths {
// ゲームシーンとこのクラスで共有する資産のパス。両者で食い違うとキャッシュに当たらないため、ここに集約する
constexpr const char *kPlayerModel = "resources/app/butterfly/butterfly.gltf";
constexpr const char *kSkyboxDds = "resources/app/dds/dds.dds";
constexpr const char *kDissolveNoise = "resources/noise/noise0.png";
constexpr const char *kDefaultParticleTexture = "resources/app/particle/circle.png";
constexpr const char *kExplosionParticleTexture = "resources/explosion/explosion.png";
} // namespace GameAssetPaths

namespace GameParticleGroups {
// ゲームシーンのパーティクルグループの名前と最大数(GameScene と共有する)
constexpr const char *kDefaultName = "default";
constexpr uint32_t kDefaultMax = 500;
constexpr const char *kExplosionName = "explosion";
constexpr uint32_t kExplosionMax = 100;
} // namespace GameParticleGroups

class GameAssetPreloader {
public:
  // 資産を読み込んで保持する。2回目以降は何もしない(読み込み済みのため)。
  // 所要時間は [CpuTimer] GameAssetPreloader.Preload としてログに出る
  static void Preload();

  // 保持している資産をすべて手放す。DirectX の破棄より前に呼ぶこと
  static void Release();
};
