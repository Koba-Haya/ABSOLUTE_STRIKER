# ステージ開始演出 引き継ぎ書(2026-10-21 時点)

要素別コンテスト「スタート演出」(予定 10/21)の作業を、別セッションで続けるための書類。
関連: [TRANSITION_HANDOFF.md](TRANSITION_HANDOFF.md)(画面遷移。雲を抜けるところまで)、[TITLE_SCENE_HANDOFF.md](TITLE_SCENE_HANDOFF.md)(タイトル)、[CLOUD_HANDOFF.md](CLOUD_HANDOFF.md)(ボリューム雲)

## 0. 作業環境・ルール(最初に読む)

- 作業フォルダ: `C:\Users\haya2\source\repos\CG2\project`(`ribbon_beam` ブランチ)。**git の書き込み(commit / push / branch)はしない。** ユーザーが差分を見てから行う
- **協働スタイル**: 「言われたから実装する」でなく、まず目的に合うかを考え、選択肢の比較(長所・短所・難易度・おすすめ)を示して、合意してから実装する。実装は小さい段階に分けて、1 段階ごとに、ユーザーが実機で確認してから次へ進む。コードのあとに、何をしたか・なぜかを短く説明する
- 見た目の判断はユーザーの目が最終。Claude は実機を直接見られない。**ユーザーの録画(mp4、`C:\Users\haya2\Videos\画面録画\`)から、Python の `cv2` でフレームを抜いて確認する**(`cv2` は入っている)。見本が必要なときは、HTML のモックを作って見せる(文字だけだと、ユーザーはイメージしづらい)
- 日本語で会話。コメントは細かく、日本語で。コメントや文字列に、絵文字・飾りの特殊文字は使わない。数値は名前付き定数にする(各クラスの `...Defaults` の名前空間)
- ボロノイ・ステージ開始演出に関係する作業だけを、この流れで進める。レンダラーの深度の整理は別の案件(記憶 `depth-class-refactor-idea`)

## 1. 目的と流れ(合意済み)

雲を抜けてゲームシーン -> **俯瞰**(カメラが引く。最初の敵と進む先が少し見える) -> **ステージ名がカメラの前**(竜より手前、画面の上寄り)に、光の筋が通った順に、**雲で組み上がる** -> カメラが竜の背後へ戻る間、**ステージ名が竜の上を弧を描いて飛び越え、竜の前へ降りる** -> **加速**(世界が最大 3 倍速、画角が広がる、速度線、軽いブラー) -> **竜が突き破る**(白いフラッシュ、雲が霧になって散り散りに広がる) -> ステージスタート(操作解禁)。デモプレイにするので、スキップはいらない。ステージが増える予定なので、画像・設定はデータにする

時間の流れ(`StageStartDefaults`): 引く 1.6 秒 -> 保つ 1.5 秒(世界は止まる) -> 戻る 1.6 秒(世界が動く。割れるのは戻り始めてから 1.0 秒後)

## 2. 実装済みのもの

| ファイル(`Application/scene/`) | 責務 |
|---|---|
| `StageStartDirector.{h,cpp}` | 演出の時間の進行(引く / 保つ / 戻る)、俯瞰のカメラのずれ(`ApplyPose`)、ブースト量(0 〜 1。加速の度合い)、割れる時刻。カメラや自機は動かさない |
| `StageNamePath.{h,cpp}` | ステージ名が通る位置の計算(カメラ前、弦、弧の高さ、**画面の上端を超えない高さの上限** `MaxLiftInView`)。純粋な数学 |
| `StageNameBoard.{h,cpp}` | ステージ名の置き場所と見せ方の切り替え(`StageNameStyle::Clouds` / `Shards`)。位置・向き(カメラの進行方向の逆を向く)を管理 |
| `StageNameClouds.{h,cpp}` | **雲の文字**。画像の不透明な所にパフを置き、光の筋が通った順に組み上げる。壊れると、霧のパーティクル(柔らかいアルファ)を出す。メイン文字とサブタイトルで 2 つ使う |
| `StageNameFragments.{h,cpp}` | 厚みのある破片(`Shards` の見せ方。石・氷・ガラス向け)。全破片を 1 つの頂点バッファで毎フレーム書き換えて描く |
| `VoronoiFracture.{h,cpp}` | ボロノイ分割(種の生成と、凸多角形への分割)。数学だけ。単体テストで、面積の合計・凸性・種の位置を確認済み |
| `StageImpactEffects.{h,cpp}` | 突き破った瞬間の白いフラッシュ(いまはこれだけ) |
| `SpeedStreaks.{h,cpp}` | 加速の間の速度線(タイトルの気流の筋と同じ方式) |
| `BasisMatrix.h` | `MakeBasisMatrix`(行 = 右・上・前・位置の行列) |
| `GameScene.{h,cpp}` | 上を呼ぶだけ。イントロのあとに演出が続き、世界の更新回数(`CalcWorldSteps_`)、カメラのずれを描画の間だけ重ねる(`ScopedStageStartCameraOffset`)、操作のロックと解除 |

エンジン側の変更(`AbsoluteEngine/graphics/`):
- `ModelResource::UpdateVertices`: 頂点バッファを毎フレーム書き換える(破片・雲のパフ用)
- `Renderer::DrawEffectModel(model, writeDepth)`: 深度を書き込む描画。雲(深度を見て後から描かれる)に隠れないために必要
- `ParticleManager`: 「柔らかいアルファ」のグループ + `ParticleDrawFilter`(`BlendMode.h`)。霧を、加算ではなく、アルファブレンドで別に描くため。既存の呼び出しは、既定値のまま変わらない
- `RingEffect`: 描画の時点のカメラで、向きと位置を計算し直す(いまはリングを使っていないが、無害なので残した)
- `PlayerComponent::SetInputLocked`(`Application/actor/Player/`): 演出の間、操作を受け付けない
- `PostEffectDirector`: `FrameState::speedBlur` で、加速の間の放射状ブラー(最大 0.03)

素材: `Application/resources/stage/`(`stage_name_01.png` = 仮のステージ名画像、`stage_name_board.obj/.mtl`)。パフの画像は、タイトルの `resources/title/puff.png` を流用

## 3. 調整する場所(どれも仮の値。実機を見て調整してきた)

- 演出の長さ・俯瞰の位置・加速の倍率・画角: `StageStartDefaults`(`StageStartDirector.h`)
- タイトルの大きさ・カメラからの距離・弧の高さ・画面の上限: `StageNameBoardDefaults`(`StageNameBoard.h`)
- 雲のパフの大きさ・間隔・縁からのはみ出し(`edgeInset`)・光の筋の幅・霧の散り方(粒数、速さ、渦、寿命): `StageCloudDefaults`(`StageNameClouds.h`)。影の濃さとずらし量は `StageNameClouds.cpp` の先頭
- フラッシュの濃さ(いま 0.6)・長さ: `StageImpactDefaults`(`StageImpactEffects.h`)
- 速度線: `SpeedStreakDefaults`(`SpeedStreaks.h`)
- 破片の厚み・飛ぶ速さ: `FragmentDefaults`、割れ方: `FractureDefaults`(`Shards` の見せ方のとき)
- いま、**すべて定数なので、調整のたびに再ビルドが必要**。ImGui の調整パネルがあると楽(未実装)

## 4. 決定事項と不採用(蒸し返さない)

- 道筋は、弧を描いて竜を飛び越える(「画面に貼って、世界へ差し替え」は不採用)
- 加速は、世界の更新を、1 フレームに複数回回す方式(見た目だけの加速は不採用)
- 突き破りに **入れない**: ヒットストップ、残像、衝突後のスロー、衝撃波のリング、カメラシェイク(いらないとのこと)
- 雲の文字に、**くっきりした輪郭線(文字)を重ねるのは不採用**(雲で作る意味がなくなる)。読みやすさは、パフを小さく・密にして、縁からはみ出させず、影を強めて出す
- 雲の壊れ方は「ふくらんで霧になる」を、散り散りに(速さ・向き・大きさ・寿命をばらつかせ、渦も加える)
- 見せ方は、ステージごとに変えられるようにしたい(空 = 雲、水 = 水、氷・石 = ボロノイの破片)。いまは雲が既定

## 5. 未完・次にやること

1. **見せ方(素材)を、ステージの設定から選べる形にする**。いまは `GameScene.cpp` の先頭で、画像のパスと `stageNameStyle_ = StageNameStyle::Clouds` を直接決めている。ステージごとの設定(ステージ名の画像、見せ方、各設定値、俯瞰のカメラの値)を、データ(JSON など)に出す。水などの新しい素材は、`StageNameBoard` に見せ方を足す(出現の動きと壊れ方を、素材ごとに持つ)
2. **Release ビルドでの確認**(Debug でしかビルドしていない): アセットが exe 相対で読めるか(`resources/stage/*` が `xcopy` で入るか)、日本語名のフォルダで、ソースツリーなしで動くか(記憶 `release_package_verification` の手順)。ImGui が無い Release では、`GameScene` は、タイトルから来た時点で自動的に Play になる
3. **デモプレイ(展示)との組み合わせの確認**: `ExhibitionMode::kEnabled` の流れ、演出を繰り返したときの状態のリセット(`StageStartDirector::Begin` は、毎回最初から始まる作りだが、通しの確認はしていない)
4. 確認していない端のケース: ImGui ありの Debug で、Play を押す前の挙動、デバッグカメラ、レールの始まりが急に曲がるステージ(板の位置は、レール上の未来の位置から求めている)、ウィンドウの縦横比の変化(画面の上限の計算は、縦の画角で行っている)、最初の敵がスポーンしていないとき
5. 軽い掃除: `StageNameBoard::DrawDebugCells` と `showDebugFracture_`(確認用の線。いまは false)、使っていない見せ方 `Shards` の扱い(残す方針)
6. 負荷の確認(未計測): 速度線 60 枚、雲のパフ(メイン約 1100 + サブタイトル約 1300、3 回描画)、霧の粒(最大約 5000)、世界の更新 3 回 / フレーム
7. 別の案件(このセッションの範囲外): 深度の区分の整理(記憶 `depth-class-refactor-idea`)

## 6. 落とし穴

- **ビルド**: PowerShell で `MSBuild.exe DirectXGame.sln -p:Configuration=Debug -p:Platform=x64 -m -v:m -nologo`(MSBuild は `C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe`)
- **新しい .cpp/.h**: `Application.vcxproj` と `Application.vcxproj.filters` に追加が必要
- **行の終わり**: 多くのファイルが CRLF。`Edit` や `Write` のあとで、CRLF にそろえる。ただし、元のファイルが「最終行が改行なし」で終わるときは、その状態を保つ(末尾に CR を足すと、git がバイナリ扱いにして、全行が差分になる)。`GameScene.cpp` は、HEAD が末尾に単独の CR を持つので、それも保つ
- **Windows のマクロ**: `std::min` / `std::max` を使う .cpp は、先頭で `#define NOMINMAX` するか、`(std::max)(...)` と書く
- **`MakeAffineMatrix`**: 回転に `{...}` を渡すと、`Vector3` と `Quaternion` で曖昧になる。`Vector3{...}` と明示する
- **カメラのずれは、描画の間だけ重ねる**(`ScopedStageStartCameraOffset`)。ゲームの進行に使うカメラは動かさない。レール進行は、毎フレーム、カメラの位置を絶対値で決め直すので、更新の中でずらすと、ずれが積み重なる
- **位置を、更新のときのカメラで固定する描画**(`RingEffect`、`ParticleManager::Update` は、1 フレーム前のカメラを使う)は、カメラをずらす演出とずれる。霧は、柔らかいので、1 フレームの遅れは見えない
- 変更した、このセッションで作った・触ったファイルは、`git status` で確認できる(`Application/actor/Enemy/BossComponent.*` は、ユーザー自身の変更で、この作業のものではない)

## 7. 見た目の確認方法

1. ユーザーが、実機で録画する(`画面録画 日付 時刻.mp4`)
2. `cv2.VideoCapture` で、0.3〜0.5 秒ごとにフレームを抜き、時刻を書き込んで、3 x 4 などに並べた画像にして、見る
3. 気になる瞬間は、30fps の連続した数フレームを、拡大して見る
4. 見本が必要なときは、HTML のモック(canvas)を作って、ローカルサーバーで開いて、エラーを確認してから出す
