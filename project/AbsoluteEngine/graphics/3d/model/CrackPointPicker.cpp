#define NOMINMAX
#include "CrackPointPicker.h"

#include <algorithm>
#include <cassert>
#include <cstring>

#include "DirectXCommon.h"
#include "Renderer.h"
#include "ShaderCompilerUtils.h"

namespace {
// 定数バッファのサイズは 256 バイトの倍数にする
constexpr UINT kConstantBufferSize = 256;
// 1 頂点に 1 スレッド。HLSL の kThreadsPerGroup と同じ値にすること
constexpr uint32_t kThreadsPerGroup = 64;
// 出力バッファの先頭(選ばれた数)のバイト数
constexpr UINT kCounterBytes = sizeof(uint32_t);
// 出力バッファの全体のバイト数(先頭の数 + 点 kMaxPoints 個 x 1 点あたりの uint の数)
constexpr UINT kOutputElements = 1 + CrackPointPicker::kMaxPoints * CrackPointPicker::kUintsPerPoint;
constexpr UINT kOutputBytes = static_cast<UINT>(sizeof(uint32_t) * kOutputElements);

// HLSL 側の PickParams と 1:1 で並びを合わせる(32 バイト)
struct CrackPointPickParams {
  float crackCellScale;
  float maxEdge;
  float chance;
  float seed;
  uint32_t vertexCount;
  uint32_t maxPoints;
  float pad[2];
  float crackOrigin[3];
  float crackSpreadRadius;
};
static_assert(sizeof(CrackPointPickParams) <= kConstantBufferSize, "params must fit in one constant buffer slot");
} // namespace

CrackPointPicker *CrackPointPicker::GetInstance() {
  static CrackPointPicker instance;
  return &instance;
}

