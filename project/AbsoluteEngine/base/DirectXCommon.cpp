#include "DirectXCommon.h"
#include "DirectXResourceUtils.h"
#include "graphics/texture/RenderTexture.h"
#include "graphics/texture/DepthTexture.h"
#include "Logger.h"
#include <cassert>
#include <dxcapi.h>

// 既存プロジェクトのヘルパを利用（実装はあなたのプロジェクト側に既にあります）
Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>
CreateDescriptorHeap(const Microsoft::WRL::ComPtr<ID3D12Device> &device,
                     D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors,
                     bool shaderVisible);
Microsoft::WRL::ComPtr<ID3D12Resource> CreateDepthStencilTextureResource(
    const Microsoft::WRL::ComPtr<ID3D12Device> &device, INT32 width,
    INT32 height);
D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(
    const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> &descriptorHeap,
    uint32_t descriptorSize, uint32_t index);
D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(
    const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> &descriptorHeap,
    uint32_t descriptorSize, uint32_t index);

#ifdef _DEBUG
#include <dxgidebug.h>
#endif

DirectXCommon::DirectXCommon() {}

DirectXCommon::~DirectXCommon() {
  if (fenceEvent_) {
    CloseHandle(fenceEvent_);
    fenceEvent_ = nullptr;
  }
}

void DirectXCommon::Initialize(const InitParams &params) {
  hInstance_ = params.hInstance;
  hwnd_ = params.hwnd;
  clientWidth_ = params.clientWidth;
  clientHeight_ = params.clientHeight;

#ifdef _DEBUG
  Microsoft::WRL::ComPtr<ID3D12Debug1> debugController;
  if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
    debugController->EnableDebugLayer();
    debugController->SetEnableGPUBasedValidation(TRUE);
  }
#endif

  CreateDeviceAndFactory_();
  ChooseAdapter_();
  CreateCommandObjects_();
  CreateSwapChain_();
  CreateDescriptorHeaps_();
  CreateRTVs_();
  CreateDepthStencil_();
  CreateFenceAndEvent_();
  gpuTimer_.Initialize(device_.Get(), commandQueue_.Get());
  SetupViewportAndScissor_();
  InitDXC_();

  // SRV割当器をDirectXCommonが所有
  srvAlloc_ = std::make_unique<SrvAllocator>();
  srvAlloc_->Init(device_.Get(), srvDescriptorHeap_.Get(), 1);
}

void DirectXCommon::BeginFrame() {
  frameTimerSlot_ = gpuTimer_.Begin(commandList_.Get(), "Frame");

  UINT backBufferIndex = swapChain_->GetCurrentBackBufferIndex();
  D3D12_RESOURCE_BARRIER barrier{};
  barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
  barrier.Transition.pResource = swapChainResources_[backBufferIndex].Get();
  barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
  barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
  commandList_->ResourceBarrier(1, &barrier);

  D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle =
      GetCPUDescriptorHandle(dsvDescriptorHeap_.Get(), descriptorSizeDSV_, 0);
  commandList_->OMSetRenderTargets(1, &rtvHandles_[backBufferIndex], FALSE, &dsvHandle);
}

