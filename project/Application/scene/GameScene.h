#pragma once
#include "BaseScene.h"
#include "GameCamera.h"
#include "Skybox.h"
#include "AbsoluteEngine/editor/EditorCamera.h"
#include "ModelInstance.h"
#include "ModelResource.h"
#include "Renderer.h"
#include "../actor/Player/PlayerComponent.h"
#include "../actor/Bullet/BulletComponent.h"
#include "../actor/Enemy/EnemyShootComponent.h"
#include "../hud/GameHUD.h"
#include "AbsoluteEngine/resources/AssetManager.h"
#include "TextureResource.h"
#include "Animation.h"
#include "HitEffectSpawner.h"
#include "PostEffectDirector.h"
#include "PlayerDeathDirector.h"
#include "DeathCameraDirector.h"
#include "../actor/Player/WingShatter.h"
#include "ScoreTracker.h"
#include "StageStartDirector.h"
#include "StageNameBoard.h"
#include "StageImpactEffects.h"
#include "SpeedStreaks.h"
#include <memory>
#include <vector>
#include <string>

class RailCameraComponent;

enum class GamePhase {
  InProgress,
  Boss,
  Dying,    // 自機の HP が 0 になって、死亡演出(PlayerDeathDirector)を進めている間
  Clear,
  GameOver  // 死亡演出が Cover へ入り、ゲームオーバーへの遷移を要求した(または、クリア・ゲームオーバーの要求を出した)
};

class GameScene final : public BaseScene {
public:
  GameScene() = default;
  ~GameScene() override = default;

  void Initialize(const SceneServices &services) override;
  void Finalize() override;
  void Update() override;
  void Draw() override;
  void DrawEditorUI() override;

  // AddRootObject をオーバーライドして EnemyComponent へのコールバックを自動注入する（タスクC）
  // タイムライン経由でスポーンされた敵にも onDestroyed が登録されるようになる
  void AddRootObject(std::shared_ptr<AbsoluteEngine::GameObject> obj) override;

protected:
  // Debug Camera トグルがONの間だけ editorCamera_ の入力操作を許可する（タスクD）
  bool IsEditorCameraOperationEnabled() const override { return isDebugCamera_; }

  // Timeline Editor内のDebug CameraトグルボタンにisDebugCamera_のアドレスを渡す
  bool* GetDebugCameraFlag() override { return &isDebugCamera_; }

  // Draw() の描画カメラ分岐と一致させる（ギズモ/マウスピッキングが実際の見た目とズレないように）
  const Camera* GetEditorViewCamera() const override;

private:
  // EnemyComponent::onDestroyed コールバックの登録を1箇所に集約する。
  // FormationMemberComponentを持つ場合は編隊トラッキングの初期カウントも行う。
  void RegisterEnemyCallbacks(const std::shared_ptr<AbsoluteEngine::GameObject>& obj);

