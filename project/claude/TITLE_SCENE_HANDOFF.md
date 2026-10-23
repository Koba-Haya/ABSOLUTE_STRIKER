# タイトルシーン 引き継ぎ書(2026-10-08 時点)

提出(要素別コンテスト No.1)は完了。このあとの続き(L1 以降の磨き込み)を、別セッションで進めるための書類。
関連: [TITLE_SCENE_PLAN.md](TITLE_SCENE_PLAN.md)(計画)/ [TITLE_SCENE_NOTES.md](TITLE_SCENE_NOTES.md)(選択理由・代替案。発表の下書き)

## 0. 作業環境・ルール(最初に読む)

- 作業フォルダ: `C:\Users\haya2\source\repos\CG2-title\project`(git worktree。ブランチ `feature/title-scene`)
- **git の書き込み(commit / push / branch)は絶対にしない**。コミットはユーザーが差分を見てから行う。
- **協働スタイル**(2026-10-03 に変更): コードは Claude が書く。書いたら、どこで何をしてどう動くかを**詳しく解説**し、理解チェックの問いを出す。ユーザーが理解できたと言ってから次へ進む。ただしユーザーが「完成度を優先したいので理解チェックは後」と言ったときは、後回しにしてよい(今は後回し中。§6 参照)。
- **編集前に、診断と方針を提案して了承を得る**。シェーダーの色・強度は、ユーザー承認なしに大きく変えない。
- 「前後左右・見た目」はユーザーの目が最終判断。Claude は実機の画面が見られない(動画のフレーム抽出は OpenCV で可能。ffmpeg は無いが `imageio_ffmpeg` と `cv2` が Python にある)。
- Release は ImGui を含めない設計。タイトルの表示物は ImGui に依存させない。
- 日本語で会話。参照するファイルは markdown リンクで書く。

## 1. 現状(実装済み)

L0(必達)に加え、次まで実装・確認済み。**提出済み。**

| 項目 | 内容 | 主なファイル |
|---|---|---|
| 昼の空 | 全画面三角形 + `invViewProj` で視線復元。グラデ・太陽・**ノイズ平面雲 2 層**(高い層=大きくゆっくり、低い層=小さく速い) | `Sky.VS/PS.hlsl`, `Sky.hlsli`, `Noise.hlsli` |
| 水面 | 全画面パスで視線と y=0 の交点。sin 4 本 + ノイズ(位置の歪み・振幅ムラ・fbm さざ波)。フレネル、空(雲)の反射(法線を平らに近づけて調べる)、太陽のきらめき、霞 | `Water.PS.hlsl`, `Water.hlsli` |
| 演出 | `TitleDirector`: 自機の S 字・バンク・バレルロール、4 カット × 6 秒ループ、ロゴ類のカメラ取り付け、スタート演出、**気流の筋**(自機の周りの格子、16 本、ローアングルのカット①では出さない) | `Application/scene/title/TitleDirector.{h,cpp}` |
| 自機 | Tripo 生成の飛竜(`dragon/dragon.obj`)。**頂点から大きさ(翼幅 3.7)・中心・翼の先端 3 か所を自動で求める**(`FitPlayerModel`) | `TitleScene.cpp` |
| 翼のプルーム | 翼の先端から、前フレームの位置との間を埋めてパーティクルを発生。ParticleManager に `endScale`・深度テスト付き描画・`color.w` 反映・`TranslateParticleGroup` を追加 | `TitleScene.cpp`, `ParticleManager.*`, `Renderer.*` |
| ライティング | 自機もロゴ類も**ライティングなし(モード 0)**。ライトを足すと白飛びしたため、タイトル専用ライトは削除 | `TitleScene.cpp` の `kUnlitMode` |
| スタート演出 | SPACE / ENTER / パッド A → ロゴ・PRESS START が上下へ消え、自機が加速して飛び去り、白フラッシュ。2.4 秒 | `TitleScene.cpp`, `TitleDirector.cpp` |
| **仮設定** | 演出のあと**ゲームへ行かずタイトルへ戻る**(展示でゲームから戻る手段が無いため)。`TitleScene.cpp` の `kReturnToTitleAfterLaunch`(今 `true`)を `false` にするとゲームへ遷移。戻るときは白からフェードイン(`kReturnFadeSec`) | `TitleScene.cpp` |
| 浮動原点 | 長時間放置するとワールド座標が増えて float 精度が荒れ、カメラがガクガクした。描画用の座標を 2400 ユニットごとに巻き戻し、巻き戻した分を空・水面のシェーダーへ渡す(`worldOffset`)。時間は double | `TitleDirector.cpp`(`kWorldWrapZ`), `Renderer.*`, `TitleScene.cpp` |

