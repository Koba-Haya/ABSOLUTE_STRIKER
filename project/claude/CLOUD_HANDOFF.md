# ボリューム雲(レイマーチ)引き継ぎ資料

次のセッションは、まずこのファイルを読むこと。ブランチは `RayMarching`。

## 1. 守るルール(ユーザーの常設ルール)

- `git commit / push / branch / merge` は実行しない。ユーザーがレビュー後に自分でやる。
- **修正の前に、原因と修正案を伝えて、承認をもらってから編集する。** 前セッションでこれを破って怒られた。
- ユーザーの「こうなってる」は症状の報告。修正指示とは限らない。まずコードで確認する。
- 1 回の作業は 1 つの目的。ステップごとに動作確認してから次へ。
- 日本語で説明。専門用語は初出で補い、「なぜそうしたか」も書く。
- コメントは日本語で細かく、絵文字なし。マジックナンバー禁止(名前付き定数)。1 クラス 1 責務。
- Release ビルドは ImGui を意図的に含まない。

## 2. 用語

- 新方式: 層(Layer)形のボリューム雲による雲海(`SeaSky` パス + ボリューム雲)
- 旧方式: 高さ場の `CloudSea`
- 箱の雲: ワールドに固定した箱のボリューム雲
- 雲突入: タイトルからゲームへの遷移 `CloudDive`(旧実装。`Transition.PS.hlsl`)

## 3. ロードマップ

- 完了: ステップ 0〜5、6-a、6-b、6-c
- 6-d: 旧 CloudSea の削除(ユーザーが見た目を承認してから)
- その後: 雲突入を新方式の層へ置き換え(A: 歪みとノイズを弱めるだけ / B: 新しい層で作り直し)
- **6-d と雲突入の作業は、下の「陽炎」問題が解決するまで保留。**

## 4. 実装の構成

描画の流れ(`Renderer::DrawVolumeCloudPass_`、`EndRenderScene` から呼ばれる):
深度の縮小(1 回)-> 雲ごとに、低解像度でマーチ -> 深度を見て拡大合成(3x3 ガウス × 距離の重み)。

| 場所 | 役割 |
|---|---|
| `AbsoluteEngine/resources/shaders/VolumeCloud.hlsli` | 密度、マーチ、ライティング、ハイライト圧縮、ブルーノイズのジッタ |
| `VolumeCloudParam.hlsli` | 3 パス共有の定数バッファ。C++ の `Renderer::VolumeCloudParam` と並びを揃える |
| `VolumeCloud.PS.hlsl` / `VolumeCloudDepth.PS.hlsl` / `VolumeCloudComposite.PS.hlsl` | マーチ / 深度縮小 / 拡大合成 |
| `SeaSky.PS.hlsl` + `graphics/SeaSkySettings.h` | 新方式の背景(空と、なめらかな下の層)。縞を避けるため模様なし |
| `graphics/VolumeCloudSettings.h` | 設定構造体。`MakeSeaLayerSettings()` が雲海の既定 |
| `graphics/VolumeCloudQuality.{h,cpp}` | 自動品質(4 段階、GPU 時間の EMA、ヒステリシス) |
| `graphics/texture/CloudNoise*` | 形 64³ RGBA8(Perlin-Worley)、細部 32³、ブルーノイズ 64²。起動時生成で 70〜200ms |
| `graphics/Renderer.*`, `pipeline/UnifiedPipeline.*` | `AddVolumeCloud`(最大 4)、`DrawSeaSky`、各パイプライン。ルートスロット t0/t1/t2 はパスごとに用途が違う |
| `Application/scene/GameScene.*`, `VolumeCloudDebugUi.*` | 新旧切替(既定は新方式)、ImGui 調整 UI |

主な既定値(層): bottom -44、top -20(-24 から未承認で変更中)、maxDistance 1000、fogDensity 0.0022、
noiseScale 0.025、detailScale 3.7、threshold 0.74、edgeFade 0.35、erosion 0.60、density 0.4、
sun 0.75、ambient 1.0、divisor 2、steps 32、lightSteps 4、highlightKnee 0.75、shadowLift 0.15。

## 5. Git の状態

- コミット `195f709`: ステップ 6-a 付近まで(GpuTimer、CloudNoise*、VolumeCloud*、Renderer、UnifiedPipeline 等)
- 未コミット: ブルーノイズ、合成のぼかし、6-b、6-c、下の「未承認の変更」
- 未追跡: `SeaSkySettings.h`、`SeaSky.PS.hlsl`、`project/art/cloud_tools/`
- `GameScene.cpp` は単独の CR を含むため、git がバイナリ扱いする。CRLF/LF に注意(作業ツリーは CRLF、index は LF)

## 6. 計測(Release、1280x720、RTX 4060 Laptop)

