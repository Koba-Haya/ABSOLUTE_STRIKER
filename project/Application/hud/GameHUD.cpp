#define NOMINMAX
#include "GameHUD.h"
#include "Camera.h"
#include "Method.h"   // Multiply, Inverse, TransformPoint
#include "Application/ScreenSize.h"
#include "graphics/pipeline/BlendMode.h" // BlendMode::Add
#include <algorithm>
#include <cmath>

// レティクルの見た目サイズ
static constexpr float kNearSize =  80.0f; // 近：大（遠近法に合わせて手前を大きく）
static constexpr float kMidSize  =  56.0f; // 中：中
static constexpr float kFarSize  =  32.0f; // 遠：小（遠近法に合わせて奥を小さく）

void GameHUD::Initialize() {
    // -----------------------------------------------------------------------
    // 3重レティクルの初期化（パンツァードラグーン風）
    // 加算合成（BlendMode::Add）で描画することで背景に埋もれず発光して見える。
    // 近 → 中 → 遠 の順にサイズと輝度を上げ、奥行き感を演出する。
    // -----------------------------------------------------------------------

    // 近レティクル（距離 10）：細く小さい、半透明の水色
    {
        Sprite::CreateInfo info;
        info.texturePath = "resources/Reticle/reticle.png";
        info.size        = { kNearSize, kNearSize };
        info.color       = { 0.5f, 0.8f, 1.0f, 0.6f }; // 薄い水色（控えめ）
        reticleNear_ = std::make_unique<Sprite>();
        reticleNear_->Initialize(info);
        reticleNear_->SetBlendMode(BlendMode::Add); // 加算合成で発光感を出す
    }

    // 中レティクル（距離 30）：明るい白、やや大きく
    {
        Sprite::CreateInfo info;
        info.texturePath = "resources/Reticle/reticle.png";
        info.size        = { kMidSize, kMidSize };
        info.color       = { 1.0f, 1.0f, 1.0f, 0.85f }; // 明るい白
        reticleMid_ = std::make_unique<Sprite>();
        reticleMid_->Initialize(info);
        reticleMid_->SetBlendMode(BlendMode::Add); // 加算合成
    }

    // 遠レティクル（距離 60）：弾道の照準、最大・最も明るい黄緑で際立たせる
    {
        Sprite::CreateInfo info;
        info.texturePath = "resources/Reticle/reticle.png";
        info.size        = { kFarSize, kFarSize };
        info.color       = { 0.2f, 1.0f, 0.4f, 1.0f }; // 鮮やかな黄緑（フル不透明）
        reticleFar_ = std::make_unique<Sprite>();
        reticleFar_->Initialize(info);
        reticleFar_->SetBlendMode(BlendMode::Add); // 加算合成で最も明るく
    }

    // ロックオン中の中央エイムサークル
    {
        Sprite::CreateInfo info;
        info.texturePath = "resources/app/particle/circle.png";
        info.size        = { 150.0f, 150.0f };
        info.color       = { 0.0f, 1.0f, 0.0f, 0.3f }; // 半透明の緑
        centerAimSprite_ = std::make_unique<Sprite>();
        centerAimSprite_->Initialize(info);
    }

    // HP表示スプライト（最大10個分確保）
    for (int i = 0; i < 10; ++i) {
        auto hpSp = std::make_unique<Sprite>();
        Sprite::CreateInfo info;
        info.texturePath = "resources/app/particle/circle.png";
        info.size        = { 32.0f, 32.0f };
        info.color       = { 0.0f, 1.0f, 0.0f, 1.0f };
        hpSp->Initialize(info);
        hpSprites_.push_back(std::move(hpSp));

        auto lockSp = std::make_unique<Sprite>();
        Sprite::CreateInfo lockInfo;
        lockInfo.texturePath = "resources/app/particle/circle.png";
        lockInfo.size        = { 48.0f, 48.0f };
        lockInfo.color       = { 1.0f, 0.0f, 0.0f, 0.8f };
        lockSp->Initialize(lockInfo);
        lockOnCursorSprites_.push_back(std::move(lockSp));
    }
}

// -----------------------------------------------------------------------
// ヘルパー：ワールド座標 → スクリーン座標
// カメラ前方にある場合のみ true を返す。
// -----------------------------------------------------------------------
bool GameHUD::WorldToScreen(Camera* camera, const Vector3& worldPos, Vector2& outScreen) const {
    if (!camera) return false;
    Matrix4x4 vp = Multiply(camera->GetViewMatrix(), camera->GetProjectionMatrix());
    Vector3 ndc = TransformPoint(worldPos, vp);
    // NDC.z が 0〜1 の範囲外はカメラ後方またはクリップ外
    if (ndc.z < 0.0f || ndc.z > 1.0f) return false;
    outScreen.x = (ndc.x + 1.0f) * 0.5f * ScreenSize::kWidth;
    outScreen.y = (1.0f - ndc.y) * 0.5f * ScreenSize::kHeight;
    return true;
}

