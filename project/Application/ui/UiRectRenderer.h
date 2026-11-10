#pragma once
#include "BlendMode.h"
#include "Vector.h"
#include <cstddef>
#include <memory>
#include <vector>

struct SpriteVertex;
class TextureResource;

/// 画面ピクセル座標の長方形(左上の座標と大きさ)
struct UiRect {
  float x = 0.0f;
  float y = 0.0f;
  float width = 0.0f;
  float height = 0.0f;
};

/// <summary>
/// 単色の長方形を画面に塗るクラス(UI の枠、背景、ハイライトの下地に使う)。
/// 1 ピクセルぶんの白い画像(resources/ui/white.png)に色を掛けて塗る。
/// 同じ色の長方形は 1 回の描画命令にまとめる(Renderer::DrawSpriteQuads。BitmapFont と同じ仕組み)。
/// 分けた理由: 「単色の四角を塗る」は UiPanel、メニューのハイライトなど複数の UI 部品が使うので、1 か所にまとめるため。
/// </summary>
class UiRectRenderer {
public:
  UiRectRenderer();
  ~UiRectRenderer();

  /// <summary>白い画像を読む。ファイルが無いときは、探した場所を表示して終了する(AssetPath の方針)</summary>
  void Initialize();

  /// <summary>同じ色の長方形を count 個まとめて塗る(描画命令は 1 回)</summary>
  /// <param name="rects">長方形の配列(画面ピクセル座標)</param>
  /// <param name="count">配列の個数</param>
  /// <param name="color">色(RGBA。アルファで透明度)</param>
  /// <param name="blendMode">ブレンド方式</param>
  void Fill(const UiRect *rects, size_t count, const Vector4 &color,
            BlendMode blendMode = BlendMode::Alpha);

  /// <summary>長方形を 1 個塗る</summary>
  void Fill(const UiRect &rect, const Vector4 &color, BlendMode blendMode = BlendMode::Alpha);

private:
  std::shared_ptr<TextureResource> white_; // 白い画像(SRV を保持するため参照を持つ)
  std::vector<SpriteVertex> scratchVertices_; // 頂点の作業用(使い回す)
};
