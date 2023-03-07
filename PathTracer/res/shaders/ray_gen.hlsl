#include "common.hlsli"

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
RWTexture2D<float4> gAlbedoMap: register(u5);
RWTexture2D<float4> gSpecularMap: register(u6);
RWTexture2D<float4> gSky: register(u7);

RaytracingAccelerationStructure SceneBVH: register(t0);

//TODO import NRD.hlsli and define encodings
float3 _NRD_LinearToYCoCg(float3 color)
{
    float Y = dot(color, float3(0.25, 0.5, 0.25));
    float Co = dot(color, float3(0.5, 0.0, -0.5));
    float Cg = dot(color, float3(-0.25, 0.5, -0.25));

    return float3(Y, Co, Cg);
}

float4 REBLUR_FrontEnd_PackRadianceAndNormHitDist(float3 radiance, float normHitDist, bool sanitize = true)
{
    if(sanitize)
    {
        radiance = any(isnan(radiance) | isinf(radiance)) ? 0 : clamp(radiance, 0, 65504.0);
        normHitDist = (isnan(normHitDist) | isinf(normHitDist)) ? 0 : saturate(normHitDist);
    }

    // "0" is reserved to mark "no data" samples, skipped due to probabilistic sampling
    if(normHitDist != 0)
        normHitDist = max(normHitDist, 1e-7);

    radiance = _NRD_LinearToYCoCg(radiance);

    return float4(radiance, normHitDist);
}

float _REBLUR_GetHitDistanceNormalization(float viewZ, float4 hitDistParams, float roughness = 1.0)
{
    return (hitDistParams.x + abs(viewZ) * hitDistParams.y) * lerp(1.0, hitDistParams.z, saturate(exp2(hitDistParams.w * roughness * roughness)));
}

float REBLUR_FrontEnd_GetNormHitDist(float hitDist, float viewZ, float4 hitDistParams, float roughness = 1.0)
{
    float f = _REBLUR_GetHitDistanceNormalization(viewZ, hitDistParams, roughness);

    return saturate(hitDist / f);
}

float2 _NRD_EncodeUnitVector(float3 v, const bool bSigned = false)
{
    v /= dot(abs(v), 1.0);

    float2 octWrap = (1.0 - abs(v.yx)) * (step(0.0, v.xy) * 2.0 - 1.0);
    v.xy = v.z >= 0.0 ? v.xy : octWrap;

    return bSigned ? v.xy : v.xy * 0.5 + 0.5;
}

float4 NRD_FrontEnd_PackNormalAndRoughness(float3 N, float roughness, uint materialID = 0)
{
    float4 p;
    p.xy = _NRD_EncodeUnitVector(N, false);
    p.z = roughness;
    p.w = saturate(materialID / 3.0);
    return p;
}

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
    payload.specularAndDistance = float4(0, 0, 0, 0);
    payload.normalAndRough = float4(0, 0, 0, 0);
    payload.albedoAndZ = float4(0, 0, 0, -1);
    payload.metalness = 0;
    payload.recursionDepth = 1;
    
    TraceRay(SceneBVH, RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, payload);
    
    //non jittered informations
    target = mul(gInvProj, float4(d.x, -d.y, 1, 1));
    ray.Direction = mul(gInvView, float4(target.xyz, 0));
    
    PosPayload pp;
    pp.hPosAndT = float4(0.0, 0.0, 0.0, -1.0);
    TraceRay(SceneBVH, RAY_FLAG_NONE, 0xFF, 3, 0, 3, ray, pp);
        
    gDepthBuffer[launchIndex] = min(pp.hPosAndT.w / (gFarPlane - gNearPlane), 1.0F);
    
    //TODO prev world multiplication for moving objects
    float4 lastPosH = mul(float4(pp.hPosAndT.xyz, 1.0), gViewProjPrev);
    float2 uv = (lastPosH.xy / lastPosH.w) * float2(0.5, -0.5) + 0.5;
    
    float2 sampleUv = d * float2(0.5, 0.5) + 0.5;
    
    gMotionVectorBuffer[launchIndex] = (uv - sampleUv) * dims;
    gNormalAndRoughness[launchIndex] = NRD_FrontEnd_PackNormalAndRoughness(payload.normalAndRough.xyz, payload.normalAndRough.w);
    
    float z = payload.albedoAndZ.w;
    if(z < 0)
        z = gFarPlane - gNearPlane;
    gZDepth[launchIndex] = z;
    
    float hitT = REBLUR_FrontEnd_GetNormHitDist(payload.colorAndDistance.a, z, float4(3.0, 0.1, 20.0, -25.0));
    gOutput[launchIndex] = REBLUR_FrontEnd_PackRadianceAndNormHitDist(payload.colorAndDistance.rgb, hitT, true);
    gAlbedoMap[launchIndex] = float4(payload.albedoAndZ.rgb, payload.metalness);
    
    hitT = REBLUR_FrontEnd_GetNormHitDist(payload.specularAndDistance.a, z, float4(3.0, 0.1, 20.0, -25.0), payload.normalAndRough.w);
    gSpecularMap[launchIndex] = REBLUR_FrontEnd_PackRadianceAndNormHitDist(payload.specularAndDistance.rgb, hitT, true);
    if(payload.albedoAndZ.w < 0)
        gSky[launchIndex] = float4(payload.colorAndDistance.rgb, 1);
    else
        gSky[launchIndex] = 0;
}