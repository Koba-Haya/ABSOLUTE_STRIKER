# 基盤スプリント 実行指示書(〜2026-10-02 金)

最終更新: 2026-09-29 / 長期計画は [PROJECT_ROADMAP.md](PROJECT_ROADMAP.md) §2.5 / 直前の短縮スプリントは [SPRINT_3DAYS.md](SPRINT_3DAYS.md)

**この文書だけで作業を開始できるように書いてある。** 実装を依頼されたら、まず「§1 進め方のルール」と該当タスクの節を読むこと。
行番号は 2026-09-29 時点(`master` = 6e71bb8)。ズレていたら関数名で探すこと。

---

## 0. 目的と完了の定義

**目的**: 演出・作り込みに入る前に、遊びの土台(照準 → 発射 → ロック → 相殺 → HP/スコア表示)を確立する。
10/19(月)の進捗発表までに、技術発表用の要素を作る時間を確保するため、基盤は**金曜までに完了**させる。

**完了の定義**(金曜夜にすべて YES なら完了)
- [ ] 通常弾がレティクルの指す位置(画面上)に当たる
- [ ] ロック中にボタンを離すと、ロック済みの敵にだけホーミング弾(リボンビーム付き)が飛ぶ。ロック対象が0体なら通常弾になる
- [ ] 自弾(通常弾)が「撃ち落とせる敵弾」を相殺できる。どの敵弾が撃ち落とせるかはプレハブ側の設定値で決まる
- [ ] スコアと体力が、ImGui なしの画面(Release構成)に表示される
- [ ] キーボード+マウス、パッドの両方で一通り操作できる
- [ ] GameScene.cpp が 1191行 → **800行未満**(挙動は変えない)
- [ ] Development / Release 両構成でビルドが通り、Release で通しプレイできる

**このスプリントのスコープ外**(触らない)
- タイトル画面とその演出(別課題。`TITLE_SCENE_PLAN.md` を参照。`TitleScene.cpp` / `SceneFactory.cpp` は編集しない)
- 回避演出(D)、ボスHPバー、ボス弱点、サウンド、クリア/ゲームオーバーのUI、レベルデザイン

---

## 1. 進め方のルール

1. **計画内のタスクは承認済み**。ただし着手前に該当コードを読み、下の「現状診断」と食い違ったら、修正案を提示してから進める。
2. **計画外の不具合を見つけたら**、診断と修正案を提示してユーザーの承認を待つ(勝手に直さない)。
3. **触ったファイルの隣接する既存の不備**(未使用関数、重複、マジックナンバー、コメントアウト放置)は、ついでに直す。
4. `Release` 構成に **ImGui を入れない**(意図的な設計)。`Debug` / `Development` は `USE_IMGUI` あり。ゲームの挙動は `#ifdef USE_IMGUI` に依存させない。
5. **git の書き込み操作は一切しない**(`commit` / `push` / ブランチ作成 / `stash` / `reset` など)。変更はワーキングツリーに残し、確認・コミット・push はユーザーが行う。タスクが終わるたびに「変更したファイルの一覧」と「確認してほしい動作」を報告する。
6. 読み取り専用の git(`status` / `diff` / `log`)は使ってよい。
7. `Application/*.bak` などのバックアップファイルは触らない(`.gitignore` で除外済み・未追跡なので、削除も不要)。
8. カメラは完全レール固定で**後ろは見られない**。敵の攻撃は前方の画面内で完結させる。
9. コード品質の基準([PROJECT_ROADMAP.md](PROJECT_ROADMAP.md) §4): enum/データ駆動、`unique_ptr`/`weak_ptr`、マジックナンバーは定数化、命名統一、未使用コードを残さない、文字列比較で分岐しない。

**確認方法**: 自動テストは無い。各タスクの「完了条件」を、Development 構成で実際にプレイして目視確認する。

---

## 2. 現状診断(コード確認済み)

### 2.1 通常弾がレティクル通りに飛ばない原因

[PlayerComponent.cpp:328](Application/actor/Player/PlayerComponent.cpp:328) で `camDirCached_` を「カメラ位置から、カーソルを通るレイの方向」として保存している。
通常弾はその方向を**そのまま速度にして、右手ボーン位置(`handPos`)から発射**している([PlayerComponent.cpp:483-493](Application/actor/Player/PlayerComponent.cpp:483))。

