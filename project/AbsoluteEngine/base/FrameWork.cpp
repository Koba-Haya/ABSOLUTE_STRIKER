#include "Framework.h"
#include <Windows.h>
#include <mmsystem.h>
#include <objbase.h>
#include <string>
#include "CpuTimer.h"
#include "Logger.h"
#include <algorithm>
#include <cstdint>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

#pragma comment(lib, "winmm.lib") // timeBeginPeriod(待ち時間の細かさを上げる)

static void FatalBoxAndTerminate_(const char* msg) {
	MessageBoxA(nullptr, msg, "Fatal", MB_OK | MB_ICONERROR);
	std::terminate();
}

static void CheckBoolOrDie_(bool ok, const char* what) {
	if (!ok) {
		FatalBoxAndTerminate_(what);
	}
}

static void CheckHROrDie_(HRESULT hr, const char* what) {
	if (FAILED(hr)) {
		std::string s = std::string(what) + " (HRESULT failed)";
		FatalBoxAndTerminate_(s.c_str());
	}
}

namespace {
// --- 固定タイムステップの設定 ---
// 論理(Update)は、画面の更新の速さに関係なく、1 秒に 60 回、1 回 1/60 秒ずつ進める。
// 各シーンは、1 回の Update を 1/60 秒として動かす作りなので、この間隔を、変えないこと。
constexpr double kStepSeconds = 1.0 / 60.0;
// 論理を進める時刻の「ずれの許し」(秒)。60Hz の画面では、更新の間隔が 16.6ms と 16.7ms の間でぶれるので、
// ぴったり 1/60 秒たつのを待つと、0 回の更新と 2 回の更新が交互に出て、動きがカクつく。少し早めに進めて、ずれは次へ持ち越す
constexpr double kStepSnapSeconds = 0.002;
// 1 回の画面の更新で、論理を進める回数の上限。重い場面で、遅れを取り戻そうとして、さらに遅くなる悪循環を防ぐ
constexpr int kMaxStepsPerFrame = 4;
// 1 回の画面の更新までに、これより長い時間がたっていたら、「止まっていた」(読み込み、ブレークポイント、ウィンドウの移動など)とみなして、
// 遅れを取り戻さない(1 回分だけ進める)。取り戻すと、止まっていた間の動きが、一気に進んでしまう
constexpr double kHitchSeconds = 0.1;
// 論理を進める時刻まで待つとき、1 回に眠る時間(ミリ秒)
constexpr DWORD kIdleSleepMs = 1;
// 待ち時間の細かさ(ミリ秒)。既定(約 15ms)だと、Sleep(1) が長く待ち、60Hz をまたぐ時刻の精度が出ない
constexpr UINT kTimerPeriodMs = 1;

// 高分解能タイマーの、現在の値と、1 秒あたりの目盛りの数
int64_t NowTicks() {
	LARGE_INTEGER t;
	QueryPerformanceCounter(&t);
	return t.QuadPart;
}
int64_t TicksPerSecond() {
	static const int64_t frequency = [] {
		LARGE_INTEGER f;
		QueryPerformanceFrequency(&f);
		return f.QuadPart;
	}();
	return frequency;
}
} // namespace

struct AbsoluteFrameWork::ComScope {
	ComScope() {
		HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
		CheckHROrDie_(hr, "CoInitializeEx failed");
	}

	~ComScope() {
		CoUninitialize();
	}

	ComScope(const ComScope&) = delete;
	ComScope& operator=(const ComScope&) = delete;
	ComScope(ComScope&&) = delete;
	ComScope& operator=(ComScope&&) = delete;
};

void AbsoluteFrameWork::DeleteComScope_(ComScope* p) noexcept {
	delete p;
}

AbsoluteFrameWork::~AbsoluteFrameWork() = default;

void AbsoluteFrameWork::InitializeEngine_() {
	comScope_.reset(new ComScope());

	winApp_.Initialize();

	DirectXCommon::InitParams params{
		winApp_.GetHInstance(),
		winApp_.GetHwnd(),
		WinApp::kClientWidth,
		WinApp::kClientHeight,
	};
	dx_.Initialize(params);

	TextureManager::GetInstance()->Initialize(&dx_);
	ModelManager::GetInstance()->Initialize(&dx_);
	ParticleManager::GetInstance()->Initialize(&dx_);

	imgui_.Initialize(&winApp_, &dx_);

	const bool inputOk = input_.Initialize(winApp_.GetHInstance(), winApp_.GetHwnd());
	CheckBoolOrDie_(inputOk, "input_.Initialize failed");

	const bool audioOk = audio_.Initialize();
	CheckBoolOrDie_(audioOk, "audio_.Initialize failed");
}

void AbsoluteFrameWork::FinalizeEngine_() {
	imgui_.Finalize();

	audio_.Shutdown();
	input_.Finalize();

	ParticleManager::GetInstance()->Finalize();

	winApp_.Finalize();

	comScope_.reset(nullptr);
}

