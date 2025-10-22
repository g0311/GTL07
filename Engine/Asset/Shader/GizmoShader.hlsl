// GizmoShader.hlsl

cbuffer constants : register(b0)
{
    row_major float4x4 world;
}

cbuffer PerFrame : register(b1)
{
    row_major float4x4 View;
    row_major float4x4 Projection;
    float4 CameraPos;
};

// totalColor는 기존과 동일
cbuffer PerFrame : register(b2)
{
    float4 totalColor;
};

struct VS_INPUT
{
    float4 position : POSITION;
    float3 normal : NORMAL;
    float4 color : COLOR;
};

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float3 worldNormal : NORMAL;
    float4 color : COLOR;
};

PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;

    // 월드 좌표계로 변환
    float4 worldPos = mul(input.position, world);
    output.position = mul(worldPos, View);
    output.position = mul(output.position, Projection);

    // 월드 노멀 계산
    output.worldNormal = normalize(mul(input.normal, (float3x3) world));
    output.color = input.color;

    return output;
}

float4 mainPS(PS_INPUT input) : SV_TARGET
{
    float3 N = normalize(input.worldNormal);

    // Directional Light (고정된 방향)
    // 오른쪽 위에서 비추는 느낌
    float3 lightDir = normalize(float3(0.4, 0.6, -1.0));

    // 기본적인 Lambert 조명
    float NdotL = saturate(dot(N, lightDir));
    float diffuse = 0.4 + 0.6 * NdotL; // 완전 어둡지 않게 0.4 기본값 추가

    // base color
    float4 baseColor = lerp(input.color, totalColor, totalColor.a);

    // 최종 색상 = 기본색 * diffuse (음영)
    float4 finalColor = baseColor * diffuse;

    return finalColor;
}
