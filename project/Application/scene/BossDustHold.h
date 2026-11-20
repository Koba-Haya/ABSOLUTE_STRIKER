#pragma once
#include "ClearDirector.h"
#include "Type/Vector.h"

// ボス撃破の演出で、崩壊(Peel)のあいだ出つづける粒を、爆破の球の中に溜めて、爆散(Burst)で解き放つ「力」を決めるクラス。
//
// 分けた理由: 粒の見た目(BossDefeatVisual の EmitCellDust_)と、粒を動かす力(ここ)は別の責務。
// 力は、GPU パーティクルの共有の「力の枠(フィールド)」を使うので、枠の割り当てと、いつ入れて、いつ外すかを、このクラスに集める。
//
// 仕組み(粒は、GPU 上で、力の合計で動く):
//   Peel  : 溜める。中心へ引くばね(Spring)と、上向きの軸まわりの渦(Vortex)と、空気抵抗(Drag)で、粒が、球の中を回りながら、中心のまわりに留まる
//   Burst : 解き放つ。始めの短い間だけ、中心から外向きに押す力(負の強さの Attractor)を入れる。そのあと、ゆるい空気抵抗だけにして、外へ漂う
//   Flow / Release : 目的地(次の島)があるときは、粒を、目的地へ引く(一定の力 + 弱いばね + 島へ向かう軸まわりの渦 + 空気抵抗)。
//                    引く力が一定なので、島に近い粒から先に着き、進行方向の列になる。渦で、らせん状に進む。目的地が無いときは、ゆるい空気抵抗だけ
//   それ以外 : 力なし(解放のあとの粒は、Clear が呼ばれるまで、ゆるい空気抵抗だけが残る)
// 力の枠: 0 〜 4 は ScaleDust(重力、風、ゆらぎ、空気抵抗、引き寄せ、渦)が毎フレーム書く。重ならないよう、5 〜 9 を使う。
// 注意: 力は、全ての粒にかかる(枠が共有のため)。撃破演出の間は、ほかの粒を出さない前提。
// 使い方: Update(毎フレーム) -> 演出が終わったら(シーンの終わり、次の演出の前) Clear

namespace BossDustHoldDefaults {
// 力の枠(GPUParticleManager::SetField の slot)
constexpr int kSlotSpring = 5;
constexpr int kSlotSwirl = 6;
constexpr int kSlotDrag = 7;
constexpr int kSlotRelease = 8;
// 溜める(Peel)
constexpr float kHoldSpring = 2.5f;   // 中心へ引くばね定数(1 / 秒^2)。大きいほど、中心へ強く集まる
constexpr float kHoldSwirl = 5.0f;    // 渦の力(加速度)。大きいほど、速く回る
constexpr float kHoldDrag = 1.2f;     // 溜めている間の空気抵抗(1 秒あたりの減衰率)。小さいと、中心を行き過ぎて大きく揺れる
// 解き放つ(Burst)
constexpr float kReleaseShare = 0.7f;      // Burst の始めの、外向きに押す時間の割合(0 〜 1)
constexpr float kReleaseStrength = 30.0f;  // 外向きに押す力(加速度)。大きいほど、速く、遠くへ飛ぶ
constexpr float kCoastDrag = 0.8f;         // 解き放ったあとの空気抵抗(1 秒あたりの減衰率)
// 次の島へ飛ぶ(Flow、Release)。粒が、ひと塊にならず、列をなして、らせんを描いて飛ぶようにする。
//   一定の力で引く(Attractor): 引く力が距離によらないので、島に近い粒から先に着く。出発の位置のちがいが、進行方向の列の長さになる
//   弱いばねを足す: 目的地の近くで、引きが少し強まり、粒が目的地のまわりに留まりやすくなる(遠くでは、一定の力が主)
//   島へ向かう軸まわりの渦: 粒が、らせんを描いて進む
// 空気抵抗と引く力で、終端速度(引く力 / 空気抵抗)が決まる。約 2.5 秒で、約 100 の距離を進めるよう、約 45 に近い値にしてある
constexpr float kFlightPull = 60.0f;       // 目的地へ引く、一定の力(加速度)
constexpr float kFlightSpring = 0.4f;      // 目的地へ引くばね定数(1 / 秒^2)。小さくして、一定の力を主にする
constexpr float kFlightSwirl = 20.0f;      // 島へ向かう軸まわりの渦の力(加速度)。大きいほど、らせんの巻きが強い
constexpr float kFlightDrag = 2.2f;        // 飛んでいる間の空気抵抗(1 秒あたりの減衰率)
} // namespace BossDustHoldDefaults

struct BossDustHoldSettings {
  float holdSpring = BossDustHoldDefaults::kHoldSpring;
  float holdSwirl = BossDustHoldDefaults::kHoldSwirl;
  float holdDrag = BossDustHoldDefaults::kHoldDrag;
  float releaseShare = BossDustHoldDefaults::kReleaseShare;
  float releaseStrength = BossDustHoldDefaults::kReleaseStrength;
  float coastDrag = BossDustHoldDefaults::kCoastDrag;
  float flightPull = BossDustHoldDefaults::kFlightPull;
  float flightSpring = BossDustHoldDefaults::kFlightSpring;
  float flightSwirl = BossDustHoldDefaults::kFlightSwirl;
  float flightDrag = BossDustHoldDefaults::kFlightDrag;
};

class BossDustHold {
public:
  // 設定を受け取り、使う枠を空にする(演出の開始時に呼ぶ)
  void Begin(const BossDustHoldSettings &settings);

  // いまのサブフェーズと進み具合(0 〜 1)から、力を決める。毎フレーム呼ぶ。
  // center: 溜める中心(爆破の球の中心。体の動きに付いてくる)
  // flightTarget: 次の島へ飛ばすときの目的地(ワールド)。nullptr なら、飛ばさない(解き放ったあと、ゆるく漂うだけ)
  void Update(ClearPhase phase, float phaseProgress, const Vector3 &center, const Vector3 *flightTarget);

  // 使っている枠を全て空にする(演出の終わり、シーンの終わり、次の演出の前)
  void Clear();

private:
  BossDustHoldSettings settings_;
};
