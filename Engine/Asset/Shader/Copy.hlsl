// Simple fullscreen copy shader, corrected for viewport clipping

cbuffer ViewportInfo : register(b1)
{
    float2 RenderTargetSize;
    float2 Padding;
}

Texture2D SceneColor : register(t0);
SamplerState PointSampler : register(s0);

struct PSInput
{
	float4 Position : SV_POSITION;
};

// Use a simple VS that generates a fullscreen triangle
PSInput mainVS(uint vertexID : SV_VertexID)
{
    PSInput output;
    switch (vertexID)
    {
    case 0:
        output.Position = float4(-1.f, -1.f, 0.f, 1.f);
        break;
    case 1:
        output.Position = float4(3.f, -1.f, 0.f, 1.f);
        break;
    case 2:
        output.Position = float4(-1.f, 3.f, 0.f, 1.f);
        break;
    default:
        output.Position = float4(0.f, 0.f, 0.f, 1.f);
        break;
    }
    return output;
}

float4 mainPS(PSInput input) : SV_Target
{
    // Calculate UVs based on viewport size, just like FXAA
    float2 uv = input.Position.xy / RenderTargetSize;
    
    // Sample the source texture and return the color
    return SceneColor.Sample(PointSampler, uv);
}