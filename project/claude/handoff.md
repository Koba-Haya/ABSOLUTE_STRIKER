# 引継ぎメモ: GPU Particle等の加点要素実装（クラッシュ解決済み）

**2026-07-28 追記: クラッシュの根本原因を特定し、修正済み。ビルド成功・ユーザー環境でPlayモード動作確認済み。**
以下、旧内容の下に解決報告を追記する形にしている。

**2026-07-28 追記2: 武器（手追従モデル）が見えない問題を解決。**
`GetBoneWorldMatrix()`が返す行列（`joint.skeletonSpaceMatrix`）には、Mixamoリグのボーン階層
（特に`RightHand`のように深くネストしたボーン）に蓄積された、スキニング用の非常に小さい
スケール成分が含まれていた。この行列をそのまま武器の`SetWorld()`に使うと、位置は正しいのに
武器がほぼ見えないサイズまで縮んでしまっていた（座標のログにはスケールの影響が出ないため
発見が遅れた。デバッグ手順：まず武器をプレイヤー自身のワールド行列(スケール1)で描画して
「独立したModelInstance自体は正常に描画される」ことを確認し、次に手のボーン行列を使うと
再現しない→ボーン行列側のスケール混入と特定）。
[PlayerComponent.cpp](Application/actor/Player/PlayerComponent.cpp)の`Draw()`で、
`GetBoneWorldMatrix()`の戻り値から**平行移動成分だけ**を取り出し、スケール1・回転なしで
行列を組み直すように修正済み。同様に手のボーン位置を使うコード（弾の発射位置・マズルフラッシュ）
は元々座標だけを取り出していたため、この問題の影響を受けていなかった。

## 背景・目的

学校の評価課題2。加点要素の配点表（写真フォルダ`photo/`参照）に基づき、実装済み分だけ加点される。
**「ゲームシーン（実際にプレイして確認できる状態）で目視確認できないと加点として認められない」**という制約がある。

既存の `task.md` / `implementation_plan.md` は Skinning表示・骨デバッグ・手からのパーティクル・武器の手追従にフォーカスしていたが、
調査の結果それらは**既に実装済み**（uncommittedのWIPとして存在）であることが判明。
そこで、配点最大のGPU Particle拡張（30点）・MultiMesh&MultiMaterial対応（5点）・Animation補間（5点）を新たに実装した。

## クラッシュの根本原因（確定済み）

ユーザーのVisual Studioが実際にクラッシュ地点でブレークしているのを確認でき、
コールスタックとD3D12デバッグレイヤーのメッセージから原因が確定した。

```
D3D12 ERROR: ID3D12CommandList::Close: An ID3D12Resource object
(0x0000026FDDFB8540:'C:/Users/haya2/source/repos/CG2/project/Application/resources/player/player.png'),
referenced in the command list being closed, was deleted prior to closing the command list.
[ EXECUTION ERROR #921: OBJECT_DELETED_WHILE_STILL_IN_USE]
```

**原因**: `GameScene::Initialize()`で、シーンJSON経由で読み込まれた `player.obj`（テクスチャ:
`player.png`）を起動直後に `resources/app/human/walk.gltf` へ強制的に差し替えている
（Skinning/Animationを常時可視化するための処置）。この差し替えは
`ModelComponent::LoadModel()` 内で `modelInstance_ = std::make_unique<ModelInstance>();`
として古い `ModelInstance`（→古い `ModelResource`→古い `TextureResource`(player.png)）を
**その場で即座に破棄**していた。

ところが、`player.obj`/`player.png` は同じ `GameScene::Initialize()` の少し前（シーンJSONの
デシリアライズ時）に読み込まれたばかりで、そのテクスチャアップロードコマンド
（`CreateFromMetadata`→`UploadTextureData`が記録する`CopyTextureRegion`+バリア）は、
**まだCloseされていない今フレームのコマンドリストに記録されたまま**だった。
この状態で参照先のリソースを破棄すると、`EndFrame()`が最後に`Close()`を呼んだ瞬間に
D3D12デバッグレイヤーが「参照中のリソースが削除されている」と検出してクラッシュする。

TextureManager/ModelManagerのキャッシュが`weak_ptr`である（＝最後の`shared_ptr`が消えた瞬間に
即破棄される）という設計自体は元々のエンジンの意図通りで問題ない。問題は
「読み込み直後・同一フレーム内に、その参照を即座に手放す」という今回追加したコード
（プレイヤーモデルの強制差し替え）が、この「即破棄」の危険な窓（コマンドリストがまだ
Closeされていない期間）にちょうど当たってしまったこと。旧仮説（MultiMeshのサブメッシュ
テクスチャロードが原因）は誤りだった。

