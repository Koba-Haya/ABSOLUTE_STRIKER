"""雲ビルボード用のスプライト(cloud0〜3.png と、貼る板の OBJ/MTL)を生成する。
実行: python gen_cloud_sprites.py  (このフォルダに出力される。numpy と Pillow が必要)

やっていること: 立体の「雲の密度」(球をいくつか重ねて、ノイズで崩したもの)を、視線方向に積算して(小さなレイマーチ)、
光の減衰まで計算した絵を、事前に 1 回だけ作る。実行時は、この絵を板に貼るだけなので軽い。
  - 密度: 球の和(metaball)をしきい値で立ち上げ、3D の値ノイズで縁を崩す。底は平らにする(積雲の形)
  - 視線: 画面に垂直な向きに N ステップ。各ステップで透過率 T を減らし、色を足す: C += T * (1-exp(-σdt)) * 放射
  - 光: 各ステップから太陽の方向へ短くマーチして、光の減衰 exp(-τ) を求める(厚い所ほど暗く、薄い縁は明るい = 逆光の縁)
  - 出力: 直線(非プリマルチ)のアルファ付き PNG。sRGB へガンマ変換して保存
"""
import os

import numpy as np
from PIL import Image

OUT = os.path.dirname(os.path.abspath(__file__))
SIZE = 256
VIEW_STEPS = 40
LIGHT_STEPS = 6
LIGHT_STEP_LEN = 0.12
SIGMA = 7.0                       # 消散係数(大きいほど濃く、不透明)
LIGHT_DIR = np.array([0.45, 0.55, 0.35])   # 太陽の方向(画面の右上、やや手前)。x=右, y=上, z=手前
LIGHT_DIR = LIGHT_DIR / np.linalg.norm(LIGHT_DIR)
LIGHT_COLOR = np.array([1.00, 0.97, 0.92]) * 1.25
AMBIENT_TOP = np.array([0.62, 0.74, 0.95]) * 0.55     # 空からの回り込みの光(上)
AMBIENT_BOTTOM = np.array([0.40, 0.48, 0.66]) * 0.45  # 下側(日陰の青灰色)


def hash3(ix, iy, iz):
    h = ix.astype(np.uint32) * np.uint32(374761393) + iy.astype(np.uint32) * np.uint32(668265263) + iz.astype(np.uint32) * np.uint32(2246822519)
    h = (h ^ (h >> np.uint32(13))) * np.uint32(1274126177)
    h ^= h >> np.uint32(16)
    return h.astype(np.float64) / 4294967295.0


def value_noise3(p):
    i = np.floor(p).astype(np.int64)
    f = p - np.floor(p)
    u = f * f * (3.0 - 2.0 * f)
    out = 0.0
    for dx in (0, 1):
        for dy in (0, 1):
            for dz in (0, 1):
                w = (u[..., 0] if dx else 1 - u[..., 0]) * (u[..., 1] if dy else 1 - u[..., 1]) * (u[..., 2] if dz else 1 - u[..., 2])
                out = out + w * hash3(i[..., 0] + dx, i[..., 1] + dy, i[..., 2] + dz)
    return out


def fbm3(p, octaves=4):
    s, a = 0.0, 0.5
    for _ in range(octaves):
        s = s + a * value_noise3(p)
        p = p * 2.03
        a *= 0.5
    return s / 0.9375


