#include "UiRectRenderer.h"
#include "../../AbsoluteEngine/resources/AssetManager.h"
#include "AssetPath.h"
#include "Renderer.h"
#include "SpriteResource.h" // SpriteVertex
#include "TextureResource.h"

namespace {
// 白い画像の論理パス(AssetPath)
constexpr const char *kWhiteTexturePath = "ui/white.png";
// 白い画像の中心の UV(縁の補間を避けるため、全頂点をここに向ける。画像は単色なので、どこでも同じ色)
constexpr float kWhiteUv = 0.5f;
} // namespace

UiRectRenderer::UiRectRenderer() = default;
UiRectRenderer::~UiRectRenderer() = default;

void UiRectRenderer::Initialize() {
  white_ = AbsoluteEngine::AssetManager::GetInstance()->Load<TextureResource>(kWhiteTexturePath);
  if (!white_) {
    AbsoluteEngine::AssetPath::ReportMissingAndExit(
        "ui white texture (png)", AbsoluteEngine::AssetPath::Resolve(kWhiteTexturePath).string());
  }
}

void UiRectRenderer::Fill(const UiRect *rects, size_t count, const Vector4 &color,
                          BlendMode blendMode) {
  if (!white_ || !rects || count == 0) return;

  // 長方形ごとに 4 頂点(左下、左上、右下、右上。BitmapFont と同じ並び)
  scratchVertices_.clear();
  for (size_t i = 0; i < count; ++i) {
    const UiRect &r = rects[i];
    if (r.width <= 0.0f || r.height <= 0.0f) continue;
    const float x1 = r.x + r.width;
    const float y1 = r.y + r.height;
    scratchVertices_.push_back({{r.x, y1, 0.0f}, {kWhiteUv, kWhiteUv}});
    scratchVertices_.push_back({{r.x, r.y, 0.0f}, {kWhiteUv, kWhiteUv}});
    scratchVertices_.push_back({{x1, y1, 0.0f}, {kWhiteUv, kWhiteUv}});
    scratchVertices_.push_back({{x1, r.y, 0.0f}, {kWhiteUv, kWhiteUv}});
  }
  if (scratchVertices_.empty()) return;

  constexpr size_t kVerticesPerQuad = 4;
  Renderer::GetInstance()->DrawSpriteQuads(scratchVertices_.data(),
                                           static_cast<uint32_t>(scratchVertices_.size() / kVerticesPerQuad),
                                           white_->GetSrvGpu(), color, blendMode);
}

void UiRectRenderer::Fill(const UiRect &rect, const Vector4 &color, BlendMode blendMode) {
  Fill(&rect, 1, color, blendMode);
}
