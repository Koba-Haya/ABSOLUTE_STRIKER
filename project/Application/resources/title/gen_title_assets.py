"""タイトル用素材(ロゴ/PRESS START の PNG と板ポリ OBJ+MTL)を生成する。
実行: python gen_title_assets.py  (このフォルダに出力される)"""
from PIL import Image, ImageDraw, ImageFont, ImageFilter
import os

OUT = os.path.dirname(os.path.abspath(__file__))
FONT = "C:/Windows/Fonts/impact.ttf"


def make_text_png(text, size, canvas, top, bottom, outline, glow, name):
    w, h = canvas
    font = ImageFont.truetype(FONT, size)
    # 文字マスク
    mask = Image.new("L", canvas, 0)
    d = ImageDraw.Draw(mask)
    bb = d.textbbox((0, 0), text, font=font)
    x = (w - (bb[2] - bb[0])) // 2 - bb[0]
    y = (h - (bb[3] - bb[1])) // 2 - bb[1]
    d.text((x, y), text, font=font, fill=255)
    # 縦グラデ塗り
    grad = Image.new("RGB", canvas)
    gd = ImageDraw.Draw(grad)
    for row in range(h):
        t = row / (h - 1)
        c = tuple(int(top[i] + (bottom[i] - top[i]) * t) for i in range(3))
        gd.line([(0, row), (w, row)], fill=c)
    # 縁取り(マスクを膨張)とグロー(ぼかし)
    edge = mask.filter(ImageFilter.MaxFilter(9))
    glow_a = mask.filter(ImageFilter.MaxFilter(15)).filter(ImageFilter.GaussianBlur(14))
    img = Image.new("RGBA", canvas, (0, 0, 0, 0))
    img.paste(Image.new("RGBA", canvas, glow + (255,)), mask=glow_a.point(lambda v: int(v * 0.85)))
    img.paste(Image.new("RGBA", canvas, outline + (255,)), mask=edge)
    img.paste(grad.convert("RGBA"), mask=mask)
    img.save(os.path.join(OUT, name + ".png"))


def make_plane(name, aspect, width_half):
    """plane.obj と同じ向き・UV 規則の板。横 = width_half*2、縦 = 横/aspect。"""
    hw, hh = width_half, width_half / aspect
    with open(os.path.join(OUT, name + ".obj"), "w") as f:
        f.write(f"mtllib {name}.mtl\no {name}\n")
        f.write(f"v {hw:.6f} {-hh:.6f} 0.000000\nv {-hw:.6f} {-hh:.6f} 0.000000\n")
        f.write(f"v {hw:.6f} {hh:.6f} 0.000000\nv {-hw:.6f} {hh:.6f} 0.000000\n")
        f.write("vn 0.0000 0.0000 -1.0000\n")
        # カメラは -Z 側から +Z を見るので画面右が +x。u は x と同じ向きにする(plane.obj は逆向き)
        f.write("vt 0.000000 0.000000\nvt 1.000000 1.000000\nvt 1.000000 0.000000\nvt 0.000000 1.000000\n")
        f.write("s 0\nusemtl Material\nf 2/1/1 3/2/1 1/3/1\nf 2/1/1 4/4/1 3/2/1\n")
    with open(os.path.join(OUT, name + ".mtl"), "w") as f:
        f.write("newmtl Material\nNs 250.000000\nKa 1.000000 1.000000 1.000000\n"
                "Kd 1.000000 1.000000 1.000000\nKs 0.000000 0.000000 0.000000\n"
                f"d 1.000000\nillum 1\nmap_Kd {name}.png\n")


make_text_png("ABSOLUTE STRIKER", 260, (2048, 512),
              (255, 245, 210), (255, 120, 60), (90, 20, 30), (255, 150, 60), "logo")
make_text_png("PRESS START", 120, (1024, 256),
              (255, 255, 255), (255, 210, 150), (80, 30, 40), (255, 140, 60), "start")
make_plane("logo", 2048 / 512, 1.0)
make_plane("start", 1024 / 256, 1.0)
