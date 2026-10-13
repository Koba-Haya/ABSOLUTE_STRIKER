# 雲ビルボード(L1-2)の保留素材

提出のため、いったん実装から外した。再開するとき:
1. `cloud*.png / .obj / .mtl` を `Application/resources/title/` へ戻す(実行ファイルへコピーされる)
2. `gen_cloud_sprites.py` で素材を作り直せる(numpy と Pillow が必要。約 1 分)
3. 実装(`TitleDirector` の雲の配置と場面転換、`TitleScene` の描画)は、会話履歴を参照
