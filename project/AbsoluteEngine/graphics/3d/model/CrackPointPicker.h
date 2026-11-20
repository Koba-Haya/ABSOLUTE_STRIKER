#pragma once
#include <wrl.h>
#include <d3d12.h>

#include <cstdint>
#include <vector>

class DirectXCommon;

// ボロノイのひびの線の上にある頂点と、そのセルが光って消える時刻を、GPU で選んで、CPU が読み戻すクラス(撃破演出の、光線の出る点と、消える時刻を決めるために使う)。
//
// 分けた理由: 「ひびの線の上か」は、描画(Object3d.PS)と同じ CrackVoronoi.hlsli で、GPU が判定する。
// CPU へ同じ計算を写すと、片方だけ直したときに、ひびの光る場所と光線の出る場所がずれるため、GPU の結果だけを受け取る。
// 責務: 選ぶ計算の依頼(Request)と、結果の受け取り(TryCollect)だけ。選ばれた点から、どう光線を作るかは、使う側が決める。
//
// 使い方:
//   Request(...)      選ぶ計算を、今フレームのコマンドリストへ積む(演出の開始時に 1 回)
//   TryCollect(...)   次のフレーム以降に呼ぶ。結果が読めたら true(選ばれた頂点の番号と、セルが光って消える時刻を返す)
// 同期: DirectXCommon::EndFrame が、フレームの終わりで GPU の完了を待つ。そのため、Request したフレームの次のフレームから、結果を読める。
class CrackPointPicker {
public:
  template <class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

  // 1 回の依頼で受け取れる、点の最大数(超えた分は捨てる)
  static constexpr uint32_t kMaxPoints = 512;
  // 出力バッファの、1 点あたりの uint の数(頂点の番号、光り始める時刻、消える時刻)。HLSL の kUintsPerPoint と同じ値にすること
  static constexpr uint32_t kUintsPerPoint = 3;

  // 選ばれた点 1 つ分
  struct PickedPoint {
    uint32_t vertexIndex = 0; // 頂点の番号(モデルの頂点バッファの中の番号)
    float heatStart = 0.0f;   // その頂点のセルが、光り始める時刻(crackBreak の値。0 〜 1)
    float vanishTime = 0.0f;  // そのセルが、消える時刻(crackBreak の値。0 〜 1)
  };

  // 選ぶ依頼の内容
  struct PickDesc {
    uint32_t vertexSrvIndex = 0; // モデルの頂点の SRV の番号(ModelResource::GetVertexSRVIndex)
    uint32_t vertexCount = 0;    // 頂点の数
    float cellSize = 1.0f;       // ボロノイのセルの大きさ(外殻の描画に設定したひび割れと同じ値)
    float maxEdge = 0.1f;        // ひびの線とみなす、境目からの距離の上限(描画のひびの線の太さと同じ値)
    float chance = 0.2f;         // ひびの線の上の頂点のうち、選ぶ割合(0 〜 1)
    float seed = 0.0f;           // 乱数の種(依頼ごとに変えると、選ばれる頂点が変わる)
    // セルが消える時刻の計算に使う、ひび割れの形(外殻の描画に設定したものと同じ値)
    float originX = 0.0f;        // ひびが広がり始める位置(モデル本来の座標)
    float originY = 0.0f;
    float originZ = 0.0f;
    float spreadRadius = 1.0f;   // origin から、いちばん遠い所までの距離
  };

  static CrackPointPicker *GetInstance();

  // パイプラインと資源を作る。起動時に 1 回呼ぶ(何度呼んでもよい)。シェーダーのコンパイルを含むので、演出の最中には呼ばない
  void Initialize(DirectXCommon *dx);

  // 選ぶ計算を、今フレームのコマンドリストへ積む。前の依頼の結果をまだ受け取っていないとき、未初期化のときは、何もせず false を返す
  bool Request(const PickDesc &desc);

  // 結果が読めるなら、選ばれた点を outPoints へ入れて true を返す(依頼 1 回につき、1 度だけ true)。
  // まだ読めない(Request と同じフレーム、または依頼が無い)ときは false
  bool TryCollect(std::vector<PickedPoint> &outPoints);

  // 依頼を出して、結果をまだ受け取っていないか
  bool IsPending() const { return pending_; }

private:
  CrackPointPicker() = default;
  ~CrackPointPicker() = default;
  CrackPointPicker(const CrackPointPicker &) = delete;
  CrackPointPicker &operator=(const CrackPointPicker &) = delete;

  // 出力バッファの状態を変える(今の状態と同じなら何もしない)
  void Transition_(D3D12_RESOURCE_STATES next);

  DirectXCommon *dx_ = nullptr;
  bool initialized_ = false;

  ComPtr<ID3D12RootSignature> rootSignature_;
  ComPtr<ID3D12PipelineState> pipelineState_;

  // 出力(GPU が書く。[0] = 数、[1 以降] = 点ごとに kUintsPerPoint 個)。状態は outputState_ で自分で管理する
  ComPtr<ID3D12Resource> outputBuffer_;
  D3D12_RESOURCE_STATES outputState_ = D3D12_RESOURCE_STATE_COMMON;
  uint32_t outputUavIndex_ = 0;
  // 出力の先頭の数を 0 に戻すための、ゼロだけが入ったバッファ
  ComPtr<ID3D12Resource> zeroBuffer_;
  // CPU が読むためのコピー先
  ComPtr<ID3D12Resource> readbackBuffer_;
  // 依頼の定数(256 バイト)
  ComPtr<ID3D12Resource> paramBuffer_;
  void *paramMapped_ = nullptr;

  bool pending_ = false;
  uint64_t requestedFrame_ = 0; // Request したときの DirectXCommon::GetFrameCounter()
};