直前に**雲ビルボード(L1-2)を試作して、提出のために戻した**(§3)。

## 2. ファイル地図

- シェーダー原本: `AbsoluteEngine/resources/shaders/`(`Sky.*`, `Water.*`, `Noise.hlsli`)。**`generated/outputs/*/resources/...` は編集しない**(ビルド時に xcopy で上書きされる)。
- 新規 C++ の追加先: アプリ側は `Application/Application.vcxproj`(と `.filters`)、エンジン側は `AbsoluteEngine/AbsoluteEngine.vcxproj`。ルートの `DirectXGame.vcxproj` は旧パスの残骸で未使用。HLSL・画像・OBJ は vcxproj への登録不要(`resources` に置けば xcopy でコピーされる)。
- 素材の生成スクリプト(再生成できる。**Blender で開いて書き出し直さない**: OBJ は読み込み時に Z が反転するため):
  - `Application/resources/title/gen_title_assets.py`: ロゴ・PRESS START・白フラッシュ・気流の筋・プルームの粒
  - `Application/resources/player/gen_dragon_pose.py` / `gen_dragon_rig.py` / `export_dragon_pose.py`: 飛竜のポーズ(Blender 4.4 で動かす)。制作用 `.blend` は `art/dragon/dragon_rig.blend`
- 保留素材: `art/title_clouds/`(雲ビルボードのスプライト 4 種と生成スクリプト、README あり。実行ファイルにはコピーされない)

## 3. 保留・未実装

| 項目 | 状態 |
|---|---|
| **L1-2 雲ビルボード** | 試作して動画で確認済み(ふくらんだ積雲に見えた)。提出のため外した。再開手順は `art/title_clouds/README.md`。設計: 立体密度場を視線方向に積算して事前に作った 4 種のスプライト、ワールド固定の格子(自機の周りで繰り返し)に 12 枚、カメラ向き、遠い順に描画。**場面転換**(カット④の終わりで雲が画面へ迫って白になり、カット①の頭で晴れる。カメラの切り替えを隠す)も設計済み |
| L1-3 水しぶき・航跡 | 未実装(翼のプルームと気流で一部代替) |
| **L2 水面の平面反射** | 未実装。自機が水面に映る(見栄えの費用対効果が高い)。計画書 §6 の L2 を参照 |
| L3 レイマーチ雲 | 未実装(挑戦枠。30fps を割るなら採用しない) |
| ドラゴンのポーズ | ユーザー側で調整中(いったん保留) |
| 理解チェック | ノイズ平面雲について出した 3 問(視線と雲の平面の交点の式、日陰の決め方、雲が流れる理由)が**未回答** |

## 4. 落とし穴(ハマりやすい点)

- **ビルド**(Developer ツールが PATH に無いので、このパスを使う):
  `"C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" DirectXGame.sln /p:Configuration=Debug /p:Platform=x64 /m /v:m /nologo`(`Release` も同様。構成は Debug / Development / Release)。
- **Release の exe が起動中だと xcopy が失敗する**。プロセスは Claude が止めない。ユーザーに閉じてもらう。
- 新しい worktree で DirectXTex のビルドが `CompileShaders` で落ちたら、元のチェックアウトの `externals/DirectXTex/Shaders/Compiled` をコピーする(git 管理外)。
- **警告はエラー扱い**(例: `double` → `float` の暗黙変換で C2220)。
- **シェーダーは起動時にコンパイルされる**ので、ビルドが通っても実行しないとエラーが分からない。事前の構文確認は dxc で可能:
  `"C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\dxc.exe" -T ps_6_0 -E main -HV 2021 -I <shadersフォルダ> Sky.PS.hlsl -Fo /dev/null`
