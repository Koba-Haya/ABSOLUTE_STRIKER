#pragma once
#include <memory>
#include <vector>
#include "Type/Vector.h"
#include "graphics/2d/Sprite.h"

class Camera;

/// <summary>
/// ゲーム中のヘッドアップディスプレイ（HUD）管理クラス。
/// パンツァードラグーン風3重レティクルを担当。(ロックオンの輪、カッコ、数の枠は HudStatusView が出す)(HP は、文字と数値の表示を担当する HudStatusView のバーが出す)
/// </summary>
class GameHUD {
public:
    GameHUD() = default;
    ~GameHUD() = default;

    void Initialize();

    /// <summary>
    /// 毎フレーム呼び出す更新関数。
    /// </summary>
    /// <param name="isLockingMode">ロックオン長押し中かどうか</param>
    /// <param name="camera">3Dレティクルのスクリーン変換に使うカメラ</param>
    /// <param name="nearWorldPos">近レティクル（距離10）のワールド座標</param>
    /// <param name="midWorldPos">中レティクル（距離30）のワールド座標</param>
    /// <param name="farWorldPos">遠レティクル（距離60）のワールド座標</param>
    void Update(bool isLockingMode,
                Camera* camera,
                const Vector3& nearWorldPos,
                const Vector3& midWorldPos,
                const Vector3& farWorldPos);

    void Draw();

private:
    // -----------------------------------------------------------------------
    // 3D パンツァードラグーン風レティクル（3重）
    // -----------------------------------------------------------------------
    std::unique_ptr<Sprite> reticleNear_;   // 近レティクル（小・細線）
    std::unique_ptr<Sprite> reticleMid_;    // 中レティクル（中・常時白）
    std::unique_ptr<Sprite> reticleFar_;    // 遠レティクル（大・弾道照準）

    // スクリーン座標（Update で計算・キャッシュ）
    Vector2 reticleNearScreen_{ 640, 360 };
    Vector2 reticleMidScreen_ { 640, 360 };
    Vector2 reticleFarScreen_ { 640, 360 };

    bool reticleNearVisible_ = false;
    bool reticleMidVisible_  = false;
    bool reticleFarVisible_  = false;

    bool isLockingMode_ = false;

    // ヘルパー：3Dワールド座標 → スクリーン座標変換。カメラ前方にあればtrueを返す。
    bool WorldToScreen(Camera* camera, const Vector3& worldPos, Vector2& outScreen) const;
};
