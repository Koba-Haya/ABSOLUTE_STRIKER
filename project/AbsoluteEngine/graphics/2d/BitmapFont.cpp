#include "BitmapFont.h"
#include "../../../externals/nlohmann/json.hpp"
#include "../../resources/AssetManager.h"
#include "AssetPath.h"
#include "Renderer.h"
#include "TextureResource.h"
#include <algorithm>
#include <fstream>

namespace {
// UTF-8 のデコードで、不正なバイト列に当たったときに返す文字(表に無いので代替文字になる)
constexpr uint32_t kInvalidCodepoint = 0xFFFD;
constexpr uint32_t kNewLine = '\n';
constexpr uint32_t kCarriageReturn = '\r';

// JSON の glyphs の各要素 [x, y, w, h, xOffset, yOffset, advance] の要素数
constexpr size_t kGlyphFieldCount = 7;

// 1 クワッドの頂点数と、頂点の並び(Sprite の単位四角形と同じ: 左下、左上、右下、右上)
constexpr uint32_t kVerticesPerQuad = 4;

/// UTF-8 の先頭バイトから、続きのバイト数を返す(1 バイト文字なら 0)。不正な先頭バイトは -1
int ContinuationByteCount(uint8_t lead) {
  if (lead < 0x80) return 0;
  if ((lead & 0xE0) == 0xC0) return 1;
  if ((lead & 0xF0) == 0xE0) return 2;
  if ((lead & 0xF8) == 0xF0) return 3;
  return -1;
}

/// 文字列の index 位置から UTF-8 を 1 文字デコードし、index を次の文字へ進める。
/// 不正なバイト列は kInvalidCodepoint を返し、1 バイトだけ進める(無限ループを避ける)
uint32_t DecodeNextCodepoint(const std::string &text, size_t &index) {
  const uint8_t lead = static_cast<uint8_t>(text[index]);
  const int extra = ContinuationByteCount(lead);
  if (extra < 0) { // 先頭バイトとして不正
    ++index;
    return kInvalidCodepoint;
  }
  if (extra == 0) { // 1 バイト文字(ASCII)
    ++index;
    return lead;
  }
  if (index + static_cast<size_t>(extra) >= text.size()) { // 続きのバイトが足りない
    ++index;
    return kInvalidCodepoint;
  }
  // 先頭バイトの下位ビット + 続きバイトの下位 6 ビットずつ
  static constexpr uint8_t kLeadMask[] = {0x7F, 0x1F, 0x0F, 0x07};
  uint32_t codepoint = lead & kLeadMask[extra];
  for (int i = 1; i <= extra; ++i) {
    const uint8_t cont = static_cast<uint8_t>(text[index + i]);
    if ((cont & 0xC0) != 0x80) { // 続きバイトの形ではない
      ++index;
      return kInvalidCodepoint;
    }
    codepoint = (codepoint << 6) | (cont & 0x3F);
  }
  index += static_cast<size_t>(extra) + 1;
  return codepoint;
}
} // namespace

BitmapFont::BitmapFont() = default;
BitmapFont::~BitmapFont() = default;

bool BitmapFont::Initialize(const std::string &atlasTexturePath,
                            const std::string &metricsJsonPath) {
  // 1. 文字表(JSON)を読む。無い・壊れているときは、探した場所を出して終了する
  const std::filesystem::path jsonPath = AbsoluteEngine::AssetPath::Resolve(metricsJsonPath);
  std::ifstream file(jsonPath);
  if (!file) {
    AbsoluteEngine::AssetPath::ReportMissingAndExit("font metrics (json)", jsonPath.string());
  }
  nlohmann::json root = nlohmann::json::parse(file, nullptr, /*allow_exceptions=*/false);
  if (root.is_discarded() || !root.contains("glyphs")) {
    AbsoluteEngine::AssetPath::ReportMissingAndExit("font metrics (valid json)", jsonPath.string());
  }

  atlasWidth_ = root.value("atlasWidth", 1.0f);
  atlasHeight_ = root.value("atlasHeight", 1.0f);
  pixelSize_ = root.value("pixelSize", 1.0f);
  ascent_ = root.value("ascent", 0.0f);
  lineHeight_ = root.value("lineHeight", 0.0f);
  fallbackCodepoint_ = root.value("fallback", 0u);

  // キーは 10 進のコードポイント、値は [x, y, w, h, xOffset, yOffset, advance]
  glyphs_.clear();
  glyphs_.reserve(root["glyphs"].size());
  for (auto it = root["glyphs"].begin(); it != root["glyphs"].end(); ++it) {
    const auto &v = it.value();
    if (!v.is_array() || v.size() != kGlyphFieldCount) continue;
    Glyph g;
    g.x = v[0].get<float>();
    g.y = v[1].get<float>();
    g.width = v[2].get<float>();
    g.height = v[3].get<float>();
    g.xOffset = v[4].get<float>();
    g.yOffset = v[5].get<float>();
    g.advance = v[6].get<float>();
    glyphs_[static_cast<uint32_t>(std::stoul(it.key()))] = g;
  }

  // 2. アトラス画像を読む(同じ画像は TextureManager が共有する)
  texture_ = AbsoluteEngine::AssetManager::GetInstance()->Load<TextureResource>(atlasTexturePath);
  if (!texture_) {
    AbsoluteEngine::AssetPath::ReportMissingAndExit(
        "font atlas (png)", AbsoluteEngine::AssetPath::Resolve(atlasTexturePath).string());
  }
  return true;
}

