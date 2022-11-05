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
	if(payload.colorAndDistance.y < 0.0F)
	{
		if(payload.instanceID != InstanceID())
			payload.colorAndDistance = float2(0.0F, -RayTCurrent());
		else
			payload.colorAndDistance = float2(0.0F, 1.0F);
	}
	else
	{
		uint seed = initRand(DispatchRaysIndex().x * gFrameIndex, DispatchRaysIndex().y * gFrameIndex, 16);

		float3 worldOrigin = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();

		float occlusion = 1.0F;
		AOHitInfo aoPayload;
		aoPayload.colorAndDistance = float2(0, -1);
		aoPayload.instanceID = InstanceID();

		RayDesc ray;
		ray.Origin = worldOrigin;
		ray.Direction = normalize(float3(nextRand(seed), nextRand(seed), nextRand(seed)) * 2.0F - 1.0F);
		ray.TMin = 0.001F;
		ray.TMax = 0.091F;

		TraceRay(SceneBVH, RAY_FLAG_NONE, 0xFF, 1, 0, 1, ray, aoPayload);

		if(aoPayload.colorAndDistance.y < 0.0F)
			occlusion = 0.0F;

		payload.colorAndDistance = float2(occlusion, 0.0F);
	}
}