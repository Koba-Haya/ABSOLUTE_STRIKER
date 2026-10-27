#include "ResultBanner.h"

#include "graphics/2d/Sprite.h"
#include <algorithm>

namespace {
// 画面(描画先)の大きさ。スプライトの座標はこの大きさのピクセル単位
constexpr float kScreenWidth = 1280.0f;
constexpr float kScreenHeight = 720.0f;

// 時間で現れる演出: 画面遷移の覆いが晴れるのを待ってから、ゆっくり濃くなる
constexpr float kFadeInDelaySec = 0.4f;
constexpr float kFadeInSec = 0.8f;
// 現れる区間の長さの下限(0 除算を避ける)
constexpr float kMinRevealSpan = 0.0001f;
} // namespace

ResultBanner::ResultBanner() = default;
ResultBanner::~ResultBanner() = default;

void ResultBanner::Initialize(const std::string &texturePath, const Layout &layout) {
  layout_ = layout;
  Sprite::CreateInfo info;
  info.texturePath = texturePath;
  info.size = {layout_.width, layout_.height};
  info.color = {1.0f, 1.0f, 1.0f, 0.0f}; // 最初は透明
  sprite_ = std::make_unique<Sprite>();
  sprite_->Initialize(info);
  sprite_->SetPosition({(kScreenWidth - layout_.width) * 0.5f, kScreenHeight * layout_.centerYRatio - layout_.height * 0.5f, 0.0f});
  time_ = 0.0f;
  appearance_ = 0.0f;
}

float ResultBanner::Smooth_(float t) {
  t = std::clamp(t, 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t); // smoothstep
}

void ResultBanner::Update(float deltaTime) {
  time_ += deltaTime;
  appearance_ = Smooth_((time_ - kFadeInDelaySec) / kFadeInSec);
}

void ResultBanner::SetRevealProgress(float progress) {
  const float span = (std::max)(layout_.revealEnd - layout_.revealStart, kMinRevealSpan);
  appearance_ = Smooth_((progress - layout_.revealStart) / span);
}

void ResultBanner::Draw() {
  if (!sprite_) return;
  sprite_->SetColor({1.0f, 1.0f, 1.0f, appearance_});
  sprite_->Draw();
}
