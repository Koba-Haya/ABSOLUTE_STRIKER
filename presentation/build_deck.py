# 発表スライドの生成スクリプト(python-pptx)。実行: python build_deck.py
# フォントは M PLUS 1p(PowerPoint を動かす PC にインストールされている必要がある)。画像は img/ に置く。
from pptx import Presentation
from pptx.util import Inches, Pt
from pptx.dml.color import RGBColor
from pptx.enum.shapes import MSO_SHAPE
from pptx.enum.text import PP_ALIGN, MSO_ANCHOR

OUT = "LE3B_12_コバヤシ_ハヤト_レイマーチ雲とリボンビーム.pptx"
FONT = "M PLUS 1p"

# 配色: 空の青を主役に、リボンビームの橙をアクセントにする
NAVY = RGBColor(0x14, 0x28, 0x4B)
BLUE = RGBColor(0x2F, 0x6F, 0xDE)
SKY = RGBColor(0xE6, 0xEF, 0xFB)
CLOUD_GRAY = RGBColor(0x8F, 0xA3, 0xBF)
ORANGE = RGBColor(0xF2, 0x8C, 0x38)
INK = RGBColor(0x1E, 0x29, 0x3B)
MUTED = RGBColor(0x5B, 0x69, 0x7F)
WHITE = RGBColor(0xFF, 0xFF, 0xFF)

SLIDE_W, SLIDE_H = 13.333, 7.5
MARGIN = 0.7
TITLE_Y, TITLE_H = 0.4, 1.0
IMG_DIR = "img/"
CONTENT_W = SLIDE_W - 2 * MARGIN

prs = Presentation()
prs.slide_width = Inches(SLIDE_W)
prs.slide_height = Inches(SLIDE_H)
BLANK = prs.slide_layouts[6]


def set_bg(slide, color):
    fill = slide.background.fill
    fill.solid()
    fill.fore_color.rgb = color


def add_text(slide, x, y, w, h, paragraphs, size=20, color=INK, bold=False, align=PP_ALIGN.LEFT,
             anchor=MSO_ANCHOR.TOP, space_after=0, name=None):
    """テキストボックスを追加する。paragraphs は文字列、または段落ごとの文字列のリスト。"""
    box = slide.shapes.add_textbox(Inches(x), Inches(y), Inches(w), Inches(h))
    if name:
        box.name = name
    tf = box.text_frame
    tf.word_wrap = True
    tf.margin_left = tf.margin_right = tf.margin_top = tf.margin_bottom = 0
    tf.vertical_anchor = anchor
    items = paragraphs if isinstance(paragraphs, list) else [paragraphs]
    for i, text in enumerate(items):
        p = tf.paragraphs[0] if i == 0 else tf.add_paragraph()
        p.alignment = align
        p.space_after = Pt(space_after)
        r = p.add_run()
        r.text = text
        r.font.name = FONT
        r.font.size = Pt(size)
        r.font.bold = bold
        r.font.color.rgb = color
    return box


def add_box(slide, x, y, w, h, fill=SKY, shape=MSO_SHAPE.ROUNDED_RECTANGLE, name=None):
    s = slide.shapes.add_shape(shape, Inches(x), Inches(y), Inches(w), Inches(h))
    if name:
        s.name = name
    s.fill.solid()
    s.fill.fore_color.rgb = fill
    s.line.fill.background()
    s.shadow.inherit = False
    if shape == MSO_SHAPE.ROUNDED_RECTANGLE:
        s.adjustments[0] = 0.08
    return s


def add_title(slide, text, color=NAVY):
    add_text(slide, MARGIN, TITLE_Y, CONTENT_W, TITLE_H, text, size=26, color=color, bold=True,
             anchor=MSO_ANCHOR.MIDDLE, name="Title")


