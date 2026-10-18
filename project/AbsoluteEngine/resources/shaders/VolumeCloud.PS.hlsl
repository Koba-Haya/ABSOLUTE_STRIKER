#include "Fullscreen.hlsli"
#include "VolumeCloud.hlsli"

// ボリューム雲のパス 2: 雲を、シーンより低い解像度で描く(ビューポートが低解像度の大きさ)。
// 描画先は R16G16B16A16_FLOAT(ブレンドなし)。rgb = 雲の光、a = 1 - 透過率。
// 物体までの距離は、パス 1 が縮めたテクスチャから読む。
// 元の解像度へ拡大してシーンへ重ねるのは、パス 3(VolumeCloudComposite.PS)。

struct PixelShaderOutput {
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input) {
    PixelShaderOutput output;

    // texcoord(0〜1, 左上原点) -> NDC(-1〜1, 上が +y)
    float32_t2 ndc = float32_t2(input.texcoord.x * 2.0f - 1.0f, 1.0f - input.texcoord.y * 2.0f);
    float32_t3 dir = RayDirectionFromNdc(ndc);

    // 物体までの距離(何も描かれていない所は無限遠)
    float32_t sceneDistance = gSceneDistance.Load(int3(int2(input.position.xy), 0));

    VolumeCloudResult cloud = MarchVolumeCloud(gCameraPosition, dir, sceneDistance, input.position.xy);
    output.color = float32_t4(cloud.color, 1.0f - cloud.transmittance);
    return output;
}
