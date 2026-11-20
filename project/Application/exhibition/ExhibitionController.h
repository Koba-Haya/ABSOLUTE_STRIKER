#pragma once

class Input;
class SceneManager;

/// <summary>
/// 展示モードの、入る / 出るの切り替えを担当するクラス。毎フレーム、TAB キーを見て、押されたら切り替える。
/// 入る: 実際の保存データ(進行、継承)を退避して仮の状態にし(展示の間は、保存ファイルを書き込まない)、展示モードを有効にして、
///       ゲームの最初から始める。出る: 退避したデータへ戻し、展示モードを無効にして、タイトルへ戻る。
/// 分けた理由: 「展示モードの切り替え」は、どのシーンにも属さない、アプリ全体の操作。シーンに書くと、シーンごとに同じ処理が要るため、
/// アプリ(GameApp)が毎フレーム呼ぶ 1 つのクラスにした。展示中の、各シーンの自動進行は、各シーンが ExhibitionMode を見て行う。
/// 切り替えは、必ず、シーンの切り替え(画面遷移)を伴う。いまの画面を途中で変えると、状態が中途半端になるため。
/// </summary>
class ExhibitionController {
public:
  /// <summary>毎フレーム呼ぶ。TAB が押されたら、展示モードに入る / 出る。画面遷移の最中は、受け付けない</summary>
  static void Update(const Input &input, SceneManager &sceneManager);

  /// <summary>展示の 1 周ごとの初期化(ゲームの開始で呼ぶ)。島の解放、ベスト記録、継承を、初期の状態へ戻す(仮の状態なので、保存はされない)</summary>
  static void ResetLoopState();
};
