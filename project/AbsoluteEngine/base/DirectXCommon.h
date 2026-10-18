#pragma once
#include "externals/DirectXTex/d3dx12.h"
#include "SrvAllocator.h"
#include "DirectXResourceUtils.h"
#include "GpuTimer.h"
#include <Windows.h>
#include <dxgi1_6.h>
#include <wrl.h>

struct IDxcUtils;
struct IDxcCompiler3;
struct IDxcIncludeHandler;

class DirectXCommon {
public:
	// namespace省略
	template <class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

public:
	struct InitParams {
		HINSTANCE hInstance;
		HWND hwnd;
		int32_t clientWidth;
		int32_t clientHeight;
	};

public:
	DirectXCommon();
	~DirectXCommon();

	// スクショ相当の初期化（デバイス/コマンド/スワップチェーン/深度/各種ヒープ/RTV初期化/DSV初期化/フェンス/ビューポート/シザー/DCX）
	void Initialize(const InitParams& params);

	// フレームの最初と最後（必要なら使用）
	void BeginFrame();
	void EndFrame();

	// 描画前後の処理
	void PreDraw();
	void PostDraw();

	// 参照用のゲッタ
	ID3D12Device* GetDevice() const { return device_.Get(); }
	ID3D12GraphicsCommandList* GetCommandList() const {
		return commandList_.Get();
	}
	ID3D12CommandAllocator* GetCommandAllocator() const {
		return commandAllocator_.Get();
	}
	ID3D12CommandQueue* GetCommandQueue() const { return commandQueue_.Get(); }
	IDXGISwapChain4* GetSwapChain() const { return swapChain_.Get(); }
	ID3D12DescriptorHeap* GetSRVHeap() const { return srvDescriptorHeap_.Get(); }
	ID3D12DescriptorHeap* GetRTVHeap() const { return rtvDescriptorHeap_.Get(); }
	ID3D12DescriptorHeap* GetDSVHeap() const { return dsvDescriptorHeap_.Get(); }
	D3D12_CPU_DESCRIPTOR_HANDLE GetRTVHandle(int index) const {
		return rtvHandles_[index];
	}
	const D3D12_VIEWPORT& GetViewport() const { return viewport_; }
	const D3D12_RECT& GetScissorRect() const { return scissorRect_; }

	IDxcUtils* GetDXCUtils() const { return dxc_.utils.Get(); }
	IDxcCompiler3* GetDXCCompiler() const { return dxc_.compiler.Get(); }
	IDxcIncludeHandler* GetDXCIncludeHandler() const {
		return dxc_.includeHandler.Get();
	}

	// SRV割当器取得
	SrvAllocator& GetSrvAllocator() { return *srvAlloc_; }
	// const版SRV割当器取得
	const SrvAllocator& GetSrvAllocator() const { return *srvAlloc_; }

	// GPU 処理時間の計測器(区間の測り方は GpuTimer.h を参照)
	GpuTimer& GetGpuTimer() { return gpuTimer_; }

	UINT GetSRVDescriptorSize() const { return descriptorSizeSRV_; }
	UINT GetDSVDescriptorSize() const { return descriptorSizeDSV_; }
	int GetBackBufferCount() const { return swapChainDesc_.BufferCount; }
	DXGI_FORMAT GetRTVFormat() const { return rtvDesc_.Format; }

	// RTV/DSV割当
	D3D12_CPU_DESCRIPTOR_HANDLE AllocateRtv();
	D3D12_CPU_DESCRIPTOR_HANDLE AllocateDsv();

