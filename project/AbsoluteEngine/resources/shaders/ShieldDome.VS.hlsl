// シールドのドーム(ShieldDome)の頂点シェーダー。単位球の頂点を、ワールドへ変換して、ピクセルシェーダーへ必要な値を渡す。
// 砕けるとき(shatter.x = breakProgress が進むとき)は、乱数の小さい面から順に、面を中心へ縮めながら、外へ飛ばす(破片になる)。
struct VertexShaderInput {
    float3 position : POSITION0;
    float3 barycentric : TEXCOORD0;
    float faceRandom : TEXCOORD1;
    float3 faceCenter : TEXCOORD2;
};

struct VertexShaderOutput {
    float4 position : SV_POSITION;
    float3 worldPos : TEXCOORD0;
    float3 direction : TEXCOORD1;   // 球の中心から、この点への向き(砕ける前の、単位球の位置)
    float3 barycentric : TEXCOORD2;
    float faceRandom : TEXCOORD3;
    float shatter : TEXCOORD4;      // この面の砕け具合(0 = まだ、1 = 飛び終えた)
};

struct TransformationMatrix {
    float4x4 WVP;
    float4x4 World;
    float4 shatter; // x = breakProgress、y = shardSpan、z = shardDistance、w = shardShrink
};
ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

VertexShaderOutput main(VertexShaderInput input) {
    VertexShaderOutput output;

    // 砕け具合: この面の乱数を、breakProgress が超えたときから、shardSpan の長さで、0 から 1 へ進む
    const float shatter = saturate((gTransformationMatrix.shatter.x - input.faceRandom) / max(gTransformationMatrix.shatter.y, 0.0001f));
    // 面の中心へ縮めて、外向き(面の中心の向き)へ飛ばす
    const float3 outward = normalize(input.faceCenter);
    const float3 shrunk = lerp(input.position, input.faceCenter, shatter * gTransformationMatrix.shatter.w);
    const float3 shardPosition = shrunk + outward * (shatter * gTransformationMatrix.shatter.z);

    const float4 local = float4(shardPosition, 1.0f);
    output.position = mul(local, gTransformationMatrix.WVP);
    output.worldPos = mul(local, gTransformationMatrix.World).xyz;
    output.direction = input.position;
    output.barycentric = input.barycentric;
    output.faceRandom = input.faceRandom;
    output.shatter = shatter;
    return output;
}
