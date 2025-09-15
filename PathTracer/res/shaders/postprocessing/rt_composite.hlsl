#define NRD_NORMAL_ENCODING 2
#define NRD_ROUGHNESS_ENCODING 1
#include "../raytracing/include/NRD.hlsli"

#include "../raytracing/path_tracing_utils.hlsli"

RWTexture2D<float4> gOutput: register(u0);

Texture2D gDiffuse: register(t0);
Texture2D gSpecular: register(t1);
Texture2D gSky: register(t2);
Texture2D gAlbedo: register(t3);
Texture2D gShadowData: register(t4);
Texture2D gRF0: register(t5);
Texture2D gDiffSH1: register(t6);
Texture2D gSpecSH1: register(t7);
Texture2D gNormals: register(t8);
Texture2D gView: register(t9);
Texture2D gZ: register(t10);

//#define NRD_VALIDATION

[numthreads(32, 16, 1)]
void main(uint3 threadID: SV_DispatchThreadID)
{
    //extract
    float4 packedDiffuse = gDiffuse[threadID.xy];
    float4 packedSpecular = gSpecular[threadID.xy];
    float4 diffSH1 = gDiffSH1[threadID.xy];
    float4 specSH1 = gSpecSH1[threadID.xy];
    float3 view = gView[threadID.xy].xyz;
    float4 skyColor = gSky[threadID.xy];
    float4 albedo = gAlbedo[threadID.xy];
    float3 rf0 = gRF0[threadID.xy].rgb;
    float4 shadowColor = gShadowData[threadID.xy];
    
    NRD_SG diffSG = REBLUR_BackEnd_UnpackSh(packedDiffuse, diffSH1);
    NRD_SG specSG = REBLUR_BackEnd_UnpackSh(packedSpecular, specSH1);

    float4 normal = NRD_FrontEnd_UnpackNormalAndRoughness(gNormals[threadID.xy]);
    
    //resolve (macro details)
    float roughness = NRD_SG_ExtractRoughnessAA(specSG);
    float4 diffuseColor = float4(NRD_SG_ResolveDiffuse(diffSG, normal.xyz), diffSG.normHitDist);
    float3 specularColor = NRD_SG_ResolveSpecular(specSG, normal.xyz, view, roughness);
    float3 shadow = 1.0 - shadowColor.yzw;
    float darkness = max(sqrt(diffuseColor.a) * pow(shadow.g * shadow.r, 0.4), 0.15);
    
    //rejitter (micro details)
    float3 Ne = NRD_FrontEnd_UnpackNormalAndRoughness(gNormals[threadID.xy + int2(1, 0)]).xyz;
    float3 Nw = NRD_FrontEnd_UnpackNormalAndRoughness(gNormals[threadID.xy + int2(-1, 0)]).xyz;
    float3 Nn = NRD_FrontEnd_UnpackNormalAndRoughness(gNormals[threadID.xy + int2(0, 1)]).xyz;
    float3 Ns = NRD_FrontEnd_UnpackNormalAndRoughness(gNormals[threadID.xy + int2(0, -1)]).xyz;
    
    float Z = gZ[threadID.xy].x;
    float Ze = gZ[threadID.xy + int2(1, 0)].x;
    float Zw = gZ[threadID.xy + int2(-1, 0)].x;
    float Zn = gZ[threadID.xy + int2(0, 1)].x;
    float Zs = gZ[threadID.xy + int2(0, -1)].x;
    
    float2 scale = NRD_SG_ReJitter(diffSG, specSG, rf0, view, roughness, Z, Ze, Zw, Zn, Zs, normal.xyz, Ne, Nw, Nn, Ns);
    
    diffuseColor.rgb *= scale.x;
    specularColor *= scale.y;
    
    float3 diffFactor;
    float3 specFactor;
    NRD_MaterialFactors(normal.xyz, view, albedo.rgb, rf0, roughness, diffFactor, specFactor);
            
    float3 result = skyColor.a > 0 ? skyColor.rgb : (diffuseColor.rgb * diffFactor + specularColor * specFactor) * darkness;
    
#ifdef NRD_VALIDATION
    if(threadID.x < 320 || threadID.x > 960 || threadID.y < 180)
        gOutput[threadID.xy] = packedDiffuse;
    else
#endif
    gOutput[threadID.xy] = float4(result, 1.0);
}