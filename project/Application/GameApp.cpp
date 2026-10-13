#define NOMINMAX
#include "GameApp.h"

#include "AbsoluteEngine/base/EnginePath.h"
#include "Logger.h"
#include "Renderer.h"
#include "SceneFactory.h"
#include "SceneIds.h"
#include "SceneManager.h"

#ifndef APPLICATION_ROOT_DIR
// Application.vcxprojでビルドしなかった場合のフォールバック（本来は常にプロジェクトの
// ビルド設定で定義される。マシンやクローン場所に依存しないよう、ユーザー名等は埋め込まない）
#define APPLICATION_ROOT_DIR "Application/"
#endif

#ifdef USE_IMGUI
#include <imgui.h>
#include <imgui_internal.h> // DockBuilder API のために必要
#endif

GameApp::GameApp() = default;
GameApp::~GameApp() = default;

void GameApp::Initialize() {
  // マシンやクローン先のパスに関わらずリソースを解決できるよう、ビルド時のプロジェクトパスで初期化する
  AbsoluteEngine::EnginePath::SetApplicationRoot(APPLICATION_ROOT_DIR);

  Logger::GetInstance().Initialize();

  sceneManager_ = std::make_unique<SceneManager>();

  SceneServices services{};
  services.input = &GetInput();
  services.audio = &GetAudio();
#ifdef USE_IMGUI
  services.imgui = &GetImGui();
#endif
  services.framework = this;

  Renderer::GetInstance()->Initialize(&GetDX());
  sceneManager_->Initialize(services);
  sceneManager_->SetFactory(std::make_unique<SceneFactory>());
  sceneManager_->Start(SceneId::Title);
}

void GameApp::Finalize() {
  Logger::GetInstance().Finalize();

  if (sceneManager_) {
    sceneManager_->Finalize();
    sceneManager_.reset();
  }
}

void GameApp::Update() {
  if (!sceneManager_) {
    return;
  }

#ifdef USE_IMGUI
  // --- フルスクリーン DockSpace ウィンドウ ---
  // メインビューポート全体を覆う透明な "ホスト" ウィンドウを作り、
  // その中に DockSpace を張る。これにより全パネルがドッキング可能になる。
  {
    ImGuiViewport *mainViewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(mainViewport->WorkPos);
    ImGui::SetNextWindowSize(mainViewport->WorkSize);
    ImGui::SetNextWindowViewport(mainViewport->ID);

    ImGuiWindowFlags hostFlags =
        ImGuiWindowFlags_NoDocking | // このウィンドウ自体はドッキングしない
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
        ImGuiWindowFlags_NoBackground; // 背景透過（ゲーム画面を隠さない）

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    ImGui::Begin("##DockSpaceHost", nullptr, hostFlags);
    ImGui::PopStyleVar(3);

    ImGuiID dockspaceId = ImGui::GetID("MainDockSpace");

    // --- 初期レイアウトの構築 ---
    // imgui.ini にレイアウトが保存済みの場合は DockBuilderGetNode が非 null
    // を返す。 その場合は ini から自動復元されるため DockBuilder を実行しない。
    // ini がない（= 初回起動）場合のみデフォルトレイアウトを構築する。
    if (!dockBuilt_) {
      dockBuilt_ = true;

      if (ImGui::DockBuilderGetNode(dockspaceId) == nullptr) {
        // ini データなし → デフォルトレイアウトを構築
        ImGui::DockBuilderRemoveNode(dockspaceId);
        ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspaceId, mainViewport->WorkSize);

        // 左(20%=Hierarchy) | 中央右(80%)
        ImGuiID leftId, centerRightId;
        ImGui::DockBuilderSplitNode(dockspaceId, ImGuiDir_Left, 0.20f, &leftId,
                                    &centerRightId);

        // 中央右 → 中央(75%) | 右(25%=Inspector)
        ImGuiID rightId, centerId;
        ImGui::DockBuilderSplitNode(centerRightId, ImGuiDir_Right, 0.25f,
                                    &rightId, &centerId);

        // 中央 → 上(75%=Viewport) | 下(25%=Assets/Console)
        ImGuiID bottomId, centerTopId;
        ImGui::DockBuilderSplitNode(centerId, ImGuiDir_Down, 0.25f, &bottomId,
                                    &centerTopId);

        // 各ウィンドウを対応ノードにドック
        ImGui::DockBuilderDockWindow("Viewport##GameView",
                                     centerTopId);         // 中央上：ゲーム画面
        ImGui::DockBuilderDockWindow("Hierarchy", leftId); // 左：ヒエラルキー
        ImGui::DockBuilderDockWindow("Inspector",
                                     rightId); // 右：インスペクタ（プロパティ）
        ImGui::DockBuilderDockWindow("Assets", bottomId);  // 下：アセット
        ImGui::DockBuilderDockWindow("Console", bottomId); // 下：コンソール
        ImGui::DockBuilderDockWindow("DebugMenu",
                                     bottomId); // 下：デバッグメニュー

        ImGui::DockBuilderFinish(dockspaceId);
      }
      // ini データあり → DockBuilder をスキップして ini から自動復元
    }

    ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f),
                     ImGuiDockNodeFlags_PassthruCentralNode);

    ImGui::End();
  }

  // --- デバッグメニュー ---
  ImGui::Begin("DebugMenu");
  if (ImGui::Button("Go to TitleScene")) {
    sceneManager_->RequestChange(SceneId::Title);
  }
  ImGui::SameLine();
  if (ImGui::Button("Go to GameScene")) {
    sceneManager_->RequestChange(SceneId::Game);
  }
  ImGui::SameLine();
  if (ImGui::Button("Go to DevScene")) {
    sceneManager_->RequestChange(SceneId::Dev);
  }
  ImGui::End();

  if (sceneManager_) {
    sceneManager_->DrawEditorUI();
  }
#endif

  sceneManager_->Update();
  sceneManager_->ApplySceneChangeIfNeeded();
}

void GameApp::Draw() {
  auto *cmdList = GetCmdList();

  if (sceneManager_) {
    sceneManager_->Draw();
  }
}
