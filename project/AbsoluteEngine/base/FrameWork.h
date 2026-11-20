#pragma once
#include "WinApp.h"
#include "DirectXCommon.h"
#include "Input.h"
#include "Audio.h"

// マネージャー群
#include "ImGuiManager.h"
#include "TextureManager.h"
#include "ModelManager.h"
#include "ParticleManager.h"

#include <memory>

class AbsoluteFrameWork {
public:
	// コンストラクタ
	AbsoluteFrameWork() = default;
	// 仮想デストラクタ
	virtual ~AbsoluteFrameWork();

	// 実行
	void Run();

	// 終了チェック
	bool IsEndRequested() { return endRequest_; };

protected:
	// 初期化
	virtual void Initialize();
	// 終了
	virtual void Finalize();
	// 論理の更新。実際の経過時間を測って、1 秒に 60 回(1 回 1/60 秒)呼ばれる(画面の更新の速さに関係なく。固定タイムステップ)。
	// 1 回の描画の前に、0 回(画面の更新が速い環境)から数回(遅い環境)呼ばれる。
	// 描画(Draw)は、Update を呼んだ回だけ呼ばれる
	virtual void Update();
	// 描画
	virtual void Draw() = 0;

	// 終了要求（ESCで終わる、ウィンドウ閉じる、などをゲーム側で判定して呼べる）
	void RequestEnd() { endRequest_ = true; }

public:
	// --- エンジン機能へアクセスするためのゲッタ ---
	WinApp& GetWinApp() { return winApp_; }
	DirectXCommon& GetDX() { return dx_; }
	Input& GetInput() { return input_; }
	AudioManager& GetAudio() { return audio_; }
	ImGuiManager& GetImGui() { return imgui_; }

	ID3D12GraphicsCommandList* GetCmdList() { return dx_.GetCommandList(); }
private:
	// エンジンの共通初期化/終了
	void InitializeEngine_();
	void FinalizeEngine_();

private:
	struct ComScope;
	static void DeleteComScope_(ComScope* p) noexcept;

private:
	// 終了要求フラグ
	bool endRequest_ = false;

	std::unique_ptr<ComScope, void(*)(ComScope*)> comScope_{ nullptr, &DeleteComScope_ };

	WinApp winApp_;
	DirectXCommon dx_;
	Input input_;
	AudioManager audio_;
	ImGuiManager imgui_;
};

