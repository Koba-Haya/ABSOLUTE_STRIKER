"""Tripo で生成した飛竜モデル(立ち姿・翼を広げた姿)を、飛行姿勢の向き・サイズへ変換して書き出す。

  入力: SOURCE_DIR の dragon+3d+model.obj / .mtl / _basecolor.jpg(Tripo の出力)
  出力: このフォルダの dragon/ 以下

実行: python gen_dragon_pose.py          … 簡易ポーズ(翼を後ろへ振り、頭を持ち上げる)へ変形して dragon/dragon.obj へ
      python gen_dragon_pose.py --rest   … 変形せず、向きとサイズだけ揃えて dragon/rest/dragon_rest.obj へ
                                           (gen_dragon_rig.py が Blender でボーンとウェイトを入れるときの入力)

簡易ポーズは、リニアブレンドスキニングを自前で実装している:
  - 翼: 肩の関節を支点に後ろへ振る。頂点ごとの重み w(0=胴体 / 1=翼)で p' = (1-w)*p + w*R(p) と混ぜる
  - 頭: 首の付け根を支点に持ち上げる(元の立ち姿では顔が胸側を向いており、寝かせると地面を向くため)
座標の注意: 書き出す OBJ はファイル空間(右手系)。読み込み側が z を反転するので、
  ファイル上で機首が -Z なら、エンジン内では +Z(前)を向く。
"""
import math
import os
import shutil
import sys

SOURCE_DIR = r"C:\Users\haya2\Downloads\dragon 3d model"
SOURCE_NAME = "dragon+3d+model"
OUT_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "dragon")
# 変形前のモデルと .blend(制作用)は、exe にコピーされる resources の外へ置く
ART_DIR = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "art", "dragon"))

# --- 姿勢パラメータ ---
SHOULDER = (0.08, 0.55)        # 肩の関節(元モデルの x, y)。x は左右対称に使う
WING_SWEEP_DEG = 62.0          # 翼を後ろへ振る角度(大きいほど後退翼)
# 頭の持ち上げ: 首の付け根(元モデルの y)から上を、x 軸まわりに回す。重みは y で 0→1、翼の腕(|x| 大)は除く
NECK_PIVOT_Y = 0.63
HEAD_PITCH_DEG = 70.0
HEAD_WEIGHT_Y = (0.60, 0.68)
HEAD_WEIGHT_X = (0.07, 0.11)   # |x| がこの範囲で 1→0(頭と角だけを対象にする)
# 翼とみなす範囲(元モデル座標)。後ろ脚の発光フィン(x<0.3, y<0.46)は胴体側に残す
WING_FAR_X = (0.26, 0.34)      # |x| がこの範囲で 0→1: 外側はどの高さでも翼
WING_ROOT_X = (0.08, 0.18)     # 翼の付け根: |x| が 0→1
WING_ROOT_Y = (0.44, 0.50)     # 付け根は、この高さより上だけ翼
# --- 書き出しの寸法 ---
BODY_CENTER_Y = 0.47           # 元モデルで胴体の中心とみなす高さ
TARGET_BODY_LENGTH = 2.8       # 頭から尾までの長さ(エンジン空間の単位)


def smoothstep(a, b, x):
    t = max(0.0, min(1.0, (x - a) / (b - a)))
    return t * t * (3.0 - 2.0 * t)


def wing_weight(x, y):
    ax = abs(x)
    far = smoothstep(WING_FAR_X[0], WING_FAR_X[1], ax)
    root = smoothstep(WING_ROOT_X[0], WING_ROOT_X[1], ax) * smoothstep(WING_ROOT_Y[0], WING_ROOT_Y[1], y)
    return max(far, root)


def sweep_wing(p, w):
    """翼の重み w で、肩を支点に z 軸まわりへ回した位置と元の位置を混ぜる"""
    x, y, z = p
    if w <= 0.0:
        return p
    side = 1.0 if x >= 0 else -1.0
    phi = math.radians(WING_SWEEP_DEG)
    dx, dy = x - side * SHOULDER[0], y - SHOULDER[1]
    c, s = math.cos(phi), math.sin(phi)
    # 右翼(x>0)は時計回り、左翼は反時計回りに回す(翼の先端が下 = 機体の後方へ向かう)
    rx, ry = (dx * c + dy * s, -dx * s + dy * c) if side > 0 else (dx * c - dy * s, dx * s + dy * c)
    qx, qy = side * SHOULDER[0] + rx, SHOULDER[1] + ry
    return (x + (qx - x) * w, y + (qy - y) * w, z)


