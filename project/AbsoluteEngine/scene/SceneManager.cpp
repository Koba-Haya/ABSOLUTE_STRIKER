#include "SceneManager.h"

#include <cassert>

#include "BaseScene.h"
#include "ISceneFactory.h"
#include "Renderer.h"
#include "CpuTimer.h"

namespace {
// 遷移の時間を進める 1 フレームの長さ。シーン側(BaseScene)の固定 deltaTime と同じ 1/60 秒。
// 実時間でなく固定値で進めるので、シーンの重い初期化で実時間が飛んでも、遷移の進行は飛ばない
constexpr float kFrameDeltaSeconds = 1.0f / 60.0f;
} // namespace

SceneManager::~SceneManager() = default;

void SceneManager::Initialize(const SceneServices &services) {
  services_ = services;
}

void SceneManager::SetFactory(std::unique_ptr<ISceneFactory> factory) {
  factory_ = std::move(factory);
}

void SceneManager::Start(const std::string &firstSceneId) {
  SetNextSceneById_(firstSceneId);
  ApplySceneChangeIfNeeded();
}

void SceneManager::RequestChange(const std::string &nextSceneId) {
  // 遷移の進行中や要求済みのときは無視する(シーン側が毎フレーム呼んでも、二重に始まらない)
  if (transition_.IsActive() || hasPendingChange_) {
    return;
  }
  std::optional<TransitionSpec> spec;
  if (current_ && transitionResolver_) {
    spec = transitionResolver_(currentSceneId_, nextSceneId);
  }
  RegisterChange_(nextSceneId, spec);
}

void SceneManager::RequestChange(const std::string &nextSceneId, const TransitionSpec &spec) {
  if (transition_.IsActive() || hasPendingChange_) {
    return;
  }
  RegisterChange_(nextSceneId, current_ ? std::optional<TransitionSpec>(spec) : std::nullopt);
}

void SceneManager::RequestChangeImmediate(const std::string &nextSceneId) {
  transition_.Cancel();
  RegisterChange_(nextSceneId, std::nullopt);
}

void SceneManager::RegisterChange_(const std::string &nextSceneId, const std::optional<TransitionSpec> &spec) {
  hasPendingChange_ = true;
  pendingSceneId_ = nextSceneId;
  if (spec) {
    // 画面を覆い切るまで、入れ替えは待つ
    switchReady_ = false;
    transition_.Begin(*spec);
    // 覆う時間が 0 の遷移は、始めた時点ですでに覆い切っているので、すぐ入れ替えてよい
    if (transition_.ConsumeSwitchRequest()) {
      switchReady_ = true;
    }
  } else {
    switchReady_ = true;
  }
}

void SceneManager::SetNextSceneById_(const std::string &sceneId) {
  assert(factory_ && "SceneManager: factory is null. Call SetFactory().");
  next_ = factory_->Create(sceneId, services_);
  assert(next_ && "SceneManager: factory returned null. Unknown sceneId?");
  next_->SetSceneId(sceneId);
  nextSceneId_ = sceneId;
}

void SceneManager::ApplySceneChangeIfNeeded() {
  // 画面遷移の覆いが完全になるまでは、要求があっても入れ替えない
  if (hasPendingChange_ && !switchReady_) {
    return;
  }

  if (hasPendingChange_) {
    SetNextSceneById_(pendingSceneId_);
    hasPendingChange_ = false;
    pendingSceneId_.clear();
    switchReady_ = false;
  }

  if (!next_) {
    return;
  }

  // 遷移時の停止の調査用: 旧シーンの後始末と、新シーンの生成・初期化の時間を測る
  if (current_) {
    CpuTimer::Scope scope("SceneManager.OldScene.Finalize");
    current_->Finalize();
    current_.reset();
  }

  CpuTimer::Scope initScope("SceneManager.NewScene.Initialize(total)");
  current_ = std::move(next_);
  currentSceneId_ = nextSceneId_;
  current_->SetSceneManager(this);
  current_->Initialize(services_);
}

void SceneManager::Update() {
  if (current_) {
    current_->Update();
  }

  // 遷移の時間を進める。覆い切った(入れ替えてよい)フレームは、直後の ApplySceneChangeIfNeeded で入れ替わる
  transition_.Update(kFrameDeltaSeconds);
  if (transition_.ConsumeSwitchRequest()) {
    switchReady_ = true;
  }
}

void SceneManager::Draw() {
  // シーンの描画(最終パス)より前に、今フレームの覆い具合を Renderer へ渡す
  auto *renderer = Renderer::GetInstance();
  if (transition_.IsActive()) {
    renderer->SetTransition(transition_.GetStyle(), transition_.GetCover(), transition_.GetElapsedSeconds(),
                            transition_.GetTotalSeconds());
  } else {
    renderer->ClearTransition();
  }

  if (current_) {
    current_->Draw();
  }
}

void SceneManager::DrawEditorUI() {
  if (current_) {
    current_->DrawEditorUI();
  }
}

void SceneManager::Finalize() {
  if (current_) {
    current_->Finalize();
    current_.reset();
  }
  if (next_) {
    next_->Finalize();
    next_.reset();
  }
  factory_.reset();
}
