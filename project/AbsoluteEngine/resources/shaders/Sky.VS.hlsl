#include "Fullscreen.hlsli"

// Fullscreen.VS と同じ「頂点バッファなしの全画面三角形」。
// 違いは z = 1.0(最奥)で出力する点。深度 LESS_EQUAL と組み合わせると、
// 描画順に関係なく空は常に他のオブジェクトの背後になる。
static const uint32_t kNumVertex = 3;
static const float32_t4 kPositions[kNumVertex] = {
    {-1.0f, 1.0f, 1.0f, 1.0f},  // 左上
    {3.0f, 1.0f, 1.0f, 1.0f},   // 右上
    {-1.0f, -3.0f, 1.0f, 1.0f}, // 左下
};
static const float32_t2 kTexcoords[kNumVertex] = {
    {0.0f, 0.0f},
    {2.0f, 0.0f},
    {0.0f, 2.0f},
};

VertexShaderOutput main(uint32_t vertexId : SV_VertexID) {
    VertexShaderOutput output;
    output.position = kPositions[vertexId];
    output.texcoord = kTexcoords[vertexId];
    return output;
}
