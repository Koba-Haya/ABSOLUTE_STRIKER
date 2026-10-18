# burst_*.png の連続フレームの差を測る(画面の下 38% = 雲海の領域)。
# mean|diff|: 差の平均(カメラ移動と風による動きを含む)、high-freq part: 9x9 の箱ぼかしで引いた残り(画素ごとの細かい変化)。
# 注意: 撮影のタイミングがゲームの進行に揃わないので、数字の絶対値は安定しない(実測では 1.8〜5.7 とばらついた)。比較には向かない。
# 使い方: python analyze_burst.py <burst_*.png のあるフォルダ>
import glob
import os
import sys
import numpy as np
from PIL import Image
folder = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.environ.get("TEMP", "."), "cloud_shots")
files = sorted(glob.glob(os.path.join(folder, "burst_*.png")))
frames = [np.asarray(Image.open(f).convert("L"), dtype=np.float32) for f in files]
h, w = frames[0].shape
r0 = int(h * 0.62)

def box_blur(a, radius):
    # 分離可能な箱ぼかし(累積和)
    k = 2 * radius + 1
    p = np.pad(a, ((radius, radius), (radius, radius)), mode="edge")
    c = np.cumsum(p, axis=0); c = np.vstack([np.zeros((1, c.shape[1]), np.float32), c])
    a1 = (c[k:] - c[:-k]) / k
    c = np.cumsum(a1, axis=1); c = np.hstack([np.zeros((c.shape[0], 1), np.float32), c])
    return (c[:, k:] - c[:, :-k]) / k

tot, hf = [], []
for a, b in zip(frames[:-1], frames[1:]):
    d = (b - a)[r0:, :]
    low = box_blur(d, 4)
    tot.append(float(np.abs(d).mean())); hf.append(float(np.abs(d - low).mean()))
print("frames %d  mean|diff| %.2f  high-freq part %.2f  ratio %.2f" % (len(frames), np.mean(tot), np.mean(hf), np.mean(hf) / max(np.mean(tot), 1e-6)))