  // --- Update の各段階 ---
  // プレイヤーの GameObject を探す（ロード時などでポインタが切り替わった場合に対応）
  std::shared_ptr<AbsoluteEngine::GameObject> FindPlayerObject_() const;
  // ゲームプレイ中の描画カメラ（デバッグカメラ or レールカメラ）
  Camera* GetGameplayCamera_() const;
  // ワールド座標をスクリーン座標(UV 0〜1)に変換する
  // PostEffectDirector に渡す毎フレームの状態を集めて更新する
  void UpdatePostEffects_(float deltaTime);
  // Playモード中のみOSマウスカーソルを隠して閉じ込める（Escで一時解除）
  void UpdateMouseCapture_();
  // 寿命切れの爆発ライトと、プレイヤーより十分後方の敵を削除する
  void RemoveExpiredObjects_();
  void SpawnBoss_();
  // プレイヤー死亡 / ボス撃破でシーン遷移を要求する
  void CheckGameEnd_();
  // 死亡演出を始める(HP が 0 になったとき、または、デバッグのショートカットから)。入力・発射・ロックオンを止め、
  // 演出クラスに、死亡の瞬間の自機の状態を渡す
  void BeginDying_();
  // 死亡演出を 1 フレーム進める(実時間)。スキップの入力、世界の時間の速さの反映、遷移の要求を行う
  void UpdateDying_(float deltaTime);
  // 死亡演出が決めた位置・向きを、自機へ反映する
  void ApplyDeathPose_();
  // 死亡演出の間、飛んでいる弾(自機のも敵のも)をすべて消す。新しく出た弾も、描かれる前に消す(当たり判定も描画も止まる)
  void RemoveBulletsDuringDeath_();
  // 死亡演出の間、カメラ・蝶の近くの敵を、描かないか(画面を邪魔しない、カメラが敵に埋まらない)
  bool IsHiddenDuringDeath_(const AbsoluteEngine::GameObject& obj) const;
  // デバッグ用: 死亡演出を中止して、蘇生する(Dying 中だけ。Cover で遷移を要求したあとは、できない)
  void ReviveForDebug_();
  void CheckDebugReviveShortcut_();
  // 自機に致命傷を与える(デバッグのショートカットと、展示の自動死亡が使う。本物の死亡処理と同じ経路を通る)
  void KillPlayer_();
  // 展示: 覆いが晴れたらすぐ、クリア / ゲームオーバーを交互に要求する(操作は受け付けない)
  void UpdateExhibitionFlow_(float deltaTime);
  // スタート演出のデモ: 演出が終わってから一定時間後に、タイトルへ戻る遷移を要求する(操作は受け付けない)
  void UpdateStageStartDemoFlow_(float deltaTime);
  // 開発用: プレイせずに、タイトル -> ゲーム -> クリア / ゲームオーバー -> タイトルの流れを確認するためのショートカット
  void CheckDebugFlowShortcut_();
  void UpdateHud_();
  // --- ゲーム開始のイントロ(画面遷移の覆いが晴れる間の、見た目だけの助走) ---
  // 覆いの中で始まったときだけ、イントロを始める。助走の基準になる位置(カメラ・自機)をここで記録する
  void BeginIntro_();
  // 覆いが晴れる進み具合に合わせて、カメラと自機を進行方向の後ろから進め、画角を落ち着かせる。
  // レール進行・敵・当たり判定は更新しない(晴れ終わるまで、ゲームの進行度は動かない)
  void UpdateIntro_(float deltaTime);
  // イントロを終える。カメラと自機を、基準の位置と画角へ正確に戻す
  void FinishIntro_();
  // --- ステージ開始演出(イントロのあと、プレイが始まるまで。俯瞰へ引き、戻りながら自機がステージ名の板を突き破る) ---
  // 時間を進め、ステージ名の板の出し入れ、板を割る時刻の判定、操作のロック解除を行う。
  // カメラの俯瞰のずれは、ここでは変えない(描画の間だけ Draw が重ねる)。
  // 俯瞰へ引く・保つ間(世界は止まる)の自機のアニメーションも、ここで進める
  void UpdateStageStart_(float deltaTime);
  // ステージ名の板の通り道を決め、最初の位置へ置く。
  // 通り道: 俯瞰のカメラの正面(画面の上寄り) -> 竜の上を弧を描いて飛び越える -> 竜が板を割る時刻に通る、レール上の位置の前
  void PlaceStageNameBoard_();
  // 今の段階での、板の衝撃点の位置(ワールド)。俯瞰へ引く・保つ間はカメラの正面、戻る間は弧の上、着いたあとは竜の前
  Vector3 CalcStageNamePosition_() const;
  // レールカメラのコンポーネントを探す(無ければ nullptr)
  RailCameraComponent *FindRailCamera_() const;
  // ブースト量(0 〜 1)から、世界の速さの倍率(1 以上)を求める
  float WorldSpeedScale_(float boost) const;
  // 今フレームに世界の更新を回す回数。ブーストで速さの倍率が 1 を超える間は、複数回になる(小数の部分は次のフレームへ持ち越す)
  int CalcWorldSteps_();
  // 画面に映すカメラのずれ。俯瞰のずれに、ブーストによる画角の広がりを重ねたもの
  StageStartPose GetStageStartViewPose_() const;
  // 自機の操作をロック / 解除する(自機が見つからないときは何もしない)
  void SetPlayerInputLocked_(bool locked);
  // 自機のモデルだけ更新する(アニメーションを進める)。入力・射撃を含む PlayerComponent は更新しない
  void UpdatePlayerModelOnly_(float deltaTime);

  // --- Draw の各段階 ---
  void DrawDebugRail_(Renderer* renderer) const;

private:
  std::unique_ptr<AbsoluteEngine::EditorCamera> debugCamera_;

  bool isDebugCamera_ = false;
  bool showDebugRail_ = false;
  bool showDebugGrid_ = false; // 地面のグリッド線(デバッグ用)
  float exhibitionHoldTimer_ = 0.0f; // 展示: 通常の更新に入ってからの経過(秒)
  float stageStartDemoTimer_ = 0.0f; // スタート演出のデモ: 演出が終わってからの経過(秒)
  bool stageStartDemoReturnRequested_ = false; // スタート演出のデモ: タイトルへの遷移を要求済みなら true
  bool showDebugSkeleton_ = false; // Bキーでトグル：プレイヤーの骨をデバッグ表示
  bool mouseCaptureSuspended_ = false; // Escで一時的にマウスの閉じ込めを解除している間 true

  Skybox skybox_;
  // 背景の雲海の見た目(ImGui で調整できる)。useCloudSea_ が false のときは従来のスカイボックスを描く(比較用)。
  // skybox_ は、雲海のときも環境マップ(反射)として使い続ける
  CloudSeaSettings cloudSea_;
  bool useCloudSea_ = true;
  // ボリューム雲(ワールドに固定した箱の雲。ImGui で調整できる)。useVolumeCloud_ が false なら描かない(比較用)。
  // 描画の仕組みは Renderer::AddVolumeCloud と shaders/VolumeCloud.hlsli を参照
  VolumeCloudSettings volumeCloud_;
  bool useVolumeCloud_ = true;
  // 雲海を、どちらの方式で描くか(比較用。既定は新方式)。
  //   OldMethod: 旧方式(高さの地図を探す簡易版。空と雲を CloudSea.hlsli が 1 つのシェーダーで描く。cloudSea_)
  //   NewMethod: 新方式。空は SeaSky.PS.hlsl(seaSky_)、雲はボリューム雲の「層」(volumeCloudSea_)。
  //              箱の雲(volumeCloud_)と同じフレームに重ねて描ける(奥から手前の順)
  enum class SeaCloudMode : int { OldMethod = 0, NewMethod = 1 };
  SeaCloudMode seaCloudMode_ = SeaCloudMode::NewMethod;
  SeaSkySettings seaSky_;
  VolumeCloudSettings volumeCloudSea_ = MakeSeaLayerSettings();
  bool volumeCloudEditSea_ = true; // 調整 UI で、雲海の層(true)と箱(false)のどちらを調整するか

  // アクター関連
  std::shared_ptr<AbsoluteEngine::GameObject> playerObj_;

  // ディゾルブのマスクテクスチャ
  std::shared_ptr<TextureResource> texNoise0_;

  HitEffectSpawner hitEffects_;    // 敵撃破時のヒットエフェクト
  PostEffectDirector postEffects_; // ポストエフェクト・カメラシェイクの発火とタイマー
  ScoreTracker scoreTracker_;      // スコア・撃破数・編隊の残数

  float time_ = 0.0f;

  // イントロの状態
  bool introActive_ = false;
  Vector3 introBaseEye_{0.0f, 0.0f, 0.0f};    // 助走の基準(スタート位置)のカメラの位置
  Vector3 introBaseTarget_{0.0f, 0.0f, 1.0f}; // 同じく注視点
  Vector3 introBasePlayerPosition_{0.0f, 0.0f, 0.0f};
  Vector3 introBasePlayerRotation_{0.0f, 0.0f, 0.0f}; // 助走の基準の、自機の向き(昇りの機首上げの前の値)
  float introRailSpeed_ = 0.0f;               // 助走の速さに使う、レールの進む速さ

  // ステージ開始演出の状態。カメラの基準は、上のイントロの基準(introBaseEye_ など)と共通
  StageStartDirector stageStart_;
  StageStartSettings stageStartSettings_; // このステージの設定(ステージが増えたら、ステージごとの値へ差し替える)
  StageNameBoard stageNameBoard_;          // ステージ名の板(俯瞰の間、自機の前に出す)
  StageNameStyle stageNameStyle_ = StageNameStyle::Clouds; // ステージ名の見せ方(ステージごとの素材へ差し替える)
  StageCloudSettings stageCloudSettings_;         // 雲の見せ方の設定(メイン文字。パフの間隔・大きさ、光の筋の幅、霧になる速さなど)
  StageCloudSettings stageSubtitleCloudSettings_ = MakeSubtitleCloudSettings(); // 雲の見せ方の設定(サブタイトル。小さいパフ)
  StageNameBoardSettings stageNameBoardSettings_; // 板の配置の設定(これもステージごとの値へ差し替える)
  FractureSettings stageNameFractureSettings_;    // 板の割れ方の設定(種の数、衝撃点への集まり具合など)
  FragmentSettings stageNameFragmentSettings_;    // 破片の設定(厚み、飛ぶ速さ、回転、重力、寿命など)
  Vector3 stageNameHoldPosition_{0.0f, 0.0f, 0.0f}; // 俯瞰を保つ間の、板の位置(弧の始点)
  Vector3 stageNameAnchor_{0.0f, 0.0f, 0.0f};       // 竜が板に触れる位置(弧の終点。板が最後に留まる位置)
  Vector3 stageNameForward_{0.0f, 0.0f, 1.0f};      // 板が向く基準の進行方向(板は、この水平な向きの逆を向く)
  float worldStepAccumulator_ = 0.0f;               // 世界の更新回数の、小数の部分の持ち越し(CalcWorldSteps_)
  StageImpactEffects stageImpact_;                  // 突き破った瞬間の衝撃(白いフラッシュ)
  StageImpactSettings stageImpactSettings_;         // 衝撃の設定(ステージごとの値へ差し替えられる)
  SpeedStreaks speedStreaks_;                       // 加速の間の速度線
  SpeedStreakSettings speedStreakSettings_;         // 速度線の設定(濃さ、伸び方)
  bool showDebugFracture_ = false;                // 板の分割の輪郭線を描く(確認用。分割の形を調整するときだけ true にする)

  GamePhase phase_ = GamePhase::InProgress;
  PlayerDeathDirector deathDirector_;       // 死亡演出の進行(サブフェーズ、世界の時間の速さ、自機の落下)
  WingShatter wingShatter_;                 // 蝶の翅が裂けて飛び散る破片(崩壊)
  DeathCameraDirector deathCamera_;         // 死亡カメラの進行(蝶を中心にした回り込み・急降下)
  DeathCameraSettings deathCameraSettings_; // 死亡カメラの設定(キーフレームの表)
  GamePhase phaseBeforeDeath_ = GamePhase::InProgress; // 死亡演出の前のフェーズ(蘇生で戻す先)
  bool debugHoldAtCover_ = false;           // デバッグ: Cover で遷移を要求せず、演出をその場で保つ(演出だけを、何度も見るため)
  PlayerDeathSettings deathSettings_;       // 死亡演出の設定(ステージごとの値へ差し替えられる)

  std::unique_ptr<GameHUD> gameHUD_;
};