// -----------------------------------------------------------------------
// Update
// -----------------------------------------------------------------------
void GameHUD::Update(int playerHp, int playerMaxHp,
                     const std::vector<Vector2>& lockOnPositions,
                     bool isLockingMode,
                     const Vector2& cursorPos,
                     Camera* camera,
                     const Vector3& nearWorldPos,
                     const Vector3& midWorldPos,
                     const Vector3& farWorldPos) {
    currentHp_  = playerHp;
    maxHp_      = playerMaxHp;
    isLockingMode_           = isLockingMode;
    currentLockOnPositions_  = lockOnPositions;

    // -----------------------------------------------------------------------
    // 3重レティクルのスクリーン座標を計算する
    // -----------------------------------------------------------------------
    reticleNearVisible_ = WorldToScreen(camera, nearWorldPos, reticleNearScreen_);
    reticleMidVisible_  = WorldToScreen(camera, midWorldPos,  reticleMidScreen_);
    reticleFarVisible_  = WorldToScreen(camera, farWorldPos,  reticleFarScreen_);

    // --- 各スプライトの位置を更新（中心揃え） ---
    if (reticleNearVisible_ && reticleNear_) {
        reticleNear_->SetPosition({ reticleNearScreen_.x - kNearSize * 0.5f,
                                    reticleNearScreen_.y - kNearSize * 0.5f, 0.0f });
    }
    if (reticleMidVisible_ && reticleMid_) {
        reticleMid_->SetPosition({ reticleMidScreen_.x - kMidSize * 0.5f,
                                   reticleMidScreen_.y - kMidSize * 0.5f, 0.0f });
    }
    if (reticleFarVisible_ && reticleFar_) {
        reticleFar_->SetPosition({ reticleFarScreen_.x - kFarSize * 0.5f,
                                   reticleFarScreen_.y - kFarSize * 0.5f, 0.0f });
    }

    // ロックオン時は遠レティクルを明るい黄色に切り替え（加算合成でより目立つ）
    if (reticleFar_ && isLockingMode_) {
        reticleFar_->SetColor({ 1.0f, 1.0f, 0.0f, 1.0f }); // 明るい黄色（ロックオン中）
    } else if (reticleFar_) {
        reticleFar_->SetColor({ 0.2f, 1.0f, 0.4f, 1.0f });
    }

    // -----------------------------------------------------------------------
    // HPスプライトの更新
    // -----------------------------------------------------------------------
    for (int i = 0; i < (int)hpSprites_.size(); ++i) {
        if (i < maxHp_) {
            hpSprites_[i]->SetPosition({ 50.0f + i * 40.0f - 16.0f, 650.0f - 16.0f, 0.0f });
            if (i < currentHp_) {
                hpSprites_[i]->SetColor({ 0.0f, 1.0f, 0.0f, 1.0f }); // 緑（残りHP）
            } else {
                hpSprites_[i]->SetColor({ 0.5f, 0.5f, 0.5f, 0.5f }); // 灰（減ったHP）
            }
        }
    }

    // -----------------------------------------------------------------------
    // ロックオンカーソルの更新
    // -----------------------------------------------------------------------
    for (int i = 0; i < (int)lockOnCursorSprites_.size(); ++i) {
        if (i < (int)currentLockOnPositions_.size()) {
            lockOnCursorSprites_[i]->SetPosition({
                currentLockOnPositions_[i].x - 24.0f,
                currentLockOnPositions_[i].y - 24.0f, 0.0f });
        }
    }

    // ロックオン中のセンターエイムサークル
    if (isLockingMode_ && centerAimSprite_) {
        centerAimSprite_->SetPosition({ cursorPos.x - 75.0f, cursorPos.y - 75.0f, 0.0f });
    }
}

// -----------------------------------------------------------------------
// Draw
// -----------------------------------------------------------------------
void GameHUD::Draw() {
    // HP
    int drawHpCount = std::min(maxHp_, (int)hpSprites_.size());
    for (int i = 0; i < drawHpCount; ++i) {
        hpSprites_[i]->Draw();
    }

    // ロックオンカーソル
    int drawLockCount = (int)std::min(currentLockOnPositions_.size(), lockOnCursorSprites_.size());
    for (int i = 0; i < drawLockCount; ++i) {
        lockOnCursorSprites_[i]->Draw();
    }

    // センターエイムサークル（ロックオン中のみ）
    if (isLockingMode_ && centerAimSprite_) {
        centerAimSprite_->Draw();
    }

    // -----------------------------------------------------------------------
    // 3重レティクルを奥から描画（遠→中→近の順で手前が上書き）
    // -----------------------------------------------------------------------
    if (reticleFarVisible_ && reticleFar_) {
        reticleFar_->Draw();
    }
    if (reticleMidVisible_ && reticleMid_) {
        reticleMid_->Draw();
    }
    if (reticleNearVisible_ && reticleNear_) {
        reticleNear_->Draw();
    }
}
