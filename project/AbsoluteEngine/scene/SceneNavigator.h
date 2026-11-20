#pragma once
#include <any>
#include <string>

#include "ISceneChanger.h"

// シーンの「次にどこへ行くか」と「前のシーンから何を受け取ったか」を担当するクラス。
//
// 分けた理由: BaseScene が、シーンの骨格とシーン切り替えの窓口を両方持っていて責務が混ざっていたため、
// 切り替えと荷物(ペイロード)だけを切り出した(単一責任)。
// 切り替えの実体は ISceneChanger(SceneManager が実装)に任せ、このクラスは具体的な SceneManager を知らない。
class SceneNavigator {
public:
  // 切り替えの窓口を設定する(所有しない)。nullptr の間は、切り替え要求は何もしない
  void SetChanger(ISceneChanger *changer) { changer_ = changer; }

  // 前のシーンから渡された荷物を受け取る。荷物なしの切り替えでは、空の std::any が渡る
  void SetPayload(std::any payload) { payload_ = std::move(payload); }

  // 荷物なしで、切り替えを要求する
  void RequestChange(const std::string &sceneId);
  // 荷物つきで、切り替えを要求する。荷物の型はコピーできること
  void RequestChange(const std::string &sceneId, std::any payload);
  // 受け取った荷物を、そのまま次のシーンへ引き継いで、切り替えを要求する。
  // 荷物を受け取っていなければ、荷物なしの切り替えと同じ。
  // 荷物は、コピーして渡す(このシーンが、遷移の間も自分の荷物を読めるように、手元にも残す)
  void RequestChangeKeepingPayload(const std::string &sceneId);

  // 前のシーンから渡された荷物を、型 T で取り出す。荷物が無い、または型が違うときは nullptr。
  // 返すポインタは、このナビゲータが生きている間だけ有効
  template <typename T>
  const T *TryGetPayload() const {
    return std::any_cast<T>(&payload_);
  }

  // 画面遷移の進行中か(窓口が未設定なら false)
  bool IsTransitioning() const;
  // 覆いが晴れる途中の進み具合(0-1)。遷移中でない、または窓口が未設定なら 1
  float GetRevealProgress() const;
  // 覆いが晴れる区間の長さ(秒)。窓口が未設定なら 0
  float GetRevealSeconds() const;

private:
  ISceneChanger *changer_ = nullptr; // 所有しない
  std::any payload_;                 // 前のシーンから渡された荷物
};
