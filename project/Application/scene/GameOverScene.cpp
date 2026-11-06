#include "GameOverScene.h"
#include "ExhibitionMode.h"
#include "GameOverAshDebugUi.h"
#include <algorithm>
#include "Input.h"
#include "GPUParticleManager.h"
#include "Renderer.h"
#include "SceneIds.h"
#include "SceneManager.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {
// 背景の時間を進める 1 フレームの長さ
constexpr float kFrameDeltaSeconds = 1.0f / 60.0f;
// 見出しの画像と、操作の案内の画像
constexpr const char *kBannerTexturePath = "resources/app/result/gameOver.png";
constexpr const char *kPromptTexturePath = "resources/app/result/gameOver_prompt.png";

// 見出しと案内の中心の高さ(画面の高さに対する割合。0 が上端)。降下の終点で、画面の中央から下の広い範囲に巣(と、集まる灰)が見えるよう、
// 見出しは上寄り、案内は下端寄りにする
constexpr float kBannerCenterYRatio = 0.15f;
constexpr float kPromptCenterYRatio = 0.90f;
// 見出しの配置と、現れる区間(遷移の晴れる進み具合 0〜1。雲が薄くなって見通しが開き始めた頃に、先に見出しが現れる)
constexpr ResultBanner::Layout kBannerLayout{720.0f, 180.0f, kBannerCenterYRatio, 0.25f, 0.65f};
// 案内の配置(元画像は 4:1)と、現れる区間(見出しのあとに、見通しがほぼ開いた頃に現れる)
constexpr ResultBanner::Layout kPromptLayout{600.0f, 150.0f, kPromptCenterYRatio, 0.60f, 0.95f};

#ifdef USE_IMGUI
// デバッグ窓の位置と、ボタンの大きさ
constexpr float kDebugWindowX = 10.0f;
constexpr float kDebugWindowY = 10.0f;
constexpr float kDebugButtonWidth = 80.0f;
constexpr float kDebugButtonHeight = 28.0f;
#endif
} // namespace

void GameOverScene::Initialize(const SceneServices &services) {
  BaseScene::Initialize(services);
  backdrop_.Initialize();
  banner_.Initialize(kBannerTexturePath, kBannerLayout);
  prompt_.Initialize(kPromptTexturePath, kPromptLayout);
  butterfly_.Initialize();
  ash_.Initialize(backdrop_.GetNestCenter()); // 灰が集まる先は、巣の島のくぼみの中心
}

void GameOverScene::Finalize() {
  GPUParticleManager::GetInstance()->SetTimeScale(1.0f); // ヒットストップで下げた粒の時間の速さを戻す
  ash_.Shutdown(); // 借りているエミッタ・粒の力・点光源を返す
}

void GameOverScene::Update() {
  // 遷移の覆いが晴れる進み具合(遷移が無ければ 1)。背景の雲の濃さとカメラの向き、見出しと案内の現れ方が、これに合わせて変わる
  const float revealProgress = GetTransitionRevealProgress();
  UpdateRebirth_(); // 復活の演出の指示を先に反映する(カメラと雲の上書きは、背景の Update で使われる)
  // 世界(雲、島、灰)の時間の速さ。復活のバーストのヒットストップで下げる(通常は 1)
  const float worldDelta = kFrameDeltaSeconds * worldTimeScale_;
  backdrop_.Update(worldDelta, revealProgress);
  // 灰の流れ: 降下が始まったら巣へ集まり始める(降下の経過秒で台本が進む)
  ash_.Update(worldDelta, backdrop_.GetSettledSeconds());

  // 見出しと案内は、雲が薄くなって見通しが開くのに合わせて現れる
  banner_.SetRevealProgress(revealProgress);
  prompt_.SetRevealProgress(revealProgress);

  if (ExhibitionMode::kEnabled) {
    UpdateExhibition_();
  } else {
    UpdateInput_();
  }

#ifdef USE_IMGUI
  if (!ExhibitionMode::kEnabled) {
    DrawDebugWindow_();
    backdrop_.DrawDebugUi();
    if (rebirth_.DrawDebugUi()) {
      BeginRebirth_();
    }
    // 蝶の確認表示(読み込み、位置、ディゾルブ、描画した回数)を、Rebirth の窓へ足す
    ImGui::Begin("Rebirth");
    butterfly_.DrawDebugUi();
    ImGui::End();
    GameOverAshDebugUi::Draw(ash_);
  }
#endif
}

