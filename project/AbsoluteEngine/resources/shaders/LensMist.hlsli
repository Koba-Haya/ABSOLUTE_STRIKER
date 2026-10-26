#ifndef LENS_MIST_HLSLI
#define LENS_MIST_HLSLI

// --- 霧とレンズの水滴(再利用する部品) ---
// 画面遷移の最終パス(Transition.PS.hlsl)から呼ぶ。場面(雲、海など)を知らず、受け取ったパラメータだけで見た目が決まる。
// 色の計算は、表示用の値(ガンマ空間)で行い、最後にリニアへ戻す(RTV が sRGB のため)。
// Noise.hlsli を先に include すること。
//
// 霧の考え方: 物体までの距離 d と、その時点の視程(どこまで見えるか)から、霧の量を 1 - exp(-d / 視程) とする。
//   覆い具合が上がると、視程が遠い値から近い値へ縮む。近い物体は残り、遠くが先に霞む。
//   覆い具合が下がる(晴れる)と、視程が広がり、近くから先に見える。

static const float32_t kMistDisplayGamma = 2.2f;
static const float32_t kMistLumaR = 0.2126f;           // 輝度の重み
static const float32_t kMistLumaG = 0.7152f;
static const float32_t kMistLumaB = 0.0722f;
static const float32_t kMistMinVisibility = 0.001f;    // 視程の下限(0 割りを避ける)

// 濃淡のムラ: 斜めに引き伸ばしたノイズ(風に流された雲の筋)が、ゆっくり流れる
static const float32_t kMistStreakAngle = 0.35f;       // 筋の傾き(ラジアン)
static const float32_t2 kMistStreakScale = float32_t2(1.2f, 5.0f); // 筋に沿う方向 / 直交する方向の細かさ(画面の高さあたりの波の数)
static const float32_t kMistStreakDrift = 0.05f;       // 筋に直交する方向へ流れる速さ(秒あたり)
static const float32_t kMistStreakOctaves = 2.0f;      // 2 層目の細かさの倍率
static const float32_t kMistStreakSecondWeight = 0.45f;
static const float32_t kMistStreakOffset = 0.5f;       // ノイズの平均
static const float32_t kMistStreakAmount = 1.1f;       // 距離への掛け率の揺れの幅(1 + 揺れ)

// 最後の覆い: 覆い具合が wrapStart を超えたら、霧の量へ足して、入れ替えの瞬間を隠す(完全に一様にはしない)
static const float32_t kMistWrapMax = 0.97f;           // 覆い具合 1 のときの下限(これ以上は、ムラで少し抜ける)
static const float32_t kMistWrapMottling = 0.12f;      // 最後の覆いに残す濃淡

// 太陽側の明るさ(霧の色)
static const float32_t kMistSunPower = 6.0f;           // 太陽の向きとの角度の絞り
static const float32_t kMistSunBrighten = 0.55f;       // 太陽側で霧の色へ足す明るさ(表示値)
static const float32_t3 kMistSunWarm = float32_t3(1.0f, 0.93f, 0.82f); // 太陽側に混ぜる色

// 画素ごとの霧の量(0〜1)。
//   distance: 視点から画素の物体までの距離(深度の無い背景は、呼び出し側が有限の遠距離にして渡す)
//   cover: 覆い具合  visibility: (近い側の視程, 遠い側の視程)  wrapStart: 最後の覆いを始める覆い具合
float32_t MistAmount(float32_t distance, float32_t2 uv, float32_t aspect, float32_t time, float32_t cover,
                     float32_t2 visibility, float32_t wrapStart) {
    // 視程: 覆い具合 0 で far(ほぼ見渡せる)、1 で near。対数で補間するので、濃さの増え方がなだらか
    float32_t visNear = max(visibility.x, kMistMinVisibility);
    float32_t visFar = max(visibility.y, visNear);
    float32_t vis = visFar * pow(visNear / visFar, cover);

    // 濃淡のムラ: 距離に掛ける(筋の濃い所は、距離が長いのと同じ効果)
    float32_t sinA = sin(kMistStreakAngle);
    float32_t cosA = cos(kMistStreakAngle);
    float32_t2 q = float32_t2(uv.x * aspect, uv.y);
    float32_t2 r = float32_t2(q.x * cosA - q.y * sinA, q.x * sinA + q.y * cosA);
    r = r * kMistStreakScale + float32_t2(0.0f, kMistStreakDrift * time);
    float32_t n = ValueNoise(r) + kMistStreakSecondWeight * ValueNoise(r * kMistStreakOctaves + 7.0f);
    n = n / (1.0f + kMistStreakSecondWeight) - kMistStreakOffset;
    float32_t mottle = max(1.0f + n * kMistStreakAmount, 0.0f);

    float32_t amount = 1.0f - exp(-distance * mottle / vis);

    // 最後の覆い: 入れ替えの瞬間を隠す。わずかな濃淡を残す
    float32_t wrap = smoothstep(wrapStart, 1.0f, cover);
    float32_t wrapAmount = wrap * (kMistWrapMax + n * kMistWrapMottling);
    return saturate(max(amount, wrapAmount));
}

