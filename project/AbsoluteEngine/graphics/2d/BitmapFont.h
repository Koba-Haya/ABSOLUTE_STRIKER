#pragma once
#include "BlendMode.h"
#include "SpriteResource.h" // SpriteVertex
#include "Vector.h"
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class TextureResource;

/// <summary>
/// ビットマップフォントで文字列(UTF-8、日本語可)を画面に描くクラス。
/// 事前に Python(tools/font_atlas/gen_font_atlas.py)で作ったアトラス画像(全文字を 1 枚に並べた PNG)と
/// 文字ごとの位置・幅の表(JSON)を読む。TTF は実行時には読まない。
/// 描くときは、文字ごとに四角形(クワッド)を作り、文字列 1 回につき描画命令 1 回でまとめて描く
/// (Renderer::DrawSpriteQuads。文字ごとに Sprite を作らないので、文字数が多くても軽い)。
/// 比例幅(文字ごとに幅が違う)で、数字 0-9 だけは固定幅(スコアの桁がぶれないように)。
/// 画面ピクセル座標は Sprite と同じ(左上が原点、y は下向き)。
/// </summary>
class BitmapFont {
public:
  /// 複数行のときの、各行の寄せ方
  enum class Align {
    Left,   // position.x が行の左端
    Center, // position.x が行の中心
    Right,  // position.x が行の右端
  };

  BitmapFont();
  ~BitmapFont();

  /// <summary>
  /// アトラス画像と文字表を読む。ファイルが無い、形式が違うときは、探した場所を表示してアプリを終了する
  /// (文字が出ない状態で黙って進むと原因が分からなくなるため。AssetPath::ReportMissingAndExit と同じ方針)。
  /// </summary>
  /// <param name="atlasTexturePath">アトラス画像のパス(AssetPath の論理パス。例: "fonts/atlas/ui_font.png")</param>
  /// <param name="metricsJsonPath">文字表 JSON のパス(例: "fonts/atlas/ui_font.json")</param>
  /// <returns>読めたら true(失敗時は終了するので、通常は常に true)</returns>
  bool Initialize(const std::string &atlasTexturePath, const std::string &metricsJsonPath);

  /// <summary>
  /// 文字列を描く。'\n' で改行する。表にない文字は '?' で描く。
  /// </summary>
  /// <param name="text">UTF-8 の文字列</param>
  /// <param name="position">1 行目の行ボックスの左上(Align により x の意味が変わる)。画面ピクセル座標</param>
  /// <param name="fontSize">文字の大きさ(px)。全角文字の 1 文字分の高さ</param>
  /// <param name="color">文字色(RGBA。アルファで透明度)</param>
  /// <param name="align">行の寄せ方(position.x を基準にする)</param>
  /// <param name="blendMode">ブレンド方式(光らせたいときは Add)</param>
  void Draw(const std::string &text, const Vector2 &position, float fontSize,
            const Vector4 &color, Align align = Align::Left,
            BlendMode blendMode = BlendMode::Alpha);

  /// <summary>
  /// 文字列を描いたときの大きさ(幅 = 最も長い行、高さ = 行数 * 行の高さ)を返す。描画はしない。
  /// 中央寄せの計算や、パネルの大きさの決定に使う。
  /// </summary>
  Vector2 Measure(const std::string &text, float fontSize) const;

  /// 1 行の高さ(px)。行ボックスの高さで、行間もこれで進む
  float GetLineHeight(float fontSize) const;

  /// 文字表とアトラスを読み込み済みか
  bool IsReady() const { return texture_ != nullptr; }

private:
  /// 1 文字ぶんの情報(JSON の [x, y, w, h, xOffset, yOffset, advance] に対応。単位はアトラスの px)
  struct Glyph {
    float x = 0.0f, y = 0.0f;           // アトラス上の左上
    float width = 0.0f, height = 0.0f;  // アトラス上の大きさ(0 なら絵なし = 空白)
    float xOffset = 0.0f;               // ペン位置から絵の左端まで
    float yOffset = 0.0f;               // ベースラインから絵の上端まで(上向きが負)
    float advance = 0.0f;               // 次の文字へ進む距離
  };

  /// コードポイントから文字情報を引く。無ければ代替文字、それも無ければ nullptr
  const Glyph *FindGlyph(uint32_t codepoint) const;

  /// 1 行の幅(px)を、アトラスの px 単位で測る(Measure と Draw の寄せ計算が共有する)
  float MeasureLineAtlasWidth(const std::vector<uint32_t> &line) const;

  /// UTF-8 の文字列を、行ごとのコードポイント列に分ける('\n' で区切る。'\r' は捨てる)
  static std::vector<std::vector<uint32_t>> SplitLines(const std::string &utf8);

  std::shared_ptr<TextureResource> texture_; // アトラス画像(SRV を保持するため参照を持つ)
  std::unordered_map<uint32_t, Glyph> glyphs_;
  uint32_t fallbackCodepoint_ = 0; // 表にない文字の代わりに出す文字
  float atlasWidth_ = 1.0f;        // アトラス画像の大きさ(UV 計算用)
  float atlasHeight_ = 1.0f;
  float pixelSize_ = 1.0f;         // アトラスを焼いたときの文字の大きさ(px)。fontSize との比で拡縮する
  float ascent_ = 0.0f;            // 行ボックスの上端からベースラインまで(アトラスの px)
  float lineHeight_ = 0.0f;        // 行ボックスの高さ(アトラスの px)

  std::vector<SpriteVertex> scratchVertices_; // 描画時の頂点の作業用。毎回確保し直さないよう使い回す
};
