#pragma once
#include <string>
#include <vector>

#include "BulletDefinition.h"

// 弾の定義を、resources/bullet/bullets.json から読み込んで持つクラス。
//
// 分けた理由: 「どんな弾があるか」(データ)の読み込みと検証だけを担当し、弾の組み立てや発射を持たないため
// (ItemCatalog と同じ形)。弾の種類を足す、見た目を変えるときは、JSON を書き換えるだけでよい。
// 持ち主は GameData(アプリの初期化で 1 回 Load する)。シーンは、部品を作る前に、型で引く表(ServiceTable)へ登録する。
// 読み込みに失敗したとき(ファイルが無い、項目が不正)は、原因を表示して終了する(黙って進むと、弾が出ない原因が分からなくなるため)。
class BulletCatalog {
public:
  // 定義をファイルから読む
  void Load();

  // id に対応する定義を返す。無ければ nullptr
  const BulletDefinition *Find(const std::string &bulletId) const;

  // すべての弾の定義(JSON に書かれた順。エディタの選択肢などが使う)
  const std::vector<BulletDefinition> &GetBullets() const { return bullets_; }

private:
  std::vector<BulletDefinition> bullets_;
};