// 霧をかける。
//   scene: 元の画面(リニア)  blurred: ぼかした画面(リニア。霧の量に応じた半径で、呼び出し側が作る)
//   amount: MistAmount の結果  viewDir: 画素の視線(世界)  sunDir: 太陽へ向かう向き(世界)
//   desaturate: 霧が濃い所の彩度の落とし量(0〜1)  sunGlow: 太陽側の明るさの強さ  fogColor: 霧の基本の色(リニア)
float32_t3 ApplyMistFog(float32_t3 scene, float32_t3 blurred, float32_t amount, float32_t3 viewDir, float32_t3 sunDir,
                        float32_t desaturate, float32_t sunGlow, float32_t3 fogColor) {
    // ぼかした画面を混ぜる(霧の向こうは、輪郭がやわらぐ)
    float32_t3 color = lerp(scene, blurred, amount);
    float32_t3 display = pow(max(color, 0.0f), 1.0f / kMistDisplayGamma);

    // 霧の濃い所ほど、彩度を落とす
    float32_t luma = dot(display, float32_t3(kMistLumaR, kMistLumaG, kMistLumaB));
    display = lerp(display, float32_t3(luma, luma, luma), amount * desaturate);

    // 霧の色: 太陽の側ほど明るく、暖かくする
    float32_t sunDot = pow(saturate(dot(viewDir, sunDir)), kMistSunPower);
    float32_t3 fogDisplay = pow(max(fogColor, 0.0f), 1.0f / kMistDisplayGamma);
    fogDisplay = lerp(fogDisplay, saturate(fogDisplay * kMistSunWarm + kMistSunBrighten), sunDot * sunGlow);

    display = lerp(display, fogDisplay, amount);
    return pow(saturate(display), kMistDisplayGamma);
}

// ---- 小粒の水滴 ----
// 画面を格子(セル)に分け、セルごとに乱数で、水滴の有無・位置・大きさ・付く順番を決める。状態を持たず、
// wetness(0〜1)だけで形が決まる: 付く順番(乱数)より wetness が大きくなった水滴から、0 から大きくなる。
// wetness が下がると、順番どおりに縮んで消える。画面の高さを基準にしたセル数なので、解像度が変わっても同じに見える。

static const float32_t kSmallDropExistence = 0.55f;      // 水滴を持つセルの割合
static const float32_t kSmallDropGrowSpan = 0.12f;       // 付き始めてから、最大の大きさになるまでの wetness の幅
static const float32_t kSmallDropRadiusMin = 0.12f;      // 水滴の半径(セルを 1 とする)
static const float32_t kSmallDropRadiusMax = 0.36f;
static const float32_t kSmallDropRadiusBias = 1.8f;      // 大きさの乱数のべき(大きいほど、小さい水滴が多い)
static const float32_t kSmallDropMinPixelStart = 0.5f;  // 半径がこのピクセル数以下の水滴は消す(画素より小さい水滴は、サンプリングの干渉で縞になる)
static const float32_t kSmallDropMinPixelEnd = 1.4f;    // この半径(ピクセル)で、完全に見える
static const float32_t kSmallDropEdgeAA = 1.5f;          // 輪郭をなめらかにする幅(ピクセル)
static const float32_t kSmallDropClusterScale = 2.0f;  // 密度のムラの細かさ(画面の高さあたりの波の数)
static const float32_t kSmallDropClusterLow = 0.12f;   // ムラの低い所で、水滴を持つセルの割合に掛ける倍率
static const float32_t kSmallDropClusterHigh = 1.9f;   // ムラの高い所の倍率
static const float32_t kSmallDropClusterEdgeLow = 0.36f; // ノイズをこの範囲で 0〜1 に引き伸ばす(固まりの輪郭の鋭さ)
static const float32_t kSmallDropClusterEdgeHigh = 0.64f;
static const int32_t kSmallDropSeedOrder = 0;            // 乱数の種(用途ごとにずらす)
static const int32_t kSmallDropSeedOffsetX = 31;
static const int32_t kSmallDropSeedOffsetY = 7;
static const int32_t kSmallDropSeedSize = 19;