void DirectXCommon::EndFrame() {
  UINT backBufferIndex = swapChain_->GetCurrentBackBufferIndex();

  // Present 遷移
  D3D12_RESOURCE_BARRIER barrier{};
  barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
  barrier.Transition.pResource = swapChainResources_[backBufferIndex].Get();
  barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
  barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
  commandList_->ResourceBarrier(1, &barrier);

  // 1 フレーム全体の計測を閉じ、結果を読み戻しバッファへ書き出す命令を積む(Close の前でないと間に合わない)
  gpuTimer_.End(commandList_.Get(), frameTimerSlot_);
  gpuTimer_.Resolve(commandList_.Get());

  HRESULT hr = commandList_->Close();
  assert(SUCCEEDED(hr));

  ID3D12CommandList *lists[]{commandList_.Get()};
  commandQueue_->ExecuteCommandLists(1, lists);
  swapChain_->Present(1, 0);

  fenceValue_++;
  commandQueue_->Signal(fence_.Get(), fenceValue_);
  if (fence_->GetCompletedValue() < fenceValue_) {
    fence_->SetEventOnCompletion(fenceValue_, fenceEvent_);
    WaitForSingleObject(fenceEvent_, INFINITE);
  }

  // GPU の完了を待ったあとなので、この時点で今フレームの計測結果を読める
  gpuTimer_.Collect();

  hr = commandAllocator_->Reset();
  assert(SUCCEEDED(hr));
  hr = commandList_->Reset(commandAllocator_.Get(), nullptr);
  assert(SUCCEEDED(hr));

  // 毎フレームの終わりにDirectXのエラーメッセージがあればログに吐き出す
  Logger::GetInstance().CheckDirectXErrors(device_.Get());
}

void DirectXCommon::PreDraw() {
  BeginFrame();

  UINT backBufferIndex = swapChain_->GetCurrentBackBufferIndex();

  float clearColor[] = {0.1f, 0.25f, 0.5f, 1.0f};
  commandList_->ClearRenderTargetView(rtvHandles_[backBufferIndex], clearColor, 0, nullptr);

  D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle =
      GetCPUDescriptorHandle(dsvDescriptorHeap_.Get(), descriptorSizeDSV_, 0);
  commandList_->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

  commandList_->RSSetViewports(1, &viewport_);
  commandList_->RSSetScissorRects(1, &scissorRect_);
}

void DirectXCommon::PostDraw() {
  EndFrame();
}

void DirectXCommon::CreateDeviceAndFactory_() {
  HRESULT hr = CreateDXGIFactory(IID_PPV_ARGS(&dxgiFactory_));
  assert(SUCCEEDED(hr));
}

void DirectXCommon::ChooseAdapter_() {
  for (UINT i = 0;
       dxgiFactory_->EnumAdapterByGpuPreference(
           i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&useAdapter_)) !=
       DXGI_ERROR_NOT_FOUND;
       ++i) {
    DXGI_ADAPTER_DESC3 desc{};
    HRESULT hr = useAdapter_->GetDesc3(&desc);
    assert(SUCCEEDED(hr));
    if ((desc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE) == 0) {
      break;
    }
    useAdapter_ = nullptr;
  }
  assert(useAdapter_ != nullptr);

  D3D_FEATURE_LEVEL levels[]{D3D_FEATURE_LEVEL_12_2, D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0};
  for (size_t i = 0; i < _countof(levels); ++i) {
    HRESULT hr = D3D12CreateDevice(useAdapter_.Get(), levels[i], IID_PPV_ARGS(&device_));
    if (SUCCEEDED(hr)) {
      break;
    }
  }
  assert(device_ != nullptr);

#ifdef _DEBUG
  Microsoft::WRL::ComPtr<ID3D12InfoQueue> infoQueue;
  if (SUCCEEDED(device_->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {
    infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
    infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
    infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, FALSE);

    D3D12_MESSAGE_ID denyIds[] = {D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE};
    D3D12_MESSAGE_SEVERITY severities[]{D3D12_MESSAGE_SEVERITY_INFO};

    D3D12_INFO_QUEUE_FILTER filter{};
    filter.DenyList.NumIDs = _countof(denyIds);
    filter.DenyList.pIDList = denyIds;
    filter.DenyList.NumSeverities = _countof(severities);
    filter.DenyList.pSeverityList = severities;
    infoQueue->PushStorageFilter(&filter);
  }
#endif
}

void DirectXCommon::CreateCommandObjects_() {
  HRESULT hr = S_OK;

  D3D12_COMMAND_QUEUE_DESC cqDesc{};
  hr = device_->CreateCommandQueue(&cqDesc, IID_PPV_ARGS(&commandQueue_));
  assert(SUCCEEDED(hr));

  hr = device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator_));
  assert(SUCCEEDED(hr));

  hr = device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator_.Get(), nullptr,
                                  IID_PPV_ARGS(&commandList_));
  assert(SUCCEEDED(hr));
}

