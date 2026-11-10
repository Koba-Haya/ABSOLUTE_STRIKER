#pragma once
#include "AbsoluteEngine/scene/Component.h"
#include <string>

/// <summary>
/// 敵のモデルのアニメーション(羽ばたきなど)を、出現したときに 1 回だけ再生開始するコンポーネント。
/// 分けた理由: 敵の動き(StraightMove など)や攻撃、体力(EnemyComponent)は、見た目のアニメとは別の責務のため。
///             プレハブに足すだけで、敵の見た目のアニメが付く。
/// 設定(プレハブの JSON): "animation" = 再生するアニメ名(既定は Flap)。
/// 注意: モデルに、その名前のアニメが無いときは、ModelComponent 側がログを出すだけで何も起きない。
/// </summary>
class EnemyModelAnimatorComponent : public AbsoluteEngine::IComponent {
public:
    EnemyModelAnimatorComponent() = default;
    ~EnemyModelAnimatorComponent() override = default;

    // 最初の Update で、アニメの再生を開始する(モデルの読み込みが済んだ後に呼ぶため、Initialize ではなく Update で行う)
    void Update(float deltaTime) override;

    std::string GetTypeName() const override { return "EnemyModelAnimatorComponent"; }

    void Serialize(nlohmann::json& j) const override;
    void Deserialize(const nlohmann::json& j) override;

private:
    // 既定のアニメ名(モデルの生成スクリプト tools/enemy/gen_enemies.py の ANIM_FLAP と揃える)
    static constexpr const char* kDefaultAnimation = "Flap";

    std::string animation_ = kDefaultAnimation;
    bool started_ = false; // 再生を開始済みか
};
