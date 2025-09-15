#include "common.hlsli"
#include "path_tracing_utils.hlsli"

#define NRD_NORMAL_ENCODING 2
#define NRD_ROUGHNESS_ENCODING 1
#include "include/NRD.hlsli"

StructuredBuffer<Vertex> vertices: register(t0);
StructuredBuffer<int> indices: register(t1);
StructuredBuffer<Material> gMaterials: register(t0, space1);
StructuredBuffer<ObjectData> gData: register(t1, space1);

Texture2DArray gTextures: register(t0, space2);
Texture2DArray gNormalMaps: register(t1, space2);
Texture2DArray gEmissiveMaps: register(t2, space2);

RaytracingAccelerationStructure SceneBVH: register(t2);

SamplerState gPointWrap: register(s0);
SamplerState gBilinearWrap: register(s1);
SamplerState gTrilinearWrap: register(s2);

#define NO_BLUE_NOISE
#include "../utils.hlsli"
#include "restir_utils.hlsli"

[shader("closesthit")]
void IndirectHit(inout IndirectInfo payload, Attributes attrib)
{
    ObjectData objectData = gData[InstanceID()];
    Material material = gMaterials[objectData.materialIndex];
    
    uint vertId = 3 * PrimitiveIndex();
    float3 worldOrigin = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
    float3 bary = float3(1.0 - attrib.bary.x - attrib.bary.y, attrib.bary.x, attrib.bary.y);
    float3 normRayDir = normalize(WorldRayDirection());
    
    //vertex data
    float2 uvs = vertices[indices[vertId]].uvs * bary.x + vertices[indices[vertId + 1]].uvs * bary.y + vertices[indices[vertId + 2]].uvs * bary.z;
    float3 norm = vertices[indices[vertId]].norm * bary.x + vertices[indices[vertId + 1]].norm * bary.y + vertices[indices[vertId + 2]].norm * bary.z;
    float3 tangent = vertices[indices[vertId]].tangent * bary.x + vertices[indices[vertId + 1]].tangent * bary.y + vertices[indices[vertId + 2]].tangent * bary.z;
    
    uint seed = initRand(DispatchRaysIndex().x * gFrameIndex, DispatchRaysIndex().y * gFrameIndex, 16);
        
    //uvs transformation
    float4 transUvs = mul(float4(uvs, 0.0, 1.0), objectData.texTransform);
    uvs = mul(transUvs, material.matTransform).xy;
    
    //world normalization
    norm = normalize(mul(norm, (float3x3) objectData.world).xyz);
    tangent = normalize(mul(tangent, (float3x3) objectData.world).xyz);
    
    //texturing
    float4 mapColor = float4(1, 1, 1, 1);
    if(gTexturing && objectData.textureIndex >= 0)
        mapColor = sampleTextureLOD(0.0, uvs, gTextures, objectData.textureIndex);
    //normal mapping
    if(gNormalMapping && objectData.normalIndex >= 0)
    {
        float3 sampleNorm = sampleTextureLOD(0.0, uvs, gNormalMaps, objectData.normalIndex).xyz;
        norm = normalSampleToWorldSpace(sampleNorm, norm, tangent);
    }
    //emissive
    float3 emissive = float3(0.0, 0.0, 0.0);
    if(objectData.emissiveIndex >= 0)
        emissive = material.emission * sampleTextureLOD(0.0, uvs, gEmissiveMaps, objectData.emissiveIndex).r;
    
    //restir
    Reservoir reservoirs[2];
    reservoirs[0] = (Reservoir) 0.0;
    reservoirs[1] = (Reservoir) 0.0;
    if(gLightCount > 0)
    {
        int i;
        for(i = 0; i < gLightCount; ++i)
            computeLightSample(reservoirs[0], gLights[i], worldOrigin, norm, i, material.fresnelR0, material.roughness, -normRayDir, seed);
        for(i = 0; i < gLightCount; ++i)
            if(i != reservoirs[0].sampleIndex)
                computeLightSample(reservoirs[1], gLights[i], worldOrigin, norm, i, material.fresnelR0, material.roughness, -normRayDir,seed);
    }
    if(gLightCount == 1)
        reservoirs[0].W = 1.0;
    
    //diffuse albedo
    float4 diffuseAlbedo = material.diffuseAlbedo * mapColor; 
    for(int i = 0; i < min(gLightCount, 2); ++i)
        payload.colorAndDistance.rgb += calcIndirectLight(reservoirs[i], diffuseAlbedo, norm, worldOrigin, material.roughness, material.metallic, material.refractionIndex) * 1.5;
    payload.colorAndDistance.rgb += emissive * 0.6;
    payload.colorAndDistance.rgb /= gLightCount;
    payload.colorAndDistance.a = RayTCurrent();
}

[shader("miss")]
void IndirectMiss(inout IndirectInfo payload: SV_RayPayload)
{
    payload.colorAndDistance = float4(0, 0, 0, 1e7);
}