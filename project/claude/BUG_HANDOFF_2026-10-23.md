# バグ調査の引き継ぎメモ(2026-10-23)

カプセル化リファクタリングの動作確認中に見つかった2件のバグの調査結果。
調査したセッションでは修正していない。別セッションで修正する想定でまとめる。

- 作業ツリーはすべて未コミット(コミットはしていない)。
- 下記の「確認済み」はコードを読んで確かめた事実、「仮説」は未検証。実機での再現はしていない。

---

## バグA: ウィンドウを最大化すると、レイアウトの右側と下側が切れて全画面に表示される

### 症状
右上の最大化ボタンを押すと、Hierarchy / Inspector(右側)と GameScene Controls / Timeline Editor(下側)が見えなくなり、
左上寄りの一部のパネルだけが拡大されて画面全体に出る。
スクリーンショットでは Viewport が右端まで広がり、文字も大きく見える。

### 結論(確認済み)
原因は ImGui の Viewport パネルではなく、スワップチェーン(裏画面)が起動時のサイズで固定されていること。
ウィンドウのサイズ変更に追従する処理が、プロジェクト内に存在しない。

| 事実 | 場所 |
|---|---|
| スワップチェーンは `clientWidth_` x `clientHeight_`(1280x720)で作られ、その後リサイズされない | `AbsoluteEngine/base/DirectXCommon.cpp` の `CreateSwapChain_()` |
| ビューポートとシザー矩形も 1280x720 で固定 | 同 `SetupViewportAndScissor_()` |
| `WM_SIZE` / `ResizeBuffers` の処理がない | `AbsoluteEngine` 以下を検索して 0 件 |
| ウィンドウは `WS_OVERLAPPEDWINDOW` なので、最大化とサイズ変更ができる | `AbsoluteEngine/base/WinApp.cpp` |
| ImGui の画面サイズ(`DisplaySize`)は、毎フレーム現在のクライアント領域から取られる | `externals/imgui/imgui_impl_win32.cpp` 515-516行 |
| ImGui の描画は `DisplaySize` をそのまま描画範囲にする | `externals/imgui/imgui_impl_dx12.cpp` 260-265行 |

### 起きていること
1. 最大化すると、ImGui は大きなクライアント領域(例: 論理 1536x864)を前提にレイアウトする(`GameApp.cpp` のフルスクリーン DockSpace は `WorkSize` に追従する)。
2. 描画先の裏画面は 1280x720 のまま。そのため、レイアウトの左上 1280x720 の範囲だけが描かれ、右側と下側は範囲外になって見えない。
3. 完成した 1280x720 の画像が、DXGI の既定の拡大(ストレッチ)で全画面に引き伸ばされる。

「一部を映している」のではなく、描画範囲が切れたものが全画面に拡大されている、という状態。
(補足: 非最大化時のスクリーンショットが 1602 ピクセル幅なのは、Windows の表示倍率 125% のため。論理サイズは 1280x720。)

### 直し方の選択肢
| 案 | 内容 | 難易度 | 備考 |
|---|---|---|---|
| 1. 最大化・サイズ変更を禁止する | `WS_OVERLAPPEDWINDOW` から `WS_THICKFRAME` と `WS_MAXIMIZEBOX` を外す | 小 | 最も安全。固定 16:9 で作っているなら、これで十分。`AdjustWindowRect` にも同じスタイルを渡すこと |
| 2. リサイズに対応する | `WM_SIZE` で `ResizeBuffers` を呼び、RTV・ビューポート・シザー・深度バッファ、さらにポストプロセスやクラウド用のオフスクリーン描画先を作り直す | 大 | 描画先の種類が多い。最小化(サイズ 0)の扱いも必要。カメラのアスペクト比も更新する必要がある |
| 3. 論理解像度を固定したまま拡縮 | ImGui の `DisplaySize` を 1280x720 に固定する | 中 | マウス座標の変換が必要になる |

おすすめは 1。固定解像度で作っていて、リサイズ対応の予定がないなら、最小の変更で確実に直る。

### 補足(別の小さな問題・今回の症状の原因ではない)
`AbsoluteEngine/editor/EditorUIManager.cpp` の `DrawViewport` は、`ImGui::Image(画像, パネルのサイズ)` で、画像を縦横比を無視してパネルいっぱいに引き伸ばす。
パネルの縦横比が 16:9 でないと、ゲーム画面がわずかに歪む。
直すなら、16:9 を保って収まる最大サイズで表示する。ギズモやマウス判定が、このパネルのサイズを前提にしている点に注意。

---

## バグB: タイムラインエディタのシークバーを動かすと、敵が2倍になる

### 症状(ユーザーの観察)
- 2倍になるのは、タイムラインエディタのシークバーを動かしたときだけ。
- その後に Play(Start)ボタンを押すと、2倍になっていない。

### 先に撤回する仮説
「Edit 中にシークして敵を出し、Play/Stop したときにバックアップから敵が復元されて2倍になる」という前回の仮説は、
上の観察(Play 後は2倍でない)と合わないので撤回する。

