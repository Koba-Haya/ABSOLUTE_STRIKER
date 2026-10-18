#pragma once
#include "../../../AbsoluteEngine/scene/Component.h"
#include "../../../AbsoluteEngine/scene/GameObject.h"
#include "../../../AbsoluteEngine/graphics/primitive/RibbonTrail.h"
#include <string>

/// <summary>
/// GameObject に付けて、その位置の軌跡(RibbonTrail)を記録・描画するコンポーネント。
/// 弾の移動処理は知らず、owner の座標を毎フレーム RibbonTrail に渡すだけにしている。
/// (RibbonTrail が履歴の管理、このコンポーネントが GameObject との橋渡し、という分担)
/// </summary>
class RibbonTrailComponent : public AbsoluteEngine::IComponent {
public:
    RibbonTrailComponent() = default;
    ~RibbonTrailComponent() override = default;

    /// <summary>
    /// 軌跡の記録ルールを設定する。
    /// </summary>
    /// <param name="params">記録ルール(点の間隔・寿命・上限)</param>
    void Initialize(const RibbonTrail::Params& params);

    void Update(float deltaTime) override;
    void Draw() override;

    /// <summary>
    /// 尾を、この GameObject から切り離して残す。弾が消えるときに呼ぶ。
    /// 軌跡を確定し、別の GameObject(RibbonTrailFadeComponent)へ引き渡してシーンへ追加する。
    /// 引き渡したあとは、このコンポーネントは何も描かない。シーンがない場合は何もしない(尾は弾と一緒に消える)。
    /// </summary>
    void DetachToScene();

    std::string GetTypeName() const override { return "RibbonTrailComponent"; }

private:
    RibbonTrail trail_;
};