void DirectXCommon::CreateSwapChain_() {
  swapChainDesc_ = {};
  swapChainDesc_.Width = clientWidth_;
  swapChainDesc_.Height = clientHeight_;
  swapChainDesc_.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  swapChainDesc_.SampleDesc.Count = 1;
  swapChainDesc_.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  swapChainDesc_.BufferCount = 2;
  swapChainDesc_.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

  HRESULT hr = dxgiFactory_->CreateSwapChainForHwnd(
      commandQueue_.Get(), hwnd_, &swapChainDesc_, nullptr, nullptr,
      reinterpret_cast<IDXGISwapChain1 **>(swapChain_.GetAddressOf()));
  assert(SUCCEEDED(hr));

  hr = swapChain_->GetBuffer(0, IID_PPV_ARGS(&swapChainResources_[0]));
  assert(SUCCEEDED(hr));
  hr = swapChain_->GetBuffer(1, IID_PPV_ARGS(&swapChainResources_[1]));
  assert(SUCCEEDED(hr));
}

namespace {
// シェーダーから見える(CBV / SRV / UAV)デスクリプタヒープの大きさ。
// モデル・テクスチャなどは、確保したデスクリプタを、破棄しても返さないものがある(ModelResource、SkinCluster、TextureResource)。
// 展示のように、シーンを何周も作り直しても尽きないよう、余裕を持たせる(使用量は、シーンの開始時にログへ出す)。
// 計測: ゲーム -> ゲームオーバーの 1 周で、約 20 個ずつ増える(128 個のままなら、展示のループは 5 周で尽きる見込みだった)。
// 16384 個なら、約 800 周(約 4.5 時間)は尽きない。シェーダーから見えるヒープの上限(約 100 万)に対して、十分に小さい
constexpr UINT kSrvDescriptorCount = 16384;
} // namespace

void DirectXCommon::CreateDescriptorHeaps_() {
  rtvDescriptorHeap_ = CreateDescriptorHeap(device_.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 16, false);
  srvDescriptorHeap_ = CreateDescriptorHeap(device_.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, kSrvDescriptorCount, true);
  dsvDescriptorHeap_ = CreateDescriptorHeap(device_.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 16, false);

  descriptorSizeRTV_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
  descriptorSizeSRV_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  descriptorSizeDSV_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
}

void DirectXCommon::CreateRTVs_() {
  rtvDesc_ = {};
  rtvDesc_.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
  rtvDesc_.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

  D3D12_CPU_DESCRIPTOR_HANDLE rtvStart =
      GetCPUDescriptorHandle(rtvDescriptorHeap_.Get(), descriptorSizeRTV_, 0);

  rtvHandles_[0] = rtvStart;
  device_->CreateRenderTargetView(swapChainResources_[0].Get(), &rtvDesc_, rtvHandles_[0]);

  rtvHandles_[1].ptr = rtvHandles_[0].ptr + descriptorSizeRTV_;
  device_->CreateRenderTargetView(swapChainResources_[1].Get(), &rtvDesc_, rtvHandles_[1]);

  nextRtvIndex_ = 2; // バックバッファ2枚分を予約
}

void DirectXCommon::CreateDepthStencil_() {
  depthStencilResource_ = CreateDepthStencilTextureResource(device_.Get(), clientWidth_, clientHeight_);

  D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
  dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
  dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

  device_->CreateDepthStencilView(
      depthStencilResource_.Get(), &dsvDesc,
      dsvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart());

  nextDsvIndex_ = 1; // バックバッファ用で1つ消費
}

void DirectXCommon::CreateFenceAndEvent_() {
  HRESULT hr = device_->CreateFence(fenceValue_, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_));
  assert(SUCCEEDED(hr));
  fenceEvent_ = CreateEvent(NULL, FALSE, FALSE, NULL);
  assert(fenceEvent_ != nullptr);
}

