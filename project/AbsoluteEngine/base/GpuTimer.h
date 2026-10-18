#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <cstdint>
#include <string>
#include <vector>

// GPU 側の処理時間を、名前をつけた区間ごとに測るクラス。
// 仕組み: コマンドリストの途中に「GPU がここへ来た時刻」を書き込む命令(タイムスタンプクエリ)を入れ、
//         フレームの最後にまとめて読み戻して、区間の前後の差を時間(ミリ秒)に換算する。
// CPU 側の時間ではなく、GPU が実際にその描画をこなした時間が分かる(雲が重いかどうかの判断材料)。
// 使い方:
//   GpuTimer::Scope scope(timer, cmdList, "CloudSea");  // この行から、スコープを抜けるまでを測る
// 1 フレームの流れ(DirectXCommon が呼ぶ): Resolve(フレーム末尾) -> GPU の完了待ち -> Collect(結果の集計とログ)
// 注意: Debug 構成は GPU ベースの検証が有効で遅くなるため、数値は Development / Release で見る。
class GpuTimer {
public:
  template <class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

  // 1 つの区間の集計結果(報告間隔ごとに更新される)
  struct SectionStats {
    std::string label;
    float averageMs = 0.0f; // 報告間隔内の平均(1 フレームあたり)
    float maxMs = 0.0f;     // 報告間隔内の最大
  };

  // 区間の開始と終了を、スコープの出入りで自動的に行う(End の呼び忘れ防止)
  class Scope {
  public:
    Scope(GpuTimer &timer, ID3D12GraphicsCommandList *cmdList, const char *label);
    ~Scope();
    Scope(const Scope &) = delete;
    Scope &operator=(const Scope &) = delete;

  private:
    GpuTimer &timer_;
    ID3D12GraphicsCommandList *cmdList_;
    int32_t slot_;
  };

  // 初期化。device: クエリヒープと読み戻しバッファの作成用、queue: GPU の時計の周波数を得る用。
  // 失敗しても止まらず、計測だけ無効になる(戻り値 false)
  bool Initialize(ID3D12Device *device, ID3D12CommandQueue *queue);

  // 区間の開始。戻り値は区間の番号(End に渡す)。枠が足りないときや無効のときは -1(End は何もしない)
  int32_t Begin(ID3D12GraphicsCommandList *cmdList, const char *label);
  void End(ID3D12GraphicsCommandList *cmdList, int32_t slot);

  // 記録したタイムスタンプを読み戻しバッファへ書き出す命令を積む。フレーム末尾(Close の直前)で呼ぶ
  void Resolve(ID3D12GraphicsCommandList *cmdList);

  // GPU の完了を待ったあとに呼ぶ。結果を集計し、報告間隔ごとにログへ出す。次のフレームの枠も空ける
  void Collect();

  // ログ出力の切り替え(集計は常に行う)
  void SetLogEnabled(bool enabled) { logEnabled_ = enabled; }

  // 直近の報告間隔の集計結果(将来、画面表示などで使う)と fps
  const std::vector<SectionStats> &GetStats() const { return reportedStats_; }
  float GetFps() const { return reportedFps_; }

  // 直近の 1 フレームの、区間 label の時間(ミリ秒)。同名の区間が複数回あれば合計。そのフレームに無ければ 0。
  // 品質の自動調整のように、報告間隔を待たずに実測値を使いたいとき用(Collect のたびに更新される)
  float GetLastFrameMs(const char *label) const;

private:
  // 1 フレームで記録した区間
  struct Slot {
    std::string label;
    bool ended = false;
  };

  // 区間名ごとの、報告間隔内の累積
  struct Accumulator {
    std::string label;
    double sumMs = 0.0;
    float maxMs = 0.0f;
  };

  void Report_(double elapsedSec);

private:
  ComPtr<ID3D12QueryHeap> queryHeap_;
  ComPtr<ID3D12Resource> readbackBuffer_;
  double ticksToMs_ = 0.0; // タイムスタンプの 1 目盛りが何ミリ秒か
  bool available_ = false;
  bool logEnabled_ = true;

  std::vector<Slot> slots_; // 今のフレームの区間(Begin の順)

  std::vector<Accumulator> accumulators_;
  uint32_t framesInInterval_ = 0;
  int64_t intervalStartTicks_ = 0; // QueryPerformanceCounter の値
  int64_t counterFrequency_ = 0;

  // 直近の 1 フレームの区間ごとの時間(同名は合計済み)
  struct FrameSection {
    std::string label;
    float ms = 0.0f;
  };
  std::vector<FrameSection> lastFrameSections_;

  std::vector<SectionStats> reportedStats_;
  float reportedFps_ = 0.0f;
};
