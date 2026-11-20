#pragma once
#include <any>
#include <string>

// シーンが「別のシーンへ切り替えたい」ときに使う窓口(インターフェース)。
//
// 分けた理由: シーン(BaseScene 側)が、シーン管理の実体(SceneManager)を直接知らなくて済むようにするため(依存性逆転)。
// シーンが必要とする機能だけを並べている(インターフェース分離)。遷移の設定を指定する切り替えや、
// 遷移なしの即時切り替えなど、シーンから使わないものは含めない。
// 実装は SceneManager が行う。
class ISceneChanger {
public:
  virtual ~ISceneChanger() = default;

  // シーンの切り替えを要求する。遷移の進行中や、すでに要求済みのときの呼び出しは無視される
  virtual void RequestChange(const std::string &nextSceneId) = 0;
  // 次のシーンへ渡す荷物つきで、切り替えを要求する。荷物の型はコピーできること。
  // 要求が無視されたときは、荷物も捨てられる
  virtual void RequestChange(const std::string &nextSceneId, std::any payload) = 0;

  // 画面遷移の進行中か(覆っている、保っている、晴れている途中)
  virtual bool IsTransitioning() const = 0;
  // 覆いが晴れる途中の進み具合(0-1)。遷移中でなければ 1
  virtual float GetRevealProgress() const = 0;
  // 覆いが晴れる区間の長さ(秒)
  virtual float GetRevealSeconds() const = 0;
};
