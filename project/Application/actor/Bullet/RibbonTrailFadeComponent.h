#pragma once
#include "../../../AbsoluteEngine/scene/Component.h"
#include "../../../AbsoluteEngine/scene/GameObject.h"
#include "../../../AbsoluteEngine/graphics/primitive/RibbonTrail.h"
#include <string>

/// <summary>
/// 弾が消えたあとに残る尾を、時間とともに細く薄くして消していくコンポーネント。
/// 弾の GameObject とは別の GameObject に付け、尾が消えきったら自分の GameObject ごと消える。
/// 軌跡の記録は RibbonTrailComponent、消えゆく尾の描画と後始末はこのコンポーネント、という分担にしている。
/// </summary>
class RibbonTrailFadeComponent : public AbsoluteEngine::IComponent {
public:
    RibbonTrailFadeComponent() = default;
    ~RibbonTrailFadeComponent() override = default;

    /// <summary>
    /// 引き継ぐ軌跡を受け取る。Freeze 済み(新しい点を記録しない状態)のものを渡すこと。
    /// </summary>
    /// <param name="trail">弾から引き継ぐ軌跡(コピーを保持する)</param>
    void Initialize(const RibbonTrail& trail);

    void Update(float deltaTime) override;
    void Draw() override;

    std::string GetTypeName() const override { return "RibbonTrailFadeComponent"; }

private:
    RibbonTrail trail_;
};
