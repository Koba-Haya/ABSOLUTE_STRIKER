"""タイトル用の自機モデル(バルファルク風の鋭角な飛竜型ジェット)を生成する。
出力: striker.obj / striker.mtl / striker.png (このフォルダ)
実行: python gen_striker_model.py

座標系の注意:
  この関数内の座標は「エンジン空間」(x=右, y=上, z=前=機首)で書く。
  OBJ ファイルへ書き出すときに z を反転する(読み込み側が aiProcess_MakeLeftHanded で z を反転するため)。
  面の向きは、パーツごとの内部の点(hint)から外向きになるよう自動で揃える。
"""
import math
import os
from PIL import Image

OUT = os.path.dirname(os.path.abspath(__file__))

# パレット(1 枚の画像に色見本を並べ、面ごとに見本の中心の UV を指定する)
PALETTE = [
    ("armor", (236, 240, 245)),   # 白い装甲
    ("shade", (150, 165, 195)),   # 淡い青灰(翼の 2 枚目)
    ("steel", (34, 38, 52)),      # 暗い鋼(骨・ノズル)
    ("glow", (255, 50, 90)),      # 赤桃の発光
    ("gold", (240, 190, 70)),     # 金の差し色(角)
]
MAT = {name: i for i, (name, _) in enumerate(PALETTE)}

tris = []  # (material, [p0, p1, p2] in file space, normal in file space)


def to_file(p):
    return (p[0], p[1], -p[2])


def sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def norm(v):
    l = math.sqrt(dot(v, v)) or 1.0
    return (v[0] / l, v[1] / l, v[2] / l)


def add_tri(mat, a, b, c, hint):
    """エンジン空間の三角形を、hint(パーツ内部の点)から外向きになる向きで登録する"""
    fa, fb, fc, fh = to_file(a), to_file(b), to_file(c), to_file(hint)
    n = cross(sub(fb, fa), sub(fc, fa))
    center = ((fa[0] + fb[0] + fc[0]) / 3, (fa[1] + fb[1] + fc[1]) / 3, (fa[2] + fb[2] + fc[2]) / 3)
    if dot(n, sub(center, fh)) < 0:
        fb, fc = fc, fb
        n = (-n[0], -n[1], -n[2])
    if dot(n, n) < 1e-12:
        return
    tris.append((MAT[mat], [fa, fb, fc], norm(n)))


def ring(section):
    z, cx, cy, hw, ht, hb = section
    shape = [(0, ht), (0.6 * hw, 0.75 * ht), (hw, 0), (0.6 * hw, -0.75 * hb),
             (0, -hb), (-0.6 * hw, -0.75 * hb), (-hw, 0), (-0.6 * hw, 0.75 * ht)]
    return [(cx + x, cy + y, z) for x, y in shape]


def loft(mat, sections):
    """断面(z, cx, cy, 半幅, 上の高さ, 下の高さ)を z 方向に繋いだ筒。両端はふさぐ"""
    rings = [ring(s) for s in sections]
    n = len(rings[0])
    for r0, r1, s0, s1 in zip(rings, rings[1:], sections, sections[1:]):
        hint = ((s0[1] + s1[1]) / 2, (s0[2] + s1[2]) / 2, (s0[0] + s1[0]) / 2)
        for i in range(n):
            j = (i + 1) % n
            add_tri(mat, r0[i], r0[j], r1[j], hint)
            add_tri(mat, r0[i], r1[j], r1[i], hint)
    for r, s, other in ((rings[0], sections[0], sections[1]), (rings[-1], sections[-1], sections[-2])):
        center = (s[1], s[2], s[0])
        hint = (other[1], other[2], other[0])
        for i in range(n):
            add_tri(mat, r[i], r[(i + 1) % n], center, hint)


def slab(mat, top, bottom):
    """同じ頂点数の 2 枚の多角形(top / bottom)で作る板。多角形は重心から見て星形であること"""
    pts = top + bottom
    hint = tuple(sum(p[k] for p in pts) / len(pts) for k in range(3))
    for poly in (top, bottom):
        c = tuple(sum(p[k] for p in poly) / len(poly) for k in range(3))
        for i in range(len(poly)):
            add_tri(mat, poly[i], poly[(i + 1) % len(poly)], c, hint)
    n = len(top)
    for i in range(n):
        j = (i + 1) % n
        add_tri(mat, top[i], top[j], bottom[j], hint)
        add_tri(mat, top[i], bottom[j], bottom[i], hint)


def wing_blade(mat, xz_points, sign, thickness, dihedral=0.12, base_y=0.08):
    """主翼の 1 枚。xz は右翼(x>0)の平面形。上反角(外側ほど上がる)と厚みを付ける"""
    top, bottom = [], []
    for x, z in xz_points:
        y = base_y + dihedral * x
        top.append((sign * x, y + thickness, z))
        bottom.append((sign * x, y - thickness, z))
    slab(mat, top, bottom)


# ---------------- 胴体(白い装甲) ----------------
# (z, cx, cy, 半幅, 上の高さ, 下の高さ): 尾 → 胴 → 首 → 頭 → 鼻先
loft("armor", [
    (-2.8, 0, 0.00, 0.04, 0.04, 0.04),
    (-2.0, 0, 0.02, 0.14, 0.12, 0.10),
    (-1.0, 0, 0.05, 0.34, 0.26, 0.20),
    (0.0, 0, 0.08, 0.50, 0.36, 0.26),
    (0.8, 0, 0.06, 0.40, 0.30, 0.22),
    (1.35, 0, 0.08, 0.20, 0.18, 0.14),
    (1.70, 0, 0.10, 0.26, 0.18, 0.12),
    (2.20, 0, 0.06, 0.20, 0.13, 0.08),
    (2.65, 0, 0.03, 0.04, 0.04, 0.04),
])