void DirectXCommon::SetupViewportAndScissor_() {
  viewport_ = {};
  viewport_.Width = static_cast<float>(clientWidth_);
  viewport_.Height = static_cast<float>(clientHeight_);
  viewport_.TopLeftX = 0.0f;
  viewport_.TopLeftY = 0.0f;
  viewport_.MinDepth = 0.0f;
  viewport_.MaxDepth = 1.0f;

  scissorRect_ = {};
  scissorRect_.left = 0;
  scissorRect_.top = 0;
  scissorRect_.right = clientWidth_;
  scissorRect_.bottom = clientHeight_;
}

void DirectXCommon::InitDXC_() {
  HRESULT hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(dxc_.utils.GetAddressOf()));
  assert(SUCCEEDED(hr));
  hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(dxc_.compiler.GetAddressOf()));
  assert(SUCCEEDED(hr));
  hr = dxc_.utils->CreateDefaultIncludeHandler(dxc_.includeHandler.GetAddressOf());
  assert(SUCCEEDED(hr));
}

void DirectXCommon::TransitionBackBufferToRenderTarget_() {
  UINT i = swapChain_->GetCurrentBackBufferIndex();
  D3D12_RESOURCE_BARRIER b{};
  b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  b.Transition.pResource = swapChainResources_[i].Get();
  b.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
  b.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
  commandList_->ResourceBarrier(1, &b);
}

void DirectXCommon::TransitionBackBufferToPresent_() {
  UINT i = swapChain_->GetCurrentBackBufferIndex();
  D3D12_RESOURCE_BARRIER b{};
  b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  b.Transition.pResource = swapChainResources_[i].Get();
  b.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
  b.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
  commandList_->ResourceBarrier(1, &b);
}

D3D12_CPU_DESCRIPTOR_HANDLE DirectXCommon::AllocateRtv() {
  assert(nextRtvIndex_ < 16);
  D3D12_CPU_DESCRIPTOR_HANDLE handle = GetCPUDescriptorHandle(rtvDescriptorHeap_, descriptorSizeRTV_, nextRtvIndex_);
  nextRtvIndex_++;
  return handle;
}

D3D12_CPU_DESCRIPTOR_HANDLE DirectXCommon::AllocateDsv() {
  assert(nextDsvIndex_ < 16);
  D3D12_CPU_DESCRIPTOR_HANDLE handle = GetCPUDescriptorHandle(dsvDescriptorHeap_, descriptorSizeDSV_, nextDsvIndex_);
  nextDsvIndex_++;
  return handle;
}

void DirectXCommon::SetRenderTarget(RenderTexture* target) {
    assert(target);
    
    // 1. リソースバリア (SRV -> RT)
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = target->GetResource();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    commandList_->ResourceBarrier(1, &barrier);

    // 2. レンダーターゲット設定
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = target->GetRtvHandle();
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = GetCPUDescriptorHandle(dsvDescriptorHeap_.Get(), descriptorSizeDSV_, 0);
    commandList_->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);
}

void DirectXCommon::SetRenderTargetWithDepth(RenderTexture* target, DepthTexture* depthTarget) {
    assert(target);
    assert(depthTarget);
    
    // 1. リソースバリア (SRV -> RT)
    D3D12_RESOURCE_BARRIER barriers[2]{};
    barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barriers[0].Transition.pResource = target->GetResource();
    barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

    // DepthTexture の状態遷移
    if (depthTarget->GetCurrentState() != D3D12_RESOURCE_STATE_DEPTH_WRITE) {
        barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barriers[1].Transition.pResource = depthTarget->GetResource();
        barriers[1].Transition.StateBefore = depthTarget->GetCurrentState();
        barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;
        depthTarget->SetCurrentState(D3D12_RESOURCE_STATE_DEPTH_WRITE);
        commandList_->ResourceBarrier(2, barriers);
    } else {
        commandList_->ResourceBarrier(1, barriers); // targetのみ
    }

    // 2. レンダーターゲット設定
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = target->GetRtvHandle();
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = depthTarget->GetDsvHandle();
    commandList_->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);
}

