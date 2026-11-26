#pragma once
#include "Type/Vector.h"
#include <memory>
#include <vector>

namespace AbsoluteEngine {
class GameObject;
class ISpawner;
}

// ボスの誘導弾の「玉」の集まりを管理するクラス。玉を、まとめて出し、ボスに合わせて動かし、順に発射し、まとめて消す。
//
// 分けた理由: 玉の出し方(数、名前、タグ、部品の組み立て)と、集まりの持ち方、発射の順番を、BossComponent に混ぜないため。
// BossComponent は、「出して」「発射して」「消して」「基準(ボスの位置)を渡す」だけを、このクラスへ頼む。
// 玉は、シーンが持つ(ISpawner で出す)。ここでは、弱い参照(weak_ptr)だけを持つ。消えた玉は、自然に参照が切れる。
// 発射したあとの玉は、ボスの動きとは関係なく飛ぶので、基準の更新(Update)は、待機の玉にだけ効く。
class BossOrbGroup {
public:
    // 玉を count 個、まとめて出す。すでに待っている玉があるときは、先にそれらを消し始める(攻撃が重なっても、玉が溜まらないようにする)。
    // 発射した玉(飛行中)には、何もしない
    // appearSec: 出現にかける秒数(攻撃の予告の長さ)。chargeSec: 溜めの秒数
    // center / right / up: ボスの中心と、画面の右と上の向き(玉を並べる基準)
    // 玉の尾を切り離すために、spawner を、玉の見た目の部品へ渡す(所有しない。シーンが持ち、玉より長く生きる)
    void Summon(AbsoluteEngine::ISpawner& spawner, int count, float appearSec, float chargeSec,
                const Vector3& center, const Vector3& right, const Vector3& up);

    // 待っている玉を、出した順に、intervalSec の間隔で 1 つずつ、target(自機)へ向けて発射する。flightSec: 飛行の時間(秒)
    void Launch(const std::weak_ptr<AbsoluteEngine::GameObject>& target, float flightSec, float intervalSec);

    // 毎フレーム。ボスが動くので、玉の基準を渡し直す。消えた玉の参照は、ここで取り除く
    void Update(const Vector3& center, const Vector3& right, const Vector3& up);

    // 待っている玉(まだ発射を命じていないもの)を、消し始める。発射を命じた玉は、そのまま飛び続ける
    void DismissHeld();

    // 出ている玉を、飛行中のものも含めて、すべて消し始める(ボスの撃破など)
    void DismissAll();

private:
    std::vector<std::weak_ptr<AbsoluteEngine::GameObject>> orbs_;
};
