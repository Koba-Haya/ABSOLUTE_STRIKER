#pragma once
#include "ScaleDust.h"
#include "Vector.h"

class WingShatter;
struct PlayerDeathDissolve;

// 死亡の崩壊の合図を、鱗粉(ScaleDust)の発生へ橋渡しするクラス。
//
// 分けた理由: ScaleDust は GameOver の雲の中でも使い回すので、「破片の解放」「ディゾルブの縁が進む」といった
// 死亡演出に固有の知識は持たせない。この層が、死亡演出の合図を読んで、ScaleDust へ「どこから、どれだけ」を渡す。
// GameScene は、毎フレーム Update を 1 回呼ぶだけでよい。
//
// 鱗粉が出る場所(引き継ぎ書 12.4):
//   - 破片が解放された瞬間: 破片の位置から、破片の大きさに応じた量が、飛んでいく向きへ少し押し出されて出る
//   - 朽ちる縁: ディゾルブが進んでいる間、蝶の翅のまわりから、続けて出る(進みの半ばで最も多く、始めと終わりは少ない)

namespace PlayerDeathDustDefaults {
constexpr uint32_t kShardBurstBase = 6;          // 破片 1 つあたりの最小の個数
constexpr float kShardBurstPerTriangle = 0.12f;  // 破片の三角形 1 つあたりの追加の個数(大きい破片ほど、多く出る)
constexpr uint32_t kShardBurstMax = 40;          // 破片 1 つあたりの個数の上限(プールを使い切らないため)
constexpr float kShardBurstRadius = 0.35f;       // 破片の位置のまわりに散らばる半径
constexpr float kShardKickSpeed = 1.2f;          // 破片の飛ぶ向きへ、鱗粉に足す速さ(破片に置いていかれない程度の尾)
constexpr float kEdgeRatePeak = 45.0f;           // 朽ちる縁から続けて出る量の最大(個 / 秒)
constexpr float kBodyRadius = 1.8f;              // 蝶の翅の半分の長さ(翼幅 3.7 の半分に近い値)。縁の発生源の球の最大の半径
constexpr float kEdgeShrink = 0.7f;              // ディゾルブが進むと、縁が内側へ寄るので、発生源の球を縮める割合(0 〜 1)
} // namespace PlayerDeathDustDefaults

struct PlayerDeathDustSettings {
  uint32_t shardBurstBase = PlayerDeathDustDefaults::kShardBurstBase;
  float shardBurstPerTriangle = PlayerDeathDustDefaults::kShardBurstPerTriangle;
  uint32_t shardBurstMax = PlayerDeathDustDefaults::kShardBurstMax;
  float shardBurstRadius = PlayerDeathDustDefaults::kShardBurstRadius;
  float shardKickSpeed = PlayerDeathDustDefaults::kShardKickSpeed;
  float edgeRatePeak = PlayerDeathDustDefaults::kEdgeRatePeak;
  float bodyRadius = PlayerDeathDustDefaults::kBodyRadius;
  float edgeShrink = PlayerDeathDustDefaults::kEdgeShrink;
};

class PlayerDeathDust {
public:
  // 毎フレーム 1 回、描画の前に呼ぶ(死亡演出が始まっていないフレームも呼ぶ。何も出さないが、残っている粒は動き続ける)。
  // active: 死亡演出が進行中か。dissolve: 蝶の今のディゾルブの状態。shatter: 破片(このフレームの解放を読む)。
  // bodyPosition / bodyVelocity: 蝶の本体の位置と速度(ワールド)
  void Update(bool active, const PlayerDeathDissolve& dissolve, const WingShatter& shatter, const Vector3& bodyPosition,
              const Vector3& bodyVelocity);

  // シーンの終わりに呼ぶ(借りているエミッタとフィールドを返す)
  void Shutdown();

  // 調整用(ImGui が書き換える)。発生の量・場所の設定と、鱗粉そのものの設定
  PlayerDeathDustSettings& GetSettings() { return settings_; }
  ScaleDust& GetDust() { return dust_; }

private:
  ScaleDust dust_;
  PlayerDeathDustSettings settings_;
};
