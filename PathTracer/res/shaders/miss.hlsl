#include "common.hlsli"

TextureCube gCubemap: register(t0);

SamplerState gsamBilinearWrap: register(s1);

[shader("miss")]
void Miss(inout HitInfo payload: SV_RayPayload)
{
    float4 color = gCubemap.SampleLevel(gsamBilinearWrap, WorldRayDirection(), 0);
    if(payload.colorAndDistance.a < 0)
        payload.colorAndDistance = float4(0, 0, 0, 1e7); //indirect
    else
       payload.colorAndDistance = float4(color.rgb, 1e7);
}