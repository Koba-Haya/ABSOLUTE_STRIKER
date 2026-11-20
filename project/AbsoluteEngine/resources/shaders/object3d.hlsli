struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
    float3 worldPosition : POSITION0;
    // スキニングやワールド変換の前の、モデル本来の頂点位置(VertexData の pad)。
    // 動くモデルの表面に張り付いた模様(撃破演出のひび割れなど)を、位置から引くために使う
    float3 localPosition : TEXCOORD1;
};
