#include "DirectXCommon.h"
#include "DirectXResourceUtils.h"
#include "graphics/texture/RenderTexture.h"
#include "graphics/texture/DepthTexture.h"
#include "Logger.h"
#include <algorithm>
#include <vector>
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

namespace {
// 遅延解放で預かっている資源と、フレーム番号。
// 静的な破棄の順序の問題を避けるため、関数の中の静的変数(使われるときに作られ、プログラムの終わりまで残す)で持つ
struct DeferredReleaseState {
  std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> pending;
  uint64_t frameCounter = 0;
  bool accepting = false; // DirectXCommon が生きている間だけ true
};

DeferredReleaseState &GetDeferredState() {
  static DeferredReleaseState *state = new DeferredReleaseState(); // 破棄しない(終了処理の順序によらず、安全に参照できる)
  return *state;
}
} // namespace

void DirectXCommon::DeferRelease(Microsoft::WRL::ComPtr<ID3D12Resource> resource) {
  DeferredReleaseState &state = GetDeferredState();
  if (!resource || !state.accepting) {
    return; // 預かれない(DirectXCommon がない)ときは、ここで resource が解放される
  }
  state.pending.push_back(std::move(resource));
}

uint64_t DirectXCommon::GetFrameCounter() {
  return GetDeferredState().frameCounter;
}

DirectXCommon::DirectXCommon() {}

DirectXCommon::~DirectXCommon() {
  // 預かっている資源をここで手放し、以降は預からない(デバイスより長く資源を持たないため)
  GetDeferredState().pending.clear();
  GetDeferredState().accepting = false;
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

  // DRED(デバイスが外れたときに、直前の GPU の命令と、不正なメモリアクセスの記録を残す機能)を、デバイスを作る前に有効にする。
  // デバッグレイヤーの無いビルド(Development / Release)でも、外れた原因を調べられるようにする。
  // 取得に失敗しても(対応していない環境)、何もせず進める
  {
    Microsoft::WRL::ComPtr<ID3D12DeviceRemovedExtendedDataSettings> dredSettings;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&dredSettings)))) {
      dredSettings->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
      dredSettings->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
    }
  }

  GetDeferredState().accepting = true; // 以降、資源の遅延解放を受け付ける
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
  swapChain_->Present(presentInterval_, 0);

  fenceValue_++;
  commandQueue_->Signal(fence_.Get(), fenceValue_);
  if (fence_->GetCompletedValue() < fenceValue_) {
    fence_->SetEventOnCompletion(fenceValue_, fenceEvent_);
    WaitForSingleObject(fenceEvent_, INFINITE);
  }

  // GPU の完了を待ったあとなので、今フレームの命令が参照していた資源は、もう解放してよい。預かっていた資源を解放し、次のフレームへ進む
  GetDeferredState().pending.clear();
  ++GetDeferredState().frameCounter;

  // GPU の完了を待ったあとなので、この時点で今フレームの計測結果を読める
  gpuTimer_.Collect();

  hr = commandAllocator_->Reset();
  assert(SUCCEEDED(hr));
  hr = commandList_->Reset(commandAllocator_.Get(), nullptr);
  assert(SUCCEEDED(hr));

  // 毎フレームの終わりにDirectXのエラーメッセージがあればログに吐き出す
  Logger::GetInstance().CheckDirectXErrors(device_.Get());

  // GPU のデバイスが外れていたら、理由と、直前の GPU の命令をログへ出す
  CheckDeviceRemoved_();
}

namespace {
// 1 つのコマンドリストについて、ログへ出す直前の命令の数(外れた位置の前後の流れが分かるようにする)
constexpr UINT kBreadcrumbContextOps = 6;
// ログに出す、メモリの割り当ての最大数(多すぎて読めなくなるのを避ける)
constexpr int kMaxLoggedAllocations = 8;
} // namespace

