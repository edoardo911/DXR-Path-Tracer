#include "common.hlsli"

TextureCube gCubemap: register(t0);

SamplerState gsamBilinearWrap: register(s1);

[shader("miss")]
void Miss(inout HitInfo payload: SV_RayPayload)
{
    float4 color = gCubemap.SampleLevel(gsamBilinearWrap, WorldRayDirection(), 0);
    payload.colorAndDistance = float4(color.rgb, 1e7);
}