def smoothstep(a, b, x):
    t = np.clip((x - a) / (b - a), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def make_cloud_density(seed):
    rng = np.random.default_rng(seed)
    count = int(rng.integers(9, 14))
    spheres = []
    for _ in range(count):
        x = rng.uniform(-0.62, 0.62)
        dome = 1.0 - (x / 0.75) ** 2                      # 中央ほど高く盛り上がる(積雲の形)
        y = -0.22 + 0.60 * dome * rng.uniform(0.35, 1.0)
        z = rng.uniform(-0.25, 0.25)
        r = rng.uniform(0.20, 0.36) * (1.0 - 0.45 * abs(x))
        spheres.append((x, y, z, r))
    noise_offset = rng.uniform(0, 50, size=3)

    def density(p):
        d = 0.0
        for x, y, z, r in spheres:
            dist2 = (p[..., 0] - x) ** 2 + (p[..., 1] - y) ** 2 + ((p[..., 2] - z) * 1.5) ** 2
            d = d + np.exp(-dist2 / (r * r) * 1.6)
        # 粗い層(大きなこぶ)と細かい層(縁のもこもこ)の 2 つのノイズで、輪郭を崩す
        n = 0.65 * fbm3(p * 3.4 + noise_offset) + 0.35 * fbm3(p * 8.5 + noise_offset * 1.7)
        d2 = d + 0.62 * (n - 0.5)
        bottom = smoothstep(-0.42, -0.26, p[..., 1])      # 底を平らに切る
        return smoothstep(0.34, 0.78, d2) * bottom

    return density


def render(seed):
    density = make_cloud_density(seed)
    ys, xs = np.meshgrid(np.linspace(1.0, -1.0, SIZE), np.linspace(-1.0, 1.0, SIZE), indexing="ij")
    transmittance = np.ones((SIZE, SIZE))
    color = np.zeros((SIZE, SIZE, 3))
    dz = 2.0 / VIEW_STEPS
    for k in range(VIEW_STEPS):
        z = 1.0 - (k + 0.5) * dz          # 手前(+z)から奥へ
        p = np.stack([xs, ys, np.full_like(xs, z)], axis=-1)
        dens = density(p)
        sigma = dens * SIGMA
        # 光の減衰: 太陽の方向へ短くマーチして光学的厚さを積算
        tau = np.zeros_like(xs)
        for j in range(1, LIGHT_STEPS + 1):
            tau += density(p + LIGHT_DIR * (j * LIGHT_STEP_LEN)) * SIGMA * LIGHT_STEP_LEN
        light = np.exp(-tau)
        height01 = np.clip((ys + 0.45) / 0.9, 0.0, 1.0)[..., None]
        ambient = AMBIENT_BOTTOM * (1 - height01) + AMBIENT_TOP * height01
        radiance = LIGHT_COLOR * light[..., None] + ambient
        absorb = 1.0 - np.exp(-sigma * dz)
        color += (transmittance * absorb)[..., None] * radiance
        transmittance *= np.exp(-sigma * dz)
    alpha = 1.0 - transmittance
    straight = np.where(alpha[..., None] > 1e-4, color / np.maximum(alpha[..., None], 1e-4), 0.0)
    straight = np.clip(straight, 0.0, 1.0) ** (1.0 / 2.2)      # sRGB へ
    rgba = np.dstack([straight, np.clip(alpha * 1.05, 0.0, 1.0)])
    return (rgba * 255).astype(np.uint8)


def make_plane(name, width_half=1.0):
    """正方形の板(板ポリ)の OBJ + MTL。向き・UV の規則は gen_title_assets.py と同じ"""
    hw = width_half
    with open(os.path.join(OUT, name + ".obj"), "w") as f:
        f.write(f"mtllib {name}.mtl\no {name}\n")
        f.write(f"v {hw:.6f} {-hw:.6f} 0.000000\nv {-hw:.6f} {-hw:.6f} 0.000000\n")
        f.write(f"v {hw:.6f} {hw:.6f} 0.000000\nv {-hw:.6f} {hw:.6f} 0.000000\n")
        f.write("vn 0.0000 0.0000 -1.0000\n")
        f.write("vt 0.000000 0.000000\nvt 1.000000 1.000000\nvt 1.000000 0.000000\nvt 0.000000 1.000000\n")
        f.write("s 0\nusemtl Material\nf 2/1/1 3/2/1 1/3/1\nf 2/1/1 4/4/1 3/2/1\n")
    with open(os.path.join(OUT, name + ".mtl"), "w") as f:
        f.write("newmtl Material\nNs 250.000000\nKa 1.000000 1.000000 1.000000\n"
                "Kd 1.000000 1.000000 1.000000\nKs 0.000000 0.000000 0.000000\n"
                f"d 1.000000\nillum 1\nmap_Kd {name}.png\n")


if __name__ == "__main__":
    for seed in range(4):
        name = f"cloud{seed}"
        Image.fromarray(render(seed), "RGBA").save(os.path.join(OUT, name + ".png"))
        make_plane(name)
        print("generated", name)
