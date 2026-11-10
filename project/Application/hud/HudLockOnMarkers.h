#pragma once
#include "../ui/UiRectRenderer.h"
#include "Vector.h"
#include <vector>

/// <summary>
/// ロックオンモードの間だけ出す、範囲の輪と、捕まえた敵の角のカッコ。
/// - 輪: カーソルの周りの細い円。半径は、ロックオンの判定の半径と同じ値を、呼び出し側が渡す(判定と表示がずれないように)。
///   上限に達したら、輪の色が橙に変わる。
/// - カッコ: 捕まえた敵の画面上の位置に、四隅の L 字を出す。矢印、数字、予測線は使わない。
/// 輪は、四角の塗り(UiRectRenderer)で、1 ピクセルの高さの行ごとに、輪の断面を塗って作る(画像は使わない)。
/// 分けた理由: 「照準まわりの図形」は、バーや枠の表示とは別の責務で、値は Draw の引数で受け取るだけにするため。
/// </summary>
class HudLockOnMarkers {
public:
  HudLockOnMarkers() = default;
  ~HudLockOnMarkers() = default;

  /// <summary>白い画像を読む</summary>
  void Initialize();

  /// <summary>描く。lockingMode が false のときは何も描かない(普段は出さない)</summary>
  /// <param name="lockingMode">ロックオンモード(長押し中)か</param>
  /// <param name="cursorPos">カーソルの画面座標(輪の中心)</param>
  /// <param name="radius">輪の半径(画面ピクセル)</param>
  /// <param name="targets">捕まえた敵の画面座標(カッコの中心)</param>
  /// <param name="isFull">捕まえている数が上限に達しているか(色を変える)</param>
  void Draw(bool lockingMode, const Vector2 &cursorPos, float radius, const std::vector<Vector2> &targets, bool isFull);

private:
  /// 輪の長方形を作る(行ごとの断面)。rects_ に入る
  void BuildRing_(const Vector2 &center, float radius);
  /// 1 体ぶんのカッコの長方形(8 個)を、rects_ の後ろに足す
  void AddBrackets_(const Vector2 &center);

  UiRectRenderer renderer_;
  std::vector<UiRect> rects_; // 作業用(毎フレーム作り直し、領域は使い回す)
};
