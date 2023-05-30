#pragma once

#define MAX_LIGHTS              16
#define MAX_RECURSION_DEPTH     3

#define LIGHT_TYPE_DIRECTIONAL  0
#define LIGHT_TYPE_POINTLIGHT   1
#define LIGHT_TYPE_SPOTLIGHT    2

#define NUM_DIR_LIGHTS 0
#define NUM_POINT_LIGHTS 0
#define NUM_SPOT_LIGHTS 1

//payloads
struct HitInfo
{
    float4 colorAndDistance;
    float4 specularAndDistance;
    float4 normalAndRough;
    float4 albedoAndZ;
    float virtualZ;
    uint recursionDepth;
};

struct ShadowHitInfo
{
    float occlusion;
    float distance;
};

struct AOHitInfo
{
    bool isHit;
    float hitT;
};

struct PosPayload
{
    float4 hPosAndT;
    int instanceID;
};

//other structs
struct Attributes
{
    float2 bary;
};

struct Vertex
{
    float3 pos;
    float2 uvs;
    float3 normal;
    float3 tangent;
};

struct Light
{
    float3 Strength;
    float FalloffStart;
    float3 Direction;
    float FalloffEnd;
    float3 Position;
    float SpotPower;
    float Radius;
    uint Type;
};

struct LightMaterial
{
    float4 DiffuseAlbedo;
    float3 FresnelR0;
    float Shininess;
};

struct Material
{
    float4 diffuseAlbedo;
    float3 fresnelR0;
    float fresnelPower;
    float roughness;
    float metallic;
    float refractionIndex;
    int flags;
};

struct ObjectData
{
    float4x4 world;
    float4x4 toPrevWorld;
    int diffuseIndex;
    int normalIndex;
    uint matIndex;
};

float CalcAttenuation(float d, float falloffStart, float falloffEnd)
{
    return saturate((falloffEnd - d) / (falloffEnd - falloffStart));
}

float3 SchlickFresnel(float3 R0, float3 normal, float3 lightVec)
{
    float cosIncidentAngle = saturate(dot(normal, lightVec));

    float f0 = 1.0F - cosIncidentAngle;
    float3 reflectPercent = R0 + (1.0F - R0) * (f0 * f0 * f0 * f0 * f0);
    return reflectPercent;
}

float3 BlinnPhong(float3 lightStrength, float3 lightVec, float3 normal, float3 toEye, LightMaterial mat, out float3 specAlbedo)
{
    const float m = mat.Shininess * 256.0F;
    float3 halfVec = normalize(toEye + lightVec);

    float roughnessFactor = (m + 8.0F) * pow(max(dot(halfVec, normal), 0.0F), m) / 8.0F;
    float3 fresnelFactor = SchlickFresnel(mat.FresnelR0, halfVec, lightVec);

    specAlbedo = fresnelFactor * roughnessFactor;
    specAlbedo = specAlbedo / (specAlbedo + 1.0F);
    specAlbedo *= lightStrength;

    return lightStrength;
}

float3 ComputeDirectionalLight(Light L, LightMaterial mat, float3 normal, float3 toEye, out float3 specAlbedo)
{
    float3 lightVec = -L.Direction;

    float ndotl = max(dot(lightVec, normal), 0.0F);
    float3 lightStrength = L.Strength * ndotl;

    return BlinnPhong(lightStrength, lightVec, normal, toEye, mat, specAlbedo);
}

float3 ComputePointLight(Light L, LightMaterial mat, float3 pos, float3 normal, float3 toEye, out float3 specAlbedo)
{
    float3 lightVec = L.Position - pos;
    float d = length(lightVec);

    if (d > L.FalloffEnd)
        return 0.0F;

    lightVec /= d;

    float ndotl = max(dot(lightVec, normal), 0.0F);
    float3 lightStrength = L.Strength * ndotl;

    float att = CalcAttenuation(d, L.FalloffStart, L.FalloffEnd);
    lightStrength *= att;

    return BlinnPhong(lightStrength, lightVec, normal, toEye, mat, specAlbedo);
}

float3 ComputeSpotLight(Light L, LightMaterial mat, float3 pos, float3 normal, float3 toEye, out float3 specAlbedo)
{
    float3 lightVec = L.Position - pos;
    float d = length(lightVec);

    if (d > L.FalloffEnd)
        return 0.0F;

    lightVec /= d;

    float ndotl = max(dot(lightVec, normal), 0.0F);
    float3 lightStrength = L.Strength * ndotl;

    float att = CalcAttenuation(d, L.FalloffStart, L.FalloffEnd);
    lightStrength *= att;

    float spotFactor = pow(max(dot(-lightVec, L.Direction), 0.0F), L.SpotPower);
    lightStrength *= spotFactor;

    return BlinnPhong(lightStrength, lightVec, normal, toEye, mat, specAlbedo);
}

float4 ComputeLighting(Light light, LightMaterial mat, float3 pos, float3 normal, float3 toEye, out float3 specAlbedo)
{
    float3 result = 0.0F;
    int i = 0;

    if(light.Type == LIGHT_TYPE_DIRECTIONAL)
        result += ComputeDirectionalLight(light, mat, normal, toEye, specAlbedo);
    else if(light.Type == LIGHT_TYPE_POINTLIGHT)
        result += ComputePointLight(light, mat, pos, normal, toEye, specAlbedo);
    else if(light.Type == LIGHT_TYPE_SPOTLIGHT)
        result += ComputeSpotLight(light, mat, pos, normal, toEye, specAlbedo);

    return float4(result, 0.0F);
}

float3 normalSampleToWorldSpace(float3 normalMapSample, float3 unitNormalW, float3 tangentW)
{
    float3 normalT = 2.0 * normalMapSample - 1.0;

    float3 N = unitNormalW;
    float3 T = normalize(tangentW - dot(tangentW, N) * N);
    float3 B = cross(N, T);

    float3x3 TBN = float3x3(T, B, N);

    return mul(normalT, TBN);
}