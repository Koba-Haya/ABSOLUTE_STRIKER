#pragma once
#include "Component.h"
#include "Transform.h"
#include "collision/CollisionShape.h"
#include <memory>
#include <string>

namespace AbsoluteEngine {

// 当たり判定の部品。
// 保存する設定(Settings。形の種類と大きさを並べただけのデータ)と、その設定から作った形状(ICollisionShape の派生)を持つ。
// 判定の計算は形状のクラスが行い、この部品は「どの形を、世界のどこに置くか」を渡すだけにする。
// Settings を残す理由: シーンのファイル(JSON)とインスペクタの編集は、形の種類を数値で持つほうが扱いやすいため(データの入れ物)
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

    ColliderComponent() { RebuildShape_(); }
    // settings: 最初の設定値
    explicit ColliderComponent(const Settings& settings) : settings_(settings) { RebuildShape_(); }
    ~ColliderComponent() override = default;

    std::string GetTypeName() const override { return "ColliderComponent"; }

    // 設定値を読み取り専用で返す。書き換えるときは、コピーを編集して SetSettings で戻す
    const Settings& GetSettings() const { return settings_; }
    // 設定値を丸ごと差し替える(書き込みの窓口はここだけ)。形状も作り直す
    void SetSettings(const Settings& settings) {
        settings_ = settings;
        RebuildShape_();
    }

    // 当たり判定の形状を返す。種類が None なら nullptr(当たり判定をしない)
    const ICollisionShape* GetShape() const { return shape_.get(); }

    // 衝突判定で使う置き方: 中心 = オブジェクトの位置 + centerOffset。大きさにオブジェクトの scale を掛けない(以前の衝突判定と同じ)
    ShapePlacement GetCollisionPlacement() const;
    // エディタ(選択のレイ判定、枠の表示)で使う置き方: centerOffset と大きさに、オブジェクトの scale を掛ける(以前のエディタと同じ)
    ShapePlacement GetEditorPlacement() const;

    // エディタのインスペクタに、当たり判定の設定を表示し、編集を受け付ける(形状に応じて、出す項目を変える)。
    // 取り消し履歴への記録はエディタ側が行う(ここでは値を変えるだけ)
    void DrawInspectorUI() override;

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
        RebuildShape_();
    }

private:
    // 設定の種類と大きさから、形状を作り直す(Factory Method: 種類の数値を、形状のクラスへ変換する場所はここだけ)
    void RebuildShape_();

    Settings settings_;                     // 当たり判定の設定値
    std::unique_ptr<ICollisionShape> shape_; // 設定から作った形状(None なら nullptr)
};

} // namespace AbsoluteEngine
