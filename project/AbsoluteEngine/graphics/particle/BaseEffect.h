#pragma once
#include "Transform.h"

class Camera;

/// <summary>
/// 一定時間だけ表示して消える、単発エフェクトの基底クラス(Template Method)。
/// 「経過時間を進める -> 寿命が来たら消す -> 進行度を計算する」という共通の流れは、この基底の Update が持つ。
/// 派生クラスは、進行度ごとの見た目の更新(OnUpdate)と描画(Draw)だけを実装する。
/// 年齢・寿命・死亡の状態は private に隠し、派生クラスから直接書き換えられない。
/// </summary>
class BaseEffect {
public:
    /// <summary>
    /// 寿命(秒)と出現位置を受け取って作る。向きと拡大率は、Transform の既定値(回転なし・等倍)になる。
    /// </summary>
    /// <param name="lifetime">寿命(秒)。この時間がたつと消える</param>
    /// <param name="position">出現位置(ワールド座標)</param>
    BaseEffect(float lifetime, const Vector3& position) : lifetime_(lifetime) { transform_.translate = position; }
    virtual ~BaseEffect() = default;

    virtual void Initialize() {}

    /// <summary>
    /// 毎フレームの更新。経過時間を進め、寿命が来たら消して終わる。生きていれば、派生の OnUpdate を呼ぶ。
    /// 派生クラスは、これを上書きせず、OnUpdate を実装する
    /// </summary>
    /// <param name="deltaTime">前回からの経過時間(秒)</param>
    /// <param name="camera">描画に使うカメラ。ビルボードなどの向きの計算に使う。nullptr のときは単位行列として扱う</param>
    void Update(float deltaTime, const Camera* camera) {
        age_ += deltaTime;
        if (age_ >= lifetime_) {
            Kill();
            return;
        }
        OnUpdate(age_ / lifetime_, deltaTime, camera);
    }

    virtual void Draw() = 0;

    bool IsDead() const { return isDead_; }
    void Kill() { isDead_ = true; }

    // 読み取り専用で取得する。書き換えは SetTransform 経由にする
    const Transform& GetTransform() const { return transform_; }
    void SetTransform(const Transform& transform) { transform_ = transform; }

protected:
    /// <summary>
    /// 生きている間の、毎フレームの見た目の更新。派生クラスが実装する
    /// </summary>
    /// <param name="progress">進行度(0 = 出現した瞬間、1 に近づくほど寿命の終わり)</param>
    /// <param name="deltaTime">前回からの経過時間(秒)</param>
    /// <param name="camera">描画に使うカメラ(nullptr あり)</param>
    virtual void OnUpdate(float progress, float deltaTime, const Camera* camera) = 0;

private:
    Transform transform_{};
    float lifetime_ = 1.0f;
    float age_ = 0.0f;
    bool isDead_ = false;
};