struct DropletHit {
    float32_t mask;      // 水滴の中(1)か外(0)。輪郭はなめらか
    float32_t2 normal;   // 水滴の中の位置(-1〜1。中心が 0、縁が長さ 1)
    float32_t radiusUv;  // 水滴の半径(画面の高さを 1 とする)
};

// 水滴の密度のムラ(0〜1)。低周波のノイズで、水滴が固まる場所と、ほぼ無い場所を作る。
// 位置は、セルの中心などの「セルごとに 1 つ決まる点」で呼ぶこと(画素ごとに呼ぶと、水滴が途中で欠ける)。
// centerUv: 画面の UV
float32_t DropletDensity(float32_t2 centerUv, float32_t aspect) {
    float32_t n = ValueNoise(float32_t2(centerUv.x * aspect, centerUv.y) * kSmallDropClusterScale + 13.7f);
    return smoothstep(kSmallDropClusterEdgeLow, kSmallDropClusterEdgeHigh, n);
}

// 1 層分の小粒。cellsPerHeight: 画面の高さあたりのセル数  seed: 層ごとの乱数の種
// rotation: 格子を回す角度(ラジアン)。層ごとに変えると、縦横の並び(網戸のような格子)が見えなくなる
DropletHit SmallDropletLayer(float32_t2 uv, float32_t aspect, float32_t wetness, float32_t cellsPerHeight, int32_t seed, float32_t rotation) {
    DropletHit hit;
    hit.mask = 0.0f;
    hit.normal = float32_t2(0.0f, 0.0f);
    hit.radiusUv = 0.0f;

    // 格子は、画面の高さを基準にした座標を rotation だけ回して作る
    float32_t sinR = sin(rotation);
    float32_t cosR = cos(rotation);
    float32_t2 world = float32_t2(uv.x * aspect, uv.y);
    float32_t2 p = float32_t2(world.x * cosR - world.y * sinR, world.x * sinR + world.y * cosR) * cellsPerHeight;
    int2 id = int2(floor(p));
    float32_t2 st = frac(p);
    float32_t aa = (fwidth(p.x) + fwidth(p.y)) * 0.5f * kSmallDropEdgeAA; // 微分は、早期 return の前に求める(分岐の中では不定になる)

    // order: 付く順番(0〜1。水滴を持たないセルは 1 を超える)
    float32_t2 cellCenterRot = (float32_t2(id) + 0.5f) / cellsPerHeight;
    float32_t2 cellCenterWorld = float32_t2(cellCenterRot.x * cosR + cellCenterRot.y * sinR, -cellCenterRot.x * sinR + cellCenterRot.y * cosR);
    float32_t2 cellCenterUv = float32_t2(cellCenterWorld.x / aspect, cellCenterWorld.y);
    float32_t density = lerp(kSmallDropClusterLow, kSmallDropClusterHigh, DropletDensity(cellCenterUv, aspect));
    float32_t order = Hash21(id + int2(seed, kSmallDropSeedOrder)) / (kSmallDropExistence * density);
    float32_t hx = Hash21(id + int2(seed + kSmallDropSeedOffsetX, 17));
    float32_t hy = Hash21(id + int2(seed + kSmallDropSeedOffsetY, 53));
    float32_t hs = Hash21(id + int2(seed + kSmallDropSeedSize, 91));

    float32_t grow = smoothstep(order, order + kSmallDropGrowSpan, wetness) * step(order, 1.0f);
    float32_t fullRadius = lerp(kSmallDropRadiusMin, kSmallDropRadiusMax, pow(hs, kSmallDropRadiusBias));
    float32_t radius = fullRadius * grow;
    if (radius <= 0.0f) {
        return hit;
    }

    // 中心は、その水滴自身の最大の半径の分だけ縁から離す(大きくなっても、隣のセルへはみ出さない)。
    // 小さい水滴ほど、セルの中で動ける範囲が広くなり、並びが不規則になる
    float32_t2 center = float32_t2(lerp(fullRadius, 1.0f - fullRadius, hx), lerp(fullRadius, 1.0f - fullRadius, hy));
    float32_t2 q = st - center;
    // 画素の大きさ(セルを 1 とする)に対する半径が小さい水滴は、薄めて消す
    float32_t pixelSize = aa / kSmallDropEdgeAA;
    float32_t visible = smoothstep(kSmallDropMinPixelStart, kSmallDropMinPixelEnd, radius / pixelSize);
    hit.mask = (1.0f - smoothstep(radius - aa, radius + aa, length(q))) * visible;
    // 屈折の向きは画面の軸で使うので、回した格子の向きから戻す
    float32_t2 n = q / radius;
    hit.normal = float32_t2(n.x * cosR + n.y * sinR, -n.x * sinR + n.y * cosR);
    hit.radiusUv = radius / cellsPerHeight;
    return hit;
}