void CrackPointPicker::Initialize(DirectXCommon *dx) {
  if (initialized_ || !dx) return;
  dx_ = dx;
  auto *device = dx_->GetDevice();
  auto &srvAlloc = dx_->GetSrvAllocator();

  // --- ルートシグネチャ: 0 = 定数(b0)、1 = 頂点の SRV(t0)、2 = 出力の UAV(u0) ---
  D3D12_DESCRIPTOR_RANGE srvRange{};
  srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
  srvRange.NumDescriptors = 1;
  srvRange.BaseShaderRegister = 0;
  D3D12_DESCRIPTOR_RANGE uavRange{};
  uavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
  uavRange.NumDescriptors = 1;
  uavRange.BaseShaderRegister = 0;

  D3D12_ROOT_PARAMETER params[3]{};
  params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
  params[0].Descriptor.ShaderRegister = 0;
  params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  params[1].DescriptorTable.NumDescriptorRanges = 1;
  params[1].DescriptorTable.pDescriptorRanges = &srvRange;
  params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  params[2].DescriptorTable.NumDescriptorRanges = 1;
  params[2].DescriptorTable.pDescriptorRanges = &uavRange;

  D3D12_ROOT_SIGNATURE_DESC rsDesc{};
  rsDesc.NumParameters = 3;
  rsDesc.pParameters = params;
  rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
  ComPtr<ID3DBlob> blob, error;
  HRESULT hr = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error);
  assert(SUCCEEDED(hr) && "CrackPointPicker: root signature serialize failed");
  hr = device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
  assert(SUCCEEDED(hr) && "CrackPointPicker: root signature create failed");

  // --- パイプライン ---
  ComPtr<IDxcBlob> csBlob;
  csBlob.Attach(CompileShader(L"resources/engine/shaders/CrackPointPick.CS.hlsl", L"cs_6_0", dx_->GetDXCUtils(), dx_->GetDXCCompiler(),
                              dx_->GetDXCIncludeHandler()));
  assert(csBlob && "CrackPointPicker: shader compile failed");
  D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc{};
  psoDesc.pRootSignature = rootSignature_.Get();
  psoDesc.CS = {csBlob->GetBufferPointer(), csBlob->GetBufferSize()};
  hr = device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState_));
  assert(SUCCEEDED(hr) && "CrackPointPicker: pipeline create failed");

  // --- 出力バッファ(GPU が書く)と、その UAV ---
  const CD3DX12_HEAP_PROPERTIES defaultHeap(D3D12_HEAP_TYPE_DEFAULT);
  const CD3DX12_RESOURCE_DESC outputDesc = CD3DX12_RESOURCE_DESC::Buffer(kOutputBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
  // 既定のヒープのバッファは、作った時点で、必ず COMMON 状態(ほかの状態を指定しても無視される)。最初の遷移は、COMMON から行う
  outputState_ = D3D12_RESOURCE_STATE_COMMON;
  hr = device->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &outputDesc, outputState_, nullptr, IID_PPV_ARGS(&outputBuffer_));
  assert(SUCCEEDED(hr) && "CrackPointPicker: output buffer create failed");
  outputBuffer_->SetName(L"CrackPointPicker::OutputBuffer");

  outputUavIndex_ = srvAlloc.Allocate();
  D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
  uavDesc.Format = DXGI_FORMAT_UNKNOWN;
  uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
  uavDesc.Buffer.FirstElement = 0;
  uavDesc.Buffer.NumElements = kOutputElements;
  uavDesc.Buffer.StructureByteStride = sizeof(uint32_t);
  uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
  device->CreateUnorderedAccessView(outputBuffer_.Get(), nullptr, &uavDesc, srvAlloc.Cpu(outputUavIndex_));

  // --- ゼロのバッファ(先頭の数を 0 に戻す用)。Upload ヒープはゼロで初期化されないので、明示的に 0 を書く ---
  auto *renderer = Renderer::GetInstance();
  zeroBuffer_ = renderer->CreateUploadBuffer(kConstantBufferSize);
  assert(zeroBuffer_);
  zeroBuffer_->SetName(L"CrackPointPicker::ZeroBuffer");
  void *zeroMapped = nullptr;
  zeroBuffer_->Map(0, nullptr, &zeroMapped);
  std::memset(zeroMapped, 0, kConstantBufferSize);
  zeroBuffer_->Unmap(0, nullptr);

  // --- 読み戻しのバッファ(CPU が読む)。Readback ヒープのバッファは、COPY_DEST のままにしておく ---
  const CD3DX12_HEAP_PROPERTIES readbackHeap(D3D12_HEAP_TYPE_READBACK);
  const CD3DX12_RESOURCE_DESC readbackDesc = CD3DX12_RESOURCE_DESC::Buffer(kOutputBytes);
  hr = device->CreateCommittedResource(&readbackHeap, D3D12_HEAP_FLAG_NONE, &readbackDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                       IID_PPV_ARGS(&readbackBuffer_));
  assert(SUCCEEDED(hr) && "CrackPointPicker: readback buffer create failed");
  readbackBuffer_->SetName(L"CrackPointPicker::ReadbackBuffer");

  // --- 依頼の定数 ---
  paramBuffer_ = renderer->CreateUploadBuffer(kConstantBufferSize);
  assert(paramBuffer_);
  paramBuffer_->SetName(L"CrackPointPicker::ParamBuffer");
  paramBuffer_->Map(0, nullptr, &paramMapped_);

  initialized_ = true;
}

void CrackPointPicker::Transition_(D3D12_RESOURCE_STATES next) {
  if (outputState_ == next) return;
  const CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(outputBuffer_.Get(), outputState_, next);
  dx_->GetCommandList()->ResourceBarrier(1, &barrier);
  outputState_ = next;
}

