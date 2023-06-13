#define NRD_NORMAL_ENCODING 2
#define NRD_ROUGHNESS_ENCODING 1
#include "include/NRD.hlsli"

RWTexture2D<float4> gOutput: register(u0);

Texture2D gInput: register(t0);
Texture2D gSpecular: register(t1);
Texture2D gSky: register(t2);
Texture2D gMapColor: register(t3);
Texture2D gShadowData: register(t4);

[numthreads(16, 16, 1)]
void main(uint3 pixel: SV_DispatchThreadID)
{
    float2 pixelPos = pixel.xy;
    float4 packedColor = gInput[pixelPos];
    float4 packedSpecular = gSpecular[pixelPos];
    float4 skyColor = gSky[pixelPos];
    float4 mapColor = gMapColor[pixelPos];
    float4 shadowColor = gShadowData[pixelPos];
    
    float4 color = REBLUR_BackEnd_UnpackRadianceAndNormHitDist(packedColor);
    float4 specular = REBLUR_BackEnd_UnpackRadianceAndNormHitDist(packedSpecular);
    float4 shadowData = SIGMA_BackEnd_UnpackShadow(shadowColor);
    float3 shadow = min(lerp(shadowData.yzw, 1.0, shadowData.x) + 0.7, 1.0);
    
    float3 diffuseComponent = color.rgb * mapColor.rgb;
    float3 specularComponent = specular.rgb;

    float3 finalColor = skyColor.a > 0 ? skyColor.rgb : (diffuseComponent + specularComponent) * color.a * shadow.r;
    gOutput[pixel.xy] = float4(finalColor, 0);
}