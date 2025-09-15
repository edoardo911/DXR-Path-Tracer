RWTexture2D<float4> gOutput: register(u0);

cbuffer passData: register(b0)
{
    float exposure;
    float brightness;
    float contrast;
    float saturation;
    float gamma;
    uint tonemapping;
}

float3 reinhard_tonemapping(float3 color)
{
    float3 colorAmplified = color * 1.2;
    return colorAmplified / (1.0 + colorAmplified);
}

float3 uncharted2_tonemapping(float3 color)
{
    const float a = 0.15;
    const float b = 0.5;
    const float c = 0.1;
    const float d = 0.2;
    const float e = 0.02;
    const float f = 0.3;
    const float W = 11.2;
    
    float3 colorAmplified = color * 2.5;
    float3 mapped = ((colorAmplified * (a * colorAmplified + c * b) + d * e) / (colorAmplified * (a * colorAmplified + b) + d * f)) - e / f;
    float3 whiteScale = ((W * (a * W + c * b) + d * e) / (W * (a * W + b) + d * f)) - e / f;
    mapped /= whiteScale;
    
    return saturate(mapped);
}

float3 aces_tonemapping(float3 color)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    float3 colorAmplified = color * 0.65;
    return saturate((colorAmplified * (a * colorAmplified + b)) / (colorAmplified * (c * colorAmplified + d) + e));
}

[numthreads(32, 32, 1)]
void main(uint3 threadID: SV_DispatchThreadID)
{    
    float e = exposure;
    
    float4 baseColor = gOutput[threadID.xy] * e;
    float3 bcColor = contrast * (baseColor.rgb - 0.5) + 0.5 + brightness;
    
    float luma = dot(bcColor, float3(0.299, 0.587, 0.144));
    float3 saturatedColor = lerp(luma, bcColor, saturation);
    float3 tonemapped;
    
    switch(tonemapping)
    {
    default:
    case 0:
        tonemapped = saturatedColor;
        break;
    case 1:
        tonemapped = reinhard_tonemapping(saturatedColor);
        break;
    case 2:
        tonemapped = uncharted2_tonemapping(saturatedColor);
        break;
    case 3:
        tonemapped = aces_tonemapping(saturatedColor);
        break;
    }
    
    float3 finalColor = pow(tonemapped, gamma);
    gOutput[threadID.xy] = float4(saturate(finalColor), 1.0);
}