bool CrackPointPicker::Request(const PickDesc &desc) {
  if (!initialized_ || pending_) return false;
  if (desc.vertexCount == 0 || desc.cellSize <= 0.0f) return false;

  auto *cmdList = dx_->GetCommandList();
  auto &srvAlloc = dx_->GetSrvAllocator();

  // 依頼の定数を書く(1 度に 1 件だけなので、1 つのバッファを使い回す)
  CrackPointPickParams params{};
  params.crackCellScale = 1.0f / desc.cellSize;
  params.maxEdge = desc.maxEdge;
  params.chance = desc.chance;
  params.seed = desc.seed;
  params.vertexCount = desc.vertexCount;
  params.maxPoints = kMaxPoints;
  params.crackOrigin[0] = desc.originX;
  params.crackOrigin[1] = desc.originY;
  params.crackOrigin[2] = desc.originZ;
  params.crackSpreadRadius = desc.spreadRadius;
  std::memcpy(paramMapped_, &params, sizeof(params));

  // 1. 出力の先頭の数を 0 に戻す(ゼロのバッファからコピー)
  Transition_(D3D12_RESOURCE_STATE_COPY_DEST);
  cmdList->CopyBufferRegion(outputBuffer_.Get(), 0, zeroBuffer_.Get(), 0, kCounterBytes);

  // 2. 選ぶ計算を実行する
  Transition_(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
  ID3D12DescriptorHeap *heaps[] = {dx_->GetSRVHeap()};
  cmdList->SetDescriptorHeaps(1, heaps);
  cmdList->SetComputeRootSignature(rootSignature_.Get());
  cmdList->SetPipelineState(pipelineState_.Get());
  cmdList->SetComputeRootConstantBufferView(0, paramBuffer_->GetGPUVirtualAddress());
  cmdList->SetComputeRootDescriptorTable(1, srvAlloc.Gpu(desc.vertexSrvIndex));
  cmdList->SetComputeRootDescriptorTable(2, srvAlloc.Gpu(outputUavIndex_));
  cmdList->Dispatch((desc.vertexCount + kThreadsPerGroup - 1) / kThreadsPerGroup, 1, 1);

  // 3. 計算の書き込みが終わってから、読み戻しのバッファへコピーする
  const CD3DX12_RESOURCE_BARRIER uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(outputBuffer_.Get());
  cmdList->ResourceBarrier(1, &uavBarrier);
  Transition_(D3D12_RESOURCE_STATE_COPY_SOURCE);
  cmdList->CopyBufferRegion(readbackBuffer_.Get(), 0, outputBuffer_.Get(), 0, kOutputBytes);

  pending_ = true;
  requestedFrame_ = DirectXCommon::GetFrameCounter();
  return true;
}

bool CrackPointPicker::TryCollect(std::vector<PickedPoint> &outPoints) {
  outPoints.clear();
  if (!pending_) return false;
  // フレームの終わりで GPU の完了を待つので、Request したフレームが終われば(フレーム番号が進めば)、コピーは終わっている
  if (DirectXCommon::GetFrameCounter() <= requestedFrame_) return false;

  const D3D12_RANGE readRange{0, kOutputBytes};
  const uint32_t *data = nullptr;
  if (FAILED(readbackBuffer_->Map(0, &readRange, reinterpret_cast<void **>(const_cast<uint32_t **>(&data)))) || !data) {
    pending_ = false;
    return false;
  }
  const uint32_t count = (std::min)(data[0], kMaxPoints);
  outPoints.reserve(count);
  for (uint32_t i = 0; i < count; ++i) {
    const uint32_t *point = data + 1 + i * kUintsPerPoint;
    PickedPoint picked;
    picked.vertexIndex = point[0];
    // 時刻は float のビット列で入っている(HLSL の asuint)。同じ並びで、そのまま読み直す
    std::memcpy(&picked.heatStart, &point[1], sizeof(float));
    std::memcpy(&picked.vanishTime, &point[2], sizeof(float));
    outPoints.push_back(picked);
  }
  const D3D12_RANGE writtenNothing{0, 0};
  readbackBuffer_->Unmap(0, &writtenNothing);

  pending_ = false;
  return true;
}
