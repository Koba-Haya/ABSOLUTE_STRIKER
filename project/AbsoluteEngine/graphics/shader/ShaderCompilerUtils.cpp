#include "ShaderCompilerUtils.h"

#include "TextureUtils.h" // ConvertString が必要なら
#include "../../base/AssetPath.h"

#include <Windows.h>
#include <cassert>
#include <format>
#include <wrl.h>

static void LogA_(const std::string &message) {
  OutputDebugStringA(message.c_str());
}

static void LogW_(const std::wstring &message) {
  OutputDebugStringW(message.c_str());
}

static void LogHr_(const wchar_t *what, HRESULT hr) {
  LogW_(std::format(L"[DXC] {} failed. hr=0x{:08X}\n", what, (unsigned)hr));
}

IDxcBlob *CompileShader(const std::wstring &filePath, const wchar_t *profile,
                        IDxcUtils *dxcUtils, IDxcCompiler3 *dxcCompiler,
                        IDxcIncludeHandler *includeHandler) {
  using Microsoft::WRL::ComPtr;

  if (!dxcUtils || !dxcCompiler || !includeHandler) {
    LogW_(L"[DXC] CompileShader: null dxc interface\n");
    return nullptr;
  }

  LogW_(std::format(L"[DXC] Begin CompileShader, path:{}, profile:{}\n", filePath,
                    profile));

  // exe 基準の実パスに解決する(#include もこのパスを起点に解決される)
  const std::wstring resolvedPath =
      AbsoluteEngine::AssetPath::Resolve(filePath).wstring();

  ComPtr<IDxcBlobEncoding> shaderSource;
  HRESULT hr = dxcUtils->LoadFile(resolvedPath.c_str(), nullptr,
                                  shaderSource.GetAddressOf());
  if (FAILED(hr) || !shaderSource) {
    LogW_(std::format(L"[DXC] shader file not found: {}\n", resolvedPath));
    LogHr_(L"IDxcUtils::LoadFile", hr);
    return nullptr;
  }

  DxcBuffer shaderSourceBuffer{};
  shaderSourceBuffer.Ptr = shaderSource->GetBufferPointer();
  shaderSourceBuffer.Size = shaderSource->GetBufferSize();
  shaderSourceBuffer.Encoding = DXC_CP_UTF8;

  LPCWSTR arguments[] = {
      resolvedPath.c_str(), L"-E",  L"main", L"-T", profile, L"-Zi",
      L"-Qembed_debug", L"-Od", L"-Zpr",
  };

  ComPtr<IDxcResult> shaderResult;
  hr = dxcCompiler->Compile(&shaderSourceBuffer, arguments, _countof(arguments),
                            includeHandler,
                            IID_PPV_ARGS(shaderResult.GetAddressOf()));
  if (FAILED(hr) || !shaderResult) {
    LogHr_(L"IDxcCompiler3::Compile", hr);
    return nullptr;
  }

  // warnings/errors text
  ComPtr<IDxcBlobUtf8> shaderError;
  shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(shaderError.GetAddressOf()),
                          nullptr);
  if (shaderError && shaderError->GetStringLength() != 0) {
    LogA_(std::string("[DXC] ") + shaderError->GetStringPointer() + "\n");
  }

  HRESULT status = E_FAIL;
  hr = shaderResult->GetStatus(&status);
  if (FAILED(hr)) {
    LogHr_(L"IDxcResult::GetStatus", hr);
    return nullptr;
  }
  if (FAILED(status)) {
    LogHr_(L"Shader compilation (status)", status);
    LogW_(std::format(L"[DXC] Failed shader: {}\n", filePath));
    return nullptr;
  }

  ComPtr<IDxcBlob> shaderBlob;
  hr = shaderResult->GetOutput(DXC_OUT_OBJECT,
                               IID_PPV_ARGS(shaderBlob.GetAddressOf()),
                               nullptr);
  if (FAILED(hr) || !shaderBlob) {
    LogHr_(L"IDxcResult::GetOutput(DXC_OUT_OBJECT)", hr);
    return nullptr;
  }

  LogW_(std::format(L"[DXC] Compile Succeeded, path:{}, profile:{}\n", filePath,
                    profile));

  shaderBlob->AddRef(); // 生ポインタ返却（呼び出し側がReleaseする契約）
  return shaderBlob.Get();
}