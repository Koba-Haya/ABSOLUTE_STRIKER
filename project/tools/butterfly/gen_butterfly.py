"""蝶のモデル(メッシュ + ボーン + ウェイト + アニメーション)を作り、glTF で書き出す。Blender の中で実行する。

実行(通常は build.ps1 から呼ぶ):
  "C:\\Program Files\\Blender Foundation\\Blender 4.4\\blender.exe" -b -P gen_butterfly.py -- --textures out_mix --out ..\\..\\Application\\resources\\butterfly

引数(-- のあと):
  --textures <dir>  gen_textures.py が作った butterfly_wing.png / butterfly_body.png のあるフォルダ
  --out <dir>       butterfly.gltf などを書き出すフォルダ
  --blend <file>    途中経過を保存する .blend(手で調整したいときに開く)。省略可

座標(Blender): X = 右、Y = 前(頭は +Y)、Z = 上。Blender の +Y は glTF では -Z になり、エンジンは MakeLeftHanded で Z を反転するので、エンジンでは頭が +Z になる
  (PlayerComponent が前方 = +Z として回すため)。以下の設計値は「頭が -Y」の座標で書いてあり、メッシュとボーンを作る瞬間に fy() で Y を反転する。
  ゲームの座標系への向きの合わせは、自機を差し替えるステップで確認する。

メッシュ(2 つに分ける。崩壊の演出で翅だけを砕き、胴は別の動きをさせるため):
  Wings ... 左右の翅。細かい三角形の格子。表と裏を別の面として持つ(裏面カリングされても両面が見える)
  Body  ... 頭・胸・腹・触角を 1 本のなめらかな筒としてつないだもの

ボーン(階層):
  root -> body -> head -> antenna_L / antenna_R / muzzle(頭の先。ウェイトなし。発射位置)
               -> abdomen -> abdomen_tip
               -> wing_base_L -> wing_mid_L -> wing_tip_L   (右も同じ。_R)

アニメーション(1 ファイルに複数。名前で選ぶ): Flap(羽ばたき・待機)、Glide(滑空)、Hit(被弾でよろめく)、Tumble(墜落のきりもみ)
"""
import math
import os
import random
import sys

import bmesh
import bpy
from mathutils import Matrix, Quaternion, Vector, geometry

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import wing_shape as ws  # noqa: E402

# ---------------------------------------------------------------- 設計値(名前付き定数)
FPS = 30

# 基本の姿勢の機首上げ(度)。ゲームのカメラは真後ろから見るので、翼を水平にすると、羽ばたきの面が体の軸を含み、
# 真横の細い線にしか見えない。実際の蝶のように機首を上げて、翼の面が後ろから見えるようにする。
# メッシュとボーンの基本の姿勢に焼き込む(エンジン側の定数にしない)。頭は +Y のまま、左右も変えない
PITCH_DEG = 30.0
PITCH_MATRIX = Matrix.Rotation(math.radians(PITCH_DEG), 4, "X")   # +X まわりの正の回転 = 頭(+Y)が上(+Z)へ

# 翅のメッシュ
WING_TRI_SPACING = 0.050      # 三角形の目安の大きさ。小さいほど細かい(崩壊の縁が自然になる)
WING_JITTER = 0.22            # 内側の点を格子からずらす割合(規則的すぎない破片にするため)
WING_THICKNESS = 0.008        # 表面と裏面の間隔
WING_DIHEDRAL = 0.05          # 翅の先が少し上がる量(u に比例)
WING_SEED = 11
WING_EDGE_MARGIN = 0.65       # 輪郭の点列の近くには内側の点を置かない(間隔に対する割合)

# 翅のボーンの区切り(u 方向)
WING_BONE_U = (0.05, 0.50, 0.95, 1.40)
WEIGHT_BASE_FADE = (0.25, 0.65)   # この間で base -> mid
WEIGHT_TIP_FADE = (0.75, 1.15)    # この間で mid -> tip

