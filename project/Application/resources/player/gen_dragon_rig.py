"""飛竜モデルにボーンとウェイトを入れ、project/art/dragon/dragon_rig.blend を作る(Blender 内で実行する)。
そのあと標準ポーズを付けて dragon/dragon.obj を書き出す(タイトルシーンが読むファイル)。

実行(Blender 4.4 の例):
  "C:\\Program Files\\Blender Foundation\\Blender 4.4\\blender.exe" -b -P gen_dragon_rig.py
前提: 先に python gen_dragon_pose.py --rest で project/art/dragon/rest/dragon_rest.obj を作っておく。

ボーン(左 = +X 側。Blender の座標: X=右, Y=機首方向, Z=上):
  胴体: Hips → Spine → Chest → Neck → Head / 尾: Tail1 → Tail2 → Tail3
  脚: Leg.L/R / 後ろの発光フィン: Fin.L/R
  翼(左右): Arm(上腕) → Forearm → Finger1〜4(指の骨 = 翼の先端 4 本)/ Claw(親指の爪)
ウェイト: 頂点ごとに「最も近い数本のボーン」への距離から決める(近いほど強く、合計 1、最大 3 本)。
  自動ウェイトは複雑な Tripo のメッシュで失敗しやすいので、距離ベースで自前計算している。
"""
import math
import os
import sys

import bpy
from mathutils import Matrix, Quaternion, Vector

HERE = os.path.dirname(os.path.abspath(__file__))
ART_DIR = os.path.normpath(os.path.join(HERE, "..", "..", "..", "art", "dragon"))
REST_OBJ = os.path.join(ART_DIR, "rest", "dragon_rest.obj")
BLEND_PATH = os.path.join(ART_DIR, "dragon_rig.blend")
POSED_OBJ_DIR = os.path.join(HERE, "dragon")

# 元モデル(立ち姿)の座標 → Blender 座標。gen_dragon_pose.py の orient() と同じスケール(頭から尾 2.8)
SRC_Y_RANGE = 0.945526123
SCALE = 2.8 / SRC_Y_RANGE
BODY_CENTER_Y = 0.47


def bl(x, y, z=0.0):
    """元モデルの (x, y, z) を Blender 座標へ。頭(+y)が +Y(機首)、背中(+z)が +Z"""
    return Vector((x * SCALE, (y - BODY_CENTER_Y) * SCALE, z * SCALE))


# (名前, 親, 頭の位置, 先の位置) ― 元モデル座標で書く。右側(-X)は左側の鏡像で作る
BODY_BONES = [
    ("Hips", None, (0, 0.36), (0, 0.46)),
    ("Spine", "Hips", (0, 0.46), (0, 0.55)),
    ("Chest", "Spine", (0, 0.55), (0, 0.63)),
    ("Neck", "Chest", (0, 0.63), (0, 0.72)),
    ("Head", "Neck", (0, 0.72), (0, 0.86)),
    ("Tail1", "Hips", (0, 0.36), (0, 0.22)),
    ("Tail2", "Tail1", (0, 0.22), (0, 0.11)),
    ("Tail3", "Tail2", (0, 0.11), (0, 0.0)),
]
# 左側(+X)のボーン。名前に .L / .R を付けて左右を作る
SIDE_BONES = [
    ("Leg", "Hips", (0.075, 0.40), (0.07, 0.24)),
    ("Fin", "Hips", (0.09, 0.42), (0.27, 0.29)),
    ("Arm", "Chest", (0.07, 0.52), (0.20, 0.62)),
    ("Forearm", "Arm", (0.20, 0.62), (0.30, 0.66)),
    ("Claw", "Arm", (0.20, 0.62), (0.16, 0.77)),
    # 指: 手首から翼の先端 4 か所へ(翼の外形から求めた先端)
    ("Finger1", "Forearm", (0.30, 0.66), (0.499, 0.944)),
    ("Finger2", "Forearm", (0.30, 0.66), (0.499, 0.793)),
    ("Finger3", "Forearm", (0.30, 0.66), (0.499, 0.567)),
    ("Finger4", "Forearm", (0.30, 0.66), (0.368, 0.465)),
]

MAX_INFLUENCES = 3
DISTANCE_EPS = 0.02      # 距離 0 付近で重みが発散しないための下駄
DISTANCE_POWER = 3.0     # 大きいほど「一番近いボーン」が強くなる

# 標準ポーズ(ワールド軸まわりの回転, 度)。軸: X=機首の左右, Y=機首方向, Z=上
# 翼は X 側(.L)が負の Z 回転で後ろへ、反対側は符号を反転する
POSE_SWEEP_ARM = -30.0
POSE_SWEEP_FOREARM = -10.0
POSE_NECK_PITCH = 20.0   # 頭を持ち上げる(元の姿勢では顔が地面を向くため)
POSE_HEAD_PITCH = 50.0


