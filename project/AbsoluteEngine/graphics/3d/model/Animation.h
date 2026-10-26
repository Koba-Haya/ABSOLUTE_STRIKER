#pragma once

#include <vector>
#include <map>
#include <string>
#include <memory>
#include "Vector.h"
#include "Method.h"

template <typename tValue>
struct Keyframe {
  float time;
  tValue value;
};

using KeyframeVector3 = Keyframe<Vector3>;
using KeyframeQuaternion = Keyframe<Quaternion>;

template <typename tValue>
struct AnimationCurve {
  std::vector<Keyframe<tValue>> keyframes;
};

struct NodeAnimation {
  AnimationCurve<Vector3> translate;
  AnimationCurve<Quaternion> rotate;
  AnimationCurve<Vector3> scale;
};

struct Animation {
  std::string name; // アニメーション名(ファイル内の名前。名前の無いファイルでは空)
  float duration; // アニメーション全体の尺（単位は秒）
  // NodeAnimationの集合。Node名で引けるようにしておく
  std::map<std::string, NodeAnimation> nodeAnimations;
};

// 1 つのファイルに入っているアニメーションの集合。名前で引ける。
// 読み込み元(AnimationManager)に依存しない単純な構造にしてあり、将来 ModelData などへそのまま持たせられる。
struct AnimationSet {
  // ファイル内の並び順。先頭 = 名前を指定しないときの既定のアニメーション
  std::vector<std::shared_ptr<Animation>> animations;

  // 先頭のアニメーションを返す(無ければ nullptr)
  std::shared_ptr<Animation> First() const {
    return animations.empty() ? nullptr : animations.front();
  }

  // 名前でアニメーションを探す(見つからなければ nullptr)
  std::shared_ptr<Animation> Find(const std::string& name) const {
    for (const auto& anim : animations) {
      if (anim->name == name) return anim;
    }
    return nullptr;
  }
};