void GameOverScene::UpdateExhibition_() {
  // 遷移の覆いが晴れてから、カメラの降下が終わって少したったら、ゲームへ戻る(タイトルは通らない)
  if (IsSceneTransitioning() || rebirthStarted_) return;
  exhibitionHoldTimer_ += kFrameDeltaSeconds;
  if (exhibitionHoldTimer_ >= backdrop_.GetDescentSeconds() + ExhibitionMode::kGameOverHoldMarginSec) {
    BeginRebirth_(); // 展示も、復活の演出を通ってゲームへ戻る
  }
}

void GameOverScene::UpdateInput_() {
  // 遷移の最中は受け付けない(死ぬ瞬間に押していたボタンで、すぐに抜けてしまうのを防ぐ)
  if (!GetInput() || IsSceneTransitioning() || rebirthStarted_) return;
  if (GetInput()->TriggerKey(DIK_SPACE) || GetInput()->TriggerKey(DIK_RETURN) || GetInput()->WasPadPressed(XINPUT_GAMEPAD_A)) {
    RequestSceneChange(SceneId::Title);
  }
  if (GetInput()->TriggerKey(DIK_R) || GetInput()->WasPadPressed(XINPUT_GAMEPAD_B)) {
    BeginRebirth_(); // リトライは、復活の演出を通ってゲームへ戻る(タイトルは通らない)
  }
}

void GameOverScene::BeginRebirth_() {
  if (rebirthStarted_ || IsSceneTransitioning()) return;
  rebirthStarted_ = true;
  // 始まりの姿勢: 核の位置(蝶が現れる所)、巣の中心、今のカメラ(降下の終点)
  RebirthStart start;
  start.corePosition = ash_.GetCorePosition();
  start.nestCenter = backdrop_.GetNestCenter();
  start.cameraEye = backdrop_.GetCameraEye();
  start.cameraTarget = backdrop_.GetCameraTarget();
  rebirth_.Begin(start);
  // 灰がまだ漂っているだけなら、集まりを始める(集まっていれば、速さを上げる指示で一気に集まる)
  if (ash_.GetPhase() == GameOverAshPhase::Drifting) {
    ash_.SetPhase(GameOverAshPhase::Gathering);
  }
}

