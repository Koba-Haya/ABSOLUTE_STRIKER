# ゲームシーン（プレイアブル）での加点要素 実装計画

「ゲームシーンで目視確認できないと加点として認められない」という制約を加味した実装計画です。
現状の`GameScene`や`Player`側では静的モデル（`.obj`）が使われており、スキニングやアニメーションの実力を見せることができない状態です。
この計画では、これらのエンジン機能をゲームシーン上に露出し、評価者がプレイした際に確実に「実装されている」と視覚的に伝わるようにします。

## Proposed Changes

### 1. スキニングモデルの表示と操作の実装 (計30点確実化)
現状の`player.obj`をアニメーション付きの `.gltf` に差し替え、描画時にアニメーションを再生します。

#### [MODIFY] `Application/scene/GameScene.cpp` & `Application/actor/Player/PlayerComponent.cpp`
- プレイヤーのモデルロードパスを `resources/human/walk.gltf` に変更。
- スキニング情報が正しくシェーダー（`Skinning.CS.hlsl`）に渡って描画され、待機/歩行アニメーションが再生されることを担保します。

---

### 2. 手からパーティクル（弾）を出す (10点)
プレイヤーが弾を撃つ際、自機の中心座標からではなく、**「モデルの右手のボーン位置」**から弾（およびマズルフラッシュ的なパーティクル）を発生させます。

#### [MODIFY] `AbsoluteEngine/graphics/3d/model/ModelInstance.cpp` (および `.h`)
- ボーン名を引数に渡し、現在のワールド行列を取得する関数 `GetBoneWorldMatrix(const std::string& boneName)` を追加。

#### [MODIFY] `Application/actor/Player/PlayerComponent.cpp`
- 弾の発射位置 `p0` の計算を `t.translate` から、`GetBoneWorldMatrix("mixamorig:RightHand")` の平行移動成分に変更。
- 弾を発射した瞬間に、その右手座標でGPUパーティクル（マズルフラッシュ等）を発生させる。

---

### 2.5. 武器を手に持たせる (10点)
`GetBoneWorldMatrix`を追加した副産物として、ほぼ低コストで実現できます。武器（仮モデルで可）を毎フレーム右手ボーンに追従させ、アニメーションに合わせて動くことを常時ゲーム画面上で示します。

#### [MODIFY] `Application/actor/Player/PlayerComponent.cpp` (または新規 `WeaponComponent`)
- 武器用の`ModelInstance`を1つ保持し、`Update`内で `weaponModelInstance->SetWorld(GetBoneWorldMatrix("mixamorig:RightHand"))` を毎フレーム呼ぶ。
- 武器モデル自体のローカルオフセット（グリップ位置合わせ）が必要な場合は、ボーン行列に軽いオフセット行列を掛け合わせる。

---

### 3. 骨のデバッグ表示 (10点)
評価者に「スケルトンをパースして正しく保持・更新していること」をアピールするため、ゲーム画面上にボーンの構造を線描画します。

#### [MODIFY] `AbsoluteEngine/graphics/3d/model/ModelInstance.cpp` (および `.h`)
- `DrawSkeleton(Renderer* renderer)` のような関数を追加。
- ボーン構造を走査し、`Renderer::DrawLine` を用いて親から子への線を描画。

#### [MODIFY] `Application/scene/GameScene.cpp`
- `Draw()` 関数内で、特定のキー（例：Bキー）が押されているときのみ、プレイヤーの骨格をデバッグ描画する。

---

### 4. Animation補間 (5点) ※余力があれば
アニメーション遷移時に徐々にブレンド（クロスフェード）させます。

#### [MODIFY] `AbsoluteEngine/graphics/3d/model/ModelInstance.cpp` (および `.h`)
- 2つのアニメーションのTransformを別々に計算し、それらを `Lerp` / `Slerp` でブレンドしてからローカル行列を確定する処理を追加。

## Verification Plan
1. **Skinning & Animation**: ゲームを実行し、プレイヤーキャラが正しく表示されアニメーションを行っているか目視確認。
2. **手からパーティクル**: 弾を撃った際、体の中心からではなく、右手の先端（`mixamorig:RightHand`）から弾とパーティクルが発生しているか確認。
3. **武器を手に持たせる**: 武器モデルが常に右手ボーンに追従し、アニメーション中も手からズレないか確認。
4. **骨のデバッグ表示**: 指定のキーを押すと、キャラクターのボーン構造が線で表示され、アニメーションに合わせて動くか確認。
