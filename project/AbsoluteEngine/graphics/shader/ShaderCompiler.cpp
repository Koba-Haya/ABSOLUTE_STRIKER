#include "ShaderCompiler.h"
#include "TextureUtils.h" // ConvertString を使うなら
#include "../../base/AssetPath.h"

#include <cassert>
#include <format>

void ShaderCompiler::Initialize(IDxcUtils *utils, IDxcCompiler3 *compiler,
                                IDxcIncludeHandler *include) {
  utils_ = utils;
  compiler_ = compiler;
  include_ = include;
}

ShaderCompiler::ComPtr<IDxcBlob>
ShaderCompiler::Compile(const std::wstring &path, const wchar_t *profile,
                        const std::vector<LPCWSTR> &extraArgs) {
  assert(utils_ && compiler_ && include_);

  // exe 基準の実パスに解決する(#include もこのパスを起点に解決される)
  const std::wstring resolvedPath = AbsoluteEngine::AssetPath::Resolve(path).wstring();

  ComPtr<IDxcBlobEncoding> shaderSource;
  HRESULT hr = utils_->LoadFile(resolvedPath.c_str(), nullptr, shaderSource.GetAddressOf());
  assert(SUCCEEDED(hr));

  DxcBuffer shaderSourceBuffer;
  shaderSourceBuffer.Ptr = shaderSource->GetBufferPointer();
  shaderSourceBuffer.Size = shaderSource->GetBufferSize();
  shaderSourceBuffer.Encoding = DXC_CP_UTF8;

  std::vector<LPCWSTR> args{resolvedPath.c_str(), L"-E",  L"main",          L"-T",
                            profile,      L"-Zi", L"-Qembed_debug", L"-Od",
                            L"-Zpr"};
  args.insert(args.end(), extraArgs.begin(), extraArgs.end());

  ComPtr<IDxcResult> shaderResult;
  hr = compiler_->Compile(&shaderSourceBuffer, args.data(), (UINT)args.size(),
                          include_.Get(), IID_PPV_ARGS(&shaderResult));
  assert(SUCCEEDED(hr));

  ComPtr<IDxcBlobUtf8> shaderError;
  shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr);
  if (shaderError && shaderError->GetStringLength() != 0) {
    OutputDebugStringA(shaderError->GetStringPointer());
    assert(false);
  }

  ComPtr<IDxcBlob> shaderBlob;
  hr = shaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob),
                               nullptr);
  assert(SUCCEEDED(hr));

  return shaderBlob;
}

IDxcBlob *ShaderCompiler::CompileRaw(const std::wstring &path,
                                     const wchar_t *profile,
                                     const std::vector<LPCWSTR> &extraArgs) {
  auto blob = Compile(path, profile, extraArgs);
  blob->AddRef(); // 呼び出し側で Release する前提
  return blob.Get();
}