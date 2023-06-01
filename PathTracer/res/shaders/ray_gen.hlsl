#include "common.hlsli"

#define NRD_NORMAL_ENCODING 2
#define NRD_ROUGHNESS_ENCODING 1
#include "include/NRD.hlsli"

cbuffer cbPass: register(b0)
{
    float4x4 gView;
    float4x4 gViewProjPrev;
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
RWTexture2D<float4> gNormalAndRoughness: register(u1);
RWTexture2D<float> gDepthBuffer: register(u2);
RWTexture2D<float2> gMotionVectorBuffer: register(u3);
RWTexture2D<float> gZDepth: register(u4);
RWTexture2D<float4> gSpecularMap: register(u5);
RWTexture2D<float4> gSky: register(u6);
RWTexture2D<float4> gAlbedoAndMetalness: register(u7);

RaytracingAccelerationStructure SceneBVH: register(t0);
StructuredBuffer<ObjectData> gData: register(t1);

[shader("raygeneration")]
void RayGen()
{
    uint2 launchIndex = DispatchRaysIndex().xy;
    float2 dims = float2(DispatchRaysDimensions().xy);
    float2 d = ((launchIndex + 0.5F) / dims) * 2.0F - 1.0F;
    float2 jitteredD = ((launchIndex + jitter + 0.5F) / dims) * 2.0F - 1.0F;
    
    RayDesc ray;
    ray.Origin = mul(gInvView, float4(0, 0, 0, 1)).xyz;
    float4 target = mul(gInvProj, float4(jitteredD.x, -jitteredD.y, 1, 1));
    ray.Direction = mul(gInvView, float4(target.xyz, 0)).xyz;
    ray.TMin = gNearPlane;
    ray.TMax = gFarPlane;
    
    HitInfo payload;
    payload.colorAndDistance = float4(0, 0, 0, 0);
    payload.specularAndDistance = float4(0, 0, 0, 0);
    payload.normalAndRough = float4(0, 0, 0, 0);
    payload.albedoAndZ = float4(0, 0, 0, -1);
    payload.virtualZ = -1;
    payload.recursionDepth = 1;
    
    TraceRay(SceneBVH, RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, payload);
    
    //non jittered informations TODO is needed?
    //target = mul(gInvProj, float4(d.x, -d.y, 1, 1));
    //ray.Direction = mul(gInvView, float4(target.xyz, 0)).xyz;
    
    PosPayload pp;
    pp.hPosAndT = float4(0.0, 0.0, 0.0, -1.0);
    pp.instanceID = -1;
    TraceRay(SceneBVH, RAY_FLAG_NONE, 0xFF, 3, 0, 3, ray, pp);
    
    gDepthBuffer[launchIndex] = min(pp.hPosAndT.w / (gFarPlane - gNearPlane), 1.0F);
    
    float4 lastPosH;
    if(pp.instanceID < 0) //if missed, use the position as is
        lastPosH = mul(float4(pp.hPosAndT.xyz, 1.0), gViewProjPrev);
    else //if hit, multiply the position by its last world matrix
    {
        float3 lastPos = mul(float4(pp.hPosAndT.xyz, 1.0), gData[pp.instanceID].toPrevWorld).xyz;
        lastPosH = mul(float4(lastPos, 1.0), gViewProjPrev);
    }
            
    float2 uv = (lastPosH.xy / lastPosH.w) * float2(0.5, -0.5) + 0.5;
    float2 sampleUv = jitteredD * 0.5 + 0.5;
    
    gMotionVectorBuffer[launchIndex] = (uv - sampleUv) * dims;
    gNormalAndRoughness[launchIndex] = NRD_FrontEnd_PackNormalAndRoughness(payload.normalAndRough.xyz, payload.normalAndRough.w);
    
    float z = payload.albedoAndZ.w, virtualZ = payload.virtualZ;
    if(z < 0)
        z = gFarPlane - gNearPlane;
    if(virtualZ < 0)
        virtualZ = gFarPlane - gNearPlane;
    gZDepth[launchIndex] = z;
    
    const float4 hitDistParams = float4(3.0, 0.1, 20.0, -25.0);
    
    float hitT = REBLUR_FrontEnd_GetNormHitDist(payload.colorAndDistance.a, z, hitDistParams);
    gOutput[launchIndex] = REBLUR_FrontEnd_PackRadianceAndNormHitDist(payload.colorAndDistance.rgb, hitT, true);
    
    hitT = REBLUR_FrontEnd_GetNormHitDist(payload.specularAndDistance.a, virtualZ, hitDistParams, payload.normalAndRough.w);
    gSpecularMap[launchIndex] = REBLUR_FrontEnd_PackRadianceAndNormHitDist(payload.specularAndDistance.rgb, hitT, true);
    if(payload.albedoAndZ.w < 0)
        gSky[launchIndex] = float4(payload.colorAndDistance.rgb, 1);
    else
        gSky[launchIndex] = 0;
    gAlbedoAndMetalness[launchIndex] = float4(payload.albedoAndZ.rgb, 1);
}