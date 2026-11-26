#pragma once
#include "Type/Vector.h"

namespace AbsoluteEngine {
class GameObject;
}

// 敵の予備動作(TelegraphComponent)の「光り方」の基底クラス(Strategy)。
//
// 作った理由: 以前は、TelegraphComponent が光り方の種類(出口に光る玉 / 体の色を替える)を、
// 始める・毎フレーム・終えるの 3 か所で if で調べて、処理を切り替えていた。種類ごとの処理を、この基底の派生へ集め、
// TelegraphComponent は、予備動作の流れ(いつ始めて、いつ終えるか、震え)だけを持つ。
// 光り方を増やすときは、派生クラスを 1 つ足し、TelegraphComponent の作る場所(RebuildGlow_)に 1 行足す。
class ITelegraphGlow {
public:
    virtual ~ITelegraphGlow() = default;

    // 予備動作が始まったとき。owner: 予備動作をする敵, progress: 予備動作の進み具合(0 〜 1。途中から始めることがある)
    virtual void OnBegin(AbsoluteEngine::GameObject& owner, float progress) = 0;

    // 予備動作の間、毎フレーム(既定では何もしない。光る玉は、ここで位置と大きさを合わせる)
    virtual void OnUpdate(AbsoluteEngine::GameObject& owner, float progress) {
        (void)owner;
        (void)progress;
    }

    // 予備動作が終わったとき(見た目を元に戻す)。
    // owner: 予備動作をした敵(破棄の途中などで、無いときは nullptr), baseColor: 予備動作の前の、体の色
    virtual void OnEnd(AbsoluteEngine::GameObject* owner, const Vector4& baseColor) = 0;
};
