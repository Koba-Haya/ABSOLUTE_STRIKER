#pragma once
#include <filesystem>
#include <string>

namespace AbsoluteEngine {

/// 実行時にアセット(テクスチャ・モデル・シェーダ)を読み込むためのパス解決クラス。
/// 「exe があるフォルダ」を基準にするので、zip を展開した先や起動方法(ショートカット等)に
/// 左右されない。ビルドしたPCのフォルダ(絶対パス)には一切フォールバックしない。
///
/// 受け付けるパスの形式:
///   1. 論理パス       "noise/noise0.png"            -> <exe>/resources/app/noise/noise0.png
///   2. 物理パス       "resources/app/..." と "resources/engine/..."  -> <exe>/ そのまま
///   3. 旧形式(移行用) "resources/noise/noise0.png"  -> <exe>/resources/app/noise/noise0.png
///      (app/ が抜けた古い表記。動作はするが、デバッグ出力に警告を出す。表記統一後に廃止する)
///   4. 絶対パス       そのまま返す(呼び出し側が解決済みの場合の再解決を防ぐため)
class AssetPath {
public:
  /// exe があるフォルダを返す(初回に取得してキャッシュする)。
  static const std::filesystem::path &GetExeDirectory();

  /// アセットのパスを、exe 基準の実パスに解決する。
  /// ファイルの存在確認はしない(存在しない場合の扱いは呼び出し側が決める)。
  /// @param assetPath 上記のいずれかの形式のパス(区切りは / と \ のどちらでも可)
  /// @return exe 基準の実パス
  static std::filesystem::path Resolve(const std::string &assetPath);

  /// Resolve() の wstring 版(シェーダなど、ワイド文字列で扱う呼び出し元用)。
  static std::filesystem::path Resolve(const std::wstring &assetPath);

  /// Resolve() の結果を UTF-8 の std::string で返す。
  /// Assimp のように char* を UTF-8 として扱うライブラリ用。
  /// path.string() は ANSI(日本語環境では CP932)になるため、日本語を含むフォルダでは
  /// そのまま渡すと開けない。
  static std::string ResolveUtf8(const std::string &assetPath);

  /// シーンJSON・タイムライン・プレハブなど「エディタで編集するデータ」のパスを解決する。
  ///   エディタビルド(USE_IMGUI): ソースツリー側(EnginePath基準)。編集結果がそのまま読み書きされる
  ///   それ以外(Release)       : exe 基準の resources/app 配下。ビルドPCのフォルダには依存しない
  /// 書き込みはエディタビルドでのみ行うこと(Release でこのパスへ書き込まない)。
  /// @param relativePath resources/app からの相対パス(例: "editor/scenes/Game.json", "prefabs")
  /// @return 解決済みのパス(存在確認はしない)
  static std::string ResolveEditableData(const std::string &relativePath);

  /// 必須データが見つからないときに、探した場所を表示してアプリを終了する。
  /// 空のシーンで黙って進行すると原因が分からなくなるため、配布ビルドではこれで止める。
  /// @param what 何が見つからないか(例: "scene file")
  /// @param path 探したパス
  [[noreturn]] static void ReportMissingAndExit(const std::string &what,
                                                const std::string &path);

private:
  /// 変換結果をキャッシュせず純粋に計算する本体。入力は UTF-8 の string 想定。
  static std::filesystem::path ResolveInternal(const std::filesystem::path &input);
};

} // namespace AbsoluteEngine