	// レンダーターゲット切り替え
	void SetRenderTarget(class RenderTexture* target);
	void SetRenderTargetWithDepth(class RenderTexture* target, class DepthTexture* depthTarget);
	void FinishRendering(class RenderTexture* target); // リソース遷移を行ってからバックバッファへ
	// 描画中のシーン(target と depthTarget)に対して、深度をテクスチャとして読みながら色だけへ描くパスを始める。
	// 深度を「書き込み用(DSV)」から「読み取り用(SRV)」へ切り替え、深度バッファを付けずに target へ描く。
	// 終わったら、通常どおり FinishRenderingWithDepth を呼ぶ(深度はすでに読み取り用なので、色だけが遷移する)
	void BeginColorPassReadingDepth(class RenderTexture* target, class DepthTexture* depthTarget);
	// 深度を「書き込み用」から「読み取り用(テクスチャ)」へ切り替える(すでに読み取り用なら何もしない)
	void EnsureDepthReadable(class DepthTexture* depthTarget);
	// 色だけを描画先にして(深度バッファなし)、読み取り用から描画用へ切り替える。終わったら TransitionToShaderResource を呼ぶ。
	// ビューポートはそのまま(SetViewportAndScissor で、描く範囲を先に決めておく)
	void SetRenderTargetColorOnly(class RenderTexture* target);
	// 描き終えた target を、描画用から読み取り用(テクスチャ)へ切り替える。描画先はそのまま(戻さない)
	void TransitionToShaderResource(class RenderTexture* target);
	// ビューポートとシザー矩形を、左上から width x height の範囲にする(低解像度の描画用)
	void SetViewportAndScissor(uint32_t width, uint32_t height);
	// ビューポートとシザー矩形を、通常(画面全体)へ戻す
	void ResetViewportAndScissor();
	void FinishRenderingWithDepth(class RenderTexture* target, class DepthTexture* depthTarget);
	void ResetRenderTarget(); // 強制的にバックバッファへ（遷移なし）

	ID3D12Resource* GetCurrentBackBuffer() const {
		return swapChainResources_[swapChain_->GetCurrentBackBufferIndex()].Get();
	}

private:
	void CreateDeviceAndFactory_();
	void ChooseAdapter_();
	void CreateCommandObjects_();
	void CreateSwapChain_();
	void CreateDescriptorHeaps_();
	void CreateRTVs_();
	void CreateDepthStencil_();
	void CreateFenceAndEvent_();
	void SetupViewportAndScissor_();
	void InitDXC_();
	void TransitionBackBufferToRenderTarget_();
	void TransitionBackBufferToPresent_();

private:
	// 初期化入力
	HINSTANCE hInstance_ = nullptr;
	HWND hwnd_ = nullptr;
	int32_t clientWidth_ = 0;
	int32_t clientHeight_ = 0;

	// DXGI/D3D
	ComPtr<IDXGIFactory7> dxgiFactory_;
	ComPtr<IDXGIAdapter4> useAdapter_;
	ComPtr<ID3D12Device> device_;
	ComPtr<ID3D12CommandQueue> commandQueue_;
	ComPtr<ID3D12CommandAllocator> commandAllocator_;
	ComPtr<ID3D12GraphicsCommandList> commandList_;
	ComPtr<IDXGISwapChain4> swapChain_;

	// バックバッファ
	ComPtr<ID3D12Resource> swapChainResources_[2];
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles_[2]{};

	// ヒープ
	ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap_;
	ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap_;
	ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap_;
	uint32_t descriptorSizeRTV_ = 0;
	uint32_t descriptorSizeSRV_ = 0;
	uint32_t descriptorSizeDSV_ = 0;

	uint32_t nextRtvIndex_ = 0;
	uint32_t nextDsvIndex_ = 0;

	// SRV割当器
	std::unique_ptr<SrvAllocator> srvAlloc_;

	// 深度ステンシル
	ComPtr<ID3D12Resource> depthStencilResource_;

	// フェンス
	ComPtr<ID3D12Fence> fence_;
	uint64_t fenceValue_ = 0;
	HANDLE fenceEvent_ = nullptr;

	// GPU 時間の計測。frameTimerSlot_ は 1 フレーム全体("Frame")の区間番号
	GpuTimer gpuTimer_;
	int32_t frameTimerSlot_ = -1;

	// ビューポート/シザー
	D3D12_VIEWPORT viewport_{};
	D3D12_RECT scissorRect_{};

	// DXC
	struct DXCBlock {
		ComPtr<IDxcUtils> utils;
		ComPtr<IDxcCompiler3> compiler;
		ComPtr<IDxcIncludeHandler> includeHandler;
	} dxc_{};

	// 一時
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc_{};
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc_{};
};