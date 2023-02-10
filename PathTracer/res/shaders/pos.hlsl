#include "common.hlsli"

StructuredBuffer<Vertex> vertices: register(t0);
StructuredBuffer<int> indices: register(t1);

[shader("closesthit")]
void PosClosestHit(inout PosPayload payload, Attributes attrib)
{
    uint vertId = 3 * PrimitiveIndex();
    float3 barycentrics = float3(1.0F - attrib.bary.x - attrib.bary.y, attrib.bary.x, attrib.bary.y);    
    float3 pos = vertices[indices[vertId]].pos * barycentrics.x + vertices[indices[vertId + 1]].pos * barycentrics.y +
				 vertices[indices[vertId + 2]].pos * barycentrics.z;
    
    payload.hPosAndT = float3(pos.xy, RayTCurrent());
}

[shader("miss")]
void PosMiss(inout PosPayload payload: SV_RayPayload)
{
    payload.hPosAndT.xy = RayTCurrent() * 9999.0F;
}