def add_badge(slide, x, y, d, label, fill=BLUE, size=18):
    c = slide.shapes.add_shape(MSO_SHAPE.OVAL, Inches(x), Inches(y), Inches(d), Inches(d))
    c.fill.solid()
    c.fill.fore_color.rgb = fill
    c.line.fill.background()
    c.shadow.inherit = False
    tf = c.text_frame
    tf.margin_left = tf.margin_right = tf.margin_top = tf.margin_bottom = 0
    tf.vertical_anchor = MSO_ANCHOR.MIDDLE
    p = tf.paragraphs[0]
    p.alignment = PP_ALIGN.CENTER
    r = p.add_run()
    r.text = label
    r.font.name = FONT
    r.font.size = Pt(size)
    r.font.bold = True
    r.font.color.rgb = WHITE
    return c


def add_arrow_down(slide, x, y, w=0.4, h=0.35, fill=MUTED):
    a = slide.shapes.add_shape(MSO_SHAPE.DOWN_ARROW, Inches(x), Inches(y), Inches(w), Inches(h))
    a.fill.solid()
    a.fill.fore_color.rgb = fill
    a.line.fill.background()
    a.shadow.inherit = False
    return a


def add_arrow_right(slide, x, y, w=0.5, h=0.4, fill=MUTED):
    a = slide.shapes.add_shape(MSO_SHAPE.RIGHT_ARROW, Inches(x), Inches(y), Inches(w), Inches(h))
    a.fill.solid()
    a.fill.fore_color.rgb = fill
    a.line.fill.background()
    a.shadow.inherit = False
    return a


def add_table(slide, x, y, w, col_w, rows, row_h=0.42, size=16, header_fill=NAVY):
    """表を追加する。rows[0] は見出し行。col_w は列幅(インチ)のリスト。"""
    shape = slide.shapes.add_table(len(rows), len(col_w), Inches(x), Inches(y), Inches(w), Inches(row_h * len(rows)))
    tbl = shape.table
    for i, cw in enumerate(col_w):
        tbl.columns[i].width = Inches(cw)
    for ri, row in enumerate(rows):
        tbl.rows[ri].height = Inches(row_h)
        for ci, text in enumerate(row):
            cell = tbl.cell(ri, ci)
            cell.margin_left = cell.margin_right = Inches(0.12)
            cell.margin_top = cell.margin_bottom = Inches(0.04)
            cell.vertical_anchor = MSO_ANCHOR.MIDDLE
            cell.fill.solid()
            cell.fill.fore_color.rgb = header_fill if ri == 0 else (WHITE if ri % 2 else SKY)
            p = cell.text_frame.paragraphs[0]
            r = p.add_run()
            r.text = text
            r.font.name = FONT
            r.font.size = Pt(size)
            r.font.bold = ri == 0
            r.font.color.rgb = WHITE if ri == 0 else INK
    return shape


def add_caption(slide, x, y, w, text):
    add_text(slide, x, y, w, 0.4, text, size=14, color=MUTED)


def notes(slide, text):
    # 話す内容はスライドに入れず、別ファイル(発表原稿)にまとめたため、何もしない
    pass


def blend(c1, c2, t):
    """2色を t(0 で c1、1 で c2)で混ぜる。"""
    return RGBColor(*(int(c1[i] + (c2[i] - c1[i]) * t) for i in range(3)))


# ---------------------------------------------------------------- 1 タイトル
s = prs.slides.add_slide(BLANK)
set_bg(s, NAVY)
add_text(s, 0.9, 1.5, 11.6, 2.0, "レイマーチ雲とリボンビームの実装", size=42, color=WHITE, bold=True, anchor=MSO_ANCHOR.BOTTOM, name="Title")
add_text(s, 0.9, 3.9, 11.5, 0.7, "― 雲海を飛ぶ3Dレールシューティングの演出として", size=26, color=SKY)
add_text(s, 0.9, 4.95, 11.5, 0.5, "LE3B_12_コバヤシ_ハヤト", size=22, color=SKY)
notes(s, "今回は、ホーミング弾のリボンビームと、雲海のレイマーチ描画を実装しました。")

