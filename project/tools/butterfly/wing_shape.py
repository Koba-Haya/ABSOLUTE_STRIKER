"""蝶の翅の輪郭と UV の対応表。Blender 側(gen_butterfly.py)と PIL 側(gen_textures.py)で共有する。

bpy に依存しない純粋な Python だけで書く。こうすると、モデルとテクスチャが同じ輪郭を見ることになり、
輪郭を直したときに両方が自動で揃う。

翅の 2 次元座標(右翅 = +X 側。左翅は X を反転して作る):
  u : 胴の中心から外側へ(0 が胴、大きいほど翅の先端)
  v : 前後方向(正が頭の側、負がお尻の側)
"""
import math

# 右翅の輪郭(反時計回り)。(u, v)。前翅 = v が正の側、後翅 = v が負の側。尾状突起は後翅の先
# この点列を Catmull-Rom で滑らかにつないだものが実際の輪郭になる
OUTLINE_CONTROL = [
    (0.05, 0.12),    # 前翅の付け根(胴の前側)
    (0.14, 0.34),
    (0.45, 0.58),
    (0.85, 0.72),    # 前縁
    (1.18, 0.73),
    (1.34, 0.56),    # 前翅の先端
    (1.34, 0.36),    # 外縁
    (1.25, 0.14),
    (1.17, 0.00),    # 前翅と後翅の境目の切れ込み
    (1.14, -0.20),
    (1.05, -0.46),
    (0.93, -0.64),
    (0.90, -0.80),   # 尾状突起の根元(外側)
    (0.80, -1.05),
    (0.74, -1.22),   # 尾状突起の先
    (0.67, -1.04),
    (0.66, -0.78),   # 尾状突起の根元(内側)
    (0.46, -0.70),
    (0.28, -0.56),
    (0.14, -0.38),
    (0.05, -0.22),   # 後翅の付け根(胴の後ろ側)
]

# 翅の外形を覆う範囲。UV はこの範囲を 0..1 に正規化して作る
U_MIN, U_MAX = 0.0, 1.40
V_MIN, V_MAX = -1.30, 0.80

# 前翅と後翅の境目(おおよそ v = 0 付近)。テクスチャで前翅と後翅の色を塗り分けるときに使う
FORE_HIND_SPLIT_V = -0.02

# 翅の外側の長さ(ボーンの分割と、ウェイトの境目に使う)
WING_SPAN = 1.34


def catmull_rom(points, samples_per_segment):
    """閉じた点列を Catmull-Rom スプラインで補間して返す(輪郭を滑らかにするため)。"""
    n = len(points)
    out = []
    for i in range(n):
        p0 = points[(i - 1) % n]
        p1 = points[i]
        p2 = points[(i + 1) % n]
        p3 = points[(i + 2) % n]
        for s in range(samples_per_segment):
            t = s / samples_per_segment
            t2, t3 = t * t, t * t * t
            x = 0.5 * ((2 * p1[0]) + (-p0[0] + p2[0]) * t
                       + (2 * p0[0] - 5 * p1[0] + 4 * p2[0] - p3[0]) * t2
                       + (-p0[0] + 3 * p1[0] - 3 * p2[0] + p3[0]) * t3)
            y = 0.5 * ((2 * p1[1]) + (-p0[1] + p2[1]) * t
                       + (2 * p0[1] - 5 * p1[1] + 4 * p2[1] - p3[1]) * t2
                       + (-p0[1] + 3 * p1[1] - 3 * p2[1] + p3[1]) * t3)
            out.append((x, y))
    return out


def smooth_outline(samples_per_segment=8):
    """滑らかにした輪郭(点列)を返す。"""
    return catmull_rom(OUTLINE_CONTROL, samples_per_segment)


def resample_closed(points, spacing):
    """閉じた折れ線を、ほぼ等間隔(spacing)の点列にし直す。三角形の大きさを揃えるため。"""
    n = len(points)
    seg_len = []
    total = 0.0
    for i in range(n):
        a, b = points[i], points[(i + 1) % n]
        d = math.hypot(b[0] - a[0], b[1] - a[1])
        seg_len.append(d)
        total += d
    count = max(8, int(round(total / spacing)))
    step = total / count
    out = []
    seg = 0
    pos_in_seg = 0.0
    for _ in range(count):
        while pos_in_seg > seg_len[seg]:
            pos_in_seg -= seg_len[seg]
            seg = (seg + 1) % n
        a, b = points[seg], points[(seg + 1) % n]
        t = pos_in_seg / seg_len[seg] if seg_len[seg] > 0 else 0.0
        out.append((a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t))
        pos_in_seg += step
    return out


def point_in_polygon(x, y, poly):
    """偶奇規則による点の内外判定(poly は点列)。"""
    inside = False
    n = len(poly)
    j = n - 1
    for i in range(n):
        xi, yi = poly[i]
        xj, yj = poly[j]
        if (yi > y) != (yj > y) and x < (xj - xi) * (y - yi) / (yj - yi) + xi:
            inside = not inside
        j = i
    return inside


def distance_to_polygon(x, y, poly):
    """点から輪郭までの最短距離(境目に近い点を間引くために使う)。"""
    best = 1e9
    n = len(poly)
    for i in range(n):
        ax, ay = poly[i]
        bx, by = poly[(i + 1) % n]
        dx, dy = bx - ax, by - ay
        l2 = dx * dx + dy * dy
        t = 0.0 if l2 == 0 else max(0.0, min(1.0, ((x - ax) * dx + (y - ay) * dy) / l2))
        px, py = ax + dx * t, ay + dy * t
        best = min(best, math.hypot(x - px, y - py))
    return best


def to_uv(u, v):
    """翅の 2 次元座標 (u, v) を、テクスチャの片側(0..1 の正方形)の UV に変換する。
    UV の (0,0) は左下。戻り値は (s, t)。"""
    s = (u - U_MIN) / (U_MAX - U_MIN)
    t = (v - V_MIN) / (V_MAX - V_MIN)
    return s, t
