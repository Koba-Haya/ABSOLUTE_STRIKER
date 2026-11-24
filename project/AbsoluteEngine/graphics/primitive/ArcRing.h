#pragma once
#include "Matrix.h"
#include "Vector.h"
#include <cstdint>
#include <d3d12.h>
#include <wrl.h>

// 切れ目のある輪(弧を円周に並べた輪)を描くための描画物。
//
// 仕組み: 板(正方形のポリゴン) 1 枚を置き、輪の形はピクセルシェーダー(ArcRing.PS.hlsl)が計算して描く。
// 分けた理由(Ring との違い): Ring は、輪の形をポリゴンで作る(切れ目は、角度の範囲で作る別のポリゴン)ので、
// 弧の端の細まり、縁のぼかし、外へにじむ光、切れ目の数や幅の連続的な変化を、描けない。
// ここでは、それらをパラメータの数値で滑らかに変えられる。攻撃の予告、範囲攻撃の衝撃波など、「切れ目のある輪」を使う演出が共通で使う。
//
// 使い方: Initialize(1 回) -> 毎フレーム SetParams と SetBillboardTransform(または SetTransform) -> Renderer::DrawArcRing
// 板の大きさ: ワールドで、板の半分の長さ(worldRadius)を決める。輪の半径は、板の半分の大きさを 1 とした割合で指定する。
class ArcRing {
public:
    struct Params {
        float innerRadius = 0.70f;   // 輪の内側の半径(板の半分の大きさを 1 とした割合)
        float outerRadius = 0.80f;   // 輪の外側の半径(同上。外側へにじむ光のために、1 より小さくしておく)
        uint32_t arcCount = 6;       // 弧の数(円周を等分した区間の数)
        float gapRatio = 0.35f;      // 1 つの区間のうち、切れ目の割合(0 = 切れ目なし = 完全な輪、1 に近いほど、細かい弧)
        float rotation = 0.0f;       // 輪の回転(ラジアン)
        float tipSoftness = 0.15f;   // 弧の両端の細まり(弧の長さに対する割合。0 で角ばる)
        float edgeSoftness = 0.25f;  // 輪の幅方向のぼかし(輪の幅に対する割合。0 〜 0.5)
        float haloStrength = 0.50f;  // 輪の外側と内側へにじむ光の強さ(0 でなし)
        float haloWidth = 0.08f;     // にじむ光の幅(板の半分の大きさを 1 とした割合)
        Vector4 edgeColor = { 1.0f, 0.15f, 0.10f, 1.0f }; // 輪の縁と、にじむ光の色
        Vector4 coreColor = { 1.0f, 0.80f, 0.70f, 1.0f }; // 輪の芯(幅の中央)の色
        float corePower = 2.0f;      // 芯の色の広がり(大きいほど、芯が細い)
        float intensity = 1.0f;      // 色に掛ける明るさ
        float alpha = 1.0f;          // 全体の濃さ(0 〜 1)
    };

    ArcRing() = default;
    ~ArcRing() = default;

    // 板と定数バッファを作る。デバイスから 1 回だけ呼ぶ。失敗したら false
    bool Initialize(ID3D12Device* device);

    // 輪の形と色を決める(定数バッファへ書く)
    void SetParams(const Params& params);

    // 変換行列を直接決める(world: 板のワールド行列。板は XY 平面、-1 〜 +1 の正方形)
    void SetTransform(const Matrix4x4& world, const Matrix4x4& view, const Matrix4x4& proj);

    // 板を、カメラのほうへ向けて置く(ビルボード)。position: 輪の中心(ワールド)。worldRadius: 板の半分の長さ(ワールド)
    void SetBillboardTransform(const Vector3& position, float worldRadius, const Matrix4x4& view, const Matrix4x4& proj);

    // 描画の前準備が済んでいるか(Initialize に成功したか)
    bool IsReady() const { return vertexBuffer_ && indexBuffer_ && transformCB_ && paramsCB_; }

    void Draw(ID3D12GraphicsCommandList* cmdList);

    D3D12_GPU_VIRTUAL_ADDRESS GetParamsCBAddress() const { return paramsCB_->GetGPUVirtualAddress(); }
    D3D12_GPU_VIRTUAL_ADDRESS GetTransformCBAddress() const { return transformCB_->GetGPUVirtualAddress(); }

private:
    struct Vertex {
        Vector4 position;
        Vector2 texcoord; // 板の中心を原点とした、-1 〜 +1 の座標(ピクセルシェーダーが、輪の形の計算に使う)
    };

    struct TransformCB {
        Matrix4x4 WVP;
        Matrix4x4 World;
    };

    // ピクセルシェーダーの ArcRingParams(ArcRing.PS.hlsl)と並びを合わせること。16 バイトごとに区切って並べる
    struct ParamsCB {
        Vector4 edgeColor;
        Vector4 coreColor;
        float innerRadius;
        float outerRadius;
        float arcCount;
        float gapRatio;
        float rotation;
        float tipSoftness;
        float edgeSoftness;
        float haloStrength;
        float haloWidth;
        float corePower;
        float intensity;
        float alpha;
    };

    Microsoft::WRL::ComPtr<ID3D12Resource> CreateUploadBuffer_(ID3D12Device* device, UINT64 size);

    Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer_;
    Microsoft::WRL::ComPtr<ID3D12Resource> indexBuffer_;
    D3D12_VERTEX_BUFFER_VIEW vbView_{};
    D3D12_INDEX_BUFFER_VIEW ibView_{};
    Microsoft::WRL::ComPtr<ID3D12Resource> transformCB_;
    Microsoft::WRL::ComPtr<ID3D12Resource> paramsCB_;
};