# ---------------------------------------------------------------- 2 作品概要
s = prs.slides.add_slide(BLANK)
set_bg(s, WHITE)
add_title(s, "1.作品概要：爽快感のある3Dレールシューティング")
IMG2_W = 7.6
s.shapes.add_picture(IMG_DIR + "game_view.png", Inches(MARGIN), Inches(1.9), width=Inches(IMG2_W))
add_caption(s, MARGIN, 1.9 + IMG2_W * 591 / 1127 + 0.12, IMG2_W, "現在のゲーム画面")
add_text(s, 8.8, 1.9, 4.0, 0.5, "前回まで", size=24, color=BLUE, bold=True)
add_text(s, 8.8, 2.5, 4.0, 1.2, ["部品化", "エディタ"], size=22, space_after=4)
add_text(s, 8.8, 4.0, 4.0, 0.5, "今回", size=24, color=ORANGE, bold=True)
add_text(s, 8.8, 4.6, 4.0, 1.8, ["敵とボス", "リボンビーム", "レイマーチ雲"], size=22, space_after=4)
notes(s, "参考にしているのはスターフォックスとパンツァードラグーンです。前回は部品化とエディタが中心でした。今回は、敵とボス、弾の軌跡、雲に取り組みました。")

# ---------------------------------------------------------------- 3 敵紹介
s = prs.slides.add_slide(BLANK)
set_bg(s, WHITE)
add_title(s, "2.実装した敵：5種類")
enemies = [
    ("通常", "直進しながら、自機を狙って撃つ", BLUE),
    ("編隊", "3〜5体がまとまって現れる。全滅でボーナス", BLUE),
    ("タレット", "動かず、狙って連射する", BLUE),
    ("ホーミング", "近づくと突進し、体当たりで自爆する", ORANGE),
    ("ボス", "3フェーズで、自機狙い弾と扇状弾を撃つ", NAVY),
]
cw_, ch_, cg_ = 3.75, 2.15, 0.4
for i, (name, desc, col) in enumerate(enemies):
    x = MARGIN + (i % 3) * (cw_ + cg_)
    y = 1.75 + (i // 3) * (ch_ + 0.4)
    add_box(s, x, y, cw_, ch_, fill=SKY)
    add_badge(s, x + 0.3, y + 0.3, 0.6, str(i + 1), fill=col, size=20)
    add_text(s, x + 1.1, y + 0.3, cw_ - 1.3, 0.6, name, size=24, color=NAVY, bold=True, anchor=MSO_ANCHOR.MIDDLE)
    add_text(s, x + 0.3, y + 1.15, cw_ - 0.6, 0.9, desc, size=17, color=INK)
notes(s, "敵は5種類です。通常の敵は直進しながら自機を狙います。編隊は3〜5体でまとまって現れ、全滅するとボーナスです。タレットは動かずに狙って連射します。ホーミングは近づくと突進して体当たりします。ボスは3つのフェーズで、自機狙い弾と扇状弾を撃ちます。")

# ---------------------------------------------------------------- 4 リボン比較GIF
s = prs.slides.add_slide(BLANK)
set_bg(s, WHITE)
add_title(s, "3.リボンビーム：球の弾から、軌跡が見える光の帯へ")
GIF_W = 5.9
GIF_H = GIF_W * 537 / 960
gx = [MARGIN, SLIDE_W - MARGIN - GIF_W]
add_text(s, gx[0], 1.65, GIF_W, 0.5, "旧：球の弾", size=24, color=MUTED, bold=True)
add_text(s, gx[1], 1.65, GIF_W, 0.5, "新：リボンビーム", size=24, color=ORANGE, bold=True)
s.shapes.add_picture(IMG_DIR + "bullet_sphere.gif", Inches(gx[0]), Inches(2.3), width=Inches(GIF_W))
s.shapes.add_picture(IMG_DIR + "bullet_ribbon.gif", Inches(gx[1]), Inches(2.3), width=Inches(GIF_W))
add_text(s, gx[0], 2.3 + GIF_H + 0.25, GIF_W, 0.6, "どこへ飛ぶのか、分かりにくい", size=20)
add_text(s, gx[1], 2.3 + GIF_H + 0.25, GIF_W, 0.6, "軌跡で、飛ぶ先が見える", size=20, color=NAVY, bold=True)
notes(s, "左が以前の球の弾、右が今回のリボンビームです。球だと、どこへ向かっているかが見えません。リボンビームは、弾の通った跡が光の帯として残るので、ロックオンした弾が飛ぶ先が分かります。")

# ---------------------------------------------------------------- 5 リボン手法と工夫
s = prs.slides.add_slide(BLANK)
set_bg(s, WHITE)
add_title(s, "3.リボンビーム：4つの手法を比べ、CPUで帯を作る方式を採用")
add_text(s, MARGIN, 1.55, 6.2, 0.5, "手法の比較", size=22, color=NAVY, bold=True)
add_table(s, MARGIN, 2.1, 6.2, [3.2, 3.0], [
    ["手法", "欠点"],
    ["A CPUで帯を作る（採用）", "仕組みを新しく作る"],
    ["B パーティクルを並べる", "速いと途切れる"],
    ["C 線でつなぐ", "太さ1pxで帯にならない"],
    ["D GPUパーティクル", "実装が重く、難しい"],
], row_h=0.5, size=15)
add_text(s, MARGIN, 4.8, 6.2, 0.9, "採用理由：作り方を理解でき、他の軌跡にも使える", size=16, color=INK)
add_text(s, 7.5, 1.55, 5.2, 0.5, "つまずいた点と対策", size=22, color=NAVY, bold=True)
fixes = [
    ("点の間隔が速度でばらつく", "一定距離ごとに点を記録"),
    ("止まると尾が太く残る", "太さと消え方を、点の年齢で決める"),
    ("弾が消えると尾も消える", "尾を弾から切り離して残す"),
]
for i, (prob, fix) in enumerate(fixes):
    y = 2.15 + i * 1.2
    add_box(s, 7.5, y, 5.15, 1.05, fill=SKY)
    add_text(s, 7.8, y + 0.12, 4.6, 0.4, prob, size=15, color=MUTED)
    add_text(s, 7.8, y + 0.52, 4.6, 0.45, "→ " + fix, size=17, color=NAVY, bold=True)
add_box(s, MARGIN, 5.9, CONTENT_W, 1.0, fill=ORANGE)
add_text(s, MARGIN + 0.4, 5.9, CONTENT_W - 0.8, 1.0, "弾が増えるとCPUでは重くなるので、のちにGPUで描く方式へ移す", size=20, color=WHITE, bold=True, anchor=MSO_ANCHOR.MIDDLE)
notes(s, "手法は4つ比べました。Aは帯のポリゴンをCPUで作る方法で、帯の作り方を理解でき、剣の軌跡など他にも流用できるので採用しました。つまずいた点は3つです。毎フレーム点を打つと速度で間隔がばらつくので、一定距離ごとに記録しました。位置の順で太さを決めると、止まったとき尾が太く残るので、点の年齢で決めました。弾が消えると尾も消えて不自然だったので、尾を弾から切り離しました。ただしCPUで作る方式は、弾が増えると重くなるので、のちにGPUで描く方式へ移す予定です。")

# ---------------------------------------------------------------- 6 雲: 目的と手法
s = prs.slides.add_slide(BLANK)
set_bg(s, WHITE)
add_title(s, "4.雲：雲の中を飛べるように、箱のボリューム雲を採用")
add_text(s, MARGIN, 1.55, CONTENT_W, 0.5, "目的：雲の中を飛べるようにして、ゲームの遊びに使う", size=20, color=NAVY, bold=True)
add_table(s, MARGIN, 2.2, CONTENT_W, [3.6, 4.6, 3.9], [
    ["手法", "長所", "短所"],
    ["A ビルボード（平面の雲）", "軽い", "中に入れない"],
    ["B 箱のボリューム雲（採用）", "中を通れる。箱の画素だけ計算すればよい", "重い。深度の受け渡しが要る"],
    ["C AとBの併用", "重さを見せ場に集中できる", "仕組みが2つ並ぶ"],
], row_h=0.62, size=16)
add_text(s, MARGIN, 5.0, CONTENT_W, 1.6, ["固定レールなので、雲の箱を置く場所を決められる"], size=18, color=INK, space_after=8)
notes(s, "雲の目的は、中を飛べるようにして、遊びに使うことです。平面のビルボードは軽いですが、中に入れません。そこで、箱のボリューム雲を採用しました。固定レールなので、雲の箱を置く場所を決められて、箱が映る画素だけ計算すれば済みます。")

# ---------------------------------------------------------------- 7 描き方
s = prs.slides.add_slide(BLANK)
set_bg(s, WHITE)
add_title(s, "4.雲：3Dノイズとレイマーチで描く")
IMG6_W = 6.9
s.shapes.add_picture(IMG_DIR + "cloud_hero.png", Inches(MARGIN), Inches(1.7), width=Inches(IMG6_W))
add_caption(s, MARGIN, 1.7 + IMG6_W * 900 / 1602 + 0.12, IMG6_W, "実際の描画結果")
steps = [
    ("形", "3Dノイズ"),
    ("描き方", "レイマーチ"),
    ("光", "太陽へも進み、影を求める"),
]
for i, (head, body) in enumerate(steps):
    y = 1.7 + i * 1.45
    add_box(s, 8.0, y, 4.65, 1.25, fill=SKY)
    add_badge(s, 8.25, y + 0.33, 0.6, str(i + 1), size=20)
    add_text(s, 9.1, y + 0.15, 3.4, 0.45, head, size=18, color=NAVY, bold=True)
    add_text(s, 9.1, y + 0.62, 3.4, 0.5, body, size=18, color=INK)
add_text(s, MARGIN, 6.2, 7.0, 0.9, ["ノイズは毎回計算せず、起動時に1回だけ作る（約265ms）", "→ 毎ステップ計算する方式より軽い"], size=16, color=MUTED, space_after=2)
notes(s, "形は、64×64×64の3Dノイズに、細部の32×32×32を重ねて作ります。描き方は、視線を雲の中で少しずつ進めて、密度を積み上げるレイマーチです。光は、太陽の方向にも進んで、影と光の散りを表します。ノイズは毎回計算すると重いので、起動時に1回だけ作った3D画像を引くようにしました。")

# ---------------------------------------------------------------- 8 負荷
s = prs.slides.add_slide(BLANK)
set_bg(s, WHITE)
add_title(s, "5.負荷：先に測り、低解像度で描く方式を採用")
add_text(s, MARGIN, 1.55, 5.6, 0.5, "先に測る", size=22, color=NAVY, bold=True)
add_text(s, MARGIN, 2.15, 5.6, 1.9, ["GPU時間を測る計測器を、最初に作った", "「計測なしの最適化は当てずっぽう」", "最初の版は、画面を覆うと最大約3.3ms", "（60fpsの枠16.6msの約2割）"], size=17, space_after=8)
add_text(s, MARGIN, 4.2, 5.6, 0.5, "他の手段との比較", size=22, color=NAVY, bold=True)
add_text(s, MARGIN, 4.8, 5.6, 2.0, ["低解像度：効果大、実装は中（採用）", "時間方向の再利用：最大だが、残像が出る", "ステップ数の削減：細部を取りこぼす"], size=16, space_after=6)
add_text(s, 6.9, 1.55, 5.8, 0.5, "解像度を変えて実測", size=22, color=NAVY, bold=True)
add_table(s, 6.9, 2.15, 5.75, [1.3, 1.9, 2.55], [
    ["分母", "雲の負荷", "見た目"],
    ["1（等倍）", "1.6〜2.4ms", "基準"],
    ["2（採用）", "0.41〜0.56ms", "等倍と区別がつかない"],
    ["4", "0.23〜0.26ms", "粒が目立つ"],
], row_h=0.6, size=15)
add_text(s, 6.9, 4.9, 5.75, 1.8, ["約1/4に下がった", "雲はぼやけた形が多く、半分にしても目立たない", "拡大時に深度を見て、物体の縁のにじみを抑える"], size=16, color=INK, space_after=6)
notes(s, "まず、GPU時間を測る計測器を作りました。計測なしの最適化は当てずっぽうになるからです。負荷を減らす手段は7つ比べて、効果が大きく実装が重くない低解像度を最初に採用しました。分母1、2、4で実測すると、分母2で約1/4になり、見た目は等倍と区別がつきませんでした。分母4は粒が目立つので不採用です。")

# ---------------------------------------------------------------- 9 自動調整
s = prs.slides.add_slide(BLANK)
set_bg(s, WHITE)
add_title(s, "5.負荷：GPUの実測で、雲の品質を4段階に自動調整")
add_text(s, MARGIN, 1.55, 5.6, 0.5, "なぜ自動調整か", size=22, color=NAVY, bold=True)
add_text(s, MARGIN, 2.15, 5.6, 1.2, ["ノートPCは、電源や熱でGPUの速さが大きく変わる", "同じ処理が、日によって1msにも3msにもなった"], size=16, space_after=6)
add_table(s, MARGIN, 3.5, 5.6, [2.3, 3.3], [
    ["方式", "判断"],
    ["設定で固定", "状況に追従しない"],
    ["GPU名で決める", "電源や熱に追従しない"],
    ["実測で調整（採用）", "追従する"],
], row_h=0.5, size=15)
add_text(s, 6.9, 1.55, 5.8, 0.5, "段と制御", size=22, color=NAVY, bold=True)
add_table(s, 6.9, 2.15, 5.75, [0.9, 2.2, 2.65], [
    ["段", "解像度の分母", "ステップ数の上限"],
    ["0", "基準どおり", "64"],
    ["1", "2", "24"],
    ["2", "2", "16"],
    ["3", "4", "16"],
], row_h=0.45, size=15)
add_text(s, 6.9, 4.6, 5.75, 2.2, ["13msを超えたら、1段下げる", "8ms未満が続けば、1段上げる", "上げてすぐ下がれば、待ち時間を倍にする"], size=16, space_after=6)
notes(s, "ノートPCは電源や熱でGPUの速さが大きく変わります。実際、同じ処理が、日によって1ミリ秒にも3ミリ秒にもなりました。設定で固定するか、GPU名で決めるか、実測で調整するかを比べて、電源や熱に追従できる実測を採用しました。13msを超えたら1段下げ、8ms未満が続けば1段上げます。上げてすぐ下がったら、次に上げるまでの待ち時間を倍にして、往復を防ぎます。")

# ---------------------------------------------------------------- 10 問題と解決
s = prs.slides.add_slide(BLANK)
set_bg(s, WHITE)
add_title(s, "6.問題と解決：縞とドットを、ずらしとぼかしで消した")
flow = [
    ("縞", "歩幅が粗く、雲の表面が段状にずれる", "画素ごとに位置をずらす", BLUE),
    ("ドット", "ずらしの乱数が格子状で、2倍拡大が重なる", "青色雑音に替え、拡大時にぼかす", BLUE),
    ("結果", "ドットは消えた", "代わりに、縁が少しやわらかくなった", ORANGE),
]
fw, fg = 3.75, 0.4
for i, (head, prob, fix, col) in enumerate(flow):
    x = MARGIN + i * (fw + fg)
    add_box(s, x, 1.7, fw, 3.6, fill=SKY)
    add_badge(s, x + 0.3, 1.95, 0.6, str(i + 1), fill=col, size=20)
    add_text(s, x + 1.1, 1.95, fw - 1.3, 0.6, head, size=24, color=NAVY, bold=True, anchor=MSO_ANCHOR.MIDDLE)
    add_text(s, x + 0.3, 2.85, fw - 0.6, 1.0, prob, size=16, color=MUTED)
    add_text(s, x + 0.3, 3.95, fw - 0.6, 1.2, "→ " + fix, size=17, color=NAVY, bold=True)
add_text(s, MARGIN, 5.6, CONTENT_W, 1.2, ["対策は4つ比べた：ぼかし、青色雑音、時間方向の平均、歩数を増やす", "効果と負担から、ぼかしと青色雑音を先に採用した"], size=17, color=INK, space_after=6)
notes(s, "雲を描くと、まず水平な縞が出ました。歩幅が粗くて、雲の表面が段状にずれるためです。そこで画素ごとに位置をずらして、縞を粒に変えました。すると今度は、網点のようなドットが出ました。ずらしの乱数が格子状の構造を持っていたことと、半分の解像度で描いて2倍に拡大することが重なったためです。対策を4つ比べて、効果が大きく負担の小さい、ぼかしと青色雑音を採用しました。ドットは消えましたが、引き換えに縁が少しやわらかくなりました。")

# ---------------------------------------------------------------- 11 陽炎
s = prs.slides.add_slide(BLANK)
set_bg(s, WHITE)
add_title(s, "7.未解決：カメラが進むと、雲海が陽炎のように揺れる")
KG_W = 6.0
s.shapes.add_picture(IMG_DIR + "kagerou.gif", Inches(MARGIN), Inches(1.8), width=Inches(KG_W))
add_caption(s, MARGIN, 1.8 + KG_W * 447 / 800 + 0.12, KG_W, "動かしたときだけ発生。静止画では出ない")
add_text(s, MARGIN, 5.85, KG_W, 1.4, ["気づいたきっかけ：", "開発用は、開始までカメラが止まっていて、", "製品版は最初から進むので、揺れが見えた"], size=15, color=INK, space_after=2)
add_text(s, 7.1, 1.7, 5.5, 0.5, "原因の仮説は4つ", size=22, color=NAVY, bold=True)
add_table(s, 7.1, 2.3, 5.55, [2.65, 2.0, 0.9], [
    ["仮説", "対策", "規模"],
    ["ノイズ層の視差", "雲底を詰める", "小"],
    ["ステップの量子化", "二分探索", "中"],
    ["画面固定ノイズ", "時間方向の蓄積", "大"],
    ["低解像度の縁", "縁だけ高解像度", "大"],
], row_h=0.58, size=16)
add_text(s, 7.1, 5.4, 5.5, 1.3, ["静止画の縦の像は、雲底を足して消えた", "動いたときは未確認。小さい対策から検証する"], size=16, color=NAVY, bold=True, space_after=4)
notes(s, "症状は、カメラが進んだときだけ出て、静止画では出ません。開発用の画面は、開始を押すまでカメラが止まっているので気づかず、製品版は最初から進むので揺れが見えました。原因は4つの仮説に分けました。静止画で見えた縦の像は、雲の底を足して消えましたが、動いたときの確認はまだです。規模の小さい対策から順に検証します。")

# ---------------------------------------------------------------- 12 雲の構想(未実装)
s = prs.slides.add_slide(BLANK)
set_bg(s, WHITE)
add_title(s, "8.今後の構想（未実装）：雲を隠れ場所にする")
zones = [
    ("雲の外", ["敵が見える", "ロックオンできる"], SKY, INK),
    ("雲の中", ["敵はシルエットだけ", "ロックオンできない"], CLOUD_GRAY, WHITE),
    ("雲を抜けた直後", ["ロックオンして", "一斉射撃"], ORANGE, WHITE),
]
zw, zg, zy, zh = 3.6, 0.55, 1.9, 2.9
for i, (head, body, fill, fg) in enumerate(zones):
    x = MARGIN + i * (zw + zg)
    add_box(s, x, zy, zw, zh, fill=fill)
    add_text(s, x + 0.35, zy + 0.4, zw - 0.7, 0.6, head, size=26, color=fg, bold=True)
    add_text(s, x + 0.35, zy + 1.3, zw - 0.7, 1.4, body, size=20, color=fg, space_after=4)
    if i < len(zones) - 1:
        add_arrow_right(s, x + zw + 0.07, zy + zh / 2 - 0.2, w=0.42, h=0.4)
add_box(s, MARGIN, 5.4, CONTENT_W, 1.2, fill=NAVY)
add_text(s, MARGIN + 0.4, 5.4, CONTENT_W - 0.8, 1.2, "未実装：現状、雲は背景だけ。この動きをこれから作る", size=22, color=WHITE,
         bold=True, anchor=MSO_ANCHOR.MIDDLE)
notes(s, "ここは、まだ実装していない構想です。雲の中では敵がシルエットだけになってロックオンできず、抜けた瞬間にロックオンして一斉に撃てる、という遊びを考えています。深度を使った合成は、すでに雲の中の敵が霞む形で動いているので、その延長で作れます。")

# ---------------------------------------------------------------- 13 今後
s = prs.slides.add_slide(BLANK)
set_bg(s, NAVY)
add_title(s, "9.今後：雲を遊びに接続し、ボスとリボンを作り込む", color=WHITE)
rows = [
    ("雲が、背景だけ", "視界とロックオンに反映"),
    ("ボスの動きが固定", "ステートパターンで切り替え"),
    ("リボンがCPU描画", "GPUで描く方式へ移行"),
    ("雲海が揺れる", "仮説を順に検証"),
]
for i, (problem, plan) in enumerate(rows):
    y = 1.7 + i * 1.3
    add_box(s, MARGIN, y, 5.4, 1.1, fill=RGBColor(0x1F, 0x3A, 0x6B))
    add_text(s, MARGIN + 0.3, y, 4.8, 1.1, problem, size=20, color=WHITE, bold=True, anchor=MSO_ANCHOR.MIDDLE)
    add_arrow_right(s, MARGIN + 5.55, y + 0.35, w=0.6, h=0.4, fill=ORANGE)
    add_box(s, MARGIN + 6.3, y, 5.6, 1.1, fill=ORANGE)
    add_text(s, MARGIN + 6.6, y, 5.0, 1.1, plan, size=20, color=WHITE, bold=True, anchor=MSO_ANCHOR.MIDDLE)
notes(s, "今後は5つです。雲を遊びに接続すること、ボスをステートパターンにすること、リボンをGPUで描く方式へ移すこと、サウンドとスコア表示、そして陽炎の解消です。")

# ---------------------------------------------------------------- 日本語フォントの指定
# python-pptx は run.font.name で欧文フォントしか指定しない。東アジア文字用のフォント(ea)と
# 言語(ja-JP)が無いと、表示側が中国語向けの字形を選んで、見慣れない漢字になる。全ての文字に追加する。
from pptx.oxml.ns import qn
from lxml import etree

JA_LANG = "ja-JP"


def fix_japanese_runs(slide):
    for rpr in slide._element.iter(qn("a:rPr"), qn("a:endParaRPr")):
        rpr.set("lang", JA_LANG)
        rpr.set("altLang", "en-US")
        latin = rpr.find(qn("a:latin"))
        if latin is None:
            continue
        for tag in ("a:ea", "a:cs"):
            old = rpr.find(qn(tag))
            if old is not None:
                rpr.remove(old)
        ea = etree.SubElement(rpr, qn("a:ea"))
        ea.set("typeface", FONT)
        cs = etree.SubElement(rpr, qn("a:cs"))
        cs.set("typeface", FONT)
        # 子要素の順序: latin, ea, cs の順にそろえる
        rpr.remove(ea); rpr.remove(cs)
        idx = list(rpr).index(latin) + 1
        rpr.insert(idx, ea); rpr.insert(idx + 1, cs)


for sl in prs.slides:
    fix_japanese_runs(sl)

prs.save(OUT)
print("saved", OUT)