旧 CloudSea 2.2〜3.7ms / 新方式 約 1.9〜2.3ms(SeaSky 0.13〜0.35 + 層 1.4〜2.0)/
箱の雲 0.3〜1.8ms(画面いっぱい、divisor 1 で最大 3.3ms)/ 遷移 2〜6ms / フレーム合計 約 5ms。
Debug レイヤーは常にクリーン。遷移の引っかかり(GameScene 初期化 477ms)は別セッションで解決済み(約 31ms)。

## 7. 未解決: ゲーム中の「陽炎」(最優先)

症状: Release で、カメラが進むと、雲海の画面全体が陽炎のように揺れる。Development は、Start を押すまで
カメラも時間も止まっているので、同じ症状が出ない(静止画では再現しない)。ユーザーの GIF では、空の部分は
フレーム差ゼロで、変化は海側だけ。静止画でも、雲の下に縦方向のコピー(反射のようなもの)が見える。

原因の仮説(どれも未検証):

| 仮説 | 対策 | 規模 |
|---|---|---|
| 縦に相関のないノイズ層を、深い穴ごしに見る視差 | 雲底を詰めた「ソリッドベース」(現在適用中・未承認) | 小(平坦になる) |
| ステップの量子化によるちらつき | 表面の二分探索による精密化 | 中 |
| 画面固定のブルーノイズによる「沸き」 | 時間方向の蓄積(再投影) | 大 |
| 低解像度の縁のちらつき | divisor 1、または縁だけ高解像度 | 大 |

次のセッションでやること(**実装せず、まず提案**):

1. ユーザーに、未承認の変更を戻すか残すか聞く(推奨: 戻して 6-c の状態へ)。
2. 調査方法を提案して選んでもらう:
   - ユーザーが Development で Play 後に、Resolution Divisor 1 / Steps 64 / Wind 0 を 1 つずつ試す(どれで消えるかで原因が絞れる)
   - デバッグ限定の「一定速度でカメラを動かして連続フレームを保存する」モードを追加(設計を先に見せる)
3. 原因 -> 修正案 -> 承認 -> 編集、の順。

## 8. 未承認の変更(戻し方)

`VolumeCloud.hlsli` と `VolumeCloudSettings.h` に、ユーザーの承認なしに入れた変更がある。
戻す場合(6-c の状態):

- `VolumeCloud.hlsli` 53〜55 行付近: `kLayerSolidTop` / `kLayerSolidFadeEnd` / `kLayerVerticalNoiseScale` の定数を削除
- `SampleDensity` 内、`if (layer)` の `noisePos.y *= kLayerVerticalNoiseScale;` を削除
- 200 行付近: `cloud = max(cloud, 1.0f - smoothstep(kLayerSolidTop, kLayerSolidFadeEnd, h));` を削除
- `VolumeCloudSettings.h`: `layerTop = -20.0f` を `-24.0f` へ

(行番号は目安。`kLayerSolid` / `kLayerVerticalNoiseScale` で検索すること)

## 9. 6-d と雲突入(陽炎が解決してから)

- 6-d で消すもの: `CloudSea.hlsli`、`CloudSea.PS.hlsl`、`CloudSeaSettings.h`、`CloudSeaDebugUi.{h,cpp}`、
  Renderer の `DrawCloudSea` / パイプライン / CB / `SplitCloudSeaCell`、`UnifiedPipeline::MakeCloudSeaDesc`、
  GameScene の旧モード切替と `cloudSea_`、vcxproj の項目
- 雲突入の置き換え: A(歪みとノイズを弱めるだけ)/ B(新しい層で作り直し)。比較して提案する

## 10. ビルドと検証の注意

- MSBuild: `C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe`、`DirectXGame.sln`、構成は Debug / Development / Release
- シェーダ単体確認: `C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\dxc.exe -T ps_6_0 -E main -HV 2021 -I <shaders フォルダ>`
- Windows の `min` / `max` マクロ対策で `(std::min)` と書く
- **ユーザーが起動中の Application.exe は絶対に kill しない**(exe がロックされてビルドが失敗したら、ユーザーに伝える)
- Debug レイヤーは Debug 構成のみ。Release に ImGui はない
- アセットは exe 相対(`AssetPath`)。配布確認は日本語名フォルダで、ソースなしで行う

## 11. 検証スクリプト(`project/art/cloud_tools/`)

- `run_and_capture.ps1`: 起動 -> SPACE(スキャンコード送信。SendKeys は効かない)-> 指定秒で PNG 保存。保存先の既定は `%TEMP%\cloud_shots`
- `burst.ps1`: SPACE の後に連続撮影(約 40ms 間隔)
- `analyze_burst.py <フォルダ>`: 連続フレームの差を測る。**タイミングがゲームに揃わず、数値がばらつく(1.8〜5.7)ので、比較には使えない**
- 陽炎の検証には、上の「一定速度のカメラ移動モード」のような、決定的な方法が要る

## 12. 見た目の残課題(陽炎のあと)

わずかな粒状感、雲の下の縦の青い筋(太陽が前方のとき、影の面)、地平線付近の帯、空の段差の可能性、太陽付近のまぶしさ。
