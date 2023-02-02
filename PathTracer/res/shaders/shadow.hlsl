#include "common.hlsli"

StructuredBuffer<Material> gMaterials: register(t0);

cbuffer objPass: register(b0)
{
    float4x4 gWorld;
    int gDiffuseIndex;
    int gNormalIndex;
    uint gMatIndex;
}

#define SHADOW_BIAS 0.3F

[shader("closesthit")]
void ShadowClosestHit(inout ShadowHitInfo hit, Attributes bary)
{
    Material data = gMaterials[gMatIndex];

    hit.occlusion = min((data.diffuseAlbedo.a / 2.0F) + SHADOW_BIAS, 1.0F);
    hit.distance = RayTCurrent();
}

[shader("miss")]
void ShadowMiss(inout ShadowHitInfo hit: SV_RayPayload)
{
    hit.distance = -1;
}