#include "common.hlsli"

TextureCube gCubemap: register(t0);

SamplerState gBilinearSampler: register(s1);

[shader("miss")]
void Miss(inout HitInfo payload: SV_RayPayload)
{
    float4 color;
    
    color = gCubemap.SampleLevel(gBilinearSampler, WorldRayDirection(), 0.0);
    
    //sun/moon
    if(gLightCount > 0 && gLights[0].type == LIGHT_TYPE_DIRECTIONAL)
    {
        float3 lightPos = -gLights[0].Direction;
        float3 lookAt = normalize(WorldRayDirection());
        float dist = length(lookAt - lightPos) * 2.4; //2.4 is an empirical value to account for the radius of the sun/moon
        
        color.rgb = lerp(color.rgb, gLights[0].Strength.rgb, saturate(1.0 / (dist / gLights[0].radius + 0.5)));
    }
    
    payload.colorAndDistance = float4(color.rgb, 1e7);
    payload.candidate = (Reservoir) 0.0;
}