void DirectXCommon::BeginColorPassReadingDepth(RenderTexture* target, DepthTexture* depthTarget) {
    assert(target);
    assert(depthTarget);

    // 深度: 書き込み用 -> 読み取り用
    EnsureDepthReadable(depthTarget);

    // 色だけを描画先にする(target はシーン描画中なので、すでに描画先の状態)
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = target->GetRtvHandle();
    commandList_->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
}

void DirectXCommon::EnsureDepthReadable(DepthTexture* depthTarget) {
    assert(depthTarget);
    if (depthTarget->GetCurrentState() == D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE) {
        return;
    }
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = depthTarget->GetResource();
    barrier.Transition.StateBefore = depthTarget->GetCurrentState();
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    depthTarget->SetCurrentState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    commandList_->ResourceBarrier(1, &barrier);
}

void DirectXCommon::SetRenderTargetColorOnly(RenderTexture* target) {
    assert(target);

    // 読み取り用(SRV) -> 描画用(RT)
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = target->GetResource();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    commandList_->ResourceBarrier(1, &barrier);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = target->GetRtvHandle();
    commandList_->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
}

void DirectXCommon::TransitionToShaderResource(RenderTexture* target) {
    assert(target);

    // 描画用(RT) -> 読み取り用(SRV)
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = target->GetResource();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    commandList_->ResourceBarrier(1, &barrier);
}

void DirectXCommon::SetViewportAndScissor(uint32_t width, uint32_t height) {
    D3D12_VIEWPORT viewport = viewport_;
    viewport.Width = static_cast<float>(width);
    viewport.Height = static_cast<float>(height);
    D3D12_RECT scissor = scissorRect_;
    scissor.right = static_cast<LONG>(width);
    scissor.bottom = static_cast<LONG>(height);
    commandList_->RSSetViewports(1, &viewport);
    commandList_->RSSetScissorRects(1, &scissor);
}

void DirectXCommon::ResetViewportAndScissor() {
    commandList_->RSSetViewports(1, &viewport_);
    commandList_->RSSetScissorRects(1, &scissorRect_);
}

void DirectXCommon::FinishRendering(RenderTexture* target) {
    assert(target);

    // 1. リソースバリア (RT -> SRV)
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = target->GetResource();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    commandList_->ResourceBarrier(1, &barrier);

    // 2. バックバッファに戻す
    ResetRenderTarget();
}

void DirectXCommon::FinishRenderingWithDepth(RenderTexture* target, DepthTexture* depthTarget) {
    assert(target);
    assert(depthTarget);

    // 1. リソースバリア (RT/DSV -> SRV)
    D3D12_RESOURCE_BARRIER barriers[2]{};
    barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barriers[0].Transition.pResource = target->GetResource();
    barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

    if (depthTarget->GetCurrentState() != D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE) {
        barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barriers[1].Transition.pResource = depthTarget->GetResource();
        barriers[1].Transition.StateBefore = depthTarget->GetCurrentState();
        barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        depthTarget->SetCurrentState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        commandList_->ResourceBarrier(2, barriers);
    } else {
        commandList_->ResourceBarrier(1, barriers); // targetのみ
    }

    // 2. バックバッファに戻す
    ResetRenderTarget();
}

void DirectXCommon::ResetRenderTarget() {
    // 常にバックバッファに戻る前提
    UINT backBufferIndex = swapChain_->GetCurrentBackBufferIndex();
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = GetCPUDescriptorHandle(dsvDescriptorHeap_.Get(), descriptorSizeDSV_, 0);
    commandList_->OMSetRenderTargets(1, &rtvHandles_[backBufferIndex], FALSE, &dsvHandle);
}
