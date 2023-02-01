#include "common.hlsli"

StructuredBuffer<Material> gMaterials: register(t0);

cbuffer objPass: register(b0)
{
    float4x4 gWorld;
    int gDiffuseIndex;
    int gNormalIndex;
    uint gMatIndex;
}

[shader("closesthit")]
void ShadowClosestHit(inout ShadowHitInfo hit, Attributes bary)
{
    Material data = gMaterials[gMatIndex];

    hit.occlusion = 0.5F + data.diffuseAlbedo.a / 2.0F;
    hit.distance = RayTCurrent();
}

[shader("miss")]
void ShadowMiss(inout ShadowHitInfo hit: SV_RayPayload)
{
    hit.distance = -1;
}