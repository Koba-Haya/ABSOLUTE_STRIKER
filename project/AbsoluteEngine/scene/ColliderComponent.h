#pragma once
#include "Component.h"
#include "Transform.h"
#include <string>

namespace AbsoluteEngine {

class ColliderComponent : public IComponent {
public:
    enum class Type {
        None,
        Sphere,
        AABB
    };

    // 当たり判定の設定値の束(値を並べただけのデータの入れ物)。
    // 外へ出すときは、この束ごと、読み取り(GetSettings)と、書き換え(SetSettings)の 1 つずつの窓口を通す
    struct Settings {
        Type type = Type::Sphere;
        Vector3 centerOffset = { 0.0f, 0.0f, 0.0f };
        float radius = 1.0f;                 // Sphere 用
        Vector3 size = { 1.0f, 1.0f, 1.0f }; // AABB 用(半幅)
    };

    ColliderComponent() = default;
    // settings: 最初の設定値
    explicit ColliderComponent(const Settings& settings) : settings_(settings) {}
    ~ColliderComponent() override = default;

    std::string GetTypeName() const override { return "ColliderComponent"; }

    // 設定値を読み取り専用で返す。書き換えるときは、コピーを編集して SetSettings で戻す
    const Settings& GetSettings() const { return settings_; }
    // 設定値を丸ごと差し替える(書き込みの窓口はここだけ)
    void SetSettings(const Settings& settings) { settings_ = settings; }

    void Serialize(nlohmann::json& j) const override {
        j["colliderType"] = static_cast<int>(settings_.type);
        j["radius"] = settings_.radius;
        j["centerOffset"] = { {"x", settings_.centerOffset.x}, {"y", settings_.centerOffset.y}, {"z", settings_.centerOffset.z} };
        j["size"] = { {"x", settings_.size.x}, {"y", settings_.size.y}, {"z", settings_.size.z} };
    }
    void Deserialize(const nlohmann::json& j) override {
        if (j.contains("colliderType")) settings_.type = static_cast<Type>(j["colliderType"].get<int>());
        if (j.contains("radius")) settings_.radius = j["radius"].get<float>();
        if (j.contains("centerOffset")) {
            settings_.centerOffset.x = j["centerOffset"].value("x", 0.0f);
            settings_.centerOffset.y = j["centerOffset"].value("y", 0.0f);
            settings_.centerOffset.z = j["centerOffset"].value("z", 0.0f);
        }
        if (j.contains("size")) {
            settings_.size.x = j["size"].value("x", 1.0f);
            settings_.size.y = j["size"].value("y", 1.0f);
            settings_.size.z = j["size"].value("z", 1.0f);
        }
    }

private:
    Settings settings_; // 当たり判定の設定値
};

} // namespace AbsoluteEngine
