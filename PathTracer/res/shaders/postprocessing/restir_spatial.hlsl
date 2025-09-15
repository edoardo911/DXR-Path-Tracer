#include "../raytracing/path_tracing_utils.hlsli"

struct Reservoir
{
    uint sampleIndex;
    float weightSum;
    float W;
    uint M;
};

struct Light
{
    float3 Strength;
    float FalloffStart;
    float3 Direction;
    float FalloffEnd;
    float3 Position;
    float SpotPower;
    int type;
    float radius;
    float2 pad;
};

StructuredBuffer<Reservoir> gInput: register(t0);
Texture2D gDepthBuffer: register(t1);
Texture2D gNormals: register(t2);
Texture2D gRF0: register(t3);

RWStructuredBuffer<Reservoir> gOutput: register(u0);

SamplerState gLinearSampler: register(s0);

cbuffer cbData: register(b0)
{
    float4x4 gInvView;
    float4x4 gInvProj;
    float3 gCamPos;
    float pad;
    uint gFrameIndex;
    uint gLightCount;
    uint gWidth;
    uint gHeight;
    Light gLights[128];
}

groupshared Reservoir reservoirCache[10][10];
groupshared float3 normalCache[10][10];
groupshared float depthCache[10][10];

#define LIGHT_TYPE_DIRECTIONAL		0
#define LIGHT_TYPE_SPOTLIGHT		1
#define LIGHT_TYPE_POINTLIGHT		2
#define NRD_EPS                     1e-6
#define COMMON_ONLY
#include "../utils.hlsli"
#include "../raytracing/restir_utils.hlsli"

[numthreads(16, 8, 1)]
void main(uint3 threadID: SV_DispatchThreadID, uint3 groupThreadID: SV_GroupThreadID)
{
    int2 localCoord = int2(groupThreadID.xy);
    uint seed = initRand(threadID.x * gFrameIndex, threadID.y * gFrameIndex, 16);
    
    if(threadID.x < gWidth && threadID.y < gHeight)
    {
        float2 uvs = (float2(threadID.xy) + 0.5) / float2(gWidth, gHeight);
        
        reservoirCache[localCoord.x + 1][localCoord.y + 1] = gInput[threadID.x + threadID.y * gWidth];
        normalCache[localCoord.x + 1][localCoord.y + 1] = gNormals.SampleLevel(gLinearSampler, uvs, 0.0).xyz;
        depthCache[localCoord.x + 1][localCoord.y + 1] = gDepthBuffer.SampleLevel(gLinearSampler, uvs, 0.0).x;
    }
    
    [unroll]
    for(int dy = -1; dy <= 1; ++dy)
    {
        for(int dx = -1; dx <= 1; ++dx)
        {
            int lx = localCoord.x + dx + 1;
            int ly = localCoord.y + dy + 1;
            
            int2 ng = threadID.xy + int2(dx, dy);
            if(lx >= 0 && lx < 10 && ly >= 0 && ly < 10 && ng.x >= 0 && ng.y >= 0 && ng.x < (int) gWidth && ng.y < (int) gHeight)
            {
                float2 uvs = (float2(ng) + 0.5) / float2(gWidth, gHeight);
                
                reservoirCache[lx][ly] = gInput[ng.x + ng.y * gWidth];
                normalCache[lx][ly] = gNormals.SampleLevel(gLinearSampler, uvs, 0.0).xyz;
                depthCache[lx][ly] = gDepthBuffer.SampleLevel(gLinearSampler, uvs, 0.0).x;
            }
        }
    }
    
    GroupMemoryBarrierWithGroupSync();
    
    Reservoir
    merged = (Reservoir) 0.0;
    
    Reservoir reservoirs[9];
    uint count = 0;
    
    float3 centerNormal = normalCache[localCoord.x + 1][localCoord.y + 1];
    float centerDepth = depthCache[localCoord.x + 1][localCoord.y + 1];
    
    [unroll]
    for(dy = -1; dy <= 1; ++dy)
    {
        [unroll]
        for(int dx = -1; dx <= 1; ++dx)
        {
            int lx = threadID.x + dx + 1;
            int ly = threadID.y + dy + 1;
            
            if(lx < 0 || ly < 0 || lx >= 10 || ly >= 10)
                continue;
            
            reservoirs[count] = (Reservoir) 0.0;
            
            float3 n = normalCache[lx][ly];
            if(dot(n, centerNormal) < 0.95)
                continue;
            
            float d = depthCache[lx][ly];
            if(abs(d - centerDepth) > 0.01)
                continue;
            
            reservoirs[count++] = reservoirCache[lx][ly];
        }
    }
    
    float2 uvs = (float2(threadID.xy) + 0.5) / float2(gWidth, gHeight);
    float2 ndc = uvs * 2.0 - 1.0;
    float4 viewPos = mul(gInvProj, float4(ndc, centerDepth, 1.0));
    viewPos /= viewPos.w;
    
    float3 pos = mul(gInvView, viewPos).xyz;
    float3 toEye = normalize(gCamPos - pos);
    float rough = gNormals.SampleLevel(gLinearSampler, uvs, 0.0).w;
    float3 RF0 = gRF0.SampleLevel(gLinearSampler, uvs, 0.0).rgb;
    
    mergeReservoirSpatial(reservoirs, pos, centerNormal, RF0, rough, toEye, seed);
    gOutput[threadID.x + threadID.y * gWidth] = merged;
}