### 修正内容

[ModelComponent.cpp](AbsoluteEngine/scene/ModelComponent.cpp) / [ModelComponent.h](AbsoluteEngine/scene/ModelComponent.h) にて、
`LoadModel()`で古い`modelInstance_`をその場で破棄せず、`pendingDestroyModelInstance_`に退避。
実際の解放は次の`Update()`（＝次のフレーム、`EndFrame()`のフェンス待機が完了した後）まで
1フレーム遅延させるようにした。これによりコマンドリストが安全に消費された後にだけ
破棄されるようになり、クラッシュが解消した。

副次的に、[TextureResource.cpp](AbsoluteEngine/graphics/texture/TextureResource.cpp) /
[ModelResource.cpp](AbsoluteEngine/graphics/3d/model/ModelResource.cpp) /
[GPUParticleManager.cpp](AbsoluteEngine/graphics/particle/GPUParticleManager.cpp) の主要な
`ID3D12Resource`に`SetName()`でデバッグ名を付与済み（次回似た問題が起きた際、VS上で
"Unnamed Object"ではなく具体的なリソース名が表示されるようにするため）。

また、[RingEffect.h](AbsoluteEngine/graphics/particle/RingEffect.h)/[.cpp](AbsoluteEngine/graphics/particle/RingEffect.cpp)が
テクスチャを生ポインタ(`TextureResource*`)で保持しており、シーンをまたいで生存する
`EffectManager`シングルトンに渡すとダングリングポインタになりうる副次的なバグも発見・修正
（`shared_ptr<TextureResource>`で保持するように変更。呼び出し元は
[GameScene.cpp](Application/scene/GameScene.cpp)と[DevScene.cpp](Application/scene/DevScene.cpp)の計3箇所）。

### 別インシデント: PlayerComponentの変更が消失した事故（復旧済み）

このセッション中、`Application/actor/Player/PlayerComponent.cpp`/`.h`の未コミット変更
（武器の右手追従・右手ボーンからのマズルフラッシュ・`Draw()`のオーバーライド）が、
おそらくVisual StudioのGit連携パネルの誤操作で**HEADの状態に巻き戻されてしまう**事故が
発生した。会話内に残っていた元の差分から手動で復元済み。今後、VSのGit変更パネルで
「変更を元に戻す」を使う際は対象ファイルに注意すること。

## 実装済みの内容

git状態: 以下のファイルがuncommittedで変更されている（コミットはしていない）。

```
M AbsoluteEngine/graphics/3d/model/ModelInstance.cpp/.h      … Animation補間(クロスフェード)
M AbsoluteEngine/graphics/3d/model/ModelResource.cpp/.h      … MultiMesh/MultiMaterial: サブメッシュ別テクスチャ + SetName
M AbsoluteEngine/graphics/3d/model/ModelUtils.cpp/.h         … SubMeshRange/ComputeSubMeshRanges追加
M AbsoluteEngine/graphics/Renderer.cpp                        … DrawModelのサブメッシュ描画対応 + DrawGPUParticles配線
M AbsoluteEngine/graphics/particle/GPUParticle.h              … Emitter配列・Field構造体追加
M AbsoluteEngine/graphics/particle/GPUParticleManager.cpp/.h  … 複数エミッタ・Field・API追加 + SetName
M AbsoluteEngine/graphics/particle/RingEffect.cpp/.h          … テクスチャをshared_ptrで保持するよう修正（ダングリングポインタ対策）
M AbsoluteEngine/graphics/texture/TextureResource.cpp         … SetName追加
M AbsoluteEngine/resources/shaders/EmitParticle.CS.hlsl       … エミッタ単位で並列化
M AbsoluteEngine/resources/shaders/GPUParticle.hlsli          … Emitter/Field構造体（HLSL側）
M AbsoluteEngine/resources/shaders/UpdateParticle.CS.hlsl     … Field(Attractor/Wind/Vortex)適用
M AbsoluteEngine/scene/ModelComponent.cpp/.h                  … Update()追加 + 遅延破棄によるクラッシュ修正（本セッションの主眼）
M Application/actor/Player/PlayerComponent.cpp/.h             … 武器手追従・手からのパーティクル（誤って消えたのを復元 + 視認性調整）
M Application/scene/GameScene.cpp/.h                          … GPU Particle露出・MultiMaterial装飾物・ロックオン連動アニメ遷移
M Application/scene/DevScene.cpp/.h                            … RingEffect呼び出し箇所をshared_ptr対応に修正
?? Application/resources/multiMaterial/monsterBall.png        … 新規（欠けていたテクスチャを別の場所からコピー）
?? Application/resources/multiMaterial/uvChecker.png          … 新規（同上）
?? implementation_plan.md, task.md, handoff.md                 … ドキュメント類
```