- 弾は「カメラのレイと平行」に飛ぶが、発射点は**カメラ位置ではなく手の位置**なので、手とカメラのオフセット分だけ、常に照準からズレて着弾する。
- 3重レティクル(near/mid/far)も、自機位置から `camDir` へ平行に伸ばして配置している([PlayerComponent.cpp:345-362](Application/actor/Player/PlayerComponent.cpp:345))。**弾の実際の軌道(手→着弾点)とは別の線**を描いている。

**画面端・移動中にずれる追加原因**(コードからの推定。実機での再現は未確認なので T2 で検証する)

- **自機の位置がカーソルのレイ上に無い**: 自機は「カメラ正面 `cameraDistance_(22)` の平面上」に置かれ、横/縦位置はレイとの差の Right/Up 成分だけで決まる([PlayerComponent.cpp:242-257](Application/actor/Player/PlayerComponent.cpp:242))。レイの前方成分(距離22の点で `22·cosθ`)を捨てているため、**画面端ほど自機がレイの内側にずれる**(θ=30°で約1.7ユニット)。
- 遠レティクルは `自機位置 + camDir×60` なので、そのずれをそのまま引き継ぐ。一方、弾の方向は `camDir`(カーソルのレイ)。**画面に描かれるレティクルと、弾の実際の軌道が別物**になり、端ほどずれが大きくなる。
- **移動中**: 自機は目標位置へ `Lerp`(`moveSpeed_=4`)で遅れて追従する([PlayerComponent.cpp:246](Application/actor/Player/PlayerComponent.cpp:246))。遅れている間は、自機位置から作るレティクルがカーソルからずれる。弾の方向はカーソル基準なので、見た目と食い違う。
- **1フレーム遅れの疑い**(要検証): `RailCameraComponent::Update` がカメラの eye/target を更新した後に、ビュー行列がいつ再計算されるかで、`PlayerComponent::Update` が前フレームの行列を使う可能性がある。右手ボーンの行列も、前フレームのアニメーション結果の可能性がある。

**修正方針**: カメラのレイ上に「照準点(AimPoint)」を決め、弾は `手 → AimPoint` の方向へ飛ばす。レティクル3つも同じ「手 → AimPoint」の線上に置く。
**この修正で、上の「レティクルと弾の軌道のズレ」(端・移動中とも)は構造的に解消する**: 遠レティクルは AimPoint 自体なので画面上のカーソル位置に一致し、弾はその点へ向かう。自機の位置ずれには依存しなくなる。
**直らないもの**: 動いている敵に対しては、通常弾は直進するので外れる(仕様。予測射撃は入れない)。また上の「1フレーム遅れ」があれば残るので、T2 の検証で確認する。

### 2.2 ロック関連

- [LockOnSystem.h](Application/actor/Player/LockOnSystem.h): ターゲットは `weak_ptr` 保持で、参照切れの基本対策は済んでいる。
- 一方で、ロックモード中に対象が0体のままボタンを離すと、**何も発射されない**([PlayerComponent.cpp:420-465](Application/actor/Player/PlayerComponent.cpp:420))。
- 対象の選別が `GetTag()` と `GetName().find("Enemy")` の文字列比較([LockOnSystem.cpp:62-66](Application/actor/Player/LockOnSystem.cpp:62))。死亡判定も `EnemyComponent` / `BossComponent` を個別に見ている。
- [LockOnSystem.cpp:155](Application/actor/Player/LockOnSystem.cpp:155) の `FireHomingBullets` は中身のない死んだ関数。
- 画面サイズ 1280x720 が LockOnSystem / PlayerComponent / GameHUD にバラバラにハードコードされている。

### 2.3 相殺・弾の識別

