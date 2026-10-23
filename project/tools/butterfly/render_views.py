"""書き出した蝶の glTF を Blender で読み直し、複数の角度とアニメーションのコマを画像にする(確認用)。

実行:
  blender -b -P render_views.py -- --gltf out\\model\\butterfly.gltf --out render\\mix

書き出した glTF を読み直して描くので、書き出しで壊れていないか(ウェイト・アニメ名・テクスチャ)も一緒に確認できる。
出力: <out>_views.png(静止の 6 方向)、<out>_anim.png(アニメーションのコマ)
"""
import math
import os
import sys

import bpy
from mathutils import Vector

RESOLUTION = 420
# (名前, カメラの方向ベクトル(蝶の中心から見た位置), 上方向)。頭は +Y、正面 = +Y 側
STATIC_VIEWS = [
    ("top", (0, 0.001, 1), (0, 1, 0)),
    ("front", (0, 1, 0.15), (0, 0, 1)),
    ("side", (1, 0, 0.1), (0, 0, 1)),
    ("back", (0, -1, 0.3), (0, 0, 1)),
    ("quarter_up", (0.75, 0.8, 0.7), (0, 0, 1)),
    ("quarter_low", (-0.8, 0.6, -0.4), (0, 0, 1)),
]
CAMERA_DISTANCE = 5.2
ANIM_VIEW = ("anim", (0.35, 1.0, 0.55), (0, 0, 1))
# (アニメ名, 描くコマ)
ANIM_FRAMES = [
    ("Flap", [0, 5, 10, 15]),
    ("Glide", [0, 20, 40]),
    ("Hit", [0, 3, 6, 10, 16, 24]),
    ("Tumble", [0, 6, 12, 18, 24, 30]),
]


def parse_args():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    args = {"gltf": "", "out": "render/butterfly", "only": ""}
    for i in range(0, len(argv) - 1, 2):
        args[argv[i].lstrip("-")] = argv[i + 1]
    return args


def setup_render(scene):
    scene.render.resolution_x = RESOLUTION
    scene.render.resolution_y = RESOLUTION
    scene.render.image_settings.file_format = "PNG"
    try:
        scene.render.engine = "BLENDER_EEVEE_NEXT"
    except TypeError:
        scene.render.engine = "BLENDER_EEVEE"
    world = bpy.data.worlds.new("W")
    scene.world = world
    world.use_nodes = True
    bg = next(n for n in world.node_tree.nodes if n.type == "BACKGROUND")
    bg.inputs["Color"].default_value = (0.55, 0.62, 0.72, 1.0)
    bg.inputs["Strength"].default_value = 0.9
    sun = bpy.data.objects.new("Sun", bpy.data.lights.new("Sun", "SUN"))
    sun.data.energy = 3.0
    sun.rotation_euler = (math.radians(50), math.radians(10), math.radians(30))
    scene.collection.objects.link(sun)
    fill = bpy.data.objects.new("Fill", bpy.data.lights.new("Fill", "SUN"))
    fill.data.energy = 1.2
    fill.rotation_euler = (math.radians(130), 0, math.radians(-120))
    scene.collection.objects.link(fill)
    cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
    cam.data.lens = 60
    scene.collection.objects.link(cam)
    scene.camera = cam
    return cam


def aim(cam, direction, up, target=Vector((0, 0.25, 0))):
    d = Vector(direction).normalized()
    cam.location = target + d * CAMERA_DISTANCE
    forward = (target - cam.location).normalized()
    cam.rotation_mode = "QUATERNION"
    cam.rotation_quaternion = forward.to_track_quat("-Z", "Y")
    # 上方向を合わせる(to_track_quat は Y を上に寄せるだけなので、上を明示して回す)
    right = forward.cross(Vector(up)).normalized()
    true_up = right.cross(forward).normalized()
    from mathutils import Matrix
    m = Matrix((right, true_up, -forward)).transposed()
    cam.rotation_mode = "XYZ"
    cam.rotation_euler = m.to_euler()


def render(scene, path):
    scene.render.filepath = path
    bpy.ops.render.render(write_still=True)


def main():
    args = parse_args()
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.render.fps = 30
    bpy.ops.import_scene.gltf(filepath=os.path.abspath(args["gltf"]))
    cam = setup_render(scene)
    out = os.path.abspath(args["out"])
    os.makedirs(os.path.dirname(out), exist_ok=True)
    arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
    print("imported actions:", [a.name for a in bpy.data.actions])

    if args["only"] in ("", "static"):
        # 静止画: 何も再生しない(静止姿勢)
        if arm.animation_data:
            arm.animation_data.action = None
        for name, d, up in STATIC_VIEWS:
            aim(cam, d, up)
            render(scene, "%s_%s.png" % (out, name))

    if args["only"] == "wire":
        # 三角形の格子の確認用: 翅を白、その複製にワイヤーフレームのモディファイアを付けて黒い線を重ねる
        scene.render.resolution_x = scene.render.resolution_y = RESOLUTION * 3
        scene.view_settings.view_transform = "Standard"
        wings = next(o for o in bpy.data.objects if o.name.startswith("Wings"))

        def emission_material(name, color):
            m = bpy.data.materials.new(name)
            m.use_nodes = True
            nodes = m.node_tree.nodes
            nodes.clear()
            em = nodes.new("ShaderNodeEmission")
            em.inputs["Color"].default_value = color
            out_node = nodes.new("ShaderNodeOutputMaterial")
            m.node_tree.links.new(em.outputs["Emission"], out_node.inputs["Surface"])
            return m

        white = emission_material("White", (1, 1, 1, 1))
        black = emission_material("Black", (0, 0, 0, 1))
        line = wings.copy()
        line.data = wings.data.copy()
        scene.collection.objects.link(line)
        wings.data.materials[0] = white
        line.data.materials[0] = black
        for mod in list(line.modifiers):
            line.modifiers.remove(mod)
        wf = line.modifiers.new("Wire", "WIREFRAME")
        wf.thickness = 0.004
        wf.use_replace = True
        for o in bpy.data.objects:
            if o.type == "MESH" and o not in (wings, line):
                o.hide_render = True
        wings.hide_render = True   # 白い面は隠し、背景(白)の上に線だけを描く
        bg = next(n for n in bpy.data.worlds["W"].node_tree.nodes if n.type == "BACKGROUND")
        bg.inputs["Color"].default_value = (1, 1, 1, 1)
        bg.inputs["Strength"].default_value = 1.0
        aim(cam, STATIC_VIEWS[0][1], STATIC_VIEWS[0][2])
        render(scene, out + "_wire.png")

    if args["only"] in ("", "anim"):
        for anim_name, frames in ANIM_FRAMES:
            action = next((a for a in bpy.data.actions if a.name.endswith(anim_name)), None)
            if action is None:
                print("missing animation:", anim_name)
                continue
            arm.animation_data_create()
            arm.animation_data.action = action
            for f in frames:
                scene.frame_set(f)
                aim(cam, ANIM_VIEW[1], ANIM_VIEW[2])
                render(scene, "%s_anim_%s_%03d.png" % (out, anim_name, f))


main()
