// ボロノイのひび割れの計算(撃破演出)。
// 描画(Object3d.PS)と、割れた場所から粒を出す計算(SurfaceEmitParticle.CS)の両方が、このファイルを読む。
// 分けた理由: セルの形と、セルが消える時刻の計算を、1 か所にだけ持つため(2 か所に写すと、片方だけ直したときに、
// セルが消える場所と、粒が出る場所がずれる)。
// 前提: 位置は、モデル本来の座標(スキニング前)を、1 / セルの大きさ 倍したものを渡す。

// 0 での除算を避けるための、小さい値
static const float CRACK_MIN_VALUE = 1.0e-5f;
// セルが消える順番: 0 = 完全にばらばら、1 = コア(origin)に近いセルから順に。大きいほど、中心から外へ向かって砕けて見える
static const float CRACK_BREAK_DISTANCE_BIAS = 0.6f;
// セルが、消える前に光る時間(crackBreak の幅)。この間に、セルの面全体が光ってから消える
static const float CRACK_BREAK_HEAT_WINDOW = 0.18f;

// 3 次元の座標から、ばらつきのある 0 〜 1 の値 3 つを作る(セルの代表点の位置を決める。同じ座標なら、いつも同じ値)
static float3 CrackHash33(float3 p)
{
    p = float3(dot(p, float3(127.1f, 311.7f, 74.7f)),
               dot(p, float3(269.5f, 183.3f, 246.1f)),
               dot(p, float3(113.5f, 271.9f, 124.6f)));
    return frac(sin(p) * 43758.5453f);
}

// 3 次元のボロノイ。p の位置から、いちばん近い代表点までの距離 f1 と、2 番目に近い代表点までの距離 f2 を求める。
// f2 - f1 は、セルの境目で 0 になり、境目から離れるほど大きくなる(これを、ひびの線として使う)。
// 隣の 27 セルだけを調べる(代表点は、自分のセルの中にしかないので、それより遠いセルは、f2 に届かない)。
// nearestFeature: いちばん近い代表点の位置(p と同じ空間)。nearestId: そのセルの整数の番号。
// どちらも、同じセルの中のどの位置でも同じ値になるので、「セルごとの値」を作るのに使う
static float CrackEdgeDistance(float3 p, out float3 nearestFeature, out float3 nearestId)
{
    float3 cell = floor(p);
    float3 local = frac(p);
    float f1 = 8.0f;
    float f2 = 8.0f;
    nearestFeature = float3(0.0f, 0.0f, 0.0f);
    nearestId = float3(0.0f, 0.0f, 0.0f);

    [unroll]
    for (int z = -1; z <= 1; ++z)
    {
        [unroll]
        for (int y = -1; y <= 1; ++y)
        {
            [unroll]
            for (int x = -1; x <= 1; ++x)
            {
                float3 offset = float3(x, y, z);
                float3 feature = offset + CrackHash33(cell + offset);
                float d = length(feature - local);
                if (d < f1)
                {
                    f2 = f1;
                    f1 = d;
                    nearestFeature = cell + feature;
                    nearestId = cell + offset;
                }
                else if (d < f2)
                {
                    f2 = d;
                }
            }
        }
    }
    return f2 - f1;
}

// セルが光り始める時刻(crackBreak の値)。消える時刻は、これに CRACK_BREAK_HEAT_WINDOW を足した値。
// 最大でも 1 - WINDOW なので、crackBreak = 1 で、必ず全部のセルが消える。
// cellScale: 1 / セルの大きさ。origin: コアの位置(モデル本来の座標)。spreadRadius: origin から、いちばん遠い所までの距離。
// 乱数と、origin からの近さを混ぜて決める(中心に近いセルほど先に消える。混ぜ方は BIAS)
static float CrackCellHeatStart(float3 nearestFeature, float3 nearestId, float cellScale, float3 origin, float spreadRadius)
{
    float cellRand = frac(sin(dot(nearestId, float3(12.9898f, 78.233f, 37.719f))) * 43758.5453f);
    float3 featureLocal = nearestFeature / max(cellScale, CRACK_MIN_VALUE);
    float cellDist01 = saturate(length(featureLocal - origin) / max(spreadRadius, CRACK_MIN_VALUE));
    return lerp(cellRand, cellDist01, CRACK_BREAK_DISTANCE_BIAS) * (1.0f - CRACK_BREAK_HEAT_WINDOW);
}
