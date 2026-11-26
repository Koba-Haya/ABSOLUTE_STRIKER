#pragma once
#include "AbsoluteEngine/graphics/primitive/EnergyRibbon.h"
#include "AbsoluteEngine/graphics/primitive/RibbonTrail.h"
#include "AbsoluteEngine/scene/Component.h"
#include <string>

// 玉が消えたあとに残る、エネルギーのリボン(光の帯)の尾を、時間とともに細く薄くして消していくコンポーネント。
//
// 分けた理由: 玉の GameObject と一緒に尾が一瞬で消えると、光の帯がぶつ切りに見える。尾は、別の GameObject に引き継いで、
// 自然に消えるまで残す(通常のリボンの RibbonTrailFadeComponent と同じ考え方)。
// 通常のリボンの版(RibbonTrailFadeComponent)は、描画が Draw で、加算合成の DrawRibbon を使うので、使えない。
// こちらは、雲のあとに描き(DrawAfterClouds)、専用シェーダー(DrawEnergyRibbon)で描く。
// 尾が消えきったら、自分の GameObject ごと消える。
class EnergyRibbonFadeComponent : public AbsoluteEngine::IComponent {
public:
    EnergyRibbonFadeComponent() = default;
    ~EnergyRibbonFadeComponent() override = default;

    // 引き継ぐ尾と、見た目を受け取る。trail: Freeze 済み(新しい点を記録しない状態)のもの。style: 帯の色と模様(コピーを保持する)
    void Initialize(const RibbonTrail& trail, const EnergyRibbonStyle& style);

    void Update(float deltaTime) override;
    void DrawAfterClouds() override;

    std::string GetTypeName() const override { return "EnergyRibbonFadeComponent"; }

private:
    RibbonTrail trail_;
    EnergyRibbonStyle style_;
};
