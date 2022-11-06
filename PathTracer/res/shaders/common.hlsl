#pragma once

#define MAX_LIGHTS 16
#define MAX_IMAGES 0
#define MAX_NORMAL_MAPS 0

//payloads
struct HitInfo
{
	float4 colorAndDistance;
    uint recursionDepth;
};

struct AOHitInfo
{
    bool isHit;
    int instanceID;
};

struct ShadowHitInfo
{
    float occlusion;
    float distance;
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

struct Material
{
	float4 DiffuseAlbedo;
	float3 FresnelR0;
	float Shininess;
};

struct Light
{
    float3 Strength;
    float FalloffStart;
    float3 Direction;
    float FalloffEnd;
    float3 Position;
    float SpotPower;
};

struct MaterialData
{
    float4 diffuseAlbedo;
    float3 fresnelR0;
    float fresnelPower;
    float roughness;
    float reflectiveIndex;
    float refractionIndex;
    int flags;
};

//random
uint initRand(uint val0, uint val1, uint backoff = 16)
{
    uint v0 = val0, v1 = val1, s0 = 0;

    [unroll]
    for(uint n = 0; n < backoff; n++)
    {
        s0 += 0x9e3779b9;
        v0 += ((v1 << 4) + 0xa341316c) ^ (v1 + s0) ^ ((v1 >> 5) + 0xc8013ea4);
        v1 += ((v0 << 4) + 0xad90777d) ^ (v0 + s0) ^ ((v0 >> 5) + 0x7e95761e);
    }
    return v0;
}

float nextRand(inout uint s)
{
    s = (1664525u * s + 1013904223u);
    return float(s & 0x00FFFFFF) / float(0x01000000);
}

#define NUM_DIR_LIGHTS 0
#define NUM_POINT_LIGHTS 0
#define NUM_SPOT_LIGHTS 1

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

float3 BlinnPhong(float3 lightStrength, float3 lightVec, float3 normal, float3 toEye, Material mat)
{
    const float m = mat.Shininess * 256.0F;
    float3 halfVec = normalize(toEye + lightVec);

    float roughnessFactor = (m + 8.0F) * pow(max(dot(halfVec, normal), 0.0F), m) / 8.0F;
    float3 fresnelFactor = SchlickFresnel(mat.FresnelR0, halfVec, lightVec);

    float3 specAlbedo = fresnelFactor * roughnessFactor;
    specAlbedo = specAlbedo / (specAlbedo + 1.0F);

    return (mat.DiffuseAlbedo.rgb + specAlbedo) * lightStrength;
}

float3 ComputeDirectionalLight(Light L, Material mat, float3 normal, float3 toEye)
{
    float3 lightVec = -L.Direction;

    float ndotl = max(dot(lightVec, normal), 0.0F);
    float3 lightStrength = L.Strength * ndotl;

    return BlinnPhong(lightStrength, lightVec, normal, toEye, mat);
}

float3 ComputePointLight(Light L, Material mat, float3 pos, float3 normal, float3 toEye)
{
    float3 lightVec = L.Position - pos;
    float d = length(lightVec);

    if(d > L.FalloffEnd)
        return 0.0F;

    lightVec /= d;

    float ndotl = max(dot(lightVec, normal), 0.0F);
    float3 lightStrength = L.Strength * ndotl;

    float att = CalcAttenuation(d, L.FalloffStart, L.FalloffEnd);
    lightStrength *= att;

    return BlinnPhong(lightStrength, lightVec, normal, toEye, mat);
}

float3 ComputeSpotLight(Light L, Material mat, float3 pos, float3 normal, float3 toEye)
{
    float3 lightVec = L.Position - pos;
    float d = length(lightVec);

    if(d > L.FalloffEnd)
        return 0.0F;

    lightVec /= d;

    float ndotl = max(dot(lightVec, normal), 0.0F);
    float3 lightStrength = L.Strength * ndotl;

    float att = CalcAttenuation(d, L.FalloffStart, L.FalloffEnd);
    lightStrength *= att;

    float spotFactor = pow(max(dot(-lightVec, L.Direction), 0.0F), L.SpotPower);
    lightStrength *= spotFactor;

    return BlinnPhong(lightStrength, lightVec, normal, toEye, mat);
}

float4 ComputeLighting(Light gLights[MAX_LIGHTS], Material mat, float3 pos, float3 normal, float3 toEye, float3 shadowFactor)
{
    float3 result = 0.0F;
    int i = 0;

#if (NUM_DIR_LIGHTS > 0)
    for(i = 0; i < NUM_DIR_LIGHTS; ++i)
        result += shadowFactor[i] * ComputeDirectionalLight(gLights[i], mat, normal, toEye);
#endif

#if (NUM_POINT_LIGHTS > 0)
    for(i = NUM_DIR_LIGHTS; i < NUM_DIR_LIGHTS + NUM_POINT_LIGHTS; ++i)
        result += ComputePointLight(gLights[i], mat, pos, normal, toEye);
#endif

#if (NUM_SPOT_LIGHTS > 0)
    for(i = NUM_DIR_LIGHTS + NUM_POINT_LIGHTS; i < NUM_DIR_LIGHTS + NUM_POINT_LIGHTS + NUM_SPOT_LIGHTS; ++i)
        result += shadowFactor[i] * ComputeSpotLight(gLights[i], mat, pos, normal, toEye);
#endif

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