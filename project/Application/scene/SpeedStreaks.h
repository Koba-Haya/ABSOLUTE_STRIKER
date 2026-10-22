#pragma once
#include "Method.h"
#include "ModelInstance.h"
#include <array>
#include <cstdint>

class Renderer;

// 速度線(進行方向に細長く伸びた白い筋)の設定。ステージごとに変えられるよう、データとして持つ。
namespace SpeedStreakDefaults {
// 筋の濃さの最大(0 〜 1)。強さが最大のときの値
constexpr float kMaxAlpha = 0.6f;
// 強さが最大のとき、筋の長さを何倍に伸ばすか(強さ 0 で 1 倍)
constexpr float kMaxStretch = 2.5f;
} // namespace SpeedStreakDefaults

struct SpeedStreakSettings {
  float maxAlpha = SpeedStreakDefaults::kMaxAlpha;
  float maxStretch = SpeedStreakDefaults::kMaxStretch;
};

// 速度線を描くクラス。カメラの前方に、進行方向へ細長い板を散らして、速く進んでいる感じを出す。
// 筋は、ワールドに固定した格子の位置に置く(カメラが進むと、筋が後ろへ流れて見える)。状態を持たず、描くたびに位置を求める。
// 描画の方法は、タイトルシーンの気流の筋と同じ(板の長さ方向を保ったまま、画面へ向ける = 軸まわりのビルボード)
class SpeedStreaks {
public:
  // 筋の板のモデルを読み込む。失敗したら、描かない
  // modelPath: 筋の板のモデル(x・y が -1 〜 1 の四角形。y が長さの方向)
  void Initialize(const char *modelPath);

  // 筋を描く。強さが 0 のときは、何も描かない。
  // eye: 描画するカメラの位置。axis: 筋が伸びる向き(進行方向。長さ 1 でなくてもよい)
  // strength: 強さ(0 〜 1)。濃さと長さに掛かる
  void Draw(Renderer *renderer, const Vector3 &eye, const Vector3 &axis, float strength, const SpeedStreakSettings &settings);

private:
  static constexpr int kCount = 60; // 筋の数
  std::array<ModelInstance, kCount> instances_;
  bool ready_ = false;
};
