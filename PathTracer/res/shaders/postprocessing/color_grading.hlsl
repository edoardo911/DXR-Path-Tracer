RWTexture2D<float4> gOutput: register(u0);

Texture2D gLUT: register(t0);

SamplerState gPointSampler: register(s0);

cbuffer passData: register(b0)
{
    float gInvWidth;
    float gInvHeight;
}

//https://lettier.github.io/3d-game-shaders-for-beginners/lookup-table.html

[numthreads(32, 32, 1)]
void main(uint3 threadID: SV_DispatchThreadID)
{
    float2 uv = (threadID.xy + 0.5) * float2(gInvWidth, gInvHeight);
    float4 color = gOutput[threadID.xy];
    
    float u = (floor(color.r * 15.0) + floor(color.b * 15.0) / 15.0 * 240.0) / 255.0;
    float v = floor(color.g * 15.0) / 255.0;
    float3 left = gLUT.SampleLevel(gPointSampler, float2(u, v), 0.0).rgb;
    
    u = (ceil(color.r * 15.0) + ceil(color.b * 15.0) / 15.0 * 240.0) / 255.0;
    v = ceil(color.g * 15.0) / 255.0;
    float3 right = gLUT.SampleLevel(gPointSampler, float2(u, v), 0.0).rgb;
    
    float3 final;
    final.r = lerp(left.r, right.r, frac(color.r * 15.0));
    final.g = lerp(left.g, right.g, frac(color.g * 15.0));
    final.b = lerp(left.b, right.b, frac(color.b * 15.0));
    
    gOutput[threadID.xy] = float4(final.rgb, 1.0);
}