- **シェーダー等の更新は、構成ごとの出力フォルダへ、ビルドしたときだけコピーされる**。Debug だけビルドすると Release は古いまま(Debug と Release で見た目が違う原因になった)。
- RTV が sRGB なので、シェーダーはリニア値で書く。画像テクスチャはそのまま(sRGB)。
- 行列は行優先・行ベクトル規約(`v * M`)。HLSL では `row_major float4x4` で受ける。座標系は左手系(カメラは -Z 側から +Z を見る、画面右が +X)。
- 改行コードが混在する: `UnifiedPipeline.cpp`(CRLF)、`ParticleManager.*`、`ModelResource.*` は CRLF。Python で書き換えるときは `open(..., newline='')` で改行を保つ。`UnifiedPipeline.cpp` は git が全行差分に見せる(以前からの設定の問題で、実際の変更は約 20 行)。
- 半透明(`DrawEffectModel`)は `depthWrite=false`。ロゴが自機より手前に出るよう、カメラのすぐ前(3 ユニット)に置いている。
- パーティクルは `ParticleManager` のシングルトン。ゲームシーンのグループ(default / explosion)と共存する。タイトルの `contrail` グループは `Finalize` でクリアする。
- 提出物が大きくならないよう、`Application/resources/player/dragon 3d model/`(元の Tripo モデル一式、約 4MB)が `resources` の中にある点に注意(ユーザーに削除・移動を提案済み)。

## 5. 調整用の定数

- 空・雲: `Sky.hlsli` の `kCloudHigh` / `kCloudLow`(`coverage`・`scale`・`wind`・`macroStrength`)、`kCloudShadowGain`、`kCloudBacklitDarken`
- 水面: `Water.PS.hlsl`(`kGlitterIntensity`, `kReflectionNormalBlend`, `kFogDensity`)、`Water.hlsli`(`kWarp*`, `kAmplitude*`, `kRipple*`)
- 演出: `TitleDirector.cpp`(`kFlightSpeed`, `kShotDurationSec`, カメラの各カット, `kStreak*`, `kLaunch*`)
- プルーム・スタート演出: `TitleScene.cpp`(`kContrail*`, `kLaunchDurationSec`, `kFlashStartRatio`, `kPlayerWingspan`)
- 気流の本数: `TitleDirector.h` の `kTitleStreakCount`(ユーザーが 16 に変更済み)

## 6. 次にやること(候補。順番はユーザーと決める)

1. 未回答の理解チェック(§3)を出し直し、答えを聞く
2. **L1-2 雲ビルボードの再開**(素材と設計は揃っている。場面転換つき)
3. **L2 水面の平面反射**: 反射用 `RenderTexture` を 1 枚、水面より上のシーン(空・自機)を y 反転カメラで描く。水面 PS は、画面座標 + 波の法線で歪ませてサンプルし、解析反射とフレネルで混ぜる(失敗時に今の見た目へ戻せるように)。ロゴ類は反射させない。**`DrawSky` の定数バッファは 1 つなので、1 フレームに 2 回呼ぶなら分ける必要がある**
4. L3 レイマーチ雲(挑戦枠。着手前に、積算式・ジッタ・光の減衰を口頭で説明してから)
5. `TITLE_SCENE_NOTES.md` に、各段階の選択理由を追記(発表・説明資料の下書き)
6. 展示用の仮設定(`kReturnToTitleAfterLaunch`)を、ゲームへ繋ぐときに `false` へ

## 7. 別セッションへ貼る指示文(コピペ用)

```
プロジェクト: C:\Users\haya2\source\repos\CG2-title\project
まず TITLE_SCENE_HANDOFF.md を全部読んでください(続いて TITLE_SCENE_NOTES.md と TITLE_SCENE_PLAN.md の §1・§6)。

状況: タイトルシーンの提出は完了。L0 に加えて、ノイズ平面雲・水面のムラ・気流・翼のプルーム・スタート演出・浮動原点まで入っている。雲ビルボードは試作して保留(art/title_clouds)。

守ること:
- git の書き込み(commit/push/branch)はしない
- 協働スタイル: コードは Claude が書き、書いたあとに詳しく解説する。理解チェックの問いを出す(ユーザーが後回しにしたいときは後回し)
- 編集前に、診断と方針を提案して了承を得る。シェーダーの色・強度を勝手に大きく変えない
- Release でも表示される(ImGui 依存にしない)

最初の作業: HANDOFF §6 の候補から、次に進めるものを提案してください(おすすめは、未回答の理解チェックを済ませてから、L1-2 雲ビルボードの再開か、L2 水面の平面反射)。
```
