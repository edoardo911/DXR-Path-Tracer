RWTexture2D<float4> gOutput: register(u0);

Texture2D gInput: register(t0);
Texture2D gSpecular: register(t1);
Texture2D gSky: register(t2);

//TODO include NRD.hlsli
float3 _NRD_YCoCgToLinear(float3 color)
{
    float t = color.x - color.z;

    float3 r;
    r.y = color.x + color.z;
    r.x = t + color.y;
    r.z = t - color.y;

    return max(r, 0.0);
}

float4 REBLUR_BackEnd_UnpackRadianceAndNormHitDist(float4 data)
{
    data.xyz = _NRD_YCoCgToLinear(data.xyz);

    return data;
}

[numthreads(16, 16, 1)]
void main(uint3 pixel: SV_DispatchThreadID)
{
    float4 packedColor = gInput[pixel.xy];
    float4 packedSpecular = gSpecular[pixel.xy];
    
    float4 color = REBLUR_BackEnd_UnpackRadianceAndNormHitDist(packedColor);
    float4 specular = REBLUR_BackEnd_UnpackRadianceAndNormHitDist(packedSpecular);
    float4 sky = gSky[pixel.xy];

    float3 Ldiff = color.rgb * color.a;
    float3 Lspec = specular.rgb;
    float3 finalColor = sky.a > 0 ? sky.rgb : (Ldiff + Lspec);
    gOutput[pixel.xy] = float4(finalColor, 0);
}