void DirectXCommon::CheckDeviceRemoved_() {
  if (deviceRemovedLogged_ || !device_) return;
  const HRESULT reason = device_->GetDeviceRemovedReason();
  if (SUCCEEDED(reason)) return;
  deviceRemovedLogged_ = true;

  // 外れた理由(DXGI_ERROR_DEVICE_HUNG: GPU が止まった、DEVICE_RESET: 不正な操作、DRIVER_INTERNAL_ERROR: ドライバの不具合 など)
  Logger::GetInstance().Log("[DeviceRemoved] reason=0x%08X", static_cast<unsigned int>(reason));

  Microsoft::WRL::ComPtr<ID3D12DeviceRemovedExtendedData> dred;
  if (FAILED(device_->QueryInterface(IID_PPV_ARGS(&dred)))) {
    Logger::GetInstance().Log("[DeviceRemoved] DRED is not available");
    return;
  }

  // 直前に GPU が実行していた命令(ブレッドクラム)。完了した最後の命令の次が、止まった所
  D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT breadcrumbs{};
  if (SUCCEEDED(dred->GetAutoBreadcrumbsOutput(&breadcrumbs))) {
    for (const D3D12_AUTO_BREADCRUMB_NODE *node = breadcrumbs.pHeadAutoBreadcrumbNode; node != nullptr; node = node->pNext) {
      const UINT completed = node->pLastBreadcrumbValue ? *node->pLastBreadcrumbValue : 0;
      if (completed >= node->BreadcrumbCount) continue; // 最後まで完了したコマンドリストは、原因ではない
      Logger::GetInstance().Log("[DeviceRemoved] unfinished command list \"%s\": completed %u of %u ops",
                                node->pCommandListDebugNameA ? node->pCommandListDebugNameA : "(unnamed)", completed,
                                node->BreadcrumbCount);
      const UINT from = completed > kBreadcrumbContextOps ? completed - kBreadcrumbContextOps : 0;
      const UINT to = (std::min)(completed + 2, node->BreadcrumbCount);
      for (UINT i = from; i < to; ++i) {
        // 命令の種類は D3D12_AUTO_BREADCRUMB_OP の値(例: 2 = DrawInstanced、3 = DrawIndexedInstanced、4 = ExecuteIndirect、6 = Dispatch)
        Logger::GetInstance().Log("[DeviceRemoved]   op[%u] = %d%s", i, static_cast<int>(node->pCommandHistory[i]),
                                  i == completed ? "  <-- stopped here" : "");
      }
    }
  }

  // 不正なメモリアクセス(ページフォールト)の記録: アクセスしようとしたアドレスと、そこにあった(または、最近解放された)資源
  D3D12_DRED_PAGE_FAULT_OUTPUT pageFault{};
  if (SUCCEEDED(dred->GetPageFaultAllocationOutput(&pageFault)) && pageFault.PageFaultVA != 0) {
    Logger::GetInstance().Log("[DeviceRemoved] page fault at VA=0x%llX", static_cast<unsigned long long>(pageFault.PageFaultVA));
    int logged = 0;
    for (const D3D12_DRED_ALLOCATION_NODE *node = pageFault.pHeadExistingAllocationNode; node != nullptr && logged < kMaxLoggedAllocations;
         node = node->pNext, ++logged) {
      Logger::GetInstance().Log("[DeviceRemoved]   existing allocation: \"%s\" (type %d)", node->ObjectNameA ? node->ObjectNameA : "(unnamed)",
                                static_cast<int>(node->AllocationType));
    }
    logged = 0;
    for (const D3D12_DRED_ALLOCATION_NODE *node = pageFault.pHeadRecentFreedAllocationNode; node != nullptr && logged < kMaxLoggedAllocations;
         node = node->pNext, ++logged) {
      Logger::GetInstance().Log("[DeviceRemoved]   recently freed: \"%s\" (type %d)", node->ObjectNameA ? node->ObjectNameA : "(unnamed)",
                                static_cast<int>(node->AllocationType));
    }
  }
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
