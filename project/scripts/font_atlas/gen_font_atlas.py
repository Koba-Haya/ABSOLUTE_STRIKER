#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
UI 用ビットマップフォントのアトラス生成スクリプト。

M PLUS 1p の ttf から、固定の文字セット(ASCII、かな、常用漢字、記号、アイコン)を
1 枚の PNG(アトラス)に焼き、各文字の位置と幅を JSON に書き出す。
実行時(C++ の BitmapFont)は、この PNG と JSON だけを読む。ttf はゲームでは読まない。

使い方(プロジェクトの project フォルダで実行):
    python scripts/font_atlas/gen_font_atlas.py
    python scripts/font_atlas/gen_font_atlas.py --weight Medium --size 40

必要なもの: Python 3、Pillow (pip install pillow)

出力(既定):
    Application/resources/fonts/atlas/ui_font.png   白色 + アルファのアトラス
    Application/resources/fonts/atlas/ui_font.json  文字ごとの矩形・オフセット・送り幅

比例幅について:
    文字ごとの送り幅(advance)をそのまま JSON に入れる。ただし数字 0-9 だけは、
    スコアが増減しても桁がぶれないように、0-9 の最大の送り幅にそろえる(固定幅)。
"""

import argparse
import json
import math
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

# ---------------------------------------------------------------------------
# 定数
# ---------------------------------------------------------------------------

# 既定値
DEFAULT_WEIGHT = "Bold"          # M PLUS 1p のウェイト名(ファイル名 MPLUS1p-<名前>.ttf)
DEFAULT_PIXEL_SIZE = 40          # アトラスに焼く文字の大きさ(px)。画面では縮小して使う
GLYPH_PADDING = 2                # 隣の文字のにじみ(バイリニア補間)を防ぐ余白(px)
ATLAS_SIZE_CANDIDATES = (1024, 1536, 2048, 2560, 3072, 4096)  # 試す正方形の一辺(小さい順)

# 文字セットの範囲(両端を含む Unicode コードポイント)
CODEPOINT_RANGES = (
    (0x0020, 0x007E),  # ASCII(空白を含む)
    (0x3041, 0x3096),  # ひらがな
    (0x309B, 0x309E),  # 濁点・半濁点・繰り返し記号(ひらがな側)
    (0x30A1, 0x30FA),  # カタカナ
    (0x30FB, 0x30FE),  # 中黒・長音・繰り返し記号(カタカナ側)
    (0xFF01, 0xFF5E),  # 全角 ASCII(！？（）％＋－：；など)
)

# 範囲にならない個別の文字(句読点、記号)
EXTRA_CODEPOINTS = (
    0x3000,                  # 全角空白
    0x3001, 0x3002,          # 、。
    0x300C, 0x300D, 0x300E, 0x300F, 0x3010, 0x3011,  # 括弧
    0x301C,                  # 波ダッシュ
    0x2026,                  # 三点リーダ
    0x00B7, 0x00D7, 0x00F7,  # 中点、掛け算、割り算
    0x8776,                  # 蝶(常用漢字にないが、このゲームの主題なので足す)
    0x2190, 0x2191, 0x2192, 0x2193,  # 矢印
    0x25A0, 0x25A1,          # 四角
    0x25B2, 0x25B3, 0x25B6, 0x25BC, 0x25BD, 0x25C0,  # 三角
    0x25C6, 0x25C7,          # ひし形
    0x25CB, 0x25CE, 0x25CF,  # 丸
    0x2605, 0x2606,          # 星
    0x2661, 0x2665,          # ハート
    0x266A,                  # 音符
    0x2713, 0x2715,          # チェック、バツ
    0x2212,                  # マイナス記号
)

# アイコン文字(私用領域)。フォントに無い絵なので、このスクリプトで描く。
ICON_LOCK = 0xE000           # 鍵(未解放のステージのロック表示に使う)

# 文字が無いときに代わりに出す文字(実行時に使う)
FALLBACK_CODEPOINT = ord("?")

# 数字は固定幅にそろえる
DIGIT_CODEPOINTS = tuple(range(ord("0"), ord("9") + 1))

# アイコン(鍵)の見た目。文字サイズに対する比率
ICON_ADVANCE_RATIO = 0.80    # 送り幅
ICON_BODY_WIDTH_RATIO = 0.62  # 鍵の本体の幅
ICON_BODY_HEIGHT_RATIO = 0.42  # 鍵の本体の高さ
ICON_SHACKLE_WIDTH_RATIO = 0.40  # つる(上の輪)の幅
ICON_SHACKLE_HEIGHT_RATIO = 0.40  # つるの高さ
ICON_LINE_RATIO = 0.08       # つるの線の太さ
ICON_BASELINE_GAP_RATIO = 0.0  # 鍵の下端をベースラインからどれだけ上げるか

JSON_FORMAT_VERSION = 1

# ---------------------------------------------------------------------------
# 文字セットの組み立て
# ---------------------------------------------------------------------------


def load_joyo_kanji(path: Path) -> list[int]:
    """常用漢字のテキストファイル(# 始まりの行は無視)を読み、コードポイントのリストを返す。"""
    codepoints = []
    for line in path.read_text(encoding="utf-8").splitlines():
        if line.startswith("#"):
            continue
        codepoints.extend(ord(ch) for ch in line.strip())
    return codepoints


def build_codepoint_list(joyo_path: Path) -> list[int]:
    """アトラスに入れる文字(コードポイント)を、重複なし・昇順で返す。"""
    codepoints = set()
    for first, last in CODEPOINT_RANGES:
        codepoints.update(range(first, last + 1))
    codepoints.update(EXTRA_CODEPOINTS)
    codepoints.update(load_joyo_kanji(joyo_path))
    return sorted(codepoints)


# ---------------------------------------------------------------------------
# 文字の描画
# ---------------------------------------------------------------------------


class Glyph:
    """1 文字分のデータ。image はアルファのみの画像(空白文字は None)。"""

    def __init__(self, codepoint, image, x_offset, y_offset, advance):
        self.codepoint = codepoint
        self.image = image          # PIL の "L" 画像 または None
        self.x_offset = x_offset    # ペン位置から画像の左端までの距離(px)
        self.y_offset = y_offset    # ベースラインから画像の上端までの距離(px、上が負)
        self.advance = advance      # 次の文字へ進む距離(px)
        self.x = 0                  # アトラス上の位置(パッキング後に設定)
        self.y = 0


def render_font_glyph(font: ImageFont.FreeTypeFont, codepoint: int, missing_mask: bytes):
    """フォントから 1 文字を描く。フォントに無い文字なら None を返す。"""
    ch = chr(codepoint)
    advance = font.getlength(ch)

    # 空白類は絵が無い。送り幅だけ持つ。
    if ch.isspace():
        return Glyph(codepoint, None, 0, 0, advance)

    # フォントに無い文字は「豆腐(.notdef)」が描かれるので、豆腐と同じ絵なら未収録とみなす
    if bytes(font.getmask(ch)) == missing_mask:
        return None

    # ベースライン左(anchor "ls")を原点にしたときの、絵の外接矩形
    left, top, right, bottom = font.getbbox(ch, anchor="ls")
    width, height = right - left, bottom - top
    if width <= 0 or height <= 0:
        return Glyph(codepoint, None, 0, 0, advance)

    image = Image.new("L", (width, height), 0)
    draw = ImageDraw.Draw(image)
    draw.text((-left, -top), ch, font=font, fill=255, anchor="ls")
    return Glyph(codepoint, image, left, top, advance)


def render_lock_icon(pixel_size: int) -> Glyph:
    """鍵のアイコンを描く(つる + 本体 + 鍵穴)。アンチエイリアスのため 4 倍で描いて縮小する。"""
    scale = 4
    advance = pixel_size * ICON_ADVANCE_RATIO

    body_w = round(pixel_size * ICON_BODY_WIDTH_RATIO)
    body_h = round(pixel_size * ICON_BODY_HEIGHT_RATIO)
    shackle_w = round(pixel_size * ICON_SHACKLE_WIDTH_RATIO)
    shackle_h = round(pixel_size * ICON_SHACKLE_HEIGHT_RATIO)
    line = max(2, round(pixel_size * ICON_LINE_RATIO))
    total_h = shackle_h + body_h

    big = Image.new("L", (body_w * scale, total_h * scale), 0)
    draw = ImageDraw.Draw(big)

    # つる(上半分が丸い輪)。本体の上端から shackle_h 分だけ上に出す
    shackle_left = (body_w - shackle_w) // 2
    draw.rounded_rectangle(
        (shackle_left * scale, 0, (shackle_left + shackle_w) * scale, (shackle_h + line * 2) * scale),
        radius=(shackle_w * scale) // 2, outline=255, width=line * scale)

    # 本体
    body_top = shackle_h
    draw.rounded_rectangle(
        (0, body_top * scale, body_w * scale, total_h * scale),
        radius=(body_h * scale) // 5, fill=255)

    # 鍵穴(本体を抜く)
    hole_r = max(2, body_h // 8)
    hole_cx = body_w // 2
    hole_cy = body_top + body_h // 2
    draw.ellipse(((hole_cx - hole_r) * scale, (hole_cy - hole_r) * scale,
                  (hole_cx + hole_r) * scale, (hole_cy + hole_r) * scale), fill=0)
    draw.rectangle(((hole_cx - hole_r // 2) * scale, hole_cy * scale,
                    (hole_cx + hole_r // 2) * scale, (hole_cy + hole_r * 2) * scale), fill=0)

    image = big.resize((body_w, total_h), Image.LANCZOS)
    x_offset = round((advance - body_w) / 2)
    y_offset = -round(total_h + pixel_size * ICON_BASELINE_GAP_RATIO)
    return Glyph(ICON_LOCK, image, x_offset, y_offset, advance)


def collect_glyphs(font, codepoints, pixel_size):
    """全文字を描いて Glyph のリストにする。フォントに無かった文字は一覧で報告する。"""
    missing_mask = bytes(font.getmask("￿"))
    glyphs, missing = [], []
    for cp in codepoints:
        glyph = render_font_glyph(font, cp, missing_mask)
        if glyph is None:
            missing.append(cp)
        else:
            glyphs.append(glyph)
    glyphs.append(render_lock_icon(pixel_size))
    return glyphs, missing


def equalize_digit_advances(glyphs):
    """数字 0-9 の送り幅を、その最大値にそろえる(固定幅)。そろえた幅を返す。"""
    digits = [g for g in glyphs if g.codepoint in DIGIT_CODEPOINTS]
    fixed = max(g.advance for g in digits)
    for g in digits:
        # 絵が幅の中で中央に来るようにずらす
        if g.image is not None:
            g.x_offset += (fixed - g.advance) / 2
        g.advance = fixed
    return fixed


# ---------------------------------------------------------------------------
# アトラスへの詰め込み(シェルフ法: 行を作って左から並べる)
# ---------------------------------------------------------------------------


def try_pack(glyphs, atlas_size):
    """atlas_size 四方に全文字が入るか試す。入れば位置を書き込んで True。"""
    # 高い文字から並べると、行の無駄が減る
    order = sorted((g for g in glyphs if g.image is not None),
                   key=lambda g: (-g.image.height, -g.image.width))
    x = y = row_h = 0
    for g in order:
        w = g.image.width + GLYPH_PADDING * 2
        h = g.image.height + GLYPH_PADDING * 2
        if w > atlas_size:
            return False
        if x + w > atlas_size:  # 行が埋まったので次の行へ
            x, y, row_h = 0, y + row_h, 0
        if y + h > atlas_size:
            return False
        g.x, g.y = x + GLYPH_PADDING, y + GLYPH_PADDING
        x += w
        row_h = max(row_h, h)
    return True


def pack_glyphs(glyphs):
    """入る最小の正方形アトラスを探して詰める。使った一辺を返す。"""
    for size in ATLAS_SIZE_CANDIDATES:
        if try_pack(glyphs, size):
            return size
    raise RuntimeError("アトラスが %d px 四方でも収まらない。--size を小さくすること" % ATLAS_SIZE_CANDIDATES[-1])


# ---------------------------------------------------------------------------
# 書き出し
# ---------------------------------------------------------------------------


def write_png(glyphs, atlas_size, path: Path):
    """白色 + アルファのアトラスを書く。色は実行時に頂点色(マテリアル色)で付ける。"""
    alpha = Image.new("L", (atlas_size, atlas_size), 0)
    for g in glyphs:
        if g.image is not None:
            alpha.paste(g.image, (g.x, g.y))
    rgba = Image.merge("RGBA", (Image.new("L", alpha.size, 255),) * 3 + (alpha,))
    path.parent.mkdir(parents=True, exist_ok=True)
    rgba.save(path, optimize=True)


def write_json(glyphs, atlas_size, font, pixel_size, digit_advance, path: Path):
    """文字ごとの [x, y, w, h, xOffset, yOffset, advance] を書く(キーは 10 進のコードポイント)。"""
    ascent, descent = font.getmetrics()
    table = {}
    for g in sorted(glyphs, key=lambda g: g.codepoint):
        w = g.image.width if g.image is not None else 0
        h = g.image.height if g.image is not None else 0
        table[str(g.codepoint)] = [g.x, g.y, w, h, round(g.x_offset, 2), round(g.y_offset, 2),
                                   round(g.advance, 2)]
    data = {
        "version": JSON_FORMAT_VERSION,
        "pixelSize": pixel_size,
        "atlasWidth": atlas_size,
        "atlasHeight": atlas_size,
        "ascent": ascent,
        "descent": descent,
        "lineHeight": ascent + descent,
        "digitAdvance": round(digit_advance, 2),
        "fallback": FALLBACK_CODEPOINT,
        "glyphs": table,
    }
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, ensure_ascii=False, separators=(",", ":")), encoding="utf-8")


# ---------------------------------------------------------------------------
# メイン
# ---------------------------------------------------------------------------


def main() -> int:
    project_dir = Path(__file__).resolve().parents[2]
    resources = project_dir / "Application" / "resources"

    parser = argparse.ArgumentParser(description="UI 用ビットマップフォントのアトラスを生成する")
    parser.add_argument("--weight", default=DEFAULT_WEIGHT, help="M PLUS 1p のウェイト名(Thin/Light/Regular/Medium/Bold/ExtraBold/Black)")
    parser.add_argument("--size", type=int, default=DEFAULT_PIXEL_SIZE, help="アトラスに焼く文字の大きさ(px)")
    parser.add_argument("--font-dir", type=Path, default=resources / "fonts" / "M_PLUS_1p")
    parser.add_argument("--out-dir", type=Path, default=resources / "fonts" / "atlas")
    parser.add_argument("--name", default="ui_font", help="出力ファイル名(拡張子なし)")
    args = parser.parse_args()

    ttf_path = args.font_dir / ("MPLUS1p-%s.ttf" % args.weight)
    joyo_path = Path(__file__).resolve().parent / "joyo_kanji.txt"
    if not ttf_path.exists():
        print("フォントが見つからない: %s" % ttf_path, file=sys.stderr)
        return 1

    font = ImageFont.truetype(str(ttf_path), args.size)
    codepoints = build_codepoint_list(joyo_path)
    glyphs, missing = collect_glyphs(font, codepoints, args.size)
    digit_advance = equalize_digit_advances(glyphs)
    atlas_size = pack_glyphs(glyphs)

    write_png(glyphs, atlas_size, args.out_dir / (args.name + ".png"))
    write_json(glyphs, atlas_size, font, args.size, digit_advance, args.out_dir / (args.name + ".json"))

    print("文字数: %d(要求 %d、フォントに無い %d、アイコン 1)" % (len(glyphs), len(codepoints), len(missing)))
    if missing:
        print("フォントに無かった文字: " + " ".join("U+%04X" % cp for cp in missing))
    print("アトラス: %d x %d px(文字サイズ %d px、ウェイト %s)" % (atlas_size, atlas_size, args.size, args.weight))
    print("数字の固定幅: %.2f px" % digit_advance)
    print("出力: %s" % args.out_dir)
    return 0


if __name__ == "__main__":
    sys.exit(main())
