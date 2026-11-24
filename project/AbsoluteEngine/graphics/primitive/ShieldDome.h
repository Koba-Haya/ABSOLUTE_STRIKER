#pragma once
#include "Matrix.h"
#include "Vector.h"
#include <cstdint>
#include <d3d12.h>
#include <wrl.h>

// 三角形の格子でできた、半透明のシールドのドーム(球)を描くための描画物。
//
// 仕組み: 正二十面体を細かく割った球(面は約 320 枚)を、面ごとに頂点を分けて持つ。各頂点に、三角形の中の位置(重心座標)と、
//   面ごとの乱数(0 〜 1)を持たせる。ピクセルシェーダー(ShieldDome.PS)が、重心座標から格子の線を描き、面の乱数で、面ごとの明滅、
//   傷み(耐久が減るほど、乱数の大きい面から傷む)、現れ(乱数の小さい面から現れる)を決める。
//   砕けるときは、頂点シェーダー(ShieldDome.VS)が、乱数の小さい面から順に、面の中心へ縮めながら、外へ飛ばす(破片になって飛び散る)。
//   被弾した向きから、波紋が球の表面を広がり、通った面と線が光る。
// 分けた理由: 描き方は「球のメッシュ + 専用シェーダー + パラメータ」という、1 つのまとまった責務。シールドの動き(いつ、何を見せるか)は、
//   アプリ側(BossShieldEffects)が決めて、数値(Params)だけをここへ渡す。
// 使い方: Initialize(1 回) -> 毎フレーム SetParams と SetTransform -> Renderer::DrawShieldDome
class ShieldDome {
public:
    static constexpr uint32_t kMaxHits = 8; // 同時に見せられる被弾の波紋の数(HLSL の ShieldParams::hits と同じ値にすること)

    // 被弾の波紋 1 つ
    struct Hit {
        Vector3 direction{ 0.0f, 0.0f, 1.0f }; // ドームの中心から、被弾した点への向き(長さ 1。ワールド)
        float age = 0.0f;                       // 被弾してからの経過秒
    };

    struct Params {
        Vector4 baseColor = { 0.45f, 0.15f, 0.95f, 1.0f };   // 面の色(紫)
        Vector4 lineColor = { 0.70f, 0.35f, 1.00f, 1.0f };   // 格子の線の色(明るい紫。明るい空の上でも、見える濃さ)
        Vector4 damageColor = { 1.00f, 0.15f, 0.30f, 1.0f }; // 傷んだ面の色(赤)
        Vector4 rippleColor = { 0.55f, 0.85f, 1.00f, 1.0f }; // 被弾の波紋の色(自機の弾の色の、青白い光)
        Vector3 cameraPosition{ 0.0f, 0.0f, 0.0f };          // カメラのワールド位置(縁の光の計算に使う)
        float time = 0.0f;                                   // 明滅の時間(秒)
        float hpRatio = 1.0f;       // 耐久の残り(1 = 満タン、0 = 壊れる寸前)。減るほど、傷んだ面が増える
        float breakProgress = 0.0f; // 砕ける進み具合(0 〜 1 + shardSpan)。進むほど、乱数の小さい面から、破片になって飛ぶ。1 + shardSpan で、すべての面が飛び終える
        float shardSpan = 0.35f;     // 1 枚の破片が飛んで消えるまでの長さ(breakProgress に対する割合)
        float shardDistance = 0.9f;  // 破片が飛ぶ距離(単位球の半径に対する割合)
        float shardShrink = 0.7f;    // 破片が、面の中心へ縮む割合(1 で、点になる)
        float appear = 1.0f;        // 現れる進み具合(0 〜 1)。進むほど、面が現れる
        float warning = 0.0f;       // 警告の強さ(0 〜 1)。赤く脈打つ
        float alpha = 1.0f;         // 全体の濃さ
        float lineWidth = 2.0f;     // 格子の線の太さ(ピクセル)
        float fresnelPower = 2.5f;  // 縁の光の鋭さ(大きいほど、縁だけが光る)
        float rippleSpeed = 2.4f;   // 波紋の広がる速さ(ラジアン / 秒。球の表面の角度)
        float rippleWidth = 0.22f;  // 波紋の幅(ラジアン)
        float rippleLife = 1.0f;    // 波紋の寿命(秒)
        uint32_t hitCount = 0;
        Hit hits[kMaxHits];
    };

    ShieldDome() = default;
    ~ShieldDome() = default;

    // 球のメッシュと定数バッファを作る。デバイスから 1 回だけ呼ぶ。失敗したら false
    bool Initialize(ID3D12Device* device);
    bool IsReady() const { return vertexBuffer_ && transformCB_ && paramsCB_; }

    // 形と色と状態を決める(定数バッファへ書く)
    void SetParams(const Params& params);
    // 球の位置と半径を決める。center: 球の中心(ワールド)。radius: 半径(ワールド)。回転はしない(被弾の向きを、ワールドの向きのまま使うため)。
    // 砕ける値(breakProgress など)は、頂点シェーダーが使うので、変換の定数バッファへ書く。SetParams のあとに呼ぶこと
    void SetTransform(const Vector3& center, float radius, const Matrix4x4& view, const Matrix4x4& proj);

    void Draw(ID3D12GraphicsCommandList* cmdList);

    D3D12_GPU_VIRTUAL_ADDRESS GetParamsCBAddress() const { return paramsCB_->GetGPUVirtualAddress(); }
    D3D12_GPU_VIRTUAL_ADDRESS GetTransformCBAddress() const { return transformCB_->GetGPUVirtualAddress(); }

private:
    struct Vertex {
        Vector3 position;   // 球の上の点(単位球)
        Vector3 barycentric; // 三角形の中の位置(頂点ごとに (1,0,0) (0,1,0) (0,0,1))
        float faceRandom;   // 面ごとの乱数(0 〜 1。同じ面の 3 頂点は同じ値)
        Vector3 faceCenter; // 面の中心(3 頂点の平均。砕けるときに、面を縮める中心と、飛ぶ向きに使う)
    };

    struct TransformCB {
        Matrix4x4 WVP;
        Matrix4x4 World;
        Vector4 shatter; // x = breakProgress、y = shardSpan、z = shardDistance、w = shardShrink(頂点シェーダーが読む)
    };

    // ピクセルシェーダーの ShieldParams(ShieldDome.hlsli)と並びを合わせること。16 バイトごとに区切って並べる
    struct ParamsCB {
        Vector4 baseColor;
        Vector4 lineColor;
        Vector4 damageColor;
        Vector4 rippleColor;
        Vector3 cameraPosition;
        float time;
        float hpRatio;
        float breakProgress;
        float appear;
        float warning;
        float alpha;
        float lineWidth;
        float fresnelPower;
        float hitCount;
        float rippleSpeed;
        float rippleWidth;
        float rippleLife;
        float pad;
        Vector4 hits[kMaxHits]; // xyz = 向き、w = 経過秒
    };

    Microsoft::WRL::ComPtr<ID3D12Resource> CreateUploadBuffer_(ID3D12Device* device, UINT64 size);

    // SetParams で受け取った、砕ける値(SetTransform が、変換の定数バッファへ書く)
    float breakProgress_ = 0.0f;
    float shardSpan_ = 0.35f;
    float shardDistance_ = 0.9f;
    float shardShrink_ = 0.7f;

    Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer_;
    D3D12_VERTEX_BUFFER_VIEW vbView_{};
    UINT vertexCount_ = 0;
    Microsoft::WRL::ComPtr<ID3D12Resource> transformCB_;
    Microsoft::WRL::ComPtr<ID3D12Resource> paramsCB_;
};
