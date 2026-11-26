#pragma once
#include "ShaderQuad.h"
#include "Vector.h"
#include <d3d12.h>

// 火の玉(球の陰影つきのエネルギーの玉)を描く描画物。
//
// 仕組み: 板 1 枚(ShaderQuad)を置き、玉の形は、ピクセルシェーダー(EnergyOrb.PS.hlsl)が計算する。
// 板の上の点を「球の表面の点」とみなして、法線から、縁の暗さ、ハイライト、表面のノイズの座標を決めるので、平らな円盤ではなく球に見える。
// 球の外には、流れる炎と、にじむ光、離れていく火花が出る。球は自転せず、表面の模様が、流れの向きへ流れて沸く。
// 色は、アルファ合成で描く。暗い輪郭と黒紫の本体があるので、明るい空の上でも、形が見える。
// 雲の合成のあと(描画先が色だけ)に描く用途のみ(Renderer::DrawEnergyOrb が、それ以外では描かない)。
//
// 使い方: Renderer::InitializeEnergyOrb(1 回) -> 毎フレーム SetParams と SetBillboardTransform -> Renderer::DrawEnergyOrb
// 板の大きさ: ワールドで、板の半分の長さ(worldRadius)を決める。玉の半径は、板の半分の大きさを 1 とした割合で指定する。
class EnergyOrb {
public:
    struct Params {
        Vector4 bodyColor = { 0.06f, 0.00f, 0.16f, 1.0f };    // 本体の色(黒紫)
        Vector4 glowColor = { 0.50f, 0.18f, 1.00f, 1.0f };    // 熱い所、炎、にじむ光の色(明るい紫)
        Vector4 coreColor = { 0.96f, 0.90f, 1.00f, 1.0f };    // 最も熱い所、ハイライト、火花の色(白に近い薄紫)
        Vector4 outlineColor = { 0.03f, 0.00f, 0.08f, 1.0f }; // 外側の暗い輪郭の色(明るい空の上でも形が見えるようにする)
        float time = 0.0f;          // 時間(秒)。模様の流れ、火花、震えに使う
        float seed = 0.0f;          // 玉ごとのずれ(同時に出た玉が、同じ模様にならないようにする)
        float appear = 1.0f;        // 現れ具合(0 = 見えない、1 = 完全に現れた)。大きさと濃さが変わる
        float charge = 0.0f;        // 溜まり具合(0 〜 1)。球が熱くなり、炎が伸び、震える
        float orbRadius = 0.30f;    // 球の半径(板の半分の大きさを 1 とした割合)
        float haloStrength = 1.0f;  // 外へにじむ光の強さ
        float intensity = 1.0f;     // 光の明るさの倍率
        float alpha = 1.0f;         // 全体の濃さ(0 〜 1)
        Vector2 flow = { 0.0f, 1.0f }; // 炎と模様が流れる向き(板の上。右が +x、上が +y)。炎は、この向きへ流れる。長さは見ない
        float stretch = 0.0f;       // 流れの向きの軸に沿って、球を引き伸ばす量(0 = 真円、1 = 軸の方向に 2 倍の長さ)。飛んでいる玉の、彗星のような見た目に使う
    };

    EnergyOrb() = default;
    ~EnergyOrb() = default;

    // 板と定数バッファを作る。デバイスから 1 回だけ呼ぶ。失敗したら false
    bool Initialize(ID3D12Device* device);

    // 玉の形と色を決める(定数バッファへ書く)
    void SetParams(const Params& params);

    // 板を、カメラのほうへ向けて置く(ビルボード)。position: 玉の中心(ワールド)。worldRadius: 板の半分の長さ(ワールド)
    void SetBillboardTransform(const Vector3& position, float worldRadius, const Matrix4x4& view, const Matrix4x4& proj) {
        quad_.SetBillboardTransform(position, worldRadius, view, proj);
    }

    bool IsReady() const { return quad_.IsReady(); }
    void Draw(ID3D12GraphicsCommandList* cmdList) { quad_.Draw(cmdList); }
    D3D12_GPU_VIRTUAL_ADDRESS GetParamsCBAddress() const { return quad_.GetParamsCBAddress(); }
    D3D12_GPU_VIRTUAL_ADDRESS GetTransformCBAddress() const { return quad_.GetTransformCBAddress(); }

private:
    // ピクセルシェーダーの EnergyOrbParams(EnergyOrb.PS.hlsl)と並びを合わせること。16 バイトごとに区切って並べる
    struct ParamsCB {
        Vector4 bodyColor;
        Vector4 glowColor;
        Vector4 coreColor;
        Vector4 outlineColor;
        float time;
        float seed;
        float appear;
        float charge;
        float orbRadius;
        float haloStrength;
        float intensity;
        float alpha;
        float flowX;
        float flowY;
        float stretch;
        float padding;
    };

    ShaderQuad quad_;
};
