// シールドのドームのピクセルシェーダー(シーンの深度を、テクスチャ(t0)として読んで、自分で比べる版。
// 雲の合成のあと、描画先が色だけで、深度バッファが付いていないときに使う)。中身は ShieldDome.hlsli
#define SHIELD_USE_SRV_DEPTH
#include "ShieldDome.hlsli"
