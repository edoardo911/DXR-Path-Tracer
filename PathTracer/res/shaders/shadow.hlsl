#include "common.hlsl"

[shader("closesthit")]
void ShadowClosestHit(inout ShadowHitInfo hit, Attributes bary)
{
	hit.isHit = true;
	hit.distance = RayTCurrent();
}

[shader("miss")]
void ShadowMiss(inout ShadowHitInfo hit: SV_RayPayload)
{
	hit.isHit = false;
}