# 背中の暗い鋼のスパイン(胴体の上に細く載せる)
loft("steel", [
    (-1.4, 0, 0.17, 0.05, 0.05, 0.03),
    (-0.4, 0, 0.40, 0.08, 0.05, 0.03),
    (0.5, 0, 0.42, 0.07, 0.05, 0.03),
    (1.2, 0, 0.25, 0.04, 0.04, 0.03),
])

# 角(頭の後ろへ流れる 2 本。外側へ開き、上へ反る)
for sign in (1, -1):
    loft("gold", [
        (1.65, sign * 0.12, 0.26, 0.05, 0.05, 0.04),
        (1.20, sign * 0.20, 0.42, 0.045, 0.04, 0.035),
        (0.75, sign * 0.30, 0.58, 0.03, 0.03, 0.025),
        (0.45, sign * 0.36, 0.68, 0.01, 0.01, 0.01),
    ])
    # 目(発光)
    loft("glow", [
        (1.92, sign * 0.20, 0.13, 0.03, 0.03, 0.03),
        (2.10, sign * 0.17, 0.11, 0.03, 0.03, 0.03),
    ])

# ---------------- 主翼(左右) ----------------
for sign in (1, -1):
    # 前縁の骨(暗い鋼)
    wing_blade("steel", [(0.45, 0.9), (1.8, 0.3), (3.3, -1.3), (3.1, -1.3), (1.7, 0.2), (0.45, 0.75)], sign, 0.07)
    wing_blade("armor", [(0.45, 0.9), (1.8, 0.3), (3.3, -1.3), (2.0, -0.8), (0.6, -0.1)], sign, 0.04)
    wing_blade("shade", [(0.55, -0.1), (2.0, -0.8), (2.9, -2.1), (1.5, -1.5), (0.5, -0.9)], sign, 0.04)
    wing_blade("steel", [(0.4, -0.9), (1.5, -1.5), (2.0, -2.6), (1.0, -2.0), (0.3, -1.6)], sign, 0.035)
    # 翼の上の発光ライン(翼の根元から先端へ)
    wing_blade("glow", [(0.7, 0.3), (1.9, -0.05), (2.8, -0.9), (1.9, -0.25), (0.7, 0.1)], sign, 0.012,
               dihedral=0.12, base_y=0.125)

    # 尾の垂直尾翼(外側へ少し傾けた板)
    fin_top, fin_bottom = [], []
    for y, z in [(0.12, -1.2), (0.78, -2.1), (0.62, -2.55), (0.10, -2.3)]:
        fin_top.append((sign * (0.12 + 0.18 * y) + 0.025, y, z))
        fin_bottom.append((sign * (0.12 + 0.18 * y) - 0.025, y, z))
    slab("armor", fin_top, fin_bottom)

    # エンジンノズル(鋼の筒 + 発光の先端)
    loft("steel", [
        (-1.25, sign * 0.28, 0.05, 0.12, 0.12, 0.12),
        (-1.95, sign * 0.28, 0.05, 0.10, 0.10, 0.10),
    ])
    loft("glow", [
        (-1.95, sign * 0.28, 0.05, 0.08, 0.08, 0.08),
        (-2.15, sign * 0.28, 0.05, 0.05, 0.05, 0.05),
    ])


# ---------------- 書き出し ----------------
def write_outputs():
    swatch_w = 32
    img = Image.new("RGB", (swatch_w * len(PALETTE), 8))
    for i, (_, color) in enumerate(PALETTE):
        img.paste(color, (i * swatch_w, 0, (i + 1) * swatch_w, 8))
    img.save(os.path.join(OUT, "striker.png"))

    with open(os.path.join(OUT, "striker.mtl"), "w") as f:
        f.write("newmtl Striker\nNs 80.000000\nKa 1.000000 1.000000 1.000000\n"
                "Kd 1.000000 1.000000 1.000000\nKs 0.300000 0.300000 0.300000\n"
                "d 1.000000\nillum 2\nmap_Kd striker.png\n")

    lines = ["mtllib striker.mtl", "o striker", "usemtl Striker"]
    uv_lines = [f"vt {(i + 0.5) / len(PALETTE):.6f} 0.500000" for i in range(len(PALETTE))]
    lines += uv_lines
    vn_lines, v_lines, f_lines = [], [], []
    for mat, pts, n in tris:
        vn_lines.append(f"vn {n[0]:.6f} {n[1]:.6f} {n[2]:.6f}")
        base = len(v_lines)
        for p in pts:
            v_lines.append(f"v {p[0]:.6f} {p[1]:.6f} {p[2]:.6f}")
        ni = len(vn_lines)
        f_lines.append("f " + " ".join(f"{base + k + 1}/{mat + 1}/{ni}" for k in range(3)))
    with open(os.path.join(OUT, "striker.obj"), "w") as f:
        f.write("\n".join(lines[:3] + v_lines + uv_lines + vn_lines + ["s off"] + f_lines) + "\n")
    print(len(tris), "triangles")


write_outputs()
