#include "common.hlsli"

StructuredBuffer<Vertex> vertices: register(t0);
StructuredBuffer<int> indices: register(t1);
StructuredBuffer<Material> gMaterials: register(t0, space1);
StructuredBuffer<ObjectData> gData: register(t1, space1);

Texture2DArray gTextures: register(t0, space2);

SamplerState gPointWrap: register(s0);

[shader("closesthit")]
void ShadowHit(inout ShadowInfo payload, Attributes bary)
{
    ObjectData data = gData[InstanceID()];
    Material mat = gMaterials[data.materialIndex];
    
    payload.occlusion = min(mat.diffuseAlbedo.a, 1.0);
    payload.distance = RayTCurrent();
}

[shader("miss")]
void ShadowMiss(inout ShadowInfo payload: SV_RayPayload)
{
    payload.distance = -1;
}

[shader("anyhit")]
void ShadowAnyHit(inout ShadowInfo payload, in Attributes attrib)
{
    ObjectData objectData = gData[InstanceID()];
    
    if(objectData.textureIndex >= 0)
    {
        uint vertId = 3 * PrimitiveIndex();
        float3 barycentrics = float3(1.0 - attrib.bary.x - attrib.bary.y, attrib.bary.x, attrib.bary.y);
    
        float2 uvs = vertices[indices[vertId]].uvs * barycentrics.x + vertices[indices[vertId + 1]].uvs * barycentrics.y + vertices[indices[vertId + 2]].uvs * barycentrics.z;
        
        float4 mapColor = gTextures.SampleLevel(gPointWrap, float3(uvs, objectData.textureIndex), 0);
        if(mapColor.a < 0.1)
            IgnoreHit();
    }
}