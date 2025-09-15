#include "common.hlsli"

float4 main(MVVertexOut pin): SV_TARGET
{
#ifdef ALPHA_TESTED
    ObjectData data = gObjectData[pin.instanceID];
    Material mat;
    
    if(data.materialIndex >= 0)
        mat = gMaterials[data.materialIndex];
    else
        mat = (Material) 0.0;
    float4 diffuseAlbedo = mat.diffuseAlbedo;
    if(diffuseAlbedo.a < 0.1)
        diffuseAlbedo.a = 0.1;
    if(data.textureIndex >= 0)
        diffuseAlbedo *= sampleTexture(pin.uvs, gTextures, data.textureIndex);
    
    clip(diffuseAlbedo.a - 0.1);
#endif
    
    float3 current = pin.posH.xyz / pin.posH.w;
    float3 last = pin.prevPosH.xyz / pin.prevPosH.w;
    
    return float4(current.xy - last.xy, pin.zPos.y - pin.zPos.x, 0.0);
}