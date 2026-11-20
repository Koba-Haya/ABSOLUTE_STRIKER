#pragma once
#include <cstdint>
#include <memory>
#include <vector>

#include "AbsoluteEngine/scene/GameObject.h"
#include "BossShardInfo.h"
#include "ClearDirector.h"
#include "Matrix.h"
#include "Type/Vector.h"

// ボス撃破の演出の、「ボスが割れて、欠片が飛び散り、鱗粉へ縮む」動きを決めるクラス。
// 責務は「欠片ごとの位置・回転・大きさを計算して、欠片のモデルの骨へ書き込むこと」だけ。
// 時間の枠は ClearDirector、ボスの見た目全体のまとめは BossDefeatVisual、鱗粉の粒は ScaleDust が担当する。
//
// 仕組み: 欠片のモデルは、1 つのスキンメッシュで、1 つの欠片に 1 本の骨が付いている(ボスの割れた状態を、基準ポーズで組み立てたもの)。
// このクラスは、毎フレーム、各骨の変換(位置・回転・大きさ)を直接書き換えて、骨の行列とスキニングを更新する(アニメーションは使わない)。
//   欠片の中心から外へ、骨が動く。骨の原点が欠片の重心なので、回転は欠片の重心まわりになる。
//
// サブフェーズごとの動き:
//   HitStop : 組み立てた状態のまま(ボスと同じ見た目)
//   Crack   : 継ぎ目が、コアに近い欠片から順に、少し開く。欠片は細かく震える。継ぎ目の隙間から、内側のコアの光が漏れて見える
//   Peel    : 外側の欠片から順に、解放される(外へ飛び、回転し、重力で落ちる)
//   Burst   : 残りの欠片をすべて、強く弾き飛ばす
//   Flow    : 欠片が縮んで、鱗粉になって消える(鱗粉の粒は、別のクラスが出す)
//   Cover   : すべて消えた状態(スキップで、直接ここへ来ても、見た目が揃うようにする)

// 動きの設定(ボスの大きさや演出の好みで変わりうる値なので、コードに埋め込まず、まとめて持つ)
namespace BossShatterDefaults {
// --- ひび(Crack) ---
constexpr float kGapDistance = 0.18f;     // 継ぎ目の、最大の開き(ワールド単位。欠片が、コアから外へ動く距離)
constexpr float kCrackDelaySpread = 0.5f; // 外側の欠片ほど、開き始めが遅れる割合(Crack の進み具合の 0 〜 この値)
constexpr float kShakeAmplitude = 0.05f;  // 欠片の震えの大きさ(ワールド単位)
constexpr float kShakeFrequency = 38.0f;  // 欠片の震えの速さ(ラジアン / 秒)
// --- 剥がれ(Peel)と破裂(Burst) ---
constexpr float kReleaseSpread = 0.85f;   // 外側から内側へ、欠片が解放されていく範囲(Peel の進み具合の 0 〜 この値)
constexpr float kReleaseJitter = 0.15f;   // 解放の時刻の、欠片ごとのばらつき(Peel の進み具合)
constexpr float kLaunchSpeedMin = 1.5f;   // 解放されたときの、外へ向かう速さ(ワールド単位 / 秒)の範囲
constexpr float kLaunchSpeedMax = 4.0f;
constexpr float kUpwardBias = 1.2f;       // 解放されたときに足す、上向きの速さ(破片が舞い上がって見えるように)
constexpr float kBurstSpeedBoost = 2.5f;  // 破裂(Burst)で解放される欠片の、速さの倍率
constexpr float kGravity = 2.0f;          // 下向きの加速度(軽く舞うので、弱くする)
constexpr float kDragPerSecond = 0.6f;    // 空気抵抗(1 秒あたりの速度の減衰率)
constexpr float kAngularSpeedMin = 1.0f;  // 回転の速さ(ラジアン / 秒)の範囲
constexpr float kAngularSpeedMax = 4.0f;
// --- 流れ(Flow) ---
constexpr float kShrinkPower = 1.5f;      // 欠片の縮み方。大きさ = 1 - (進み具合 ^ この値)。大きいほど、終わり際に急に縮む
constexpr float kMinScale = 0.001f;       // 欠片の大きさの下限。0 にすると、骨の行列が潰れて(逆行列が求まらず)、描画が壊れる
// --- 乱数 ---
constexpr uint32_t kRandomSeed = 0x5EED1234u; // 欠片ごとのばらつきの種。固定して、毎回同じ割れ方にする(調整しやすくするため)
} // namespace BossShatterDefaults

