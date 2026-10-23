# スキルシート（このプロジェクト用の前提メモ）

新しい会話の冒頭でこのファイルを貼る/参照させれば、作品の方向性や技術スタックの説明は省略できる。
各会話で伝える必要があるのは「今回の目標・タスク」だけでよい。

## 1. 作品概要

- ジャンル: 3D固定レールシューティング（ロックオン操作のあるアクション要素あり）
- 目指す方向性の参考作品:
  - **Panzer Dragoon Remake**
  - **スターフォックス リメイク**
- 付属的な参考作品:
  - **Rez Infiniteシリーズ**
- 個人開発（一人で企画・実装）。就職活動用ポートフォリオとして、就職が決まるまで継続的に制作・改修していく作品（区切られた制作期間があるわけではない）

## 2. 開発環境

- 言語: C++（C++17/20相当）
- グラフィックスAPI: DirectX12 + HLSL
- Compute Shaderを積極活用（GPUスキニング、GPUパーティクル等）
- IDE/ビルド: Visual Studio 2022（`DirectXGame.sln`）。コマンドラインビルドは MSBuild を使用
- バージョン管理: Git / GitHub
- デバッグ: D3D12デバッグレイヤー、Visual Studioデバッガ、Windowsイベントログ

## 3. 自作エンジン「AbsoluteEngine」の構成

- `graphics/`（2d, 3d, particle, pipeline, primitive, resources, shader, texture, descriptor）… 描画まわり一式を自前実装
- `audio/` … サウンド再生
- `camera/` … カメラ制御
- `editor/` … 独自シーンエディタ（`EditorCamera`, `EditorUIManager`、Commandパターンによる Undo/Redo 対応の `CommandManager`）
- `scene/timeline/` … タイムラインベースのイベント/敵配置システム（`TimelineTrack`, `SpawnEvent`, `PrefabRegistry` によるプレハブ管理）
- `math/` … 独自数学ライブラリ
- `base/input/` … 入力管理

ゲーム本体（`Application/`）はこのエンジンの上に actor（Player/Enemy/Bullet）、component、hud、camera、scene 等を積む構成。

## 4. 実装済みの主要機能（技術的な強み）

- スキニングアニメーション（Compute ShaderによるGPUスキニング）＋アニメーション間のクロスフェード補間
- GPUパーティクル（Compute Shaderベース、複数エミッタ + Field（Attractor/Wind/Vortex）対応）
- ポストエフェクトパイプライン（Grayscale, Vignette, BoxFilter, GaussianFilter, LuminanceBasedOutline, DepthBasedOutline, RadialBlur, Dissolve, Random(グリッチ) など）。状況（被弾・体力低下・ロックオン・敵/ボス出現など）に応じてゲームプレイ中に自動で切り替わる仕組み
- MultiMesh / MultiMaterial対応のモデル描画
- 独自シーンエディタ（配置・タイムライン編集、Undo/Redo）

## 5. 参照先（このファイルには含めない、都度変わる情報）

- 現在着手中のタスク・引継ぎ事項 → [task.md](task.md) / [handoff.md](handoff.md) / [implementation_plan.md](implementation_plan.md)
- 加点要素の詳細な演出説明 → [ReadMe.md](ReadMe.md)
