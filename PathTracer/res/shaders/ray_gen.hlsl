#include "common.hlsl"

cbuffer cbPass: register(b0)
{
	float4x4 gInvView;
	float4x4 gInvProj;
	uint gFrameIndex;
}

RWTexture2D<float4> gOutput: register(u0);
RWTexture2D<float4> gSumBuffer: register(u1);

RaytracingAccelerationStructure SceneBVH: register(t0);
Texture2D<float4> gLastSum: register(t1);

[shader("raygeneration")]
void RayGen()
{
	uint2 launchIndex = DispatchRaysIndex().xy;
	float2 dims = float2(DispatchRaysDimensions().xy);
	float2 d = ((launchIndex + 0.5F) / dims) * 2.0F - 1.0F;

	RayDesc ray;
	ray.Origin = mul(gInvView, float4(0, 0, 0, 1));
	float4 target = mul(gInvProj, float4(d.x, -d.y, 1, 1));
	ray.Direction = mul(gInvView, float4(target.xyz, 0.0F));
	ray.TMin = 0.01F;
	ray.TMax = 100000;

	//ao pass
	AOHitInfo aoPayload;
	aoPayload.colorAndDistance = float2(0, 1.0F);
	aoPayload.instanceID = -1;

	TraceRay(SceneBVH, RAY_FLAG_NONE, 0xFF, 1, 0, 1, ray, aoPayload);

	//standard pass
	HitInfo payload;
	payload.colorAndDistance = float4(0, 0, 0, 0);
	payload.ambientAccess = aoPayload.colorAndDistance.r;
	payload.recursionDepth = 1;

	TraceRay(SceneBVH, RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, payload);

	if(gFrameIndex == 1)
	{
		gSumBuffer[launchIndex] = payload.colorAndDistance;
		gOutput[launchIndex] = payload.colorAndDistance;
	}
	else
	{
		float3 color = gSumBuffer[launchIndex].rgb + payload.colorAndDistance.rgb;
		gSumBuffer[launchIndex].rgb = color;
		gOutput[launchIndex].rgb = color / gFrameIndex;
	}
}