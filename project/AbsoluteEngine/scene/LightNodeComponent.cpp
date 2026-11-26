#include "LightNodeComponent.h"

#ifdef USE_IMGUI
#include "../../externals/imgui/imgui.h"
#endif

namespace AbsoluteEngine {

#ifdef USE_IMGUI
namespace {
// インスペクタのスライダーの刻みと範囲(以前 EditorUIManager に書かれていた値と同じ)
constexpr float kIntensityStep = 0.05f;
constexpr float kIntensityMax = 100.0f;
constexpr float kRadiusStep = 0.1f;
constexpr float kRadiusMax = 1000.0f;
constexpr float kDecayStep = 0.05f;
constexpr float kDecayMax = 10.0f;
constexpr float kDistanceStep = 0.1f;
constexpr float kDistanceMax = 1000.0f;
constexpr float kConeAngleStep = 0.5f;
constexpr float kConeAngleMaxDeg = 180.0f;
// 種類の表示名。Type の並び(None, Directional, Point, Spot)と同じ順にする
constexpr const char* kTypeNames[] = { "None", "Directional", "Point", "Spot" };
} // namespace
#endif

void LightNodeComponent::DrawInspectorUI() {
#ifdef USE_IMGUI
    // 設定のコピーを編集し、最後に SetSettings で 1 回だけ戻す(書き込みの窓口を 1 つに保つため)
    Settings l = settings_;

    int currentType = static_cast<int>(l.type);
    if (ImGui::Combo("Type", &currentType, kTypeNames, IM_ARRAYSIZE(kTypeNames))) {
        l.type = static_cast<Type>(currentType);
    }

    // 種類ごとに意味のある項目だけを出す(None は何も照らさないので、色も強さも出さない)
    if (l.type != Type::None) {
        float color[3] = { l.color.x, l.color.y, l.color.z };
        if (ImGui::ColorEdit3("Color", color)) {
            l.color = { color[0], color[1], color[2] };
        }
        ImGui::DragFloat("Intensity", &l.intensity, kIntensityStep, 0.0f, kIntensityMax);

        // 点光源とスポットライトは、届く範囲と減衰を持つ
        if (l.type == Type::Point || l.type == Type::Spot) {
            ImGui::DragFloat("Radius", &l.radius, kRadiusStep, 0.0f, kRadiusMax);
            ImGui::DragFloat("Decay", &l.decay, kDecayStep, 0.0f, kDecayMax);
        }
        // スポットライトだけが、照らす距離と円錐の角度を持つ
        if (l.type == Type::Spot) {
            ImGui::DragFloat("Distance", &l.distance, kDistanceStep, 0.0f, kDistanceMax);
            ImGui::DragFloat("ConeAngle (deg)", &l.coneAngleDeg, kConeAngleStep, 0.0f, kConeAngleMaxDeg);
        }
    }

    SetSettings(l);
#endif
}

} // namespace AbsoluteEngine
