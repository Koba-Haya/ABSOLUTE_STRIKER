#include "Framework.h"
#include <Windows.h>
#include <objbase.h>
#include <string>
#include "CpuTimer.h"

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

	while (!endRequest_) {
		if (!winApp_.ProcessMessage()) {
			RequestEnd();
			break;
		}

		// 1 フレームの CPU 時間の測定(停止の調査用。結果はログへ出る)
		CpuTimer::BeginFrame();

		input_.Update();
		imgui_.Begin();

		Update();

		imgui_.End();

		dx_.PreDraw();
		Draw();
		imgui_.Draw(dx_.GetCommandList());
		dx_.PostDraw();

		CpuTimer::EndFrame();
	}

	Finalize();
	FinalizeEngine_();
}

void AbsoluteFrameWork::Initialize() {
}

void AbsoluteFrameWork::Finalize() {
}

void AbsoluteFrameWork::Update() {
}