def build_bone_defs():
    defs = []
    for name, parent, head, tail in BODY_BONES:
        defs.append((name, parent, bl(head[0], head[1]), bl(tail[0], tail[1])))
    for side, sign in ((".L", 1.0), (".R", -1.0)):
        for name, parent, head, tail in SIDE_BONES:
            parent_name = parent + side if parent in {n for n, *_ in SIDE_BONES} else parent
            defs.append((name + side, parent_name, bl(sign * head[0], head[1]), bl(sign * tail[0], tail[1])))
    return defs


def point_segment_distance(p, a, b):
    ab = b - a
    t = max(0.0, min(1.0, (p - a).dot(ab) / max(ab.length_squared, 1e-12)))
    return (p - (a + ab * t)).length


def assign_weights(obj, defs):
    groups = {name: obj.vertex_groups.new(name=name) for name, *_ in defs}
    to_world = obj.matrix_world  # OBJ インポートはオブジェクトを 90° 回すので、ボーン(ワールド座標)と比べるには変換が要る
    for v in obj.data.vertices:
        co = to_world @ v.co
        scored = []
        for name, _, head, tail in defs:
            d = point_segment_distance(co, head, tail)
            scored.append((1.0 / ((d + DISTANCE_EPS) ** DISTANCE_POWER), name))
        scored.sort(reverse=True)
        top = scored[:MAX_INFLUENCES]
        total = sum(w for w, _ in top)
        for w, name in top:
            groups[name].add([v.index], w / total, "REPLACE")


def make_armature(defs):
    arm_data = bpy.data.armatures.new("DragonRig")
    arm = bpy.data.objects.new("DragonRig", arm_data)
    bpy.context.collection.objects.link(arm)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.mode_set(mode="EDIT")
    for name, parent, head, tail in defs:
        eb = arm_data.edit_bones.new(name)
        eb.head = head
        eb.tail = tail
        eb.parent = arm_data.edit_bones[parent] if parent else None
        eb.use_connect = False
    bpy.ops.object.mode_set(mode="OBJECT")
    arm_data.display_type = "STICK"
    arm.show_in_front = True
    return arm


def rotate_world_axis(arm, bone_name, axis, degrees):
    """ボーンを、アーマチュア空間の軸まわりに回す(回転軸が親の回転で変わらないポーズ用)"""
    pb = arm.pose.bones[bone_name]
    rest = pb.bone.matrix_local.to_3x3()
    local_axis = rest.inverted() @ Vector(axis)
    pb.rotation_mode = "QUATERNION"
    pb.rotation_quaternion = Quaternion(local_axis, math.radians(degrees))


def apply_default_pose(arm):
    for side, sign in ((".L", 1.0), (".R", -1.0)):
        rotate_world_axis(arm, "Arm" + side, (0, 0, 1), sign * POSE_SWEEP_ARM)
        rotate_world_axis(arm, "Forearm" + side, (0, 0, 1), sign * POSE_SWEEP_FOREARM)
    rotate_world_axis(arm, "Neck", (1, 0, 0), POSE_NECK_PITCH)
    rotate_world_axis(arm, "Head", (1, 0, 0), POSE_HEAD_PITCH)
    bpy.context.view_layer.update()


def export_posed_obj(obj, out_dir):
    """評価後(ボーン変形後)のメッシュを OBJ で書き出す。
    インポート時にオブジェクト側へ回転が入っているので、ローカル座標がそのままファイル空間の座標になる"""
    depsgraph = bpy.context.evaluated_depsgraph_get()
    eval_obj = obj.evaluated_get(depsgraph)
    me = eval_obj.to_mesh()
    me.calc_loop_triangles()
    uv = me.uv_layers.active.data
    os.makedirs(out_dir, exist_ok=True)
    with open(os.path.join(out_dir, "dragon.obj"), "w") as f:
        f.write("mtllib dragon.mtl\no dragon\nusemtl Dragon\n")
        f.write("".join(f"v {v.co.x:.6f} {v.co.y:.6f} {v.co.z:.6f}\n" for v in me.vertices))
        f.write("".join(f"vt {d.uv.x:.6f} {d.uv.y:.6f}\n" for d in uv))
        f.write("s 1\n")
        for tri in me.loop_triangles:
            f.write("f " + " ".join(f"{me.loops[l].vertex_index + 1}/{l + 1}" for l in tri.loops) + "\n")
    eval_obj.to_mesh_clear()


def main():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.wm.obj_import(filepath=REST_OBJ)
    obj = next(o for o in bpy.context.scene.objects if o.type == "MESH")
    obj.name = "dragon"

    defs = build_bone_defs()
    arm = make_armature(defs)
    assign_weights(obj, defs)

    mod = obj.modifiers.new("Armature", "ARMATURE")
    mod.object = arm
    obj.parent = arm

    bpy.ops.file.pack_all()  # テクスチャを .blend に埋め込む
    bpy.ops.wm.save_as_mainfile(filepath=BLEND_PATH, compress=True)
    print("saved", BLEND_PATH)

    # 標準ポーズで書き出す(.blend には保存しない)
    apply_default_pose(arm)
    export_posed_obj(obj, POSED_OBJ_DIR)
    print("exported posed obj ->", POSED_OBJ_DIR)


main()
