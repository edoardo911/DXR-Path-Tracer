#include "common.hlsl"

[shader("miss")]
void Miss(inout HitInfo payload: SV_RayPayload)
{
	payload.colorAndDistance = float4(0.0F, 0.0F, 0.0F, -1.0F);
}