void GameOverScene::UpdateRebirth_() {
  if (!rebirthStarted_) return;
  rebirth_.Update(kFrameDeltaSeconds);
  const RebirthState &state = rebirth_.GetState();

  // 世界の時間の速さ(ヒットストップ)。灰、島、雲の Update には worldDelta として渡り、粒の更新には GPUParticleManager の時間の速さで渡る
  worldTimeScale_ = state.timeScale;
  GPUParticleManager::GetInstance()->SetTimeScale(state.timeScale);

  // 灰と核
  ash_.SetGatherSpeedScale(state.gatherSpeedScale);
  ash_.SetConvergeScale(state.convergeScale);
  ash_.SetCoreBoost(state.coreBoost);
  ash_.SetCoreSizeScale(state.coreSizeScale);
  // 見出しと案内は消える
  banner_.SetOpacityScale(state.uiOpacity);
  prompt_.SetOpacityScale(state.uiOpacity);
  // カメラ(バーストの寄りを含む)と雲
  if (state.cameraOverride) {
    backdrop_.SetCameraOverride(state.cameraEye, state.cameraTarget);
  }
  backdrop_.SetFovScale(state.fovScale);
  backdrop_.SetCloudFill(state.cloudFill);
  // 蝶
  butterfly_.Update(kFrameDeltaSeconds, state);
  // 核と灰の集まる先を、蝶の胴体へ追従させる(蝶が見えない、またはボーンが無い間は、巣のまま)
  Vector3 bodyPosition;
  const bool hasBody = butterfly_.GetBodyPosition(bodyPosition);
  ash_.SetCoreFollow(bodyPosition, hasBody ? state.coreFollowBlend : 0.0f);
  ash_.SetSpawnScale(state.ashSpawnScale);
  // バースト: 蝶の胴体から、再生の色(金から青緑、青)の鱗粉を放射状に。巣の地面から、灰と土を外側へ
  if (rebirth_.ConsumeBurst()) {
    const RebirthSettings &rs = rebirth_.GetSettings();
    if (hasBody) {
      GameOverAshBurst burst;
      burst.count = static_cast<uint32_t>((std::max)(rs.burstCount, 0.0f));
      burst.radius = rs.burstRadius;
      burst.speedMin = rs.burstSpeedMin;
      burst.speedMax = (std::max)(rs.burstSpeedMax, rs.burstSpeedMin);
      burst.lifeMin = rs.burstLifeMin;
      burst.lifeMax = (std::max)(rs.burstLifeMax, rs.burstLifeMin);
      burst.sizeMin = rs.burstSizeMin;
      burst.sizeMax = (std::max)(rs.burstSizeMax, rs.burstSizeMin);
      burst.startColor = rs.burstGold;
      burst.endColor = rs.burstTeal;
      burst.upward = rs.burstUpward;
      ash_.ReleaseBurst(bodyPosition, burst);
    }
    GameOverAshGroundBlast ground;
    ground.count = static_cast<uint32_t>((std::max)(rs.groundCount, 0.0f));
    ground.speedMin = rs.groundSpeedMin;
    ground.speedMax = (std::max)(rs.groundSpeedMax, rs.groundSpeedMin);
    ground.upward = rs.groundUpward;
    ash_.ReleaseGroundBlast(backdrop_.GetNestCenter(), ground);
  }
  // 雲の層を抜ける間に、ゲームへ切り替える(雲抜けの遷移が、これをつなぐ)
  if (rebirth_.ConsumeSceneChange()) {
    RequestSceneChange(SceneId::Game);
  }
}

#ifdef USE_IMGUI
void GameOverScene::DrawDebugWindow_() {
  ImGui::SetNextWindowPos(ImVec2(kDebugWindowX, kDebugWindowY), ImGuiCond_Always);
  ImGui::Begin("Game Over Debug", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove);
  if (ImGui::Button("Title", ImVec2(kDebugButtonWidth, kDebugButtonHeight))) {
    RequestSceneChange(SceneId::Title);
  }
  ImGui::SameLine();
  if (ImGui::Button("Retry", ImVec2(kDebugButtonWidth, kDebugButtonHeight))) {
    BeginRebirth_();
  }
  ImGui::End();
}
#endif

void GameOverScene::Draw() {
  backdrop_.Begin();
  // 復活する蝶。島のあと(同じ光)・灰のまえ。深度を書いて描くので、雲のもやでかすみ、島や灰と前後が正しく重なる
  butterfly_.Draw();
  // 灰と核の光。島を描いたあと・雲の合成の前に、深度を書いて描く(雲のもやでかすみ、島と前後が正しく重なる)
  Renderer::GetInstance()->DrawGPUParticles(BlendMode::Alpha, true);
  // 雲を先に合成し、その上に見出しと案内を描く(逆だと、雲が見出しを隠す)
  backdrop_.ResolveClouds();
  // 灰と核の発光(雲のあとに加算で描く。雲のもやで色が薄まらず、見出しの下に描く)
  ash_.DrawGlow();
  banner_.Draw();
  // 展示中は操作を受け付けないので、案内は出さない
  if (!ExhibitionMode::kEnabled) {
    prompt_.Draw();
  }
  backdrop_.End();
}
