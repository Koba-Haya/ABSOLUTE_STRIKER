#pragma once
#include <d3d12.h>
#include <functional>
#include <memory>
#include <optional>
#include <string>

#include "SceneServices.h"
#include "BaseScene.h"
#include "ISceneFactory.h"
#include "SceneTransition.h"

class SceneManager {
public:
  // (今のシーンの ID, 次のシーンの ID) から、使う遷移の設定を返す関数。
  // std::nullopt を返した組み合わせは、遷移なしで即座に切り替わる。
  // エンジン側はシーン ID の意味を知らないので、組み合わせの表はアプリ側が持ち、ここへ渡す。
  using TransitionResolver =
      std::function<std::optional<TransitionSpec>(const std::string &fromSceneId, const std::string &toSceneId)>;

  SceneManager() = default;
  ~SceneManager();

  void Initialize(const SceneServices &services);
  void Finalize();

  void Update();
  void Draw();
  void DrawEditorUI();

  void SetFactory(std::unique_ptr<ISceneFactory> factory);
  void SetTransitionResolver(TransitionResolver resolver) { transitionResolver_ = std::move(resolver); }
  void Start(const std::string &firstSceneId);

  // シーンの切り替えを要求する。組み合わせの表(TransitionResolver)に遷移があれば、覆ってから入れ替える。
  // 遷移の進行中や、すでに要求済みのときの呼び出しは無視する(シーン側が毎フレーム呼んでも、二重に始まらない)
  void RequestChange(const std::string &nextSceneId);
  // 遷移の設定を指定して切り替える(デバッグ用など)
  void RequestChange(const std::string &nextSceneId, const TransitionSpec &spec);
  // 遷移なしで、すぐに切り替える。進行中の遷移があれば中断する
  void RequestChangeImmediate(const std::string &nextSceneId);
  void ApplySceneChangeIfNeeded();

  // 今のシーンの ID(Start 前は空)
  const std::string &GetCurrentSceneId() const { return currentSceneId_; }
  // 画面遷移の進行中か(覆っている、保っている、晴れている途中)
  bool IsTransitioning() const { return transition_.IsActive(); }
  // 新しいシーンが表示され始め、覆いが晴れている途中か
  bool IsRevealing() const { return transition_.IsRevealing(); }
  // 晴れる途中の進み具合(0 = 晴れ始める前、1 = 完全に晴れた。時間に比例する。遷移中でなければ 1)
  float GetRevealProgress() const { return transition_.GetRevealProgress(); }
  // 晴れる区間の長さ(秒)
  float GetRevealSeconds() const { return transition_.GetInSeconds(); }

  const SceneServices &GetServices() const { return services_; }

private:
  void SetNextSceneById_(const std::string &sceneId);
  // 切り替えの要求を登録する。spec があれば遷移を始め、なければ即座に切り替える状態にする
  void RegisterChange_(const std::string &nextSceneId, const std::optional<TransitionSpec> &spec);

private:
  SceneServices services_{};
  std::unique_ptr<ISceneFactory> factory_;
  std::unique_ptr<BaseScene> current_;
  std::unique_ptr<BaseScene> next_;
  std::string currentSceneId_;
  std::string nextSceneId_;
  bool hasPendingChange_ = false;
  std::string pendingSceneId_;

  // 画面遷移
  TransitionResolver transitionResolver_;
  SceneTransition transition_;
  bool switchReady_ = false; // 画面が完全に覆われ、シーンを入れ替えてよいか
};