const BitmapFont::Glyph *BitmapFont::FindGlyph(uint32_t codepoint) const {
  auto it = glyphs_.find(codepoint);
  if (it == glyphs_.end()) {
    it = glyphs_.find(fallbackCodepoint_);
    if (it == glyphs_.end()) return nullptr;
  }
  return &it->second;
}

std::vector<std::vector<uint32_t>> BitmapFont::SplitLines(const std::string &utf8) {
  std::vector<std::vector<uint32_t>> lines(1);
  size_t index = 0;
  while (index < utf8.size()) {
    const uint32_t cp = DecodeNextCodepoint(utf8, index);
    if (cp == kNewLine) {
      lines.emplace_back();
    } else if (cp != kCarriageReturn) {
      lines.back().push_back(cp);
    }
  }
  return lines;
}

float BitmapFont::MeasureLineAtlasWidth(const std::vector<uint32_t> &line) const {
  float width = 0.0f;
  for (uint32_t cp : line) {
    if (const Glyph *g = FindGlyph(cp)) width += g->advance;
  }
  return width;
}

float BitmapFont::GetLineHeight(float fontSize) const {
  return lineHeight_ * (fontSize / pixelSize_);
}

Vector2 BitmapFont::Measure(const std::string &text, float fontSize) const {
  const float scale = fontSize / pixelSize_;
  const auto lines = SplitLines(text);
  float maxWidth = 0.0f;
  for (const auto &line : lines) {
    maxWidth = (std::max)(maxWidth, MeasureLineAtlasWidth(line));
  }
  return {maxWidth * scale, lineHeight_ * scale * static_cast<float>(lines.size())};
}

void BitmapFont::Draw(const std::string &text, const Vector2 &position, float fontSize,
                      const Vector4 &color, Align align, BlendMode blendMode) {
  if (!texture_ || text.empty()) return;

  const float scale = fontSize / pixelSize_; // アトラスの px から画面の px への倍率
  const auto lines = SplitLines(text);

  // 全行ぶんの頂点を 1 本の配列に集める(描画命令は最後に 1 回)
  scratchVertices_.clear();
  float lineTop = position.y;
  for (const auto &line : lines) {
    // 行の寄せ: position.x を、行の左端 / 中心 / 右端のどれにするか
    const float lineWidth = MeasureLineAtlasWidth(line) * scale;
    float penX = position.x;
    if (align == Align::Center) penX -= lineWidth * 0.5f;
    else if (align == Align::Right) penX -= lineWidth;

    const float baselineY = lineTop + ascent_ * scale;
    for (uint32_t cp : line) {
      const Glyph *g = FindGlyph(cp);
      if (!g) continue;
      if (g->width > 0.0f && g->height > 0.0f) { // 空白は絵が無いので、送り幅だけ進める
        const float x0 = penX + g->xOffset * scale;
        const float y0 = baselineY + g->yOffset * scale;
        const float x1 = x0 + g->width * scale;
        const float y1 = y0 + g->height * scale;
        const float u0 = g->x / atlasWidth_;
        const float v0 = g->y / atlasHeight_;
        const float u1 = (g->x + g->width) / atlasWidth_;
        const float v1 = (g->y + g->height) / atlasHeight_;
        // 左下、左上、右下、右上(Sprite の単位四角形と同じ並び)
        scratchVertices_.push_back({{x0, y1, 0.0f}, {u0, v1}});
        scratchVertices_.push_back({{x0, y0, 0.0f}, {u0, v0}});
        scratchVertices_.push_back({{x1, y1, 0.0f}, {u1, v1}});
        scratchVertices_.push_back({{x1, y0, 0.0f}, {u1, v0}});
      }
      penX += g->advance * scale;
    }
    lineTop += lineHeight_ * scale;
  }

  const uint32_t quadCount = static_cast<uint32_t>(scratchVertices_.size() / kVerticesPerQuad);
  if (quadCount == 0) return;
  Renderer::GetInstance()->DrawSpriteQuads(scratchVertices_.data(), quadCount,
                                           texture_->GetSrvGpu(), color, blendMode);
}