# 胴のプロファイル (y, 半径)。頭(-Y)から腹の先(+Y)まで
BODY_PROFILE = [
    (-0.52, 0.000), (-0.50, 0.050), (-0.45, 0.088), (-0.39, 0.100), (-0.33, 0.085), (-0.29, 0.070),
    (-0.24, 0.095), (-0.16, 0.135), (-0.05, 0.150), (0.07, 0.140), (0.15, 0.105), (0.23, 0.088),
    (0.40, 0.088), (0.60, 0.078), (0.80, 0.056), (0.94, 0.030), (1.00, 0.000),
]
BODY_RING_SEGMENTS = 14
BODY_RINGS = 56
BODY_Z_SQUASH = 0.9
BODY_Y_FRONT = -0.52
BODY_Y_REAR = 1.00

# 胴のボーンの区切り(y)
HEAD_FADE = (-0.36, -0.26)
ABDOMEN_FADE = (0.08, 0.24)
ABDOMEN_TIP_FADE = (0.45, 0.70)

# 触角
ANTENNA_BASE = Vector((0.030, -0.47, 0.070))
ANTENNA_MID = Vector((0.110, -0.78, 0.230))
ANTENNA_END = Vector((0.230, -1.00, 0.270))
ANTENNA_RINGS = 9
ANTENNA_SEGMENTS = 5
ANTENNA_RADIUS = 0.011
ANTENNA_CLUB_RADIUS = 0.024
ANTENNA_DARK_UV = (0.5, 0.97)

# 発射位置の印のボーン(エンジンは GetBoneWorldMatrix(名前) でこの位置を引く)。名前は Application 側の定数と揃える
MUZZLE_BONE = "muzzle"
MUZZLE_BONE_LENGTH = 0.10     # ボーンの長さ(向きを持たせるためだけの値。位置は頭の先)

# 見た目の名前
WING_MATERIAL = "ButterflyWing"
BODY_MATERIAL = "ButterflyBody"
WING_TEXTURE = "butterfly_wing.png"
BODY_TEXTURE = "butterfly_body.png"

# アニメーション名(エンジン側からこの名前で選ぶ)
ANIM_FLAP = "Flap"
ANIM_GLIDE = "Glide"
ANIM_HIT = "Hit"
ANIM_TUMBLE = "Tumble"


def fy(y):
    """設計値の Y(頭が -Y)を、実際の Y(頭が +Y)へ変換する。メッシュとボーンの位置を作る瞬間にだけ使う。"""
    return -y


def smoothstep(e0, e1, x):
    t = max(0.0, min(1.0, (x - e0) / (e1 - e0)))
    return t * t * (3.0 - 2.0 * t)


# ---------------------------------------------------------------- シーンの準備
def reset_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.render.fps = FPS
    return scene


