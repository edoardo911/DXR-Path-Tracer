#include "common.hlsl"

RaytracingAccelerationStructure SceneBVH: register(t0);

cbuffer cbPass: register(b0)
{
	float4x4 gInvView;
	float4x4 gInvProj;
	uint gFrameIndex;
}

[shader("closesthit")]
void AOClosestHit(inout AOHitInfo payload, Attributes attrib)
{
	if(payload.instanceID != InstanceID())
		payload.isHit = true;
}