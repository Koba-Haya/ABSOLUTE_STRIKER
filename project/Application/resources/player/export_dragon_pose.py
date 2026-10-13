"""dragon_rig.blend の「いまのポーズ」を、タイトルシーンが読む dragon/dragon.obj へ書き出す(Blender 内で実行する)。

使い方: Blender で DragonRig をポーズモードで動かし、.blend を保存(Ctrl+S)してから、コマンドを実行する。
  "C:\\Program Files\\Blender Foundation\\Blender 4.4\\blender.exe" -b ..\\..\\..\\art\\dragon\\dragon_rig.blend -P export_dragon_pose.py
Blender 標準の OBJ エクスポートを使わない理由: マテリアル(dragon.mtl / dragon.jpg の参照)を、
エンジンが読める形のまま保つため。座標は、インポート時にオブジェクトへ回転が入っているので、
メッシュのローカル座標がそのままファイル空間(機首 = -Z)の座標になる。
"""
import os

import bpy

OUT_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "dragon")


def export_posed_obj(obj, out_dir):
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
    print("exported ->", os.path.join(out_dir, "dragon.obj"))


export_posed_obj(bpy.data.objects["dragon"], OUT_DIR)
