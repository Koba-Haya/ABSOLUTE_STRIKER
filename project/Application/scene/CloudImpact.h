#pragma once
#include "Method.h"
#include "VolumeCloudSettings.h"

// 雲海への「着地の反応」の進行を管理するクラス(ステップ 5)。
// 物体が雲の面へ落ちた瞬間に Trigger し、毎フレーム Update で時間を進める。
// 描画側へは、ApplyToSea で雲海の層の設定(VolumeCloudSettings::impact)へ現在の状態を書き込んで渡す。
// 実際のくぼみと波の描画は、層のシェーダー(VolumeCloud.hlsli の ImpactDisplacement)が行う。
//
// 分けた理由: シーン(GameScene)に時間管理や数値を直接書かず、シーンは「着地した」と伝えるだけにするため。
// 着地点に湧く雲の塊(箱のボリューム雲を使う。5a)の形と時間の調整値。
// 塊は、着地点の上で、小さく濃い所から、膨らみながら立ち上がり、ほどけて消える。蝶が雲に沈むころ(着地の約 0.9 秒後)、蝶の周りに雲がある形にする
struct CloudImpactPuff {
  Vector3 halfSizeStart{2.5f, 1.2f, 2.5f};  // 着地の瞬間の大きさ(箱の半分の大きさ)
  Vector3 halfSizeEnd{7.0f, 4.5f, 7.0f};    // 膨らみきったときの大きさ
  float riseStart = -1.0f;                  // 中心の高さ(着地の高さからの差)の最初の値
  float riseEnd = 3.0f;                     // 立ち上がりきったときの値
  float growSeconds = 1.4f;                 // 膨らみきるまでの秒(だんだん遅くなる動き)
  float appearSeconds = 0.35f;              // 濃くなりきるまでの秒
  float holdSeconds = 1.0f;                 // 濃いまま保つ秒(濃くなりきってから)
  float fadeSeconds = 1.6f;                 // ほどけて消えるまでの秒
  float density = 0.12f;                    // 濃いときの消光係数
  float threshold = 0.5f;                   // 雲になるしきい値(小さいほど塊がつまる)
  float edgeFade = 0.5f;                    // 縁を薄くする幅
};

class CloudImpact {
public:
  // 着地の反応を始める(もう始まっていれば、やり直す)。
  // position: 着地点のワールド座標(y は使わない)、strength: 反応の強さ(1 が標準。くぼみと波の高さに掛かる)
  void Trigger(const Vector3 &position, float strength = 1.0f);

  // 反応を止めて、最初の状態へ戻す(デバッグの蘇生など)
  void Reset();

  // 実時間の 1 フレーム分、時間を進める。反応が終わったら無効になる
  void Update(float deltaTime);

  // 反応が続いているか(Trigger から、くぼみが埋まり、波が消えるまで)
  bool IsActive() const { return active_; }

  // Trigger からの経過秒。続いていなければ 0
  float GetAge() const { return active_ ? age_ : 0.0f; }

  // 雲海の層の設定へ、現在の反応を書き込む(続いていなければ、強さ 0 = 無効にする)
  void ApplyToSea(VolumeCloudSettings &sea) const;

  // 着地点の雲の塊を、箱のボリューム雲の設定として作る(続いていなければ false を返し、out は変えない)。
  // 返した設定を、シーンが Renderer::AddVolumeCloud へ渡す(雲海の層の次に、手前の雲として追加する)
  bool BuildPuff(VolumeCloudSettings &out) const;

  // 塊の調整値(デバッグ UI が編集する)
  CloudImpactPuff &GetPuff() { return puff_; }

  // 形の調整値(くぼみの大きさ、波の速さなど)。デバッグ UI が編集する。強さ、位置、経過秒は内部で管理する
  VolumeCloudImpact &GetShape() { return shape_; }
  const VolumeCloudImpact &GetShape() const { return shape_; }

private:
  // 反応が終わったと見なすまでの、最後の動きが終わってからの余裕(秒)
  static constexpr float kLifetimeMarginSeconds = 0.2f;

  // 反応が続く長さ(秒)。くぼみが埋まる時間、波が消える時間、塊が消える時間のいちばん長いもの + 余裕
  float Lifetime_() const;

  VolumeCloudImpact shape_;
  CloudImpactPuff puff_;
  float centerY_ = 0.0f;
  bool active_ = false;
  float age_ = 0.0f;
  float strength_ = 0.0f;
  float centerX_ = 0.0f;
  float centerZ_ = 0.0f;
};
