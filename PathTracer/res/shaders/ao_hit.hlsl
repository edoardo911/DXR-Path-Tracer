#include "common.hlsli"

[shader("closesthit")]
void AOClosestHit(inout AOHitInfo payload, Attributes attrib)
{
	payload.isHit = true;
    payload.hitT = RayTCurrent();
}