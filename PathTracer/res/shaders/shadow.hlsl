#include "common.hlsli"

StructuredBuffer<Material> gMaterials: register(t0);
StructuredBuffer<ObjectData> gData: register(t1);

[shader("closesthit")]
void ShadowClosestHit(inout ShadowHitInfo hit, Attributes bary)
{
    ObjectData objData = gData[InstanceID()];
    Material data = gMaterials[objData.matIndex];

    hit.occlusion = min(data.diffuseAlbedo.a, 1.0F);
    hit.distance = RayTCurrent();
}

[shader("miss")]
void ShadowMiss(inout ShadowHitInfo hit: SV_RayPayload)
{
    hit.distance = -1;
}