#pragma once
#include "Vector.h"
#include <cstdint>
#include <d3d12.h>
#include <wrl.h>

// エネルギーのリボン(光の帯)の見た目。リボンの形(通る位置と幅)は RibbonTrail が決め、
// 色、流れる模様、明るさは、専用のピクセルシェーダー(EnergyRibbon.PS.hlsl)が、この値から計算して描く。
//
// 見た目: 中心に細い白い芯、その外側に明るい紫の光、さらに外側に暗い輪郭(明るい空の上でも、帯の形が見える)。
// 帯の中を、ノイズの模様が、尾から先端へ向かって流れる。先端が最も明るく、尾へ向かって薄れる。
// 使い方: Renderer::DrawEnergyRibbon(trail, style)。雲の合成のあと(描画先が色だけ)にだけ描く。
struct EnergyRibbonStyle {
    Vector4 glowColor = { 0.50f, 0.18f, 1.00f, 1.0f };    // 帯の光の色(明るい紫)
    Vector4 coreColor = { 0.96f, 0.90f, 1.00f, 1.0f };    // 芯の色(白に近い薄紫)
    Vector4 outlineColor = { 0.03f, 0.00f, 0.08f, 1.0f }; // 外側の暗い輪郭の色
    float time = 0.0f;       // 時間(秒)。模様の流れに使う
    float seed = 0.0f;       // 帯ごとのずれ(同時に出た帯が、同じ模様にならないようにする)
    float intensity = 1.0f;  // 光の明るさの倍率
    float flowSpeed = 1.5f;  // 模様の流れる速さ(帯の長さに対する割合 / 秒)
};

// リボンのパラメータを置く、定数バッファの置き場。
//
// 分けた理由: リボンは、1 フレームに何本も描く(帯ごとに、色や時間が違う)。定数バッファが 1 つだと、
// 描画の命令が実際に実行されるときには、最後に書いた値だけが残り、全部の帯が同じ値になってしまう。
// そこで、バッファを「枠」に区切り、描くたびに次の枠へ書いて、その枠の場所を命令に渡す(RibbonDrawer の頂点バッファと同じ考え方)。
// 毎フレームの最初に Reset で、先頭の枠へ戻す。
class EnergyRibbonParamsRing {
public:
    // 枠を maxSlots 個、確保する。失敗したら false
    bool Initialize(ID3D12Device* device, uint32_t maxSlots);

    // 書き込み位置を、先頭の枠へ戻す。毎フレームの最初に呼ぶ
    void Reset() { usedSlots_ = 0; }

    // style を次の枠へ書き、その枠のアドレスを返す。枠が足りない(または未初期化の)ときは 0 を返す
    D3D12_GPU_VIRTUAL_ADDRESS Write(const EnergyRibbonStyle& style);

private:
    // ピクセルシェーダーの EnergyRibbonParams(EnergyRibbon.PS.hlsl)と並びを合わせること。16 バイトごとに区切って並べる
    struct ParamsCB {
        Vector4 glowColor;
        Vector4 coreColor;
        Vector4 outlineColor;
        float time;
        float seed;
        float intensity;
        float flowSpeed;
    };

    Microsoft::WRL::ComPtr<ID3D12Resource> buffer_;
    uint8_t* mapped_ = nullptr;     // 作成時から Map したままにする(RibbonDrawer と同じ方式)
    uint32_t maxSlots_ = 0;
    uint32_t usedSlots_ = 0;
};
