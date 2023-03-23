#define NRD_NORMAL_ENCODING 2
#define NRD_ROUGHNESS_ENCODING 1
#include "include/NRD.hlsli"

RWTexture2D<float4> gOutput: register(u0);

Texture2D gInput: register(t0);
Texture2D gSpecular: register(t1);
Texture2D gSky: register(t2);
Texture2D gMapColor: register(t3);

#define TEST 1

[numthreads(16, 16, 1)]
void main(uint3 pixel: SV_DispatchThreadID)
{
    float2 pixelPos = pixel.xy + 0.5;
    float4 packedColor = gInput[pixelPos];
    float4 packedSpecular = gSpecular[pixelPos];
    float4 skyColor = gSky[pixelPos];
    float4 mapColor = gMapColor[pixelPos];
    
    float4 color = REBLUR_BackEnd_UnpackRadianceAndNormHitDist(packedColor);
    float4 specular = REBLUR_BackEnd_UnpackRadianceAndNormHitDist(packedSpecular);

#if !TEST
    float3 finalColor = skyColor.a > 0 ? skyColor.rgb : (color.rgb * color.a * mapColor.rgb + specular.rgb);
    gOutput[pixel.xy] = float4(finalColor, 0);
#else
    if(pixel.x < 320 || pixel.y < 180 || pixel.x > 960)
        gOutput[pixel.xy] = float4(packedColor.rgb, 0);
    else
    {
        float3 finalColor = skyColor.a > 0 ? skyColor.rgb : (specular.rgb);
        gOutput[pixel.xy] = float4(finalColor, 0);
    }
#endif
}