#include "common.hlsl"

[shader("miss")]
void AOMiss(inout AOHitInfo payload: SV_RayPayload)
{
	payload.colorAndDistance = float2(0.0F, 1.0F);
}