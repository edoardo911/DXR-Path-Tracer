#include "common.hlsli"

cbuffer cbPass: register(b0)
{
    float4x4 gInvView;
    float4x4 gInvProj;
    float gFov;
    float gAspectRatio;
    float gNearPlane;
    float gFarPlane;
    float gLODOffset;
    uint gFrameIndex;
    float2 jitter;
}

RWTexture2D<float4> gOutput: register(u0);
RWTexture2D<float4> gSumBuffer: register(u1);
RWTexture2D<float2> gLastPosition: register(u2);
RWTexture2D<float> gDepthBuffer: register(u3);
RWTexture2D<float2> gMotionVectorBuffer: register(u4);

RaytracingAccelerationStructure SceneBVH: register(t0);

[shader("raygeneration")]
void RayGen()
{
    uint2 launchIndex = DispatchRaysIndex().xy;
    float2 dims = float2(DispatchRaysDimensions().xy);
    float2 d = ((launchIndex + 0.5F) / dims) * 2.0F - 1.0F;
    float2 jitteredD = ((launchIndex + 0.5F + jitter) / dims) * 2.0F - 1.0F;
    
    RayDesc ray;
    ray.Origin = mul(gInvView, float4(0, 0, 0, 1));
    float4 target = mul(gInvProj, float4(jitteredD.x, -jitteredD.y, 1, 1));
    ray.Direction = mul(gInvView, float4(target.xyz, 0));
    ray.TMin = gNearPlane;
    ray.TMax = gFarPlane;
    
    HitInfo payload;
    payload.colorAndDistance = float4(0, 0, 0, 0);
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
        gOutput[launchIndex].rgb = clamp(color / gFrameIndex, 0.0F, 1.0F);
    }
    
    //non jittered informations
    target = mul(gInvProj, float4(d.x, -d.y, 1, 1));
    ray.Direction = mul(gInvView, float4(target.xyz, 0));
    
    PosPayload pp;
    pp.hPosAndT = float3(0.0F, 0.0F, -1.0F);
    TraceRay(SceneBVH, RAY_FLAG_NONE, 0xFF, 3, 0, 3, ray, pp);
    
    if (pp.hPosAndT.z < 0.0F)
        gDepthBuffer[launchIndex] = 1.0F;
    else
        gDepthBuffer[launchIndex] = min(pp.hPosAndT.z / (gFarPlane - gNearPlane), 1.0F);
    
    float2 lastPos = gLastPosition[launchIndex];
    gMotionVectorBuffer[launchIndex] = lastPos - pp.hPosAndT.xy;
    gLastPosition[launchIndex] = pp.hPosAndT.xy;
}