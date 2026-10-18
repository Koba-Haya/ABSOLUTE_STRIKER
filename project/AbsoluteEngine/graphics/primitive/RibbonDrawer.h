#pragma once
#include "RibbonTrail.h"
#include <cstdint>
#include <d3d12.h>
#include <vector>
#include <wrl.h>

/// <summary>
/// リボンの頂点を GPU へ渡して描くクラス。
/// 全てのリボンで1つの動的頂点バッファ(Upload ヒープ)を共有し、描くたびに末尾へ追記する。
/// 弾ごとにバッファを作ると、弾の発射のたびに GPU リソースの生成が走って重くなるため、共有にしている。
/// パイプライン(シェーダ)や定数バッファの設定は呼び出し側(Renderer)が済ませておく。
/// </summary>
class RibbonDrawer {
public:
    RibbonDrawer() = default;
    ~RibbonDrawer();

    /// <summary>
    /// 頂点バッファを作る。
    /// </summary>
    /// <param name="device">D3D12 デバイス</param>
    /// <param name="maxVertices">1フレームに描ける頂点の総数</param>
    void Initialize(ID3D12Device* device, uint32_t maxVertices);

    /// <summary>
    /// 書き込み位置を先頭に戻す。毎フレームの最初に呼ぶ。
    /// </summary>
    void Reset();

    /// <summary>
    /// 軌跡から帯の頂点を作り、バッファへ追記して描画する。
    /// 点が少なくて帯が作れないとき、バッファの空きが足りないときは何もしない。
    /// </summary>
    /// <param name="cmdList">コマンドリスト</param>
    /// <param name="trail">描く軌跡</param>
    /// <param name="cameraPosition">カメラのワールド座標</param>
    void Draw(ID3D12GraphicsCommandList* cmdList, const RibbonTrail& trail, const Vector3& cameraPosition);

private:
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer_;
    RibbonVertex* mappedVertices_ = nullptr; // バッファは作成時から Map したままにする(PrimitiveDrawer と同じ方式)
    uint32_t maxVertices_ = 0;
    uint32_t usedVertices_ = 0; // 今フレームで書き込み済みの頂点数

    std::vector<RibbonVertex> scratch_; // 頂点生成の作業用。毎回メモリを確保し直さないよう使い回す
};
