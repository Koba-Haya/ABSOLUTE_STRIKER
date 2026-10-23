"""蝶のテクスチャを描く(PIL + numpy。普通の Python で実行する)。

実行:
  python gen_textures.py --style mix              (出力先は既定の out/ フォルダ)
  python gen_textures.py --style morpho --out some/folder

作るもの:
  butterfly_wing.png ... 翅のテクスチャ。左半分 = 翅の表(玉虫色)、右半分 = 翅の裏(地味な茶色)。
                         右翅の 2 次元座標(wing_shape.py)をそのまま絵にしている。左翅は UV を鏡像にして同じ絵を使う。
  butterfly_body.png ... 胴(頭・胸・腹・触角)のテクスチャ。縦方向 = 頭から腹の先まで、横方向 = 周囲。

スタイル(玉虫色の方向性。ユーザーに選んでもらう):
  morpho ... 参考画像 3(モルフォ蝶)寄り。全面が鮮やかな青の金属光沢で、縁は茶色
  sunset ... 参考画像 2(ニシキオオツバメガ)寄り。黒地に緑の帯、後翅は橙・桃・青、尾の先は白
  mix    ... 前翅はモルフォの青(先端は黒と緑の帯)、後翅はニシキオオツバメガの橙と桃。尾の先は白(既定)
"""
import argparse
import colorsys
import math
import os

import numpy as np
from PIL import Image, ImageDraw

import wing_shape as ws

# 片側(表または裏)のピクセル数。幅 : 高さ = (U_MAX-U_MIN) : (V_MAX-V_MIN) にほぼ合わせる
HALF_W = 1024
HALF_H = 1536
ATLAS_W = HALF_W * 2
BODY_SIZE = 512

# 縁の暗い帯の幅と、白い点の間隔(翅の座標系の長さ)
EDGE_BAND = 0.055
EDGE_SOFT = 0.03
FRINGE_DOT_SPACING = 0.11

# 翅脈(放射状の線)の本数と太さ
VEIN_COUNT = 26
VEIN_WIDTH = 0.012
VEIN_ORIGIN = (0.06, 0.0)

SEED = 7


def hsv_to_rgb_np(h, s, v):
    """numpy 配列の HSV(0..1)を RGB に変換する(colorsys のベクトル版)。"""
    h = np.mod(h, 1.0)
    i = np.floor(h * 6.0).astype(int)
    f = h * 6.0 - i
    p = v * (1.0 - s)
    q = v * (1.0 - f * s)
    t = v * (1.0 - (1.0 - f) * s)
    i = i % 6
    r = np.choose(i, [v, q, p, p, t, v])
    g = np.choose(i, [t, v, v, q, p, p])
    b = np.choose(i, [p, p, t, v, v, q])
    return np.stack([r, g, b], axis=-1)


