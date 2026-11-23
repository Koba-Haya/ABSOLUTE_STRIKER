#pragma once
#include <cstdint>

// CPU 側の処理時間を測るクラス(ImGui には依存しない。結果は Logger でログへ出す)。
// GpuTimer が「GPU が描画をこなした時間」を測るのに対し、こちらは「CPU(メインスレッド)が
// 読み込みなどで止まっていた時間」を測る。シーン遷移の停止の原因調査のために追加した。
// 時計は QueryPerformanceCounter(高分解能タイマー)を使う。
//
// このクラスは状態を持たない(時計の読み取りと、区間の計測だけ)。
// 1 フレームごとの平均・最大の集計は、状態を持つので FrameTimer(別クラス)が担当する。
//
// 使い方: 区間を測る(スコープを抜けた時点で「[CpuTimer] ラベル = 12.34 ms」を即ログへ出す)
//   { CpuTimer::Scope scope("GameScene.LoadEditorScene"); LoadEditorScene(); }
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

  // 高分解能タイマーの現在値(目盛り)を返す
  static int64_t NowTicks();
  // 目盛りの差を、ミリ秒に換算する
  static double TicksToMs(int64_t ticks);
};
