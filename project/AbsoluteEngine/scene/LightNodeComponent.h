#pragma once
#include "Component.h"
#include "Transform.h"
#include <string>

namespace AbsoluteEngine {

class LightNodeComponent : public IComponent {
public:
    enum class Type { None, Directional, Point, Spot };

    // ライトの設定値の束(値を並べただけのデータの入れ物)。
    // 外へ出すときは、この束ごと、読み取り(GetSettings)と、書き換え(SetSettings)の 1 つずつの窓口を通す
    struct Settings {
        Type type = Type::None;
        Vector3 color = {1.0f, 1.0f, 1.0f};
        float intensity = 1.0f;
        float radius = 10.0f;       // Point, Spot用
        float decay = 2.0f;         // Point, Spot用
        float distance = 10.0f;     // Spot用
        float coneAngleDeg = 30.0f; // Spot用
    };

    LightNodeComponent() = default;
    // settings: 最初の設定値
    explicit LightNodeComponent(const Settings& settings) : settings_(settings) {}
    ~LightNodeComponent() override = default;

    std::string GetTypeName() const override { return "LightNodeComponent"; }

    // 設定値を読み取り専用で返す。書き換えるときは、コピーを編集して SetSettings で戻す
    const Settings& GetSettings() const { return settings_; }
    // 設定値を丸ごと差し替える(書き込みの窓口はここだけ)
    void SetSettings(const Settings& settings) { settings_ = settings; }

    void Serialize(nlohmann::json& j) const override {
        j["lightType"] = static_cast<int>(settings_.type);
        j["color"] = { {"x", settings_.color.x}, {"y", settings_.color.y}, {"z", settings_.color.z} };
        j["intensity"] = settings_.intensity;
        j["radius"] = settings_.radius;
        j["decay"] = settings_.decay;
        j["distance"] = settings_.distance;
        j["coneAngleDeg"] = settings_.coneAngleDeg;
    }
    void Deserialize(const nlohmann::json& j) override {
        if (j.contains("lightType")) settings_.type = static_cast<Type>(j["lightType"].get<int>());
        if (j.contains("color")) {
            settings_.color.x = j["color"].value("x", 1.0f);
            settings_.color.y = j["color"].value("y", 1.0f);
            settings_.color.z = j["color"].value("z", 1.0f);
        }
        if (j.contains("intensity")) settings_.intensity = j["intensity"].get<float>();
        if (j.contains("radius")) settings_.radius = j["radius"].get<float>();
        if (j.contains("decay")) settings_.decay = j["decay"].get<float>();
        if (j.contains("distance")) settings_.distance = j["distance"].get<float>();
        if (j.contains("coneAngleDeg")) settings_.coneAngleDeg = j["coneAngleDeg"].get<float>();
    }

private:
    Settings settings_; // ライトの設定値
};

} // namespace AbsoluteEngine
