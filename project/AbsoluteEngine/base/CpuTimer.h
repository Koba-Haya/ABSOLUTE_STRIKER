#pragma once
#include <cstdint>

// CPU 側の処理時間を測るクラス(ImGui には依存しない。結果は Logger でログへ出す)。
// GpuTimer が「GPU が描画をこなした時間」を測るのに対し、こちらは「CPU(メインスレッド)が
// 読み込みなどで止まっていた時間」を測る。シーン遷移の停止の原因調査のために追加した。
// 時計は QueryPerformanceCounter(高分解能タイマー)を使う。
//
// 使い方 1: 区間を測る(スコープを抜けた時点で「[CpuTimer] ラベル = 12.34 ms」を即ログへ出す)
//   { CpuTimer::Scope scope("GameScene.LoadEditorScene"); LoadEditorScene(); }
// 使い方 2: 1 フレームの CPU 時間を測る(メインループの先頭と末尾で呼ぶ)
//   CpuTimer::BeginFrame(); ... CpuTimer::EndFrame();
//   1 秒ごとに平均と最大を出し、しきい値を超えた 1 フレームはその場で出す(停止の瞬間を逃さないため)
// 注意: 1 フレームの時間には垂直同期(60fps に合わせる待ち)も含まれるので、通常は約 16.7ms になる。
class CpuTimer {
public:
  // 区間の開始と終了を、スコープの出入りで自動的に行う(終了の呼び忘れ防止)
  class Scope {
  public:
    // label: ログに出す区間名。文字列リテラルなど、スコープより長く生きる文字列を渡すこと
    explicit Scope(const char *label);
    ~Scope();
    Scope(const Scope &) = delete;
    Scope &operator=(const Scope &) = delete;

  private:
    const char *label_;
    int64_t startTicks_;
  };

  // 何度も呼ばれる処理(オブジェクトごとのモデル読み込みなど)の合計時間を測る区間。
  // 1 回ごとにログへ出すと量が多すぎるので、ラベルごとに回数と合計をためておき、
  // FlushAccumulated() でまとめてログに出す
  class AccumScope {
  public:
    // label: 集計のキー。文字列リテラルなど、プログラム終了まで有効な文字列を渡すこと
    explicit AccumScope(const char *label);
    ~AccumScope();
    AccumScope(const AccumScope &) = delete;
    AccumScope &operator=(const AccumScope &) = delete;

  private:
    const char *label_;
    int64_t startTicks_;
  };

  // AccumScope でためた「回数と合計時間」をラベルごとにログへ出し、集計を空にする
  static void FlushAccumulated();

  // 1 フレームの測定の開始(メインループの先頭で呼ぶ)
  static void BeginFrame();
  // 1 フレームの測定の終了(メインループの末尾で呼ぶ)。集計とログ出力を行う
  static void EndFrame();
};