def smoothstep(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def value_noise(shape, cells, rng):
    """低解像度の乱数を双三次で拡大した、なめらかなノイズ(0..1)。cells = (横, 縦)の格子数。"""
    small = rng.random((cells[1], cells[0])).astype(np.float32)
    img = Image.fromarray((small * 255).astype(np.uint8)).resize((shape[1], shape[0]), Image.BICUBIC)
    return np.asarray(img, dtype=np.float32) / 255.0


def make_coords():
    """片側のピクセルごとの翅座標 (u, v) と、輪郭の内側の距離場・内外マスクを作る。"""
    xs = (np.arange(HALF_W) + 0.5) / HALF_W
    ys = 1.0 - (np.arange(HALF_H) + 0.5) / HALF_H
    u = ws.U_MIN + xs[None, :] * (ws.U_MAX - ws.U_MIN)
    v = ws.V_MIN + ys[:, None] * (ws.V_MAX - ws.V_MIN)
    u = np.broadcast_to(u, (HALF_H, HALF_W)).astype(np.float32)
    v = np.broadcast_to(v, (HALF_H, HALF_W)).astype(np.float32)

    outline = ws.smooth_outline(8)
    # 内外マスク: 輪郭を PIL で塗りつぶす
    mask_img = Image.new("L", (HALF_W, HALF_H), 0)
    pts = [(((p[0] - ws.U_MIN) / (ws.U_MAX - ws.U_MIN)) * HALF_W,
            (1.0 - (p[1] - ws.V_MIN) / (ws.V_MAX - ws.V_MIN)) * HALF_H) for p in outline]
    ImageDraw.Draw(mask_img).polygon(pts, fill=255)
    inside = np.asarray(mask_img, dtype=np.float32) > 127

    # 輪郭までの距離(輪郭を細かい点列にして、最も近い点との距離で近似する)
    dense = np.array(ws.resample_closed(outline, 0.01), dtype=np.float32)
    dist = np.full((HALF_H, HALF_W), 9.0, dtype=np.float32)
    chunk = 128
    for y0 in range(0, HALF_H, chunk):
        uu = u[y0:y0 + chunk, :, None]
        vv = v[y0:y0 + chunk, :, None]
        d = np.sqrt((uu - dense[None, None, :, 0]) ** 2 + (vv - dense[None, None, :, 1]) ** 2)
        dist[y0:y0 + chunk] = d.min(axis=2)
    dist = np.where(inside, dist, 0.0)
    return u, v, dist, inside


def vein_mask(u, v):
    """付け根から放射状に伸びる翅脈の線(0 = 線の上、1 = 線なし)。"""
    du = u - VEIN_ORIGIN[0]
    dv = v - VEIN_ORIGIN[1]
    r = np.sqrt(du * du + dv * dv) + 1e-4
    theta = np.arctan2(dv, du)
    # 角度を VEIN_COUNT 等分し、線までの角度のずれ x 半径 = 実際の距離
    cell = theta / (2.0 * math.pi) * VEIN_COUNT * 1.6
    frac = np.abs(cell - np.round(cell)) / (VEIN_COUNT * 1.6) * 2.0 * math.pi
    d = frac * r
    return smoothstep(VEIN_WIDTH * 0.4, VEIN_WIDTH, d)


def fringe_dots(u, v, dist):
    """縁の少し内側に並ぶ白い点(蝶の縁の模様)。輪郭沿いに並べるため、角度ではなく距離と座標の格子で近似する。"""
    # 縁から一定の距離にある帯のなかで、座標の細かい格子の中心に点を置く
    band = smoothstep(0.012, 0.022, dist) * (1.0 - smoothstep(0.034, 0.046, dist))
    gx = (u * 1.0 + v * 1.0) / FRINGE_DOT_SPACING
    gy = (u * 1.0 - v * 1.0) / FRINGE_DOT_SPACING
    cx = np.abs(gx - np.round(gx))
    cy = np.abs(gy - np.round(gy))
    dot = 1.0 - smoothstep(0.18, 0.30, np.sqrt(cx * cx + cy * cy))
    return band * dot


def style_top(style, u, v, dist, rng):
    """翅の表の色(HxWx3, 0..1)を返す。玉虫色 = 色相を位置でゆるやかに変え、光の筋を重ねる。"""
    hind = smoothstep(ws.FORE_HIND_SPLIT_V + 0.06, ws.FORE_HIND_SPLIT_V - 0.06, v)  # 後翅 = 1
    r = np.sqrt((u - 0.06) ** 2 + (v - 0.0) ** 2)
    ang = np.arctan2(v - 0.0, u - 0.06)

    # 光の筋(斜めの帯)。玉虫色の金属光沢の見せかけ
    sheen = 0.5 + 0.5 * np.sin((u * 3.2 - v * 1.3) * 5.5 + value_noise(u.shape, (6, 9), rng) * 3.0)
    mottle = value_noise(u.shape, (14, 20), rng)
    fine = value_noise(u.shape, (180, 270), rng)

    if style == "morpho":
        h = 0.60 - 0.05 * np.clip(r / 1.4, 0, 1) + 0.03 * (sheen - 0.5) + 0.02 * (mottle - 0.5)
        s = 0.88 - 0.25 * sheen * smoothstep(0.2, 0.9, r)
        val = 0.55 + 0.45 * sheen * smoothstep(0.05, 0.5, r) + 0.10 * (mottle - 0.5)
        rgb = hsv_to_rgb_np(h, s, np.clip(val, 0, 1))
        # 前翅の先端は黒く
        apex = smoothstep(0.78, 1.05, (u - 0.5) * 0.8 + (v - 0.30) * 1.0 + 0.55)
        apex = np.clip(apex * smoothstep(0.22, 0.42, v), 0, 1)
    elif style == "sunset":
        base = np.array([0.03, 0.05, 0.04], dtype=np.float32)
        rgb = np.broadcast_to(base, u.shape + (3,)).copy()
        # 前翅: 斜めの緑の帯(色相は緑から黄緑、端は青緑へ)
        bands = 0.5 + 0.5 * np.sin((u * 1.0 + v * 2.2) * 9.0 + mottle * 1.2)
        bands = smoothstep(0.45, 0.8, bands)
        h = 0.28 + 0.08 * (sheen - 0.5) + 0.10 * np.clip(r / 1.4, 0, 1)
        green = hsv_to_rgb_np(h, 0.85, 0.35 + 0.65 * bands)
        fore_w = (1.0 - hind) * smoothstep(0.0, 0.25, r)
        rgb = rgb * (1 - fore_w[..., None] * bands[..., None]) + green * (fore_w * bands)[..., None]
        # 後翅: 青緑の帯 -> 橙 -> 桃
        hb = smoothstep(0.30, 0.70, -v + 0.25 * u - 0.05)
        c1 = hsv_to_rgb_np(0.50 + 0.1 * (sheen - 0.5), 0.9, 0.85)
        rgb = rgb * (1 - (hind * hb * 0.6)[..., None]) + c1 * (hind * hb * 0.6)[..., None]
        patch = np.exp(-(((u - 1.0) / 0.22) ** 2 + ((v + 0.52) / 0.28) ** 2))
        orange = hsv_to_rgb_np(0.07 - 0.07 * smoothstep(0.2, 0.9, patch), 0.9, 0.95)
        pink = hsv_to_rgb_np(0.92, 0.7, 0.95)
        mixc = orange * (1 - smoothstep(0.55, 0.9, patch))[..., None] + pink * smoothstep(0.55, 0.9, patch)[..., None]
        rgb = rgb * (1 - (hind * patch)[..., None]) + mixc * (hind * patch)[..., None]
        apex = np.zeros_like(u)
    else:  # mix
        # 前翅: 付け根は青緑、外側へ青 -> 紫と色相が動く玉虫色
        hf = 0.50 + 0.14 * np.clip(r / 1.3, 0, 1) + 0.03 * (sheen - 0.5) + 0.02 * (mottle - 0.5)
        sf = 0.90 - 0.30 * sheen * smoothstep(0.3, 1.1, r)
        vf = 0.50 + 0.50 * sheen * smoothstep(0.05, 0.5, r) + 0.08 * (mottle - 0.5)
        fore = hsv_to_rgb_np(hf, np.clip(sf, 0, 1), np.clip(vf, 0, 1))
        # 前翅の先端: 黒地に緑の筋
        apex_core = smoothstep(0.80, 1.10, (u - 0.55) * 0.9 + (v - 0.15) * 0.9 + 0.35)
        apex_core = apex_core * (v > 0.12)
        streak = smoothstep(0.55, 0.85, 0.5 + 0.5 * np.sin((u * 0.7 + v * 1.6) * 24.0 + mottle * 2.0))
        green = hsv_to_rgb_np(0.30 + 0.05 * (sheen - 0.5), 0.85, 0.30 + 0.70 * streak)
        fore = fore * (1 - apex_core[..., None]) + (0.05 + 0.95 * green) * apex_core[..., None] * 0.9
        # 後翅: 付け根は青紫 -> 外側へ青緑 -> 縁へ橙と桃(尾の付近)
        hh = 0.62 - 0.10 * smoothstep(0.1, 0.9, r) + 0.04 * (sheen - 0.5)
        hind_rgb = hsv_to_rgb_np(hh, 0.85, 0.40 + 0.55 * sheen * smoothstep(0.05, 0.6, r))
        patch = np.exp(-(((u - 0.98) / 0.24) ** 2 + ((v + 0.58) / 0.34) ** 2))
        orange = hsv_to_rgb_np(0.07 - 0.06 * smoothstep(0.1, 0.9, patch), 0.92, 0.97)
        pink = hsv_to_rgb_np(0.93, 0.75, 0.95)
        pm = smoothstep(0.6, 0.95, patch)[..., None]
        patch_rgb = orange * (1 - pm) + pink * pm
        hind_rgb = hind_rgb * (1 - smoothstep(0.15, 0.55, patch)[..., None]) + patch_rgb * smoothstep(0.15, 0.55, patch)[..., None]
        # 後翅の黒い斑点(橙の中)
        for (cu, cv) in [(0.90, -0.46), (1.02, -0.60), (0.84, -0.66), (1.0, -0.38)]:
            spot = 1.0 - smoothstep(0.035, 0.055, np.sqrt((u - cu) ** 2 + (v - cv) ** 2))
            hind_rgb = hind_rgb * (1 - spot[..., None] * 0.95)
        rgb = fore * (1 - hind[..., None]) + hind_rgb * hind[..., None]
        apex = np.zeros_like(u)

    if style == "morpho":
        rgb = rgb * (1 - apex[..., None]) + np.array([0.03, 0.03, 0.04]) * apex[..., None]

    # 細かい鱗の粒感
    rgb = rgb * (0.90 + 0.20 * fine[..., None])

    # 翅脈(暗い線)
    vm = vein_mask(u, v)
    rgb = rgb * (0.55 + 0.45 * vm[..., None])

    # 縁の茶色い帯と、その内側の白い点
    edge_col = np.array([0.16, 0.09, 0.05], dtype=np.float32)
    edge = 1.0 - smoothstep(EDGE_BAND, EDGE_BAND + EDGE_SOFT, dist)
    if style == "sunset":
        edge = edge * 0.8
    rgb = rgb * (1 - edge[..., None]) + edge_col * edge[..., None]
    dots = fringe_dots(u, v, dist)
    rgb = rgb * (1 - dots[..., None]) + np.array([0.95, 0.93, 0.85]) * dots[..., None]

    # 尾状突起の先は白(輪郭の最下部)
    if style in ("sunset", "mix"):
        tail = smoothstep(-1.02, -1.14, v) * (u > 0.55) * (u < 0.9)
        rgb = rgb * (1 - tail[..., None]) + np.array([0.95, 0.95, 0.9]) * tail[..., None]
    return np.clip(rgb, 0, 1)


def style_under(u, v, dist, rng):
    """翅の裏。地味な茶色。縁の近くに目玉模様を置く(スタイル共通)。"""
    mottle = value_noise(u.shape, (16, 24), rng)
    fine = value_noise(u.shape, (180, 270), rng)
    base = np.array([0.30, 0.19, 0.10], dtype=np.float32)
    light = np.array([0.55, 0.40, 0.22], dtype=np.float32)
    k = (0.5 * mottle + 0.5 * smoothstep(0.0, 0.9, np.sqrt((u - 0.06) ** 2 + v ** 2) / 1.4))[..., None]
    rgb = base * (1 - k) + light * k
    rgb = rgb * (0.88 + 0.24 * fine[..., None])
    vm = vein_mask(u, v)
    rgb = rgb * (0.5 + 0.5 * vm[..., None])
    for (cu, cv, rad) in [(1.18, 0.30, 0.07), (1.02, -0.42, 0.08), (0.86, -0.60, 0.06)]:
        d = np.sqrt((u - cu) ** 2 + (v - cv) ** 2)
        ring = 1.0 - smoothstep(rad, rad + 0.012, d)
        inner = 1.0 - smoothstep(rad * 0.55, rad * 0.55 + 0.01, d)
        rgb = rgb * (1 - ring[..., None]) + np.array([0.85, 0.75, 0.45]) * ring[..., None]
        rgb = rgb * (1 - inner[..., None]) + np.array([0.05, 0.03, 0.03]) * inner[..., None]
    edge = 1.0 - smoothstep(EDGE_BAND, EDGE_BAND + EDGE_SOFT, dist)
    rgb = rgb * (1 - edge[..., None]) + np.array([0.12, 0.07, 0.04]) * edge[..., None]
    return np.clip(rgb, 0, 1)


def make_body_texture(rng):
    """胴のテクスチャ。縦(画像の上 = 頭、下 = 腹の先)に、頭の黒、胸の茶色い毛、腹の縞を描く。"""
    h = w = BODY_SIZE
    ys = np.arange(h)[:, None] / h   # 0 = 頭, 1 = 腹の先
    xs = np.arange(w)[None, :] / w
    fine = value_noise((h, w), (120, 120), rng)
    base = np.zeros((h, w, 3), dtype=np.float32)
    dark = np.array([0.04, 0.035, 0.03])
    brown = np.array([0.20, 0.11, 0.06])
    ochre = np.array([0.55, 0.30, 0.10])
    thorax = smoothstep(0.12, 0.18, ys) * (1 - smoothstep(0.36, 0.42, ys))
    abdomen = smoothstep(0.38, 0.45, ys)
    stripe = smoothstep(0.35, 0.65, 0.5 + 0.5 * np.sin(ys * 2 * math.pi * 14.0))
    col = dark + (brown - dark) * thorax[..., None] * (0.6 + 0.4 * fine[..., None])
    abd_col = dark + (ochre - dark) * (stripe * 0.5)[..., None]
    col = col * (1 - abdomen[..., None]) + abd_col * abdomen[..., None]
    # 触角の先と胴の左右(接合部)も同じ色でよい。点を少し足して虫っぽく
    col = col * (0.85 + 0.3 * fine[..., None])
    base[:] = col
    return np.clip(base, 0, 1)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--style", choices=["morpho", "sunset", "mix"], default="mix")
    parser.add_argument("--out", default=os.path.join(os.path.dirname(os.path.abspath(__file__)), "out"))
    args = parser.parse_args()
    os.makedirs(args.out, exist_ok=True)
    rng = np.random.default_rng(SEED)

    u, v, dist, inside = make_coords()
    top = style_top(args.style, u, v, dist, rng)
    under = style_under(u, v, dist, rng)
    # 輪郭の外側は縁の色で塗りつぶす(ミップマップで外の乱れた色が翅へにじまないように)
    outside_top = np.array([0.16, 0.09, 0.05], dtype=np.float32)
    outside_under = np.array([0.12, 0.07, 0.04], dtype=np.float32)
    top = np.where(inside[..., None], top, outside_top)
    under = np.where(inside[..., None], under, outside_under)
    atlas = np.concatenate([top, under], axis=1)
    Image.fromarray((atlas * 255).astype(np.uint8)).save(os.path.join(args.out, "butterfly_wing.png"))

    body = make_body_texture(rng)
    Image.fromarray((body * 255).astype(np.uint8)).save(os.path.join(args.out, "butterfly_body.png"))
    print("wrote textures to", args.out, "style =", args.style)


if __name__ == "__main__":
    main()
