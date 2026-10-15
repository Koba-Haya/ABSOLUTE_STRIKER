#include "ResultBanner.h"

#include "graphics/2d/Sprite.h"
#include <algorithm>

namespace {
// 画面(描画先)の大きさ。スプライトの座標はこの大きさのピクセル単位
constexpr float kScreenWidth = 1280.0f;
constexpr float kScreenHeight = 720.0f;

// 見出しの大きさ(元画像は 4:1)と位置(画面の中央から上へずらす割合)
constexpr float kBannerWidth = 720.0f;
constexpr float kBannerHeight = 180.0f;
constexpr float kBannerCenterYRatio = 0.42f;

// 現れる演出: 画面遷移の覆いが晴れるのを待ってから、ゆっくり濃くなる
constexpr float kFadeInDelaySec = 0.4f;
constexpr float kFadeInSec = 0.8f;
} // namespace

ResultBanner::ResultBanner() = default;
ResultBanner::~ResultBanner() = default;

void ResultBanner::Initialize(const std::string &texturePath) {
  Sprite::CreateInfo info;
  info.texturePath = texturePath;
  info.size = {kBannerWidth, kBannerHeight};
  info.color = {1.0f, 1.0f, 1.0f, 0.0f}; // 最初は透明
  sprite_ = std::make_unique<Sprite>();
  sprite_->Initialize(info);
  sprite_->SetPosition({(kScreenWidth - kBannerWidth) * 0.5f, kScreenHeight * kBannerCenterYRatio - kBannerHeight * 0.5f, 0.0f});
  time_ = 0.0f;
}

void ResultBanner::Update(float deltaTime) {
  time_ += deltaTime;
}

void ResultBanner::Draw() {
  if (!sprite_) return;
  const float t = std::clamp((time_ - kFadeInDelaySec) / kFadeInSec, 0.0f, 1.0f);
  const float alpha = t * t * (3.0f - 2.0f * t); // smoothstep
  sprite_->SetColor({1.0f, 1.0f, 1.0f, alpha});
  sprite_->Draw();
}