def head_weight(x, y):
    return smoothstep(HEAD_WEIGHT_Y[0], HEAD_WEIGHT_Y[1], y) * (1.0 - smoothstep(HEAD_WEIGHT_X[0], HEAD_WEIGHT_X[1], abs(x)))


def pitch_head(p, w):
    """首の付け根を支点に x 軸まわりへ回す(頭の顔が胸側 -z から前方 +y へ向く)"""
    x, y, z = p
    if w <= 0.0:
        return p
    t = math.radians(HEAD_PITCH_DEG)
    dy = y - NECK_PIVOT_Y
    qy = NECK_PIVOT_Y + dy * math.cos(t) - z * math.sin(t)
    qz = dy * math.sin(t) + z * math.cos(t)
    return (x, y + (qy - y) * w, z + (qz - z) * w)


def orient(p, scale):
    """胴体の中心を原点に、頭(+y)を -Z_file(=エンジンの前)、背中(+z)を +Y へ。x はそのまま"""
    x, y, z = p
    y -= BODY_CENTER_Y
    return (x * scale, z * scale, -y * scale)


def load_obj(path):
    V, VT, F = [], [], []
    for line in open(path, errors="ignore"):
        t = line.split()
        if not t:
            continue
        if t[0] == "v":
            V.append(tuple(map(float, t[1:4])))
        elif t[0] == "vt":
            VT.append((float(t[1]), float(t[2])))
        elif t[0] == "f":
            idx = []
            for item in t[1:]:
                a = item.split("/")
                idx.append((int(a[0]) - 1, int(a[1]) - 1 if len(a) > 1 and a[1] else 0))
            for k in range(1, len(idx) - 1):
                F.append((idx[0], idx[k], idx[k + 1]))
    return V, VT, F


def main(rest=False):
    V, VT, F = load_obj(os.path.join(SOURCE_DIR, SOURCE_NAME + ".obj"))
    ys = [p[1] for p in V]
    scale = TARGET_BODY_LENGTH / (max(ys) - min(ys))

    posed = []
    for p in V:
        if rest:
            posed.append(orient(p, scale))
            continue
        ww = wing_weight(p[0], p[1])
        wh = head_weight(p[0], p[1]) * (1.0 - ww)   # 翼と頭の重みの合計が 1 を超えないようにする
        q = sweep_wing(p, ww)
        q = pitch_head(q, wh) if wh > 0.0 else q
        posed.append(orient(q, scale))

    out_dir = os.path.join(ART_DIR, "rest") if rest else OUT_DIR
    obj_name = "dragon_rest" if rest else "dragon"
    os.makedirs(out_dir, exist_ok=True)
    shutil.copyfile(os.path.join(SOURCE_DIR, SOURCE_NAME + "_basecolor.jpg"), os.path.join(out_dir, "dragon.jpg"))
    with open(os.path.join(out_dir, obj_name + ".mtl"), "w") as f:
        f.write("newmtl Dragon\nKd 1.000000 1.000000 1.000000\nKs 0.200000 0.200000 0.200000\nNs 60.000000\n"
                "d 1.000000\nillum 2\nmap_Kd dragon.jpg\n")
    # 法線は書かない(読み込み時に Assimp が滑らかな法線を作る)。
    # 向き(z 反転と剛体回転)の変換は回転+一様スケールなので、面の巻き方向は元のまま
    with open(os.path.join(out_dir, obj_name + ".obj"), "w") as f:
        f.write(f"mtllib {obj_name}.mtl\no dragon\nusemtl Dragon\n")
        f.write("".join(f"v {p[0]:.6f} {p[1]:.6f} {p[2]:.6f}\n" for p in posed))
        f.write("".join(f"vt {u:.6f} {v:.6f}\n" for u, v in VT))
        f.write("s 1\n")
        for tri in F:
            f.write("f " + " ".join(f"{a + 1}/{b + 1}" for a, b in tri) + "\n")
    print(len(V), "verts,", len(F), "tris ->", out_dir)


if __name__ == "__main__":
    main(rest="--rest" in sys.argv)
