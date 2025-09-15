//constants
#define MAX_LIGHTS                  16

#define TEX_FILTER_NEAREST			0
#define TEX_FILTER_BILINEAR			1
#define TEX_FILTER_TRILINEAR		2
#define TEX_FILTER_ANISOTROPIC		3

#define LIGHT_TYPE_DIRECTIONAL		0
#define LIGHT_TYPE_SPOTLIGHT		1
#define LIGHT_TYPE_POINTLIGHT		2

#define HEIGHT_SCALE                0.05
#define POM_MIN_LAYERS              8
#define POM_MAX_LAYERS              32

//IO
struct VertexIn
{
    float3 pos: POSITION0;
    float3 normal: NORMAL;
    float2 uvs: TEXCOORD;
    float3 tangent: TANGENT;
};

struct MVVertexOut
{
    float4 pos: SV_POSITION;
    float4 posH: POSITION0;
    float4 prevPosH: POSITION1;
    float2 uvs: TEXCOORD;
    float2 zPos: ZPOS;
    nointerpolation uint instanceID: INSTANCEID;
};

//utility
struct Light
{
    float3 Strength;
    float FalloffStart;
    float3 Direction;
    float FalloffEnd;
    float3 Position;
    float SpotPower;
    int type;
    float radius;
    float2 pad;
};

struct Material
{
    float4 diffuseAlbedo;
    float3 fresnelR0;
    float roughness;
    float4x4 matTransform;
    float3 emission;
    float metallic;
    float refractionIndex;
    float specular;
    int castsShadows;
};

struct ObjectData
{
    float4x4 world;
    float4x4 prevWorld;
    float4x4 texTransform;
    int materialIndex;
    int textureIndex;
    int normalIndex;
    int roughIndex;
    int heightIndex;
    int aoIndex;
    int emissiveIndex;
    int metallicIndex;
    int isWater;
};

//input
cbuffer cbPass: register(b0)
{
    float4x4 gView;
    float4x4 gProj;
    float4x4 gInvView;
    float4x4 gInvProj;
    float4x4 gViewProj;
    float4x4 gViewProjPrev;
    float4x4 gViewPrev;
    float4x4 gShadowTransform;
    float4x4 gViewProjTex;
    float3 gEyePosW;
    uint gTextureFilter;
    uint gFrameIndex;
    int gLightCount;
    int gTexturing;
    int gNormalMapping;
    int gRoughMapping;
    int gHeightMapping;
    int gAOMapping;
    int gSpecular;
    int gReflectionsRT;
    int gRefractionsRT;
    int gRTAO;
    int gIndirect;
    uint gRayReconstruction;
    uint gMipmaps;
    int gShadowsRT;
    int gMetallicMapping;
    float2 jitter;
    float gFov;
    float gAspectRatio;
    Light gLights[MAX_LIGHTS];
}

Texture2DArray gTextures: register(t0);

StructuredBuffer<Material> gMaterials: register(t0, space2);
StructuredBuffer<ObjectData> gObjectData: register(t1, space2);

SamplerState gPointWrap: register(s0);
SamplerState gBilinearWrap: register(s1);
SamplerState gTrilinearWrap: register(s2);

#include "utils.hlsli"