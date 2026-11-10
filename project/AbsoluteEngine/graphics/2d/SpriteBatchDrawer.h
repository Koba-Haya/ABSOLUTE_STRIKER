#pragma once
#include "Matrix.h"
#include "SpriteResource.h" // SpriteVertex
#include "Vector.h"
#include <cstdint>
#include <d3d12.h>
#include <wrl.h>

class Renderer;

/// <summary>
/// 「たくさんの四角形(クワッド)を 1 回の描画命令でまとめて描く」ための、頂点とバッファの置き場。
/// 文字(BitmapFont)のように、1 画面に数百の小さな四角形を出す用途のために分けてある。
/// Sprite は 1 枚ごとに定数バッファを 2 つ作るので、文字ごとに Sprite を作ると生成が重く、
/// 描画命令も文字数ぶん増える。ここでは次のように共有して、その負荷を避ける。
///   - 頂点バッファは 1 本を全員で共有し、1 フレームの中で末尾へ追記していく(RibbonDrawer と同じ方式)
///   - インデックスバッファは固定(0,1,2,2,1,3 の繰り返し)を 1 本だけ作る
///   - 定数バッファ(色と変換行列)は、描画 1 回ごとに 1 組ずつ、共有バッファの別の場所を使う
/// 書き込み位置は、フレーム番号が変わった最初の Submit で先頭へ戻る(シーンが何もしなくてよい)。
/// このクラスは「データを置いて、描画命令に必要なアドレスを返す」だけ。パイプラインの設定や
/// 描画命令の発行は Renderer が行う。
/// </summary>
class SpriteBatchDrawer {
public:
  /// 1 回の描画命令に必要な情報(Submit が返す)
  struct Submission {
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
    D3D12_INDEX_BUFFER_VIEW indexBufferView{};
    D3D12_GPU_VIRTUAL_ADDRESS materialCB = 0;  // 色と UV 変換(ピクセルシェーダ b0)
    D3D12_GPU_VIRTUAL_ADDRESS transformCB = 0; // 変換行列(頂点シェーダ b0)
    uint32_t indexCount = 0;
    int32_t baseVertex = 0; // DrawIndexedInstanced の BaseVertexLocation に渡す
  };

  /// <summary>
  /// バッファを作る。Renderer の初期化中に 1 回だけ呼ぶ。
  /// </summary>
  /// <param name="renderer">アップロードバッファの作成を依頼する Renderer</param>
  void Initialize(Renderer *renderer);

  /// <summary>
  /// クワッドの頂点を共有バッファへ追記し、描画に必要なアドレスを返す。
  /// 頂点は 1 クワッドあたり 4 つで、並びは Sprite の単位四角形と同じ(左下、左上、右下、右上)。
  /// </summary>
  /// <param name="frameCount">今のフレーム番号。前回と違えば書き込み位置を先頭へ戻す</param>
  /// <param name="vertices">頂点の配列(quadCount * 4 個。位置は画面ピクセル座標)</param>
  /// <param name="quadCount">クワッドの数</param>
  /// <param name="color">全クワッド共通の乗算色</param>
  /// <param name="wvp">変換行列(画面ピクセル座標から画面へ変える正射影)</param>
  /// <param name="out">描画に必要な情報の出力先</param>
  /// <returns>追記できたら true。1 フレームの上限を超えるとき、quadCount が 0 のときは false</returns>
  bool Submit(uint64_t frameCount, const SpriteVertex *vertices, uint32_t quadCount,
              const Vector4 &color, const Matrix4x4 &wvp, Submission *out);

  /// 1 フレームに描けるクワッドの総数(文字なら、画面に出せる文字数の上限)
  static constexpr uint32_t kMaxQuads = 8192;
  /// 1 フレームの描画命令の上限(文字列を 1 回描くごとに 1 つ使う)
  static constexpr uint32_t kMaxDraws = 512;

private:
  Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer_;
  Microsoft::WRL::ComPtr<ID3D12Resource> indexBuffer_;
  Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;
  SpriteVertex *mappedVertices_ = nullptr;
  uint8_t *mappedConstants_ = nullptr;

  uint64_t lastFrameCount_ = UINT64_MAX; // 最後に Submit されたフレーム番号
  uint32_t usedQuads_ = 0;               // 今フレームで使用済みのクワッド数
  uint32_t usedDraws_ = 0;               // 今フレームで使用済みの描画命令数
};
