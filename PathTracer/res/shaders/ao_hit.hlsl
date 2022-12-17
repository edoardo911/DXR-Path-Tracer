#include "common.hlsl"

[shader("closesthit")]
void AOClosestHit(inout AOHitInfo payload, Attributes attrib)
{
	payload.isHit = true;
}