- 弾の識別はすべて文字列タグ(`"PlayerBullet"` / `"EnemyBullet"` / `"Enemy"` / `"Boss"` / `"Player"`)と `GetName().find(...)`。
- [BulletComponent.cpp:30-44](Application/actor/Bullet/BulletComponent.cpp:30): 自弾と敵弾は互いに反応しないようにしてある(コメントで明記)。
- 衝突判定は [CollisionManager.cpp:26-51](AbsoluteEngine/scene/CollisionManager.cpp:26) が**全コライダーの総当たり**で、両方の `OnCollision` を呼ぶ。弾同士の衝突通知はすでに来ているので、`OnCollision` 側のロジックだけで相殺は実装できる。
  - 総当たり(O(n²))だが、判定は球同士の距離比較だけで軽い。現在の規模(弾+敵で数十〜100前後)なら問題にならない(100個で約5,000ペア/フレーム)。コライダーが数百個を超えるあたりから負荷が見え始めるので、**T5 の後にフレーム時間だけ確認する**。
  - コライダーのクラスとタグは「当たった後に何をするか」のためのもので、**判定の絞り込みには使われていない**(タグ/陣営でペアを事前に除外する仕組みは無い)。必要になれば、`Faction` を使ったペアの除外をエンジン側に足せる(エンジン層の変更なので、その時は相談する)。

### 2.4 弾の生成コードの重複

弾を組み立てるコード(GameObject + ModelComponent + ColliderComponent + 弾コンポーネント)が**5箇所**にコピーされている。

| 場所 | 種別 |
|---|---|
| [PlayerComponent.cpp:432-463](Application/actor/Player/PlayerComponent.cpp:432) | 自機ホーミング弾 |
| [PlayerComponent.cpp:471-499](Application/actor/Player/PlayerComponent.cpp:471) | 自機通常弾 |
| [EnemyShootComponent.cpp:94](Application/actor/Enemy/EnemyShootComponent.cpp:94) | 敵通常弾 |
| [TurretShootComponent.cpp:112](Application/actor/Enemy/TurretShootComponent.cpp:112) | タレット弾 |
| [BossComponent.cpp:142](Application/actor/Enemy/BossComponent.cpp:142) | ボス弾 |

### 2.5 PlayerComponent の肥大化

`PlayerComponent::Update` が約440行。カーソル→ワールド変換(NDC逆投影)のコードが**2回**コピーされている([PlayerComponent.cpp:186-226](Application/actor/Player/PlayerComponent.cpp:186) と [同293-328](Application/actor/Player/PlayerComponent.cpp:293))。
入力判定も分散: 移動は [PlayerComponent.cpp:88-101](Application/actor/Player/PlayerComponent.cpp:88)、回避は :130、発射は :395、ロック押下は [LockOnSystem.cpp:34](Application/actor/Player/LockOnSystem.cpp:34)。

### 2.6 入力

- [Input.h](AbsoluteEngine/base/input/Input.h): キーボード / マウス / XInput パッドが実装済み。
- **マウスは現状「発射ボタン」としてのみ使用**。カーソルは**キーボード(WASD/矢印)とパッド左スティックでしか動かない**。
- マウスは DirectInput の**相対移動量**(`MouseState::dx/dy`)しか取れず、絶対座標は無い。

### 2.7 GameScene(1191行)の中身

- `Update`(526〜約950行)に、カメラシェイク/各種ポストエフェクトのタイマー更新(:531-600 付近)、ポストエフェクト優先度チェーン(:608-)、スポーン管理、HUD更新([GameScene.cpp:885-925](Application/scene/GameScene.cpp:885)付近)、フェーズ判定(:851)が全部入っている。
- ポストエフェクト系のメンバ変数([GameScene.h:126-154](Application/scene/GameScene.h:126))が大量にある。
- スコア: `score_`、`killCount_`、編隊の残数管理(`formationRemaining_` / `formationTotal_`)が GameScene 内にある。表示は ImGui のみ([GameScene.cpp:1110](Application/scene/GameScene.cpp:1110))。
- 未使用/放置: [GameScene.h:69](Application/scene/GameScene.h:69) の `//void InitSpawnEvents_();`、`SpawnEvent` 構造体(タイムライン駆動に置換済みなら不要)。

### 2.8 HUD

- [GameHUD](Application/hud/GameHUD.h): 3重レティクル、ロックカーソル、**HP表示(円スプライト、最大10個)は実装済み**。
- `Update` の引数が9個と多い(構造体にまとめるべき)。
- **スコア表示は無い**。エンジンに文字描画機能は無い(`resources/fonts` にTTFはあるが描画コードは無い)。→ 数字スプライトで作る。

