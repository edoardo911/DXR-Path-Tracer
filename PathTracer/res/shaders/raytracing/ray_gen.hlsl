#include "common.hlsli"

#define NRD_NORMAL_ENCODING 2
#define NRD_ROUGHNESS_ENCODING 1
#include "include/NRD.hlsli"

RWTexture2D<float4> gDiffuse: register(u0);
RWTexture2D<float4> gSpecularOut: register(u1);
RWTexture2D<float4> gNormalAndRoughness: register(u2);
RWTexture2D<float> gZDepth: register(u3);
RWTexture2D<float4> gSky: register(u4);
RWTexture2D<float4> gAlbedo: register(u5);
RWTexture2D<float> gShadowData: register(u6);
RWTexture2D<float4> gShadowTranslucency: register(u7);
RWTexture2D<float4> gRF0: register(u8);
RWTexture2D<float4> gDiffSH1: register(u9);
RWTexture2D<float4> gSpecSH1: register(u10);
RWTexture2D<float4> gViewAndRF0: register(u11);
RWTexture2D<float> gDiffConfidence: register(u12);
RWTexture2D<float> gSpecConfidence: register(u13);
RWStructuredBuffer<Reservoir> gCandidates: register(u14);

StructuredBuffer<Material> gMaterials: register(t1);

RaytracingAccelerationStructure SceneBVH: register(t0);
Texture2D gMotionVectors: register(t2);
Texture2D gDepthBuffer: register(t3);
StructuredBuffer<Reservoir> gCandidateHistory: register(t4);

SamplerState gPointWrap: register(s0);
SamplerState gBilinearWrap: register(s1);
SamplerState gTrilinearWrap: register(s2);

#define COMMON_ONLY
#include "../utils.hlsli"

#include "path_tracing_utils.hlsli"

[shader("raygeneration")]
void RayGen()
{
    uint2 launchIndex = DispatchRaysIndex().xy;
    float2 dims = float2(DispatchRaysDimensions().xy);
    float2 d = ((launchIndex + 0.5) / dims + jitter) * 2.0 - 1.0;
    
    //color
    RayDesc ray;
    ray.Origin = mul(gInvView, float4(0.0, 0.0, 0.0, 1.0)).xyz;
    float4 target = mul(gInvProj, float4(d.x, -d.y, 1.0, 1.0));
    ray.Direction = mul(gInvView, float4(target.xyz, 0.0)).xyz;
    ray.TMin = 0.1;
    ray.TMax = 1000.0;
    
    HitInfo payload;
    payload.colorAndDistance = float4(0, 0, 0, 0);
    payload.specAndDistance = float4(0, 0, 0, 0);
    payload.roughAndZ = float2(0, -1);
    payload.normal = 0;
    payload.albedo = 0;
    payload.cosW = 0;
    payload.vndf = 0;
    payload.shadow = 0.0;
    payload.recursionDepth = 1;
    payload.candidate = (Reservoir) 0.0;
    if(gRayReconstruction)
        payload.shadow.a = 1;
    
    float2 uvs = (launchIndex + 0.5) / dims;
    float2 mv = gMotionVectors.SampleLevel(gPointWrap, uvs, 0.0).xyz;
    int2 prevPixel = launchIndex - int2(mv * float2(0.5 * dims.x, -0.5 * dims.y));
    
    float depth = gDepthBuffer.SampleLevel(gBilinearWrap, uvs, 0.0).r;
    float prevDepth = gDepthBuffer.SampleLevel(gBilinearWrap, (prevPixel + 0.5) / dims, 0.0).r;
    
    if(gRayReconstruction)
    {
        uint seed = initRand(launchIndex.x * gFrameIndex, launchIndex.y * gFrameIndex, 16);
        int2 offset = int2(nextRand(seed) * 3.0, nextRand(seed) * 3.0) - int2(1, 1);
        prevPixel += offset;
    }
    
    if(abs(depth - prevDepth) < 0.01 && prevPixel.x >= 0 && prevPixel.y >= 0 && prevPixel.x < dims.x && prevPixel.y < dims.y)
        payload.candidate = gCandidateHistory[prevPixel.x + prevPixel.y * dims.x];
    
    TraceRay(SceneBVH, RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, payload);
    
    gCandidates[launchIndex.x + launchIndex.y * dims.x] = payload.candidate;
    if(gRayReconstruction)
    {
        gDiffuse[launchIndex] = float4(payload.colorAndDistance.rgb, payload.shadow.a);
        gNormalAndRoughness[launchIndex] = float4(unpackDirection(payload.normal), payload.roughAndZ.x);
        gDiffConfidence[launchIndex] = payload.colorAndDistance.a;
        gSpecConfidence[launchIndex] = payload.specAndDistance.a;
        gAlbedo[launchIndex] = float4(unpackColorLDR(payload.albedo), 0.0);
        gShadowTranslucency[launchIndex] = float4(payload.shadow.rgb, 0.0);
        gZDepth[launchIndex] = payload.roughAndZ.y;
        
        if(payload.roughAndZ.y < 0)
        {
            gRF0[launchIndex] = float4(0.0, 0.0, 0.0, 0.0);
            gSky[launchIndex] = float4(payload.colorAndDistance.rgb, 1.0);
        }
        else
        {
            gSky[launchIndex] = float4(0, 0, 0, 0);
            gRF0[launchIndex] = float4(gMaterials[payload.recursionDepth].fresnelR0, 0.0);
        }
    }
    else
    {
        float z = payload.roughAndZ.y;
        if(z < 0)
            z = 999;
    
        const float4 hitDistParams = float4(3.0, 0.1, 20.0, -25.0);
        
        float hitT = REBLUR_FrontEnd_GetNormHitDist(payload.colorAndDistance.a, z, hitDistParams, 1.0);
        gDiffuse[launchIndex] = REBLUR_FrontEnd_PackSh(payload.colorAndDistance.rgb, hitT, unpackDirection(payload.cosW), gDiffSH1[launchIndex], true);
    
        hitT = REBLUR_FrontEnd_GetNormHitDist(payload.specAndDistance.a, z, hitDistParams, payload.roughAndZ.x);
        gSpecularOut[launchIndex] = REBLUR_FrontEnd_PackSh(payload.specAndDistance.rgb, hitT, unpackDirection(payload.vndf), gSpecSH1[launchIndex], true);
    
        gNormalAndRoughness[launchIndex] = NRD_FrontEnd_PackNormalAndRoughness(unpackDirection(payload.normal), payload.roughAndZ.x, payload.recursionDepth);
        gZDepth[launchIndex] = z;
        gAlbedo[launchIndex] = float4(unpackColorLDR(payload.albedo), 0.0);
        gViewAndRF0[launchIndex] = float4(-normalize(ray.Direction), 0.0);
    
        gShadowTranslucency[launchIndex] = float4(payload.shadow.rgb, 0.0);
        gShadowData[launchIndex] = payload.shadow.w;
    
        if(payload.roughAndZ.y < 0)
        {
            gSky[launchIndex] = float4(payload.colorAndDistance.rgb, 1.0);
            gRF0[launchIndex] = float4(0.0, 0.0, 0.0, 0.0);
            gDiffConfidence[launchIndex] = 0.0;
            gSpecConfidence[launchIndex] = 0.0;
        }
        else
        {
            gSky[launchIndex] = float4(0, 0, 0, 0);
            gRF0[launchIndex] = float4(gMaterials[payload.recursionDepth].fresnelR0, 0.0);
            gDiffConfidence[launchIndex] = 1.0;
            gSpecConfidence[launchIndex] = 1.0;
        }
    }
}