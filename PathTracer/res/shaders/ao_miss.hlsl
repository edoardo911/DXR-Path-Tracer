#include "common.hlsl"

[shader("miss")]
void AOMiss(inout AOHitInfo payload: SV_RayPayload)
{
	payload.isHit = false;
}