// ---- 流れる大粒 ----
// 画面の中心から外へ向かって流れる(前進の風)。セルは、極座標(角度の区画 x 半径の帯)で作る。
// 水滴は、自分のセルの中を、内側から外側へ滑り、通った所に細い跡を残す。
//   付く: 小粒と同じ wetness の順番で、大きくなる
//   流れる: lensTime(水滴が付き始めてからの秒)と、粒ごとの遅れ・速さで、位置が決まる(状態を持たない)
//   消える: wetness が下がると、水滴と跡が縮んで消える。滑り終えた水滴は、セルの外側の端で止まる
// 水滴は、半径が画面の高さを 1 として一定になるよう、弧の長さ(角度 x 半径)で測る。中心の近くは区画が細いので、小さくして避ける。

static const float32_t kMistPi = 3.14159265f;
static const float32_t kMistTwoPi = 6.2831853f;
static const float32_t kRunDropSectors = 40.0f;        // 角度の区画の数(整数にする。継ぎ目で合わせるため)
static const float32_t kRunDropBand = 0.42f;           // 半径の帯の長さ(画面の高さを 1 とする)。1 つの水滴が滑れる長さの上限
static const float32_t kRunDropExistence = 0.36f;      // 水滴を持つセルの割合
static const float32_t kRunDropRadiusMin = 0.008f;     // 半径(画面の高さを 1 とする)
static const float32_t kRunDropRadiusMax = 0.027f;
static const float32_t kRunDropRadiusBias = 1.5f;
static const float32_t kRunDropWidthFill = 0.40f;      // 区画の幅に対する半径の上限(区画の外へ欠けないように)
static const float32_t kRunDropStartMin = 0.14f;       // 滑り始める位置・止まる位置(帯の長さに対する割合)
static const float32_t kRunDropStartMax = 0.30f;
static const float32_t kRunDropEndMin = 0.45f;
static const float32_t kRunDropEndMax = 0.92f;
static const float32_t kRunDropDelayMin = 0.15f;       // 付いてから滑り始めるまでの遅れ(秒)
static const float32_t kRunDropDelaySpread = 0.9f;
static const float32_t kRunDropDurationMin = 1.1f;     // 滑り終えるまでの時間(秒)
static const float32_t kRunDropDurationSpread = 1.9f;
static const float32_t kRunDropStretchBase = 1.12f;    // 縦(流れる向き)に長くする度合い(止まっているとき)
static const float32_t kRunDropStretchMovingMin = 0.20f; // 滑っている間に、さらに長くなる量(水滴ごとに幅を持たせる)
static const float32_t kRunDropStretchMovingMax = 0.95f;
static const float32_t kRunDropStayChance = 0.25f;     // この割合の水滴は、滑らずにその場に留まる(大粒が残る)
static const float32_t kRunDropMeanderFreq = 55.0f;    // 滑る道筋の、横のうねり(半径方向の細かさ。画面の高さあたり)
static const float32_t kRunDropMeanderAmount = 0.30f;  // うねりの大きさ(水滴の半径に対する倍率)
static const float32_t kRunDropTrailDryOffset = 0.15f; // 跡は、水滴より先に乾く(付く・乾くのしきい値を、この割合だけ濡れ具合の高い側へずらす)
static const float32_t kRunDropCenterFadeStart = 0.10f; // 画面の中心の近くは、水滴を小さくする(半径がこの範囲で 0 から 1)
static const float32_t kRunDropCenterFadeEnd = 0.32f;
static const float32_t kRunDropTrailRoot = 0.85f;      // 跡の幅(半径に対する割合。水滴に近い側 / 始まりの側)
static const float32_t kRunDropTrailTip = 0.35f;
static const float32_t kRunDropTrailFadeMin = 0.35f;   // 跡の始まりの側の濃さ(水滴の側が 1)
static const float32_t kRunDropMinTravel = 0.004f;     // これ以上滑ってから、跡を出す
static const float32_t kRunDropClusterLow = 0.15f;
static const float32_t kRunDropClusterHigh = 1.8f;
static const float32_t kRunDropEdgeAA = 1.5f;
static const int32_t kRunDropSeed = 401;
static const int32_t kRunDropSeedX = 37;
static const int32_t kRunDropSeedStart = 11;
static const int32_t kRunDropSeedEnd = 59;
static const int32_t kRunDropSeedSize = 23;
static const int32_t kRunDropSeedDelay = 71;
static const int32_t kRunDropSeedDuration = 97;
static const int32_t kRunDropSeedStretch = 113;
static const int32_t kRunDropSeedStay = 131;

// 流れる水滴(drop)と、その跡(trail)。wetness: 濡れ具合  lensTime: 水滴が付き始めてからの秒
void RunningDropletLayer(float32_t2 uv, float32_t aspect, float32_t wetness, float32_t lensTime, out DropletHit drop, out DropletHit trail) {
    drop.mask = 0.0f;
    drop.normal = float32_t2(0.0f, 0.0f);
    drop.radiusUv = 0.0f;
    trail = drop;

    float32_t2 centered = (uv - 0.5f) * float32_t2(aspect, 1.0f);
    float32_t r = length(centered);
    float32_t angle = atan2(centered.y, centered.x);
    float32_t aa = fwidth(r) * kRunDropEdgeAA; // 微分は、早期 return の前に求める(分岐の中では不定になる)
    float32_t sectorWidthAngle = kMistTwoPi / kRunDropSectors;
    int32_t sector = clamp(int32_t(floor((angle / kMistTwoPi + 0.5f) * kRunDropSectors)), 0, int32_t(kRunDropSectors) - 1);
    int32_t band = int32_t(floor(r / kRunDropBand));
    int2 id = int2(sector, band);

    // セルごとの乱数
    float32_t hOrder = Hash21(id + int2(kRunDropSeed, 0));
    float32_t hx = Hash21(id + int2(kRunDropSeedX, 17));
    float32_t hStart = Hash21(id + int2(kRunDropSeedStart, 29));
    float32_t hEnd = Hash21(id + int2(kRunDropSeedEnd, 43));
    float32_t hSize = Hash21(id + int2(kRunDropSeedSize, 61));
    float32_t hDelay = Hash21(id + int2(kRunDropSeedDelay, 83));
    float32_t hDuration = Hash21(id + int2(kRunDropSeedDuration, 101));
    float32_t hStretch = Hash21(id + int2(kRunDropSeedStretch, 127));
    float32_t hStay = Hash21(id + int2(kRunDropSeedStay, 139));

    // 密度のムラ(セルの中心の画面位置で、セルごとに 1 つ決める)
    float32_t cellAngle = (float32_t(sector) + 0.5f) * sectorWidthAngle - kMistPi;
    float32_t cellRadius = (float32_t(band) + 0.5f) * kRunDropBand;
    float32_t2 cellCenterUv = float32_t2(cos(cellAngle), sin(cellAngle)) * cellRadius / float32_t2(aspect, 1.0f) + 0.5f;
    float32_t density = lerp(kRunDropClusterLow, kRunDropClusterHigh, DropletDensity(cellCenterUv, aspect));
    float32_t order = hOrder / (kRunDropExistence * density);
    float32_t grow = smoothstep(order, order + kSmallDropGrowSpan, wetness) * step(order, 1.0f);
    if (grow <= 0.0f) {
        return;
    }

    // 滑る位置(半径方向)。付いてから遅れて滑り始め、ゆっくり加速して、ゆっくり止まる
    float32_t startR = (float32_t(band) + lerp(kRunDropStartMin, kRunDropStartMax, hStart)) * kRunDropBand;
    float32_t endR = (float32_t(band) + lerp(kRunDropEndMin, kRunDropEndMax, hEnd)) * kRunDropBand;
    float32_t slideTime = lensTime - (kRunDropDelayMin + hDelay * kRunDropDelaySpread);
    float32_t u = saturate(slideTime / (kRunDropDurationMin + hDuration * kRunDropDurationSpread)) * step(kRunDropStayChance, hStay);
    float32_t centerR = lerp(startR, endR, smoothstep(0.0f, 1.0f, u));

    // 角度方向の位置(区画の中で、少し偏らせる)
    float32_t centerAngle = cellAngle + (hx - 0.5f) * sectorWidthAngle * 0.5f;
    // 半径: 区画の幅と中心からの距離で抑える
    float32_t width = sectorWidthAngle * centerR;
    float32_t fullRadius = lerp(kRunDropRadiusMin, kRunDropRadiusMax, pow(hSize, kRunDropRadiusBias));
    fullRadius = min(fullRadius, width * kRunDropWidthFill) * smoothstep(kRunDropCenterFadeStart, kRunDropCenterFadeEnd, centerR);
    float32_t radius = fullRadius * grow;
    if (radius <= 0.0f) {
        return;
    }

    // 滑る道筋は、まっすぐでなく、半径方向の位置に応じて横へ少しうねる(水滴は、道筋の上の今の位置にいる)
    float32_t meanderPhase = hx * kMistTwoPi;
    float32_t meanderAmount = fullRadius * kRunDropMeanderAmount;
    float32_t arcAtCenter = sin(centerR * kRunDropMeanderFreq + meanderPhase) * meanderAmount;
    float32_t arcAtPixel = sin(r * kRunDropMeanderFreq + meanderPhase) * meanderAmount;
    float32_t arc = (angle - centerAngle) * centerR - arcAtCenter; // 水滴の中心から、横方向(接線)へのずれ(画面の高さを 1 とする)
    float32_t arcTrail = (angle - centerAngle) * r - arcAtPixel;    // 跡の中心線からのずれ
    float32_t radial = r - centerR;                                 // 流れる向き(外向き)へのずれ

    // 水滴: 滑っている間は、流れる向きに長く伸びる
    float32_t stretch = kRunDropStretchBase + lerp(kRunDropStretchMovingMin, kRunDropStretchMovingMax, hStretch) * sin(kMistPi * u);
    float32_t2 q = float32_t2(arc, radial / stretch);
    drop.mask = 1.0f - smoothstep(radius - aa, radius + aa, length(q));
    drop.normal = q / radius;
    drop.radiusUv = radius;

    // 跡: 滑り始めの位置から水滴の位置まで。水滴に近いほど太く、濃い。始まりの側は細く、薄い
    float32_t travel = centerR - startR;
    if (travel > kRunDropMinTravel) {
        float32_t along = saturate((r - startR) / travel); // 0 = 始まりの側、1 = 水滴の側
        float32_t trailWidth = radius * lerp(kRunDropTrailTip, kRunDropTrailRoot, along);
        float32_t inRange = step(startR, r) * step(r, centerR);
        float32_t across = 1.0f - smoothstep(trailWidth - aa, trailWidth + aa, abs(arcTrail));
        // 跡は、水滴より遅く付き、先に乾く
        float32_t trailThreshold = lerp(kRunDropTrailDryOffset, 1.0f, order);
        float32_t trailGrow = smoothstep(trailThreshold, trailThreshold + kSmallDropGrowSpan, wetness);
        trail.mask = across * inRange * lerp(kRunDropTrailFadeMin, 1.0f, along) * trailGrow;
        trail.normal = float32_t2(arcTrail / max(trailWidth, 0.0001f), 0.0f);
        trail.radiusUv = trailWidth;
    }
}

#endif