---

## 3. タスク

**実行順(依存順)と見積もり**は下の表のとおり。見積もりは目安。稼働は1日4〜7時間。

| 順 | ID | タスク | 見積もり | 金曜までに |
|---|---|---|---|---|
| 0 | T0 | 準備(ベースライン確認) | 0.5h | 必須 |
| 1 | T1 | GameScene分割 + PlayerComponent整理 + 入力抽象化 + 弾の生成の共通化 | 6〜9h | 必須 |
| 2 | T2 | 照準(A)とホーミング発射条件(A') + ロック管理 | 5〜7h | 必須 |
| 3 | T3 | 弾の陣営/種別 + 相殺機能(C-fn) | 3〜4h | 必須 |
| 4 | T4 | HUD最小版(スコア表示、HUD構造整理) | 4〜5h | 必須 |
| 5 | T5 | リボンビーム(B)。まず最小版 | 6〜10h | 最小版だけ。間に合わなければ翌週前半 |
| 6 | T6 | Release構成の通しプレイ確認 | 1h | 余力があれば |

目安: 火 T0+T1 着手 / 水 T1 完了→T2 / 木 T3→T4 / 金 T5(最小版)→T6。
**遅れたら T5 の作り込みと T6 を翌週に送る**(T1〜T4 は落とさない)。

---

### T0. 準備

- `git status` で作業ツリーの状態を確認する(書き込み操作はしない。§1 ルール5)。
- Development 構成でビルドし、そのままタイトル→プレイ→ボス→クリアの通しができることを1回確認(退行検知の基準)。
- ターゲット構成は `Debug` / `Development` / `Release`(`.github/workflows` に CI 3種あり)。

---

### T1. GameScene分割 + PlayerComponent整理 + 入力抽象化 + 弾の生成共通化

**ゴール**: T2〜T4 が触る箇所を、肥大したクラスから切り出す。**挙動は変えない**(リファクタリング)。

**やること**(この順)

1. **入力の抽象化**: `Application/actor/Player/PlayerInput.h/.cpp`(名前は既存の命名規則に合わせる)を新設し、生のキー/パッド判定を1箇所に集約する。
   - 出力(アクション単位): カーソル移動量(x,y) / 発射ボタン(押下中・押した瞬間・離した瞬間) / 回避トリガー。
   - **既存の割り当てを維持**: 移動=WASD・矢印・パッド左スティック(デッドゾーン0.2) / 発射・ロック=Space・パッドA・マウス左(押し続けるとロック、離すと発射) / 回避=左Shift・パッドB。
   - **マウス照準を追加**: マウスの相対移動量(`GetMouse().dx/dy`)でカーソルを動かす。感度は定数化。キーボード・パッドと共存させる。
     - 動作はパッドのスティックと同じ(カーソル=レティクルが画面内を動き、自機がそれに追従する。パンツァードラグーンの操作感)。
     - **Playモード中のみ**、OSのマウスカーソルを非表示にし、ウィンドウ内に閉じ込める(エディット中はこれまでどおりマウスを使えるようにする)。
   - PlayerComponent と LockOnSystem の両方が `PlayerInput` を経由するようにする(LockOnSystem 内の `input->PressKey(...)` 直呼びを無くす)。
2. **照準計算の切り出し**: カーソル→カメラレイ(NDC逆投影)の重複コード2箇所を、1つの関数(例: `ReticleAim` クラス or 静的関数)にまとめる。T2 で中身を作り直すため、ここでは挙動を変えず切り出すだけ。
3. **BulletFactory の新設**: `Application/actor/Bullet/BulletFactory.h/.cpp`。§2.4 の5箇所の弾生成を置き換える。
   - 関数例: `CreatePlayerNormal(...)`, `CreatePlayerHoming(...)`, `CreateEnemy(...)`。GameObject の名前、タグ、モデル、コライダー、コンポーネントの組み立てをここに集約する。
   - T3 で「陣営/種別/撃ち落とし可否」の引数を追加するので、引数は構造体(例: `BulletSpec`)で受ける形にしておく。
4. **GameScene の分割**(目標: 800行未満)。
   - `PostEffectDirector`(新規クラス): ポストエフェクト系のタイマー・パラメータ・優先度チェーン(`GameScene.h:126-154` のメンバと `Update` の該当ブロック)を丸ごと移す。GameScene からは「イベントを通知する」だけにする。
   - `ScoreTracker`(新規クラス): `score_`、`killCount_`、編隊の残数管理を移す。T4 のスコア表示は、これを参照する。
   - HUD 更新のグルー処理([GameScene.cpp:885-925](Application/scene/GameScene.cpp:885) 付近)を、GameScene の private 関数か別クラスに抜き出す。
5. **後始末**: `LockOnSystem::FireHomingBullets`(死んだ関数)を削除。`//InitSpawnEvents_` のコメントアウトと未使用の `SpawnEvent` 構造体を、参照が無ければ削除。
6. 画面サイズ 1280x720 の定数を1箇所(共通ヘッダ)にまとめ、PlayerComponent / LockOnSystem / GameHUD から参照する。

**触るファイル**: `Application/scene/GameScene.{h,cpp}`、`Application/actor/Player/*`、`Application/actor/Bullet/*`(新規含む)、`Application/actor/Enemy/{EnemyShoot,TurretShoot,Boss}Component.cpp`(弾生成の呼び出し先変更のみ)、`Application/hud/GameHUD.*`(画面サイズ定数のみ)。プロジェクトファイル(`Application.vcxproj` と `.filters`)への新規ファイル登録を忘れないこと。

**完了条件**
- [ ] ビルドが通り、T0 で確認した通しプレイが同じように動く(撃つ/ロック/回避/被弾/ポストエフェクトの発火)
- [ ] GameScene.cpp が 800行未満
- [ ] 弾の生成箇所が `BulletFactory` のみ
- [ ] LockOnSystem / PlayerComponent に、生のキー/パッド/マウス判定が残っていない(`PlayerInput` 経由)
- [ ] マウス移動でカーソルが動く

---

### T2. 照準(A)とホーミング発射条件(A')+ロック管理

**ゴール**: 通常弾がレティクル通りに当たる。ホーミングはロック対象がいる時だけ飛ぶ。

**仕様**

- **AimPoint の決め方**(逆投影の行列計算は使わない。既存の重複した逆投影コード2箇所は、この式に置き換えて削除する):
  1. **カメラ空間の固定奥行き平面から求める**: `AimPoint = eye + forward×D + right×(ndcX×halfW) + up×(ndcY×halfH)`。`halfH = D×tan(fov/2)`、`halfW = halfH×aspect`。`D` は定数 `kAimMaxDistance`(まず PlayerComponent.cpp 冒頭の `kFarReticleDist=60` 相当から調整)。`ndcX/ndcY` はカーソルのスクリーン座標から求める。視野角とアスペクト比は、カメラから取得する(1280/720 の直書きをしない)。
  2. **奥行きは常に固定**(敵の有無で変えない)。弾は `手 → AimPoint` の直線上を飛び、**途中に敵がいれば通常の衝突判定で当たる**。レティクル3つもこの線上にあるので、弾が通る線はプレイヤーに見えている。
  3. `AimPoint` は画面に投影すると**カーソルの位置に一致する**こと(遠レティクル=AimPoint として描くため)。
  - (採用しない案・参考) 手がカメラ軸から外れている分、「遠レティクルが敵に重なって見えるのに、弾は敵の横を通る」ことが起こりうる(ずれは手のずれの半分程度)。プレイして問題と感じた場合のみ、カメラからの線と敵の球の交差で `D` を縮める補正を、この関数1箇所に後付けする。今は実装しない。
- **通常弾**: 発射方向 = `normalize(AimPoint − handPos)`。速度は現行の `kBulletSpeed=50`。
- **3重レティクル**: near/mid/far を「`handPos → AimPoint` の線分上」に置く(例: far=AimPoint、mid/near は手側に補間)。これで、far がカーソルの画面位置に一致し、near/mid が弾の軌道上に並ぶ(パンツァードラグーン風の見え方)。`PlayerComponent` の getter(`GetNear/Mid/FarReticleWorldPos`)は維持し、`GameHUD` 側は変更しなくてよい。
- **ホーミング発射条件**: ボタンを離した時に、
  - ロック済みの有効な敵が1体以上 → ホーミング弾を全対象に発射(現行どおり)。
  - **0体 → 通常弾を発射**(ロックモード中に何も出ない現状を解消)。通常弾の連射制限(`shootCooldown_`)は通常どおり適用。
  - 発射直前に `weak_ptr::lock()` と生存チェックを再確認する(現行のとおり)。
- **ロック管理の整理**:
  - 「ロック可能か」「生きているか」を、`EnemyComponent` / `BossComponent` を個別に見る代わりに、共通の窓口(例: `ILockable` 相当のインターフェース、または `IsAlive()` を持つ共通の基底/ヘルパー)に集約する。
  - 対象の選別を、タグ/名前の文字列検索から、この窓口経由に変える。
  - 画面外に出た/カメラの後ろに回った対象も、ロックから外す(現状は死亡と破棄のみ)。

**触るファイル**: `PlayerComponent.{h,cpp}`、`LockOnSystem.{h,cpp}`、T1 で作った照準クラス、`BulletFactory`、必要なら `EnemyComponent` / `BossComponent`(共通窓口の実装)。

**検証用の補助(T2 の最初に入れる)**: Development 構成のみ、「手 → AimPoint」の線をデバッグ線で描き、レティクル3つがその線上に並ぶことを目視できるようにする(既存の `PrimitiveDrawer` / デバッグライン描画を使う)。
**1フレーム遅れの確認**: カメラ更新→`PlayerComponent::Update` の順序、ビュー行列の更新タイミング、ボーン行列の更新タイミングを調べて報告する。遅れがあれば、修正案(更新順の調整など)を提示して確認を取る。

**完了条件**
- [ ] 画面の四隅・四辺の中央にカーソルを置いて撃っても、弾がレティクルの位置に向かって飛ぶ(斜めに逸れない)
- [ ] 自機を大きく動かしている最中に撃っても、同様にレティクルの位置に飛ぶ
- [ ] 遠くの敵・近くの敵の両方で、レティクルの far を敵に重ねて撃つと当たる
- [ ] カーソルを画面端に動かしても、far レティクルの位置に弾が着弾する(手の位置とのズレが無い)
- [ ] 敵がいない空間を撃っても、弾がレティクル方向に飛ぶ
- [ ] ロック0体でボタンを離すと通常弾が出る。ロックが1体以上なら、その敵にだけホーミング弾が出る
- [ ] ロック中に対象が撃破/画面外になっても、クラッシュせず、発射時に無効な対象は無視される
- [ ] マウス/パッド/キーボードのどれでも同じ結果になる

---

### T3. 弾の陣営/種別 + 相殺機能(C-fn)

**ゴール**: 「どの敵弾が撃ち落とせるか」を弾の属性として持たせ、自弾との衝突で相殺できるようにする。**どれを撃ち落とせるかの調整はレベルデザインの仕事なので、ここでは機能と設定口だけ作る**。

**仕様**

- `Application/actor/Bullet/BulletTypes.h`(新規)に定義する。
  - `enum class Faction { Player, Enemy }`
  - `struct BulletSpec`(T1 で作ったもの)に、`Faction faction`、`bool shootable`(撃ち落とせるか)、ダメージなどを持たせる。
  - `BulletComponent` はこの値を保持し、`OnCollision` の分岐を文字列タグではなくこの属性で行う。
- **相殺のルール**
  - 自弾(通常弾)が `shootable == true` の敵弾に当たったら、**両方消える**。`shootable == false` の敵弾は、自弾を素通し(現行どおり反応しない)。
  - ホーミング弾は、相殺の対象外(敵/ボスを狙うため)。
  - 相殺の瞬間に、既存の小さなヒットエフェクトを出す。**イベントの発火点(コールバック or 関数)は必ず残す**(後でSEやスコア加算をつけるため)。
- **設定口**: `shootable` を、敵弾を撃つコンポーネント(`EnemyShootComponent` / `TurretShootComponent` / `BossComponent`)のプレハブ設定(JSON)から指定できるようにする。既存の `DrawInspectorUI` / 保存/復元の仕組みに合わせる。
  - 動作確認用に、`resources/prefabs/EnemyTurret.json` の弾を `shootable=true` にする(他は false のまま)。
- タグ(`SetTag`)は、エンジン側の衝突通知や既存コード(`EnemyComponent.cpp:91` など)が使っているため、ファクトリ内で Faction から設定する形にして、**新しいロジックでは文字列比較を使わない**。既存の文字列比較は、触ったものから置き換える。
- 高速弾のすり抜けに注意: 自弾は50/秒 × 1/60秒 ≈ 0.83、当たり判定の半径和は 0.5+0.5=1.0 でぎりぎり。すり抜けが起きたら報告する(対策は相談)。

**触るファイル**: `BulletComponent.{h,cpp}`、`BulletFactory`、`BulletTypes.h`(新規)、`EnemyShootComponent` / `TurretShootComponent` / `BossComponent`、`resources/prefabs/*.json`。

**完了条件**
- [ ] タレットの弾を自弾で撃つと相殺される(両方消えてエフェクトが出る)
- [ ] 通常の敵弾は撃っても消えず、素通しする
- [ ] プレハブJSONの設定を変えると、撃ち落とせる弾が変わる(エディタのインスペクターからも変更できる)
- [ ] ホーミング弾は敵弾を無視する
- [ ] 相殺時のイベント発火点(後でSE/スコアを付けられる箇所)がある

---

### T4. HUD最小版(スコア表示 + HUD構造整理)

**ゴール**: ImGuiなしで、スコアと体力が見える。

**仕様**

- **スコア表示**: 画面右上に、6桁ゼロ埋めのスコアを数字スプライトで表示する。データは T1 の `ScoreTracker` から取る。
  - エンジンに文字描画が無いため、**数字スプライト(0〜9を横に並べた画像)**を使う。画像は `Application/resources/app/hud/digits.png` に新規作成する(例: 10個 × 64px の横長。`resources/fonts/Noto_Sans_JP/` のTTF(OFLライセンス)から Python+Pillow 等で生成してよい。生成手順は、完了報告に書く)。
  - 表示は `Sprite::SetUVTransform`([Sprite.h:40](AbsoluteEngine/graphics/2d/Sprite.h:40))で桁ごとにUVをずらして行う。
  - `Application/hud/` に数字表示専用のクラス(例: `NumberDisplay`)を作る。GameHUD がそれを持つ。
- **体力表示**: 既存の円スプライト(最大10個)をそのまま使う。見た目の作り込みは後回し。
- **ロック数表示**: 余裕があれば「LOCK n/8」を、同じ数字表示で出す。無ければ後回し。
- **GameHUD::Update の引数整理**: 9個の引数を、`HudData` 構造体(HP、最大HP、スコア、ロック位置、ロックモード、カーソル位置、カメラ、レティクル3点)にまとめる。GameScene のグルー処理もそれに合わせる。
- 表示は `#ifdef USE_IMGUI` に依存させず、**Release でも見える**こと。

**触るファイル**: `Application/hud/GameHUD.{h,cpp}`、`Application/hud/NumberDisplay.*`(新規)、`Application/resources/app/hud/digits.png`(新規)、`GameScene.cpp`(呼び出し側)。

**完了条件**
- [ ] 敵を撃破するとスコアが増え、画面右上の数字が更新される
- [ ] 被弾すると、体力表示が減る
- [ ] Release 構成でも、両方が表示される
- [ ] `GameHUD::Update` の引数が構造体になっている

---

### T5. リボンビーム(B)

**ゴール**: ホーミング弾を、パンツァードラグーン風の「ビーム状」に見せる。**方式は案2: 弾頭がベジエ曲線で飛び、その軌跡をリボン状ビームで描く**。ベジエ誘導([HomingBulletComponent](Application/actor/Bullet/HomingBulletComponent.h))は維持する。

**段階**

1. **調査(先に30分)**: `AbsoluteEngine/graphics/primitive`(`Ring` / `Cylinder` / `PrimitiveDrawer`)、`AbsoluteEngine/graphics/particle`(`ParticleManager::Emit`、`GPUParticleManager`、`RingEffect` など)を読み、**動的に頂点を書き換えて描く経路**が既にあるかを確認する。結果とともに、どちらの方式で作るかを報告して、ユーザーに確認する。
2. **最小版(金曜までの目標)**: 弾頭の過去位置を一定数(例: 直近0.2〜0.3秒分)リングバッファに保存し、それを結ぶ**カメラ向きの帯(ビルボード)**を、加算合成で描く。
   - 先頭が太く、末尾に向かって細く・薄くなる(幅と透明度のテーパー)。
   - 色は既存のロックオン演出に合わせた水色系。
   - 弾が消えたら、帯も末尾から徐々に消える(即消えしない)。
   - 動的頂点バッファの経路が見つからない場合の**フォールバック**: `ParticleManager::Emit` を毎フレーム弾頭の位置から連続で出して、帯状に見せる。
3. **発射の間引き**: ロック対象が複数のとき、各ビームの発射を少しずつずらす(例: 0.03秒ずつ)。一斉に出るだけだと単調に見えるため。
4. **磨き込み(時間があれば)**: 命中時の閃光、ビームの発射源を手のボーンに追従させる。

**注意**
- 総当たりの衝突判定(§2.3)があるので、ビームの見た目にコライダーを付けない。判定は弾頭の球だけ。
- 最小版で「ビームに見える」レベルまで作った時点で一度報告し、磨き込みはその後に進める。

**触るファイル**: `HomingBulletComponent.{h,cpp}`、新規(例: `Application/actor/Bullet/BeamTrail.*` またはエンジン側のリボン描画クラス)、`BulletFactory`。エンジン側に汎用クラスを置くかどうかは、調査結果を見て相談する。

**完了条件**
- [ ] ホーミング弾の後ろに、太さと透明度が変わる帯が伸びて見える
- [ ] 8体同時ロックで撃っても、フレームレートが極端に落ちない(60fps維持を目安に確認して数値を報告)
- [ ] 弾が命中して消えた後、帯が自然に消える

---

### T6. Release構成の通しプレイ確認

- Release 構成でビルド・起動し、タイトル → プレイ → ボス → クリア/ゲームオーバー → 戻る、が通ることを確認する。
- `#ifdef USE_IMGUI` / `#ifndef USE_IMGUI` の箇所([GameScene.cpp:19](Application/scene/GameScene.cpp:19)、:303、:667、:1081)を読み、**ゲームの挙動に関わる処理が ImGui 側に入っていない**ことを確認する。
- クリア/ゲームオーバー画面のUIが無い件は、このスプリントのスコープ外(状況だけ報告する)。

---

## 4. 決定済みの仕様(再確認不要)

| 項目 | 決定 |
|---|---|
| ホーミング | ロック対象がいる時だけ発射。0体なら通常弾 |
| 通常弾 | 対象指定なし。レティクル(画面上)の位置へ飛ぶ |
| ビーム | 弾頭 + 軌跡のリボン(案2)。ベジエ誘導は維持 |
| 相殺 | 機能と設定口だけ。撃ち落とせる弾の選定はレベルデザインで行う |
| 入力 | マウス + パッド両対応(キーボードも維持)。既存の割り当てを維持 |
| 照準点 | カメラのレイ上。当たるものがなければ遠方の固定距離 |
| ステージ | 1ステージ密度重視 |

## 5. 実装者が判断してよいこと / 確認すること

**判断してよい**: 新規クラスの細かい名前、定数の値(感度、距離、幅、色)、ファイル配置(既存の `Application/actor/*` の流儀に合わせる)。
**必ず確認する**: T5 の方式(調査結果の報告後)、エンジン層(`AbsoluteEngine/`)への汎用クラス追加、計画外の不具合の修正、すり抜け問題が起きた場合の対策。

## 6. スプリント後(参考)

10/3〜10/8: 回避演出(D)、サウンド最小組み込み(AudioManager は未配線)、ボス弱点 / 10/9〜10/14: 発表用の技術トピック / 10/15〜10/18: 資料・リハーサル / 10/19: 発表。
