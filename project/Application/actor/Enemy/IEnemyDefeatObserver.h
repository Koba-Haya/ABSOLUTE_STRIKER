#pragma once
#include "Type/Vector.h"

namespace AbsoluteEngine {
class GameObject;
}

/// <summary>
/// 敵が倒れた理由。受け取る側(報酬の係など)が、「自機が倒した」ときだけ報酬(スコア、撃破数、アイテム)を
/// 出すために、倒す側(弾、体当たり)が渡す。爆発などの演出は、どちらの理由でも出す。
/// </summary>
enum class EnemyDestroyReason {
    Player,       // 自機の弾(ロックオン弾を含む)で倒された
    SelfDestruct, // 自機への体当たりなどで、自分で壊れた(自機が倒したのではない)
};

/// <summary>
/// 敵が倒れたことを知らせる中身(通知の 1 回分)。
/// enemy は、通知の間だけ使ってよい(受け取る側は、参照を保持しないこと。通知のあと、敵は消えていく)。
/// 敵の種類(名前)や編隊の番号など、受け取る側が要る情報は、enemy から取り出す
/// (敵の側が、受け取る側の都合を知らずに済むようにするため)。
/// </summary>
struct EnemyDefeatEvent {
    const AbsoluteEngine::GameObject& enemy; // 倒れた敵
    Vector3 position;                        // 倒れた位置(ワールド。爆発やアイテムを出す位置)
    EnemyDestroyReason reason;               // 倒れた理由
};

/// <summary>
/// 敵が倒れたことを受け取る側の基底(Observer パターンの Observer)。
///
/// 分けた理由: 敵(EnemyComponent = Subject)は「倒れた」と知らせるだけにし、倒れたあとに何が起きるか(スコア、アイテム、演出)は知らない。
/// 受け取る側がこの基底を継承して、自分の処理を書く。依存は「受け取る側 -> 敵」の一方向になる。
/// 寿命: 敵は受け取る側を所有しない(ポインタで持つ)。受け取る側は、登録した敵より長く生きるか、先に RemoveObserver すること。
/// </summary>
class IEnemyDefeatObserver {
public:
    virtual ~IEnemyDefeatObserver() = default;

    // 敵が倒れたときに、1 体につき 1 回呼ばれる
    virtual void OnEnemyDefeated(const EnemyDefeatEvent& event) = 0;
};