### 確認済みの事実
- `Game.json`(シーン)には、敵は入っていない(RailCamera / Player / Directional Light の3つだけ)。
- `Game_timeline.json` には、19個のイベント(SpawnEvent 11個、FormationSpawnEvent 8個)が、4秒から68秒まで入っている。同じ時刻・同じ種類の重複はない。したがって、データ上の重複ではない。
- シーク時の流れ: `TimelineEditorWindow::DrawSeekBar` のスライダーが動くたびに(ドラッグ中は毎フレーム)`TimelineManager::Seek` が呼ばれる。
  各トラックの `Update(現在, 前回)` が、前進なら「時刻以下で未発火」のイベントを `Fire()`、後退なら「時刻より後で発火済み」のイベントを `Rewind()` する。
- `SpawnEvent::Fire()` は、スポーン済みなら何もしない(`spawnedObject_` が生きていればスキップ)。`FormationSpawnEvent::Fire()` も、スポーン済みなら何もしない。
- `SpawnEvent::Rewind()` は `Destroy()` を呼ぶが、これは `isActive_ = false` にするだけ。実際に `rootObjects_` から消えるのは、`BaseScene::UpdateEditor()` の最後のガベージコレクションの時点。
- 描画側(`ModelComponent` など)に `IsActive()` の確認は見当たらなかった。Rewind したオブジェクトは、消されるまで1フレーム描かれる可能性がある(要確認)。
- `TimelineEditorWindow` のレーンでイベントを選択すると、`BaseScene::UpdateEditor()` がプレビュー用のオブジェクト(`SetTimelinePreview(true)`)を `AddRootObject` で追加する。プレビューは同じプレハブで、保存からは除外される。シークで出た本物の敵と同じ位置にプレビューが重なると、2倍に見える可能性がある。

### 仮説(未検証)
| # | 仮説 | 確認方法 |
|---|---|---|
| H1 | オブジェクトの数は増えておらず、同じ敵が2回描画されているだけ | シーク前後で、Hierarchy の項目数を数える。増えていれば H1 は否定される |
| H2 | プレビュー用オブジェクトと、シークで出た本物の敵が重なっている | イベントを選択していない状態(`GetSelectedEvent` が空)でシークして、再現するか確認する |
| H3 | `Rewind` で `Destroy()` した敵が、同じフレームの描画で残り、新しく `Fire` された敵と重なる | 後退してから前進する操作でだけ起きるか確認する(前進だけで起きるなら否定) |
| H4 | 今回のリファクタリングによる退行 | 下の「変更前との比較」で確認する |

### 切り分けの手順
1. 再現条件を絞る: (a) 前進だけ、(b) 後退してから前進、(c) イベントを選択した状態としていない状態、(d) Edit モードか Play 中か。
2. Hierarchy の項目数を、シーク前後で数える(H1 の確認)。
3. `SpawnEvent::Fire` / `Rewind` と `FormationSpawnEvent::Fire` / `Rewind` に、ログを入れる(呼ばれた回数と、スポーンしたオブジェクトのアドレス)。
   同じイベントで `Fire` が2回出れば、発火判定の問題。1回なのに2倍なら、描画か別経路の問題。
4. 変更前のコミットでビルドして、同じ操作で再現するか比べる(下記)。

### 変更前との比較(退行かどうかの確認)
```bash
git stash
git checkout dbd5db8
```
上は例。作業ツリーに未コミットの変更があるので、安全には別の作業ディレクトリ(`git worktree add ../CG2_base dbd5db8`)でビルドするのがよい。
比較後は、元のブランチ(`class-design`)に戻し、`git stash pop` で変更を戻すこと。

### 今回のリファクタリングが触った、タイムライン関連の場所(退行の疑いがある場合の確認箇所)
- `AbsoluteEngine/scene/timeline/TimelineTrack.h` / `.cpp`: 発火時刻と発火済みフラグを、イベントからトラックの内部(`Entry`)に移した。`Update` / `RewindAll` / `AddEvent` / 並べ替え(`stable_sort`)を書き換えた。
- `AbsoluteEngine/scene/timeline/TimelineManager.h` / `.cpp`: `GetTracksRef` を `GetTrack(index)` に変更。`GetEventTriggerTime` / `SetEventTriggerTime` を追加。
- `AbsoluteEngine/scene/timeline/TimelineEditorWindow.cpp`: レーン描画を新しい API に変更。
- `AbsoluteEngine/editor/EditorUIManager.cpp`: インスペクタの時刻変更を、マネージャー経由に変更。
- `AbsoluteEngine/scene/BaseScene.h` / `.cpp`: `rootObjects_` を private にし、`GetRootObjects` / `AddRootObject` / `RemoveRootObject` / `RemoveRootObjectsIf` 経由に変更。`PlayTimeline()` を追加。
- 論理上は、`Fire` / `Rewind` の判定条件は変えていない(前進は「時刻以下かつ未発火」、後退は「時刻より後かつ発火済み」)。

---

## 作業ツリーの状態(参考)
- ブランチは `class-design`。すべて未コミット。
- 次のファイルは、このリファクタリングの変更ではなく、ユーザー自身の変更: `Application/scene/StageStartDemoMode.h`(`kEnabled` を `false` に変更)。
- Debug と Release の x64 ビルドは、直近の変更後に成功している(`project/DirectXGame.sln`)。
- ユーザーによる実機確認は、BaseScene のカメラ・オブジェクト一覧の変更までは問題なし。タイムラインの変更後に、上記2件が見つかった。
