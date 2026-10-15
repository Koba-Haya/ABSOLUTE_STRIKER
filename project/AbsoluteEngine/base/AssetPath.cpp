#include "AssetPath.h"
#include "EnginePath.h"

#include <Windows.h>

#include <mutex>
#include <string_view>
#include <unordered_set>

namespace AbsoluteEngine {

namespace {

// 出力フォルダ内の配置(Application.vcxproj のポストビルドで作られる構成)に対応する名前
constexpr std::wstring_view kResourcesPrefix = L"resources/";
constexpr std::wstring_view kAppDirName = L"app";
constexpr std::wstring_view kEngineDirName = L"engine";
constexpr std::wstring_view kAppRoot = L"resources/app";

// UTF-8 の string を wstring に変換する(既存の ConvertString と同じ前提)
std::wstring Utf8ToWide(const std::string &str) {
  if (str.empty()) {
    return {};
  }
  const int size = MultiByteToWideChar(CP_UTF8, 0, str.data(),
                                       static_cast<int>(str.size()), nullptr, 0);
  if (size <= 0) {
    return {};
  }
  std::wstring result(size, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()),
                      result.data(), size);
  return result;
}

// 先頭が prefix と一致するか(大文字小文字は区別しない。Windows のファイル名に合わせる)
bool StartsWithNoCase(std::wstring_view s, std::wstring_view prefix) {
  if (s.size() < prefix.size()) {
    return false;
  }
  return CompareStringOrdinal(s.data(), static_cast<int>(prefix.size()),
                              prefix.data(), static_cast<int>(prefix.size()),
                              TRUE) == CSTR_EQUAL;
}

// s が name そのもの、または "name/" で始まるか(例: "app" と "app/x.png")
bool IsUnderDir(std::wstring_view s, std::wstring_view name) {
  if (!StartsWithNoCase(s, name)) {
    return false;
  }
  return s.size() == name.size() || s[name.size()] == L'/';
}

// 旧形式("resources/" の直後に app/ や engine/ が無い表記)を見つけたとき、
// 同じパスについて一度だけデバッグ出力に警告する
void WarnLegacyPathOnce(const std::wstring &path) {
  static std::mutex mutex;
  static std::unordered_set<std::wstring> warned;
  std::lock_guard<std::mutex> lock(mutex);
  if (!warned.insert(path).second) {
    return;
  }
  const std::wstring msg =
      L"[AssetPath] warning: legacy path (no app/ or engine/): " + path + L"\n";
  OutputDebugStringW(msg.c_str());
}

} // namespace

const std::filesystem::path &AssetPath::GetExeDirectory() {
  static const std::filesystem::path dir = [] {
    // 長いパスにも対応するため、足りなければバッファを広げて取り直す
    std::wstring buf(MAX_PATH, L'\0');
    for (;;) {
      const DWORD len =
          GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
      if (len == 0) {
        // 取得できない場合は空にして、カレントディレクトリ基準の相対パスで動かす
        return std::filesystem::path{};
      }
      if (len < buf.size()) {
        buf.resize(len);
        break;
      }
      buf.resize(buf.size() * 2);
    }
    return std::filesystem::path(buf).parent_path();
  }();
  return dir;
}

std::filesystem::path AssetPath::Resolve(const std::string &assetPath) {
  return ResolveInternal(std::filesystem::path(Utf8ToWide(assetPath)));
}

std::filesystem::path AssetPath::Resolve(const std::wstring &assetPath) {
  return ResolveInternal(std::filesystem::path(assetPath));
}

std::string AssetPath::ResolveUtf8(const std::string &assetPath) {
  const std::wstring wide = Resolve(assetPath).wstring();
  if (wide.empty()) {
    return {};
  }
  const int size = WideCharToMultiByte(CP_UTF8, 0, wide.data(),
                                       static_cast<int>(wide.size()), nullptr, 0,
                                       nullptr, nullptr);
  if (size <= 0) {
    return {};
  }
  std::string result(size, '\0');
  WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()),
                      result.data(), size, nullptr, nullptr);
  return result;
}

std::string AssetPath::ResolveEditableData(const std::string &relativePath) {
#ifdef USE_IMGUI
  // エディタビルド: ソースツリーの resources 配下(EnginePath の基準)を直接読み書きする
  return EnginePath::Resolve("resources/" + relativePath);
#else
  // 配布ビルド: exe 基準の resources/app 配下(読み取り専用として扱う)
  return Resolve("resources/app/" + relativePath).string();
#endif
}

void AssetPath::ReportMissingAndExit(const std::string &what,
                                     const std::string &path) {
  const std::string msg = "Required data not found: " + what + "\n\nsearched:\n" +
                          path + "\n\nThe resources folder may be missing or incomplete.";
  MessageBoxA(nullptr, msg.c_str(), "Asset Fatal", MB_OK | MB_ICONERROR);
  ExitProcess(1);
}

std::filesystem::path
AssetPath::ResolveInternal(const std::filesystem::path &input) {
  if (input.empty()) {
    return input;
  }

  // 解決済みの絶対パスは触らない(二重に解決しないため)
  if (input.is_absolute()) {
    return input;
  }

  // 区切りを / に揃え、先頭の "./" と "/" を除いた相対表記にする
  std::wstring s = input.generic_wstring();
  while (s.rfind(L"./", 0) == 0) {
    s.erase(0, 2);
  }
  while (!s.empty() && s.front() == L'/') {
    s.erase(0, 1);
  }

  const std::filesystem::path &exeDir = GetExeDirectory();

  if (StartsWithNoCase(s, kResourcesPrefix)) {
    const std::wstring_view rest = std::wstring_view(s).substr(kResourcesPrefix.size());

    // 物理パス: resources/app/... , resources/engine/...
    if (IsUnderDir(rest, kAppDirName) || IsUnderDir(rest, kEngineDirName)) {
      return exeDir / s;
    }

    // 旧形式: resources/xxx -> resources/app/xxx として扱う(警告を出す)
    WarnLegacyPathOnce(s);
    return exeDir / std::filesystem::path(kAppRoot) / std::wstring(rest);
  }

  // 論理パス: resources/app からの相対
  return exeDir / std::filesystem::path(kAppRoot) / s;
}

} // namespace AbsoluteEngine
