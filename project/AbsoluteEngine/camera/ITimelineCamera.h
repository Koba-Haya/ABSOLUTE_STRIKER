#pragma once
#include "../Type/Vector.h"

namespace AbsoluteEngine {

// レール上のある距離における座標系(フレーム)。
// 「レール座標」(距離 s, 右へ lateral, 上へ vertical)を世界座標へ変換するときの土台になる。
//   世界座標 = position + right * lateral + up * vertical
// 全て左手系のワールド座標で、forward / right / up は長さ1の直交基底。
struct RailFrame {
    Vector3 position; // レール上の位置
    Vector3 forward;  // レールの進行方向(接線)
    Vector3 right;    // 道の右方向
    Vector3 up;       // 道の上方向
};

// タイムラインが「レールに沿って進行するカメラ」を操作するための最小インターフェース。
// 具体的なレール実装（ウェイポイント方式か等）はApplication層に委ねることで、
// エンジン(TimelineManager/BaseScene)がApplication層の具象クラスに依存しないようにする。
class ITimelineCamera {
public:
    virtual ~ITimelineCamera() = default;

    // レールを最初から最後まで等速で走破するのに必要な秒数を返す
    virtual float GetDuration() const = 0;

    // レール上の指定progress(0-1)における位置・前方向ベクトルを計算する（読み取り専用、カメラは動かさない）
    virtual void GetPointAndForward(float progress, Vector3& outPos, Vector3& outForward) const = 0;

    // レール全体の弧長(実距離、ユニット)を返す。
    // 距離と進行度(0-1)の変換(progress * GetTotalLength())に使う
    virtual float GetTotalLength() const = 0;

    // レール上の「距離 s(弧長、ユニット)」から、位置・前・右・上を返す(読み取り専用)。
    // GetPointAndForwardと違い、範囲をクランプしない:
    //   s < 0 のとき      : 始点から、始点の接線方向の逆へ直線で延長する
    //   s > 全長のとき    : 終点から、終点の接線方向へ直線で延長する
    // 始点より前・終点より先に物を置く用途(島の外側の敵など)のための関数
    virtual RailFrame GetRailFrame(float distance) const = 0;

    // タイムラインからカメラ進行度(0-1)を直接設定する
    virtual void SetProgress(float progress) = 0;
};

} // namespace AbsoluteEngine
