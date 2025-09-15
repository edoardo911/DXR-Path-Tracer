#define RT_RAYTRACING

#define MAX_LIGHTS                  128

#define TEX_FILTER_NEAREST			0
#define TEX_FILTER_BILINEAR			1
#define TEX_FILTER_TRILINEAR		2

#define LIGHT_TYPE_DIRECTIONAL		0
#define LIGHT_TYPE_SPOTLIGHT		1
#define LIGHT_TYPE_POINTLIGHT		2

#define HEIGHT_SCALE                0.05
#define POM_MIN_LAYERS              8
#define POM_MAX_LAYERS              32

//other structures
struct Attributes
{
    float2 bary;
};

struct Vertex
{
    float3 pos;
    float3 norm;
    float2 uvs;
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

struct Reservoir
{
    uint sampleIndex;
    float weightSum;
    float W;
    uint M;
};

//payloads
struct HitInfo
{
    float4 colorAndDistance;
    float4 specAndDistance;
    float4 shadow;
    float2 roughAndZ;
    uint albedo;
    uint normal;
    uint cosW;
    uint vndf;
    uint recursionDepth;
    Reservoir candidate;
};

struct IndirectInfo
{
    float4 colorAndDistance;
};

struct ShadowInfo
{
    float occlusion;
    float distance;
};

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