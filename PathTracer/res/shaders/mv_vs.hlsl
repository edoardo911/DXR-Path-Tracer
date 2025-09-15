#include "common.hlsli"

MVVertexOut main(VertexIn vin, uint instanceID: SV_InstanceID, uint vertID: SV_VertexID)
{
    ObjectData data = gObjectData[instanceID];
    Material mat;
    
    if(data.materialIndex >= 0)
        mat = gMaterials[data.materialIndex];
    else
        mat.matTransform = float4x4(1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0);
    
    MVVertexOut vout = (MVVertexOut) 0.0;
    
    float4 currentPosW = mul(float4(vin.pos, 1.0), data.world);
    
    vout.zPos.x = mul(currentPosW, gView).z;
    vout.zPos.y = mul(currentPosW, gViewPrev).z;
    
    vout.pos = mul(currentPosW, gViewProj);
    
    vout.posH = vout.pos;
    vout.prevPosH = mul(currentPosW, gViewProjPrev);
            
    float4 uvs = mul(float4(vin.uvs, 0.0, 1.0), data.texTransform);
    vout.uvs = mul(uvs, mat.matTransform).xy;
    vout.instanceID = instanceID;
    
    return vout;
}