#pragma once
#include "../../AbsoluteEngine/scene/Component.h"
#include "../../AbsoluteEngine/Type/Matrix.h"
#include "../../AbsoluteEngine/Type/Vector.h"
#include "../../AbsoluteEngine/math/Spline.h"
#include "../../AbsoluteEngine/camera/ITimelineCamera.h"
#include <vector>
#include <string>

class GameCamera;
class Renderer;

// AbsoluteEngine::ITimelineCameraを実装することで、エンジンのTimelineManager/BaseSceneが
// このApplication固有のクラスを直接知らなくても（インターフェース越しに）レール進行を制御できる
class RailCameraComponent : public AbsoluteEngine::IComponent, public AbsoluteEngine::ITimelineCamera {
public:
    RailCameraComponent();
    ~RailCameraComponent() override = default;

    void Update(float deltaTime) override;

    std::string GetTypeName() const override { return "RailCameraComponent"; }

    void Serialize(nlohmann::json& j) const override;
    void Deserialize(const nlohmann::json& j) override;

    // レールのウェイポイントを設定・取得
    void SetWaypoints(const std::vector<Vector3>& points) { waypoints_ = points; }
    const std::vector<Vector3>& GetWaypoints() const { return waypoints_; }


    // ウェイポイントの編集機能
    void AddWaypoint(const Vector3& pos);
    void InsertWaypoint(size_t index, const Vector3& pos);
    void RemoveWaypoint(size_t index);

    // エディタUI描画
    void DrawInspectorUI() override;
    void DrawGizmo(const Matrix4x4& viewMatrix, const Matrix4x4& projectionMatrix, float windowPosX, float windowPosY, float windowSizeX, float windowSizeY);
    void HandleMousePicking(const Matrix4x4& viewMatrix, const Matrix4x4& projectionMatrix, float windowPosX, float windowPosY, float windowSizeX, float windowSizeY);

    // パラメータ調整用
    // speed_ は秒速ユニット（ワールド座標系での実距離/秒）。弧長ベースで等速移動する
    void SetSpeed(float speed) { speed_ = speed; }
    float GetSpeed() const { return speed_; }
    // 注視点を、現在地からレールに沿って何ユニット先にするか(距離指定)。
    // 進行度(全長に対する割合)ではなく距離にしているのは、レールの全長や曲がり方を変えても、先読みの距離が一定で、
    // 曲がり角でのカメラの向きの追従(曲がる前から曲がる向きを向き始める早さ)が変わらないようにするため
    void SetLookAheadDistance(float distance) { lookAheadDistance_ = distance; }
    float GetLookAheadDistance() const { return lookAheadDistance_; }
    void ResetProgress() { progress_ = 0.0f; }
    // progress_ は弧長（実距離）ベースの進行割合(0-1)。生のスプラインパラメータtとは異なる
    float GetProgress() const { return progress_; }
    // タイムラインからカメラ進行度を直接設定するセッター（シーク同期用）
    // progress_ を書き換えるだけでなく、即座にカメラ位置を再計算して反映する
    // これによりエディットモードでのスライダー操作が即座に画面に反映される
    void SetProgress(float progress) override;

    // レール上の指定progress(0-1)における位置・前方向ベクトルを計算する（読み取り専用、カメラは動かさない）
    // タイムラインエディタでのスポーンイベント初期位置の自動配置などに使う
    void GetPointAndForward(float progress, Vector3& outPos, Vector3& outForward) const override;

    // レール上の距離 s(弧長、ユニット)から、位置・前・右・上を返す(読み取り専用)。
    // 範囲をクランプしない: s < 0 は始点から接線の逆方向へ、s > 全長 は終点から接線方向へ、直線で外挿する。
    // 前方向はレールの接線(カメラの先読み注視点への弦ではない)。曲がっていない区間では両者は一致する。
    // 右・上は前方向と世界の上(0,1,0)から求める。前方向が真上・真下に近く右が定まらないときは、
    // 世界の +X を右とする固定の代替を使う(状態を持たないので、どの s から呼んでも同じ結果になる)。
    AbsoluteEngine::RailFrame GetRailFrame(float distance) const override;

    // 進行度(0-1)と距離(ユニット)の相互変換。レールが無い(全長0)ときは0を返す。範囲はクランプしない
    float ProgressToDistance(float progress) const { return progress * GetTotalLength(); }
    float DistanceToProgress(float distance) const {
        const float length = GetTotalLength();
        return (length > 0.0f) ? (distance / length) : 0.0f;
    }

    // レール全体の弧長（実距離）を取得する
    float GetTotalLength() const override { return arcLengthTable_.GetTotalLength(); }

    // レールを最初から最後まで等速で走破するのに必要な秒数を取得する（TimelineManagerのDuration算出に使用）
    // speed_ が0以下、またはウェイポイントが不足している場合は0を返す
    float GetDuration() const override {
        const float length = GetTotalLength();
        if (speed_ <= 0.0f || length <= 0.0f) return 0.0f;
        return length / speed_;
    }

    // レールの軌道とウェイポイントをデバッグラインで描画する（カメラの背後にある部分は描かない）
    void DrawDebugLine(Renderer* renderer) const;

    // 変更検知用（エディタ連携など）
    bool ConsumeModifiedFlag() {
        bool flag = isModified_;
        isModified_ = false;
        return flag;
    }
    void SetModifiedFlag() { isModified_ = true; }

private:
    // 指定progressにおける位置・注視点（look-ahead込み）を計算する共通ヘルパー
    // ApplyCameraTransform_とGetPointAndForwardの両方から呼ばれる
    void ComputePositionAndTarget_(float progress, Vector3& outPos, Vector3& outTarget) const;

    // 弧長距離 d(0..全長の範囲内)におけるレール上の位置と、接線方向(長さ1)を返す。GetRailFrameの部品
    Vector3 PointAtDistance_(float distance) const;
    Vector3 TangentAtDistance_(float distance) const;

    // カメラ位置の計算とSet処理を共通化したヘルパー（UpdateとSetProgressから呼ばれる）
    void ApplyCameraTransform_();

    // ウェイポイントが変更された際に弧長ルックアップテーブルを再構築する
    void RebuildArcLengthTable_();

    std::vector<Vector3> waypoints_;
    Spline::ArcLengthTable arcLengthTable_;
    float progress_ = 0.0f;
    float speed_ = 10.0f;
    float lookAheadDistance_ = 25.0f; // 注視点までのレール上の距離(ユニット)。コンストラクタで kDefaultLookAheadDistance を入れる

    // エディタ操作用
    int selectedPointIndex_ = -1;
    bool isGizmoUsing_ = false;
    bool isModified_ = false;

    std::vector<Vector3> waypointsBeforeEdit_;
};