def make_material(name, texture_path):
    """画像テクスチャを基本色にしたマテリアル。glTF の書き出しが baseColorTexture にする。"""
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    bsdf = next(n for n in mat.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    tex = mat.node_tree.nodes.new("ShaderNodeTexImage")
    tex.image = bpy.data.images.load(texture_path)
    mat.node_tree.links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = 0.4
    bsdf.inputs["Metallic"].default_value = 0.0
    return mat


# ---------------------------------------------------------------- 翅のメッシュ
def triangulate_wing():
    """右翅の 2 次元の三角形分割。(頂点リスト [(u,v)], 三角形リスト [(a,b,c)]) を返す。
    輪郭の点列 + 内側のずらした格子点を、制約付きドロネー三角形分割でつなぐ。"""
    rng = random.Random(WING_SEED)
    outline = ws.smooth_outline(8)
    boundary = ws.resample_closed(outline, WING_TRI_SPACING)
    pts = list(boundary)

    # 内側の点: 六角格子(三角形がほぼ正三角形になる)を少しずらす
    h = WING_TRI_SPACING
    row_h = h * math.sqrt(3.0) / 2.0
    v = ws.V_MIN
    row = 0
    while v < ws.V_MAX:
        u = ws.U_MIN + (0.5 * h if row % 2 else 0.0)
        while u < ws.U_MAX:
            pu = u + rng.uniform(-1, 1) * WING_JITTER * h
            pv = v + rng.uniform(-1, 1) * WING_JITTER * h
            if ws.point_in_polygon(pu, pv, outline) and \
                    ws.distance_to_polygon(pu, pv, boundary) > h * WING_EDGE_MARGIN:
                pts.append((pu, pv))
            u += h
        v += row_h
        row += 1

    n_b = len(boundary)
    edges = [(i, (i + 1) % n_b) for i in range(n_b)]
    coords = [Vector(p) for p in pts]
    res = geometry.delaunay_2d_cdt(coords, edges, [], 0, 1e-6)
    out_verts, out_faces = res[0], res[2]
    tris = []
    for f in out_faces:
        if len(f) != 3:
            continue
        cx = sum(out_verts[i].x for i in f) / 3.0
        cy = sum(out_verts[i].y for i in f) / 3.0
        if ws.point_in_polygon(cx, cy, outline):
            tris.append(tuple(f))
    return [(p.x, p.y) for p in out_verts], tris


def wing_bone_weights(u, side):
    """翅の頂点のウェイト {ボーン名: 重み}。u(外側へ)に応じて base -> mid -> tip へ受け渡す。"""
    tip = smoothstep(WEIGHT_TIP_FADE[0], WEIGHT_TIP_FADE[1], u)
    base = 1.0 - smoothstep(WEIGHT_BASE_FADE[0], WEIGHT_BASE_FADE[1], u)
    mid = max(0.0, 1.0 - base - tip)
    return {"wing_base_" + side: base, "wing_mid_" + side: mid, "wing_tip_" + side: tip}


def build_wings(material):
    """左右の翅(表と裏)を 1 つのメッシュにまとめ、頂点グループでウェイトを付ける。"""
    verts2d, tris = triangulate_wing()
    mesh = bpy.data.meshes.new("WingsMesh")
    obj = bpy.data.objects.new("Wings", mesh)
    bpy.context.scene.collection.objects.link(obj)
    bm = bmesh.new()
    uv_layer = bm.loops.layers.uv.new("UVMap")
    weights = []   # (bmesh 頂点, {グループ名: 重み})
    half = WING_THICKNESS * 0.5

    for side, sx in (("R", 1.0), ("L", -1.0)):
        for surface in ("top", "bottom"):
            sz = 1.0 if surface == "top" else -1.0
            bverts = []
            for (u, v) in verts2d:
                x = sx * u
                y = fy(-v)
                z = WING_DIHEDRAL * u + sz * half
                bv = bm.verts.new((x, y, z))
                bverts.append(bv)
                weights.append((bv, wing_bone_weights(u, side)))
            vert_index = {bv: i for i, bv in enumerate(bverts)}
            for (a, b, c) in tris:
                # 表は +Z、裏は -Z を向くように、頂点順を実際の位置から決める
                pa, pb, pc = bverts[a].co, bverts[b].co, bverts[c].co
                nz = (pb - pa).cross(pc - pa).z
                order = (a, b, c) if (nz > 0) == (sz > 0) else (a, c, b)
                face = bm.faces.new([bverts[i] for i in order])
                for loop in face.loops:
                    idx = vert_index[loop.vert]
                    s, t = ws.to_uv(*verts2d[idx])
                    s = s * 0.5 if surface == "top" else 0.5 + s * 0.5
                    loop[uv_layer].uv = (s, t)
    bm.to_mesh(mesh)
    bm.free()
    mesh.transform(PITCH_MATRIX)   # 基本の姿勢の機首上げを焼き込む(ウェイトは頂点の番号で持つので、動かしても変わらない)
    mesh.materials.append(material)
    for poly in mesh.polygons:
        poly.use_smooth = True

    groups = {}
    for vi, (bv, wmap) in enumerate(weights):
        for name, w in wmap.items():
            if w <= 1e-4:
                continue
            if name not in groups:
                groups[name] = obj.vertex_groups.new(name=name)
            groups[name].add([vi], w, "REPLACE")
    return obj


# ---------------------------------------------------------------- 胴のメッシュ
def body_radius(y):
    """胴のプロファイルを cos 補間した半径。"""
    for i in range(len(BODY_PROFILE) - 1):
        y0, r0 = BODY_PROFILE[i]
        y1, r1 = BODY_PROFILE[i + 1]
        if y0 <= y <= y1:
            t = (y - y0) / (y1 - y0)
            t = 0.5 - 0.5 * math.cos(t * math.pi)
            return r0 + (r1 - r0) * t
    return 0.0


def body_bone_weights(y):
    """胴の頂点のウェイト(y に応じて head / body / abdomen / abdomen_tip)。"""
    head = 1.0 - smoothstep(HEAD_FADE[0], HEAD_FADE[1], y)
    abd_all = smoothstep(ABDOMEN_FADE[0], ABDOMEN_FADE[1], y)
    tip = smoothstep(ABDOMEN_TIP_FADE[0], ABDOMEN_TIP_FADE[1], y) * abd_all
    abd = abd_all - tip
    body = max(0.0, 1.0 - head - abd_all)
    return {"head": head, "body": body, "abdomen": abd, "abdomen_tip": tip}


def add_tube(bm, uv_layer, ring_centers, radii, segments, uv_for_ring, weights_for_ring, weights_out):
    """中心線に沿った筒を作る(最初と最後の半径が 0 なら先端は閉じる)。"""
    rings = []
    for ci, center in enumerate(ring_centers):
        r = radii[ci]
        ring = []
        if r <= 1e-6:
            bv = bm.verts.new(center)
            weights_out.append((bv, weights_for_ring(ci)))
            ring = [bv] * segments
        else:
            for k in range(segments):
                ang = 2.0 * math.pi * k / segments
                p = center + Vector((math.cos(ang) * r, 0.0, math.sin(ang) * r * BODY_Z_SQUASH))
                bv = bm.verts.new(p)
                weights_out.append((bv, weights_for_ring(ci)))
                ring.append(bv)
        rings.append(ring)
    for ci in range(len(rings) - 1):
        for k in range(segments):
            k2 = (k + 1) % segments
            quad = [rings[ci][k], rings[ci + 1][k], rings[ci + 1][k2], rings[ci][k2]]  # Y を反転したので、面の向きを保つため頂点の順を逆にする
            uniq = []
            for q in quad:
                if q not in uniq:
                    uniq.append(q)
            if len(uniq) < 3:
                continue
            face = bm.faces.new(uniq)
            for loop in face.loops:
                s, t = uv_for_ring(ci, loop.vert, rings, segments)
                loop[uv_layer].uv = (s, t)
    return rings


def build_body(material):
    """頭・胸・腹をなめらかな 1 本の筒として作り、触角を足す。"""
    mesh = bpy.data.meshes.new("BodyMesh")
    obj = bpy.data.objects.new("Body", mesh)
    bpy.context.scene.collection.objects.link(obj)
    bm = bmesh.new()
    uv_layer = bm.loops.layers.uv.new("UVMap")
    weights = []

    # 本体: y を等間隔に刻んだリングの列
    ys = [BODY_Y_FRONT + (BODY_Y_REAR - BODY_Y_FRONT) * i / (BODY_RINGS - 1) for i in range(BODY_RINGS)]
    centers = [Vector((0.0, fy(y), 0.0)) for y in ys]
    radii = [body_radius(y) for y in ys]

    def body_uv(ci, vert, rings, segments):
        ring = rings[ci]
        k = ring.index(vert) if vert in ring else 0
        s = k / segments
        t = 1.0 - (ys[ci] - BODY_Y_FRONT) / (BODY_Y_REAR - BODY_Y_FRONT)
        return s, t

    add_tube(bm, uv_layer, centers, radii, BODY_RING_SEGMENTS, body_uv,
             lambda ci: body_bone_weights(ys[ci]), weights)

    # 触角(左右): 3 点のベジェに沿った細い筒。先は少し太くして棒状にする
    for side, sx in (("R", 1.0), ("L", -1.0)):  # 頭が +Y のとき、+X が右
        p0 = Vector((ANTENNA_BASE.x * sx, fy(ANTENNA_BASE.y), ANTENNA_BASE.z))
        p1 = Vector((ANTENNA_MID.x * sx, fy(ANTENNA_MID.y), ANTENNA_MID.z))
        p2 = Vector((ANTENNA_END.x * sx, fy(ANTENNA_END.y), ANTENNA_END.z))
        centers_a, radii_a = [], []
        for i in range(ANTENNA_RINGS):
            t = i / (ANTENNA_RINGS - 1)
            p = p0 * (1 - t) ** 2 + p1 * 2 * (1 - t) * t + p2 * t * t
            centers_a.append(p)
            club = smoothstep(0.78, 0.92, t)
            radii_a.append(ANTENNA_RADIUS + (ANTENNA_CLUB_RADIUS - ANTENNA_RADIUS) * club)
        radii_a[-1] = 0.0
        name = "antenna_" + side
        add_tube(bm, uv_layer, centers_a, radii_a, ANTENNA_SEGMENTS,
                 lambda ci, vert, rings, segments: ANTENNA_DARK_UV,
                 lambda ci, n=name: {n: 1.0}, weights)

    bm.to_mesh(mesh)
    bm.free()
    mesh.transform(PITCH_MATRIX)   # 翅と同じ機首上げ
    mesh.materials.append(material)
    for poly in mesh.polygons:
        poly.use_smooth = True

    groups = {}
    for vi, (bv, wmap) in enumerate(weights):
        for name, w in wmap.items():
            if w <= 1e-4:
                continue
            if name not in groups:
                groups[name] = obj.vertex_groups.new(name=name)
            groups[name].add([vi], w, "REPLACE")
    return obj


# ---------------------------------------------------------------- ボーン
def build_armature():
    """ボーンを作る。頭の位置、先の位置、親を表で書く(あとから手で動かすとき分かりやすいように)。"""
    arm_data = bpy.data.armatures.new("ButterflyArmature")
    arm_obj = bpy.data.objects.new("ButterflyArmature", arm_data)
    bpy.context.scene.collection.objects.link(arm_obj)
    bpy.context.view_layer.objects.active = arm_obj
    arm_obj.select_set(True)
    bpy.ops.object.mode_set(mode="EDIT")

    u0, u1, u2, u3 = WING_BONE_U
    bones = [
        # (名前, 親, 頭, 先)
        ("root", None, (0, 0.0, 0.0), (0, 0.0, 0.20)),
        ("body", "root", (0, 0.15, 0.0), (0, -0.22, 0.0)),
        ("head", "body", (0, -0.22, 0.0), (0, -0.52, 0.0)),
        ("abdomen", "body", (0, 0.15, 0.0), (0, 0.58, 0.0)),
        ("abdomen_tip", "abdomen", (0, 0.58, 0.0), (0, 1.00, 0.0)),
        ("antenna_R", "head", tuple(ANTENNA_BASE), tuple(ANTENNA_END)),
        ("antenna_L", "head", (-ANTENNA_BASE.x, ANTENNA_BASE.y, ANTENNA_BASE.z),
         (-ANTENNA_END.x, ANTENNA_END.y, ANTENNA_END.z)),
        # 弾・尾・ミサイルの発射位置。頭の先に置く、ウェイトを持たない(メッシュを動かさない)印だけのボーン
        (MUZZLE_BONE, "head", (0, BODY_Y_FRONT, 0.0), (0, BODY_Y_FRONT - MUZZLE_BONE_LENGTH, 0.0)),
    ]
    for side, sx in (("R", 1.0), ("L", -1.0)):
        bones.append(("wing_base_" + side, "body", (sx * u0, 0, 0), (sx * u1, 0, 0)))
        bones.append(("wing_mid_" + side, "wing_base_" + side, (sx * u1, 0, 0), (sx * u2, 0, 0)))
        bones.append(("wing_tip_" + side, "wing_mid_" + side, (sx * u2, 0, 0), (sx * u3, 0, 0)))

    for name, parent, head, tail in bones:
        eb = arm_data.edit_bones.new(name)
        eb.head = PITCH_MATRIX @ Vector((head[0], fy(head[1]), head[2]))
        eb.tail = PITCH_MATRIX @ Vector((tail[0], fy(tail[1]), tail[2]))
        eb.roll = 0.0
        if parent:
            eb.parent = arm_data.edit_bones[parent]
            eb.use_connect = False
    bpy.ops.object.mode_set(mode="OBJECT")
    return arm_obj


def bind_mesh(mesh_obj, arm_obj):
    """メッシュをアーマチュアの子にして、Armature モディファイアを付ける(頂点グループ名 = ボーン名)。"""
    mesh_obj.parent = arm_obj
    mod = mesh_obj.modifiers.new("Armature", "ARMATURE")
    mod.object = arm_obj


# ---------------------------------------------------------------- アニメーション
# アニメの回転軸。基本の姿勢を機首上げ(X まわり)で傾けたので、体の前後軸と上方向は、その分だけ傾いた軸を使う
# (羽ばたき・ロールは体の前後軸まわり、触角・首ふりは体の上方向まわり)。X(左右)は傾けても変わらない
PITCH_ROTATION = PITCH_MATRIX.to_3x3()
X_AXIS = Vector((-1, 0, 0))  # 頭が +Y なので、-X まわりの正の回転が「機首を下げる」向き(従来と同じ意味)
Y_AXIS = PITCH_ROTATION @ Vector((0, 1, 0))
Z_AXIS = PITCH_ROTATION @ Vector((0, 0, 1))


def set_rot(pb, axis_world, angle):
    """ボーンを、ワールド軸まわりに angle 回す(ボーンの向きに依存せず、軸を世界で指定できるようにする)。
    ポーズの回転はボーンの静止姿勢の座標系で表すので、世界軸を静止姿勢の逆回転で変換する。"""
    rest = pb.bone.matrix_local.to_quaternion()
    axis_local = rest.inverted() @ axis_world
    pb.rotation_mode = "QUATERNION"
    pb.rotation_quaternion = Quaternion(axis_local, angle)


def add_rot(pb, axis_world, angle):
    """すでに入っている回転に、さらにワールド軸まわりの回転を重ねる(ロールとピッチを同時に付けたいとき)。"""
    rest = pb.bone.matrix_local.to_quaternion()
    axis_local = rest.inverted() @ axis_world
    pb.rotation_mode = "QUATERNION"
    pb.rotation_quaternion = pb.rotation_quaternion @ Quaternion(axis_local, angle)


def set_loc(pb, offset_world):
    rest = pb.bone.matrix_local.to_quaternion()
    pb.location = rest.inverted() @ offset_world


def wing_pose(pbs, side, base_up, mid_up, tip_up):
    """片側の翅を、付け根・中間・先端の「上向きの角度」(ラジアン、上が正)で曲げる。
    翅は X 方向に伸びていて、上下の羽ばたきは Y 軸まわりの回転。右翅(+X)は正の回転で下がる、左翅は逆。"""
    sign = -1.0 if side == "R" else 1.0
    set_rot(pbs["wing_base_" + side], Y_AXIS, sign * base_up)
    set_rot(pbs["wing_mid_" + side], Y_AXIS, sign * mid_up)
    set_rot(pbs["wing_tip_" + side], Y_AXIS, sign * tip_up)


def clear_pose(pbs):
    for pb in pbs.values():
        pb.rotation_mode = "QUATERNION"
        pb.rotation_quaternion = Quaternion((1, 0, 0, 0))
        pb.location = Vector((0, 0, 0))
        pb.scale = Vector((1, 1, 1))


def key_all(pbs, frame):
    for pb in pbs.values():
        pb.keyframe_insert("rotation_quaternion", frame=frame)
        if pb.name == "root":
            pb.keyframe_insert("location", frame=frame)


def anim_flap(pbs, f, n):
    """羽ばたき(待機)。付け根が先に動き、中間と先端が遅れて追うので、翅がしなる。"""
    ph = 2.0 * math.pi * f / n
    base = 0.10 + 0.65 * math.sin(ph)
    mid = 0.30 * math.sin(ph - 0.7)
    tip = 0.30 * math.sin(ph - 1.4)
    for side in ("L", "R"):
        wing_pose(pbs, side, base, mid, tip)
    set_loc(pbs["root"], PITCH_ROTATION @ Vector((0, 0, -0.04 * math.cos(ph))))
    set_rot(pbs["abdomen"], X_AXIS, 0.12 * math.sin(ph - 1.0))
    set_rot(pbs["abdomen_tip"], X_AXIS, 0.10 * math.sin(ph - 1.6))
    set_rot(pbs["head"], X_AXIS, -0.06 * math.sin(ph - 1.0))
    set_rot(pbs["antenna_L"], Z_AXIS, 0.08 * math.sin(2 * ph))
    set_rot(pbs["antenna_R"], Z_AXIS, -0.08 * math.sin(2 * ph + 0.5))


def anim_glide(pbs, f, n):
    """滑空。翅を V 字に上げたまま、ゆっくり小さく揺れる。"""
    ph = 2.0 * math.pi * f / n
    base = 0.30 + 0.04 * math.sin(ph)
    mid = 0.10 + 0.03 * math.sin(ph - 0.6)
    tip = 0.08 + 0.04 * math.sin(ph - 1.2)
    for side in ("L", "R"):
        wing_pose(pbs, side, base, mid, tip)
    set_rot(pbs["root"], Y_AXIS, 0.05 * math.sin(ph))
    set_loc(pbs["root"], PITCH_ROTATION @ Vector((0, 0, 0.015 * math.sin(2 * ph))))
    set_rot(pbs["abdomen"], X_AXIS, 0.05 * math.sin(ph - 0.8))
    set_rot(pbs["antenna_L"], Z_AXIS, 0.05 * math.sin(ph))
    set_rot(pbs["antenna_R"], Z_AXIS, -0.05 * math.sin(ph + 0.4))


def anim_hit(pbs, f, n):
    """被弾でよろめく。最初と最後は Glide の基本姿勢に合わせ、その間に衝撃と減衰の揺れを入れる(ループしない)。
    衝撃の強さ g(t) は、素早く立ち上がり、指数的に減る。左右の翅は位相と大きさを変えて不規則にする。"""
    t = f / n
    g = (1.0 - math.exp(-t * 30.0)) * math.exp(-4.0 * t)
    wobble = math.sin(2.0 * math.pi * 3.0 * t)
    for side, amp, shift in (("L", 1.0, 0.0), ("R", 0.7, 1.1)):
        base = 0.30 + g * amp * (0.9 + 0.7 * math.cos(2.0 * math.pi * 3.0 * t + shift))
        mid = 0.10 + g * amp * 0.5 * math.sin(2.0 * math.pi * 3.0 * t + shift)
        tip = 0.08 + g * amp * 0.7 * math.sin(2.0 * math.pi * 3.0 * t - 0.8 + shift)
        wing_pose(pbs, side, base, mid, tip)
    set_rot(pbs["root"], Y_AXIS, 0.95 * g * math.sin(2.0 * math.pi * 2.0 * t + 0.5))
    add_rot(pbs["root"], X_AXIS, 0.70 * g * math.sin(2.0 * math.pi * 1.5 * t))
    set_loc(pbs["root"], PITCH_ROTATION @ Vector((0, -0.25 * g, -0.12 * g)))  # 後ろ(-Y)へのけぞる
    set_rot(pbs["head"], X_AXIS, -0.35 * g * wobble)
    set_rot(pbs["abdomen"], X_AXIS, 0.5 * g * math.sin(2.0 * math.pi * 3.0 * t - 1.0))
    set_rot(pbs["abdomen_tip"], X_AXIS, 0.4 * g * math.sin(2.0 * math.pi * 3.0 * t - 1.8))
    set_rot(pbs["antenna_L"], Z_AXIS, 0.4 * g * wobble)
    set_rot(pbs["antenna_R"], Z_AXIS, -0.4 * g * math.sin(2.0 * math.pi * 3.0 * t + 0.7))


def anim_tumble(pbs, f, n):
    """墜落のきりもみ。前後軸(Y)まわりに 1 回転しつつ、ふらつく。翅は折れて力なく上へ垂れ、不規則にはためく。"""
    ph = 2.0 * math.pi * f / n
    set_rot(pbs["root"], Y_AXIS, ph)
    add_rot(pbs["root"], X_AXIS, 0.45 * math.sin(2.0 * ph))
    add_rot(pbs["root"], Z_AXIS, 0.30 * math.sin(ph + 0.5))
    for side, shift in (("L", 0.0), ("R", 1.3)):
        base = 0.90 + 0.35 * math.sin(2.0 * ph + 1.0 + shift)
        mid = -0.40 + 0.50 * math.sin(3.0 * ph + shift)
        tip = -0.60 + 0.60 * math.sin(2.0 * ph + 2.0 + shift)
        wing_pose(pbs, side, base, mid, tip)
    set_rot(pbs["head"], X_AXIS, 0.30 * math.sin(ph + 1.0))
    set_rot(pbs["abdomen"], X_AXIS, 0.50 + 0.25 * math.sin(2.0 * ph))
    set_rot(pbs["abdomen_tip"], X_AXIS, 0.40 + 0.30 * math.sin(2.0 * ph - 0.8))
    set_rot(pbs["antenna_L"], Z_AXIS, 0.35 * math.sin(3.0 * ph))
    set_rot(pbs["antenna_R"], Z_AXIS, -0.35 * math.sin(3.0 * ph + 0.9))


# (名前, 関数, フレーム数, ループするか)。ループするものは最後のフレーム = 最初のフレームの姿勢にする
ANIMATIONS = [
    (ANIM_FLAP, anim_flap, 20, True),
    (ANIM_GLIDE, anim_glide, 60, True),
    (ANIM_HIT, anim_hit, 24, False),
    (ANIM_TUMBLE, anim_tumble, 36, True),
]


def build_animations(arm_obj):
    """各アニメーションをアクションとして作り、NLA トラックに積む(トラック名 = glTF のアニメ名になる)。"""
    pbs = {pb.name: pb for pb in arm_obj.pose.bones}
    arm_obj.animation_data_create()
    ad = arm_obj.animation_data
    for name, func, n, _loop in ANIMATIONS:
        action = bpy.data.actions.new(name)
        action.use_fake_user = True
        ad.action = action
        for f in range(n + 1):
            clear_pose(pbs)
            func(pbs, f, n)
            key_all(pbs, f)
        track = ad.nla_tracks.new()
        track.name = name
        strip = track.strips.new(name, 0, action)
        strip.name = name
        ad.action = None
    clear_pose(pbs)


# ---------------------------------------------------------------- 書き出し
def export_gltf(out_dir):
    os.makedirs(out_dir, exist_ok=True)
    path = os.path.join(out_dir, "butterfly.gltf")
    bpy.ops.object.select_all(action="DESELECT")
    opts = dict(
        filepath=path,
        export_format="GLTF_SEPARATE",
        export_animations=True,
        export_animation_mode="NLA_TRACKS",
        export_skins=True,
        export_apply=False,
        export_yup=True,
        export_texcoords=True,
        export_normals=True,
        export_image_format="AUTO",
        export_materials="EXPORT",
        use_selection=False,
        export_force_sampling=True,
        export_optimize_animation_size=False,
        export_nla_strips=True,
    )
    valid = set(bpy.ops.export_scene.gltf.get_rna_type().properties.keys())
    opts = {k: v for k, v in opts.items() if k in valid}
    bpy.ops.export_scene.gltf(**opts)
    print("exported ->", path)


def parse_args():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    args = {"textures": os.path.join(HERE, "out_mix"),
            "out": os.path.join(HERE, "out", "model"), "blend": ""}
    i = 0
    while i < len(argv):
        key = argv[i].lstrip("-")
        if key in args and i + 1 < len(argv):
            args[key] = os.path.abspath(argv[i + 1])
            i += 2
        else:
            i += 1
    return args


def main():
    args = parse_args()
    reset_scene()
    wing_mat = make_material(WING_MATERIAL, os.path.join(args["textures"], WING_TEXTURE))
    body_mat = make_material(BODY_MATERIAL, os.path.join(args["textures"], BODY_TEXTURE))
    wings = build_wings(wing_mat)
    body = build_body(body_mat)
    arm = build_armature()
    bind_mesh(wings, arm)
    bind_mesh(body, arm)
    build_animations(arm)
    if args["blend"]:
        os.makedirs(os.path.dirname(args["blend"]), exist_ok=True)
        bpy.ops.wm.save_as_mainfile(filepath=args["blend"])
    export_gltf(args["out"])
    print("wing triangles:", len(wings.data.polygons), " body triangles:", len(body.data.polygons))


main()
