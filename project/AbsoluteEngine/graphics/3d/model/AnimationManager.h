#pragma once

#include <string>
#include <map>
#include <memory>
#include "Animation.h"

// アニメーションファイル(gltf など)の読み込みとキャッシュを担当する。
// 1 つのファイルに複数のアニメーションが入っていてもよい(名前で選べる)。
class AnimationManager {
public:
  static AnimationManager* GetInstance() {
    static AnimationManager inst;
    return &inst;
  }

  // ファイル内の全アニメーションを読み込み、集合として返す。ファイルが読めない、またはアニメーションが無いときは nullptr。
  // 同じファイルは 1 回だけ読み、以降はキャッシュを返す。
  std::shared_ptr<const AnimationSet> LoadAnimationSet(const std::string& directoryPath, const std::string& filename);

  // ファイルの先頭のアニメーションを返す(従来の呼び出し方。名前を指定しないときはこれを使う)
  std::shared_ptr<Animation> LoadAnimation(const std::string& directoryPath, const std::string& filename);

  // 名前を指定してアニメーションを返す(無ければ nullptr)
  std::shared_ptr<Animation> LoadAnimation(const std::string& directoryPath, const std::string& filename,
                                           const std::string& animationName);

  void Clear() { cache_.clear(); }

private:
  AnimationManager() = default;

  std::map<std::string, std::shared_ptr<const AnimationSet>> cache_;
};
