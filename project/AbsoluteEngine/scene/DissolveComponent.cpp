#include "DissolveComponent.h"

#ifdef USE_IMGUI
#include "../../externals/imgui/imgui.h"
#endif

namespace AbsoluteEngine {

#ifdef USE_IMGUI
namespace {
// インスペクタのスライダーの範囲(以前 EditorUIManager に書かれていた値と同じ)
constexpr float kThresholdMax = 1.0f;
constexpr float kEdgeRangeMax = 0.1f;
} // namespace
#endif

void DissolveComponent::DrawInspectorUI() {
#ifdef USE_IMGUI
    // 設定のコピーを編集し、最後に SetSettings で 1 回だけ戻す(書き込みの窓口を 1 つに保つため)
    Settings dissolve = settings_;

    ImGui::Checkbox("Enable Dissolve", &dissolve.enable);
    // 無効のときは、ほかの値は見た目に効かないので出さない
    if (dissolve.enable) {
        ImGui::SliderFloat("Threshold", &dissolve.threshold, 0.0f, kThresholdMax);
        ImGui::SliderFloat("Edge Range", &dissolve.edgeRange, 0.0f, kEdgeRangeMax);

        float edgeCol[3] = { dissolve.edgeColor.x, dissolve.edgeColor.y, dissolve.edgeColor.z };
        if (ImGui::ColorEdit3("Edge Color", edgeCol)) {
            dissolve.edgeColor = { edgeCol[0], edgeCol[1], edgeCol[2] };
        }
        float maskCol[3] = { dissolve.maskColor.x, dissolve.maskColor.y, dissolve.maskColor.z };
        if (ImGui::ColorEdit3("Mask Color", maskCol)) {
            dissolve.maskColor = { maskCol[0], maskCol[1], maskCol[2] };
        }
    }

    SetSettings(dissolve);
#endif
}

} // namespace AbsoluteEngine