void AbsoluteFrameWork::Run() {
	endRequest_ = false;

	InitializeEngine_();
	Initialize();

	// 固定タイムステップ: 論理(入力、Update)は、実際の経過時間を測って、1/60 秒ずつ必要な回数だけ進める。
	// 描画は、論理を進めたときだけ行う(進めなかった回は、前の画面がそのまま残る)。
	// 分けた理由: 以前は、画面の更新(垂直同期)ごとに 1 回ずつ論理を進めていたので、120Hz や 144Hz の画面では、
	// ゲームが 2 倍や 2.4 倍の速さで動いた。論理を「時間」で進めると、画面の更新の速さに関係なく、同じ速さで動く。
	timeBeginPeriod(kTimerPeriodMs);
	double accumulator = 0.0; // 論理に回していない時間(秒)の持ち越し
	int64_t lastTicks = NowTicks();

	// 論理と描画の回数(1 秒ごとに更新して、ログへ出す。60 の固定になっているかの確認に使う)
	int64_t statsStartTicks = lastTicks;
	int statsSteps = 0;
	int statsRenders = 0;
	double statsLogicHz = 0.0;
	double statsRenderFps = 0.0;
	int stepsLastFrame = 0;

	while (!endRequest_) {
		if (!winApp_.ProcessMessage()) {
			RequestEnd();
			break;
		}

		// 前の繰り返しからの経過時間を、持ち越しに足す。止まっていた(読み込みなど)ときは、1 回分だけにする
		const int64_t nowTicks = NowTicks();
		double elapsed = static_cast<double>(nowTicks - lastTicks) / static_cast<double>(TicksPerSecond());
		lastTicks = nowTicks;
		if (elapsed > kHitchSeconds) {
			elapsed = kStepSeconds;
		}
		accumulator += elapsed;

		// 1 フレームの CPU 時間の測定(停止の調査用。結果はログへ出る)。論理を進める回(描画する回)だけ測る
		if (accumulator >= kStepSeconds - kStepSnapSeconds) {
			CpuTimer::BeginFrame();
		}

		// 論理を、時間がたまった分だけ進める
		int steps = 0;
		while (accumulator >= kStepSeconds - kStepSnapSeconds && steps < kMaxStepsPerFrame) {
			input_.Update();
			imgui_.Begin();

			Update();

#ifdef USE_IMGUI
			// 開発用: 固定タイムステップの確認の窓(疑似的に遅い環境を作る、回数の表示)
			ImGui::SetNextWindowPos(ImVec2(8.0f, 8.0f), ImGuiCond_FirstUseEver);
			ImGui::Begin("Frame Timing");
			int presentInterval = static_cast<int>(dx_.GetPresentInterval());
			if (ImGui::SliderInt("Present interval", &presentInterval, 1, 6)) {
				dx_.SetPresentInterval(static_cast<UINT>(presentInterval));
			}
			ImGui::Text("logic  : %.1f steps/s (target 60)", statsLogicHz);
			ImGui::Text("render : %.1f frames/s", statsRenderFps);
			ImGui::Text("steps in the last frame: %d", stepsLastFrame);
			ImGui::End();
#endif

			imgui_.End();

			accumulator -= kStepSeconds;
			++steps;
		}
		// 上限に達して、まだ時間が残っているときは、取り戻さずに捨てる(遅い環境で、遅れが積もり続けないように)
		if (steps >= kMaxStepsPerFrame && accumulator > kStepSeconds) {
			accumulator = 0.0;
		}

		// 論理を進めなかったときは、描画せずに、次の時刻まで少し眠る(画面の更新が速い環境で、無駄に回らないように)
		if (steps == 0) {
			Sleep(kIdleSleepMs);
			continue;
		}

		dx_.PreDraw();
		Draw();
		imgui_.Draw(dx_.GetCommandList());
		dx_.PostDraw();

		CpuTimer::EndFrame();

		stepsLastFrame = steps;
		statsSteps += steps;
		++statsRenders;
		const double statsElapsed = static_cast<double>(NowTicks() - statsStartTicks) / static_cast<double>(TicksPerSecond());
		if (statsElapsed >= 1.0) {
			statsLogicHz = statsSteps / statsElapsed;
			statsRenderFps = statsRenders / statsElapsed;
			statsSteps = 0;
			statsRenders = 0;
			statsStartTicks = NowTicks();
			Logger::GetInstance().Log("[FrameTiming] logic=%.1f steps/s (target 60) render=%.1f frames/s presentInterval=%u", statsLogicHz,
			                          statsRenderFps, dx_.GetPresentInterval());
		}
	}

	timeEndPeriod(kTimerPeriodMs);

	Finalize();
	FinalizeEngine_();
}

void AbsoluteFrameWork::Initialize() {
}

void AbsoluteFrameWork::Finalize() {
}

void AbsoluteFrameWork::Update() {
}