struct BossShatterSettings {
  float gapDistance = BossShatterDefaults::kGapDistance;
  float crackDelaySpread = BossShatterDefaults::kCrackDelaySpread;
  float shakeAmplitude = BossShatterDefaults::kShakeAmplitude;
  float shakeFrequency = BossShatterDefaults::kShakeFrequency;
  float releaseSpread = BossShatterDefaults::kReleaseSpread;
  float releaseJitter = BossShatterDefaults::kReleaseJitter;
  float launchSpeedMin = BossShatterDefaults::kLaunchSpeedMin;
  float launchSpeedMax = BossShatterDefaults::kLaunchSpeedMax;
  float upwardBias = BossShatterDefaults::kUpwardBias;
  float burstSpeedBoost = BossShatterDefaults::kBurstSpeedBoost;
  float gravity = BossShatterDefaults::kGravity;
  float dragPerSecond = BossShatterDefaults::kDragPerSecond;
  float angularSpeedMin = BossShatterDefaults::kAngularSpeedMin;
  float angularSpeedMax = BossShatterDefaults::kAngularSpeedMax;
  float shrinkPower = BossShatterDefaults::kShrinkPower;
};

class BossShatter {
public:
  // 動きを始める準備をする。shardsObject: 欠片のモデル(ModelComponent を持ち、骨が欠片の数だけある)のオブジェクト。
  // 位置・向き・大きさは、ボスと同じにしておくこと(欠片の中心の、ワールド座標を求めるため)。
  // info: 欠片の付属データ(どの骨が、どの欠片か)。骨が見つからない欠片は、読み飛ばす。
  // 戻り値: 動かせる欠片が 1 つ以上あれば true。呼んだ直後に、組み立てた状態の骨を書き込む(最初の描画で、骨が未設定にならないように)
  bool Begin(std::shared_ptr<AbsoluteEngine::GameObject> shardsObject, const BossShardInfo &info, const BossShatterSettings &settings);

  // いまのサブフェーズと、その中の進み具合(0 〜 1)に合わせて、欠片を動かし、骨へ反映する。毎フレーム呼ぶ。
  // deltaTime: 1 フレームの実時間(秒)。欠片の飛び方を積分するのに使う
  void Update(ClearPhase phase, float phaseProgress, float deltaTime);

  // 動かせる状態か(Begin に成功したあと)
  bool IsActive() const { return !shards_.empty(); }

private:
  // 欠片 1 つ分の状態
  struct Shard {
    // --- 開始時に決まる値(動かない) ---
    int32_t jointIndex = -1;                  // 骨(ジョイント)の番号
    Vector3 restTranslate{0.0f, 0.0f, 0.0f};  // 基準ポーズでの、骨のローカルの位置・回転・大きさ
    Quaternion restRotate{0.0f, 0.0f, 0.0f, 1.0f};
    Vector3 restScale{1.0f, 1.0f, 1.0f};
    Matrix4x4 parentInverse{};                // 骨の親の、骨格空間の行列の逆行列(ワールドの移動量を、骨のローカルの移動量へ直すため)
    Vector3 direction{0.0f, 1.0f, 0.0f};      // コアの中心から、欠片の中心へ向かう向き(ワールド。長さ 1)
    float rank = 0.0f;                        // コアからの距離の順位(0: 最もコアに近い 〜 1: 最も外側)
    float releaseAt = 0.0f;                   // Peel の進み具合が、この値に達したら解放する
    float launchSpeed = 0.0f;                 // 解放されたときの、外へ向かう速さ
    Vector3 shakeAxis{1.0f, 0.0f, 0.0f};     // 震える向き(ワールド。長さ 1)
    float shakePhase = 0.0f;                  // 震えの位相(欠片ごとにずらす)
    Vector3 angularAxis{0.0f, 1.0f, 0.0f};   // 回転の軸(長さ 1)
    float angularSpeed = 0.0f;                // 回転の速さ(ラジアン / 秒)

    // --- 毎フレーム変わる値 ---
    bool released = false;                    // 解放されたか(解放されたあとは、積分で動く)
    Vector3 offset{0.0f, 0.0f, 0.0f};        // 基準の位置からの移動量(ワールド)
    Vector3 velocity{0.0f, 0.0f, 0.0f};      // 解放されたあとの速度(ワールド)
    Quaternion rotation{0.0f, 0.0f, 0.0f, 1.0f}; // 基準からの回転(欠片の重心まわり)
    float scale = 1.0f;                       // 大きさの倍率(1 で元の大きさ)
  };

  // 欠片を解放する(外へ飛ばす速度を与える)。speedBoost: 速さの倍率
  void Release_(Shard &shard, float speedBoost) const;
  // 解放されたあとの欠片を、deltaTime ぶん動かす(重力、空気抵抗、回転)
  void Integrate_(Shard &shard, float deltaTime) const;
  // 解放前の欠片の、移動量(継ぎ目の開きと、震え)を求める
  Vector3 HeldOffset_(const Shard &shard, ClearPhase phase, float progress) const;
  // 欠片の大きさの倍率(Flow で縮み、Cover で最小)
  float ScaleFor_(ClearPhase phase, float progress) const;
  // 全欠片の状態を、骨へ書き込み、骨の行列とスキニングを更新する
  void WritePose_();

  std::shared_ptr<AbsoluteEngine::GameObject> object_;
  std::vector<Shard> shards_;
  Matrix4x4 invWorld_{}; // 欠片のオブジェクトのワールド行列の逆行列(ワールドの移動量を、モデル空間へ直す)
  BossShatterSettings settings_;
  float time_ = 0.0f;    // Begin からの実時間(震えの位相に使う)
};
