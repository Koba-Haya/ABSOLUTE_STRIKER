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
    std::vector<Vector3>& GetWaypointsRef() { return waypoints_; }
    
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
    void SetLookAhead(float offset) { lookAheadOffset_ = offset; }
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

    // レール全体の弧長（実距離）を取得する
    float GetTotalLength() const { return arcLengthTable_.GetTotalLength(); }

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

    // カメラ位置の計算とSet処理を共通化したヘルパー（UpdateとSetProgressから呼ばれる）
    void ApplyCameraTransform_();

    // ウェイポイントが変更された際に弧長ルックアップテーブルを再構築する
    void RebuildArcLengthTable_();

    std::vector<Vector3> waypoints_;
    Spline::ArcLengthTable arcLengthTable_;
    float progress_ = 0.0f;
    float speed_ = 10.0f;
    float lookAheadOffset_ = 0.01f;

    // エディタ操作用
    int selectedPointIndex_ = -1;
    bool isGizmoUsing_ = false;
    bool isModified_ = false;

    std::vector<Vector3> waypointsBeforeEdit_;
};
