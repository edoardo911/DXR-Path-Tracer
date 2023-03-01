RWTexture2D<float4> gOutput: register(u0);

Texture2D gInput: register(t0);
Texture2D gAlbedo: register(t1);

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
    float4 color = REBLUR_BackEnd_UnpackRadianceAndNormHitDist(packedColor);
    gOutput[pixel.xy] = color * gAlbedo[pixel.xy];
}