### 実装内容の要約

1. **GPU Particle拡張(30点)**: `GPUParticleManager`にエミッタ配列(`kMaxGPUEmitters=8`個、常駐用4+バースト用4)・Field配列(`kMaxGPUFields=4`)を追加。`CreateEmitter`/`SetEmitterTransform`/`SetEmitterEnabled`/`EmitBurst`/`SetField`のAPIを新設。`EmitParticle.CS.hlsl`をエミッタごとに1スレッドグループ(64スレッド)で並列化。`UpdateParticle.CS.hlsl`にField(Attractor/Wind/Vortex)を統合。`GameScene::Initialize()`でプレイヤー付近に常駐Box型エミッタ+Vortex Fieldを設置、`SpawnHitEffect()`(敵撃破時)で`EmitBurst`を呼ぶよう配線。
   - GPU側パーティクルは`EmitParticle.CS.hlsl`でパーティクルごとにランダムなRGB色(`generator.Generate3d()`)を割り当てるため、既存のCPU側爆発パーティクル（白色固定, scale 1.5）とは見た目で区別できる（虹色にきらめく方がGPU Particle）。デモ時の説明用メモ。
2. **MultiMesh & MultiMaterial対応(5点)**: `ModelUtils`に`SubMeshRange`/`ComputeSubMeshRanges`を追加。`ModelResource`が複数メッシュ(`meshes.size()>1`)の場合のみサブメッシュごとのテクスチャを保持。`Renderer::DrawModel`がサブメッシュ数>0の場合はサブメッシュごとにテクスチャを差し替えて個別描画（単一メッシュモデルは従来通り1回描画にフォールバックするので既存モデルへの影響なし）。`GameScene::Initialize()`で`multiMaterial.obj`を使った装飾オブジェクトを常時配置。
   - `multiMaterial.obj`は2マテリアル（`monsterBall.png`と`uvChecker.png`、視覚的に全く異なる柄）を持つため、正しく描画できていれば同一オブジェクト上に2種類の異なるテクスチャが同時に見えるはず。デモ時は近づいて両方の柄が見えることを確認するとよい。
3. **Animation補間(5点)**: `ModelInstance::PlayAnimation`に`blendDuration`引数を追加し、遷移開始時のポーズをスナップショットしてLerp/Slerpでクロスフェード（配点表の式`tB+(1-t)A`通り）。`GameScene`でロックオンON/OFF切り替え時に`walk.gltf`⇔`sneakWalk.gltf`をクロスフェード切り替え。**ユーザー確認済み・完璧に動作。**

### 既存確認済み（今回コードを変更していない、実装済みの項目）
- Skinningモデル表示+パッド操作(20点)、ComputeShaderによるSkinning(10点)
- 骨のデバッグ表示(10点、Bキー): **ユーザー確認済み**
- 手からのパーティクル(10点)・武器を手に持たせる(10点): コード上は実装済み。レールシューティングでカメラが自機からかなり離れているため、元のスケール/色では視認しづらかった。武器の色を目立つ赤系(0.95,0.25,0.15)に、スケールも一回り大きく(0.3/0.3/1.5 → 0.4/0.4/2.0)、マズルフラッシュのパーティクルもスケール(0.3→0.6)・寿命(0.15秒→0.3秒)を引き上げ済み。**再確認が必要。**

## まだ確認が必要なこと
- 武器モデルが実際に画面上で見えるか（色・サイズ調整後の再確認）
- マズルフラッシュパーティクルが見えるか（同上）
- 全体を通して一通りプレイし、クラッシュが再発しないか

## 参考: この環境固有の制約
- この開発環境のスクリーンショット/GUI自動操作は不安定で、**実際のユーザーデスクトップを誤って写してしまう**ことがあった（プライバシー上の理由で使用中止）。プロセスの生存監視（`Get-Process`）・Windowsイベントログ（`Get-WinEvent -FilterHashtable @{LogName='Application'; ProviderName='Application Error'}`）で検証すること。
- cdb.exeのパス: `C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\cdb.exe`
- MSBuildのパス: `C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe`
- ビルドコマンド例: `MSBuild.exe DirectXGame.sln -p:Configuration=Debug -p:Platform=x64 -m -v:minimal`
- **ユーザー自身のVisual Studioデバッガが最も確実**。JITデバッグでVSがアタッチしている場合、`Application.exe`プロセスは`Get-Process`で`Responding: False`のまま残り続けるので、ビルド出力ロック(`LNK1168`)の原因になる。安易に`Stop-Process`しないこと（今回は幸いプロセスが実際には終了せず、後でVS側の情報を確認できた）。
