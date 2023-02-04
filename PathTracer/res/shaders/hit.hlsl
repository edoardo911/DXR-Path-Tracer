#include "common.hlsli"
#include "path_tracing_utils.hlsli"

StructuredBuffer<Vertex> vertices: register(t0);
StructuredBuffer<int> indices: register(t1);
StructuredBuffer<Material> gMaterials : register(t0, space1);

RaytracingAccelerationStructure SceneBVH: register(t2);

Texture2D gDiffuseMap[1]: register(t3);
Texture2D gNormalMap[2]: register(t4);

SamplerState gsamPointWrap: register(s0);
SamplerState gsamBilinearWrap: register(s1);
SamplerState gsamTrilinearWrap: register(s2);

cbuffer cbPass: register(b0)
{
    float4x4 gInvView;
    float4x4 gInvProj;
    float gFov;
    float gAspectRatio;
    float gNearPlane;
    float gFarPlane;
    uint gFrameIndex;
}

cbuffer objPass: register(b1)
{
    float4x4 gWorld;
    int gDiffuseIndex;
    int gNormalIndex;
    uint gMatIndex;
}

/*
    0: fake light
    1: xenon light
    2: warm light
    3: neon light
*/
#define USE_LIGHT 0

#define _USE_MIPMAPS

#ifdef _USE_MIPMAPS
    #define _ANISOTROPIC
#endif

#ifdef _ANISOTROPIC
    #define MIPMAP_FUNC min
#else
    #define MIPMAP_FUNC max
#endif

//ray differentials
float computeTextureLOD(uint2 size, float3 d, float t, out float2 anisotropicDir)
{
#ifdef _USE_MIPMAPS
    uint vertId = 3 * PrimitiveIndex();
    float2 dims = float2(DispatchRaysDimensions().xy);
    
    float3 e1 = vertices[indices[vertId + 1]].pos - vertices[indices[vertId]].pos;
    float3 e2 = vertices[indices[vertId + 2]].pos - vertices[indices[vertId]].pos;
    
    float3 cu = cross(e2, d);
    float3 cv = cross(d, e1);
    float k = 1.0F / dot(cross(e1, e2), d);
    
    float2 g1 = vertices[indices[vertId + 1]].uvs - vertices[indices[vertId]].uvs;
    float2 g2 = vertices[indices[vertId + 2]].uvs - vertices[indices[vertId]].uvs;
    
    float f = tan(gFov / 2);
    
    float3 r_ = 2 * gAspectRatio * f / dims.x;
    float3 u_ = -2 * f / dims.y;
    
    float dotDD = dot(d, d);
    
    float3 dddx = (dotDD * r_ - dot(d, r_) * d) / sqrt(dotDD * dotDD * dotDD);
    float3 dddy = (dotDD * u_ - dot(d, u_) * d) / sqrt(dotDD * dotDD * dotDD);
    
    float3 q = t * dddx;
    float3 r = t * dddy;
    
    float dudx = k * dot(cu, q);
    float dudy = k * dot(cu, r);
    float dvdx = k * dot(cv, q);
    float dvdy = k * dot(cv, r);
    
    float dsdx = size.x * (dudx * g1.x + dvdx * g2.x);
    float dsdy = size.x * (dudy * g1.x + dvdy * g2.x);
    float dtdx = size.y * (dudx * g1.y + dvdy * g2.y);
    float dtdy = size.y * (dudy * g1.y + dvdy * g2.y);
    
#ifdef _ANISOTROPIC
    if(sqrt(dsdx * dsdx + dtdx * dtdx) > sqrt(dsdy * dsdy + dtdy * dtdy))
        anisotropicDir = float2(dsdx, dtdx);
    else
        anisotropicDir = float2(dsdy, dtdy);
#endif
    
    float p = MIPMAP_FUNC(sqrt(dsdx * dsdx + dtdx * dtdx), sqrt(dsdy * dsdy + dtdy * dtdy));
    return log2(ceil(p));
#else
    return 0;
#endif
}

[shader("closesthit")]
void ClosestHit(inout HitInfo payload, Attributes attrib)
{
    uint vertId = 3 * PrimitiveIndex();
    float3 worldOrigin = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
    float3 barycentrics = float3(1.0F - attrib.bary.x - attrib.bary.y, attrib.bary.x, attrib.bary.y);
    float3 normRayDir = normalize(WorldRayDirection());
    
    float3 pos = vertices[indices[vertId]].pos * barycentrics.x + vertices[indices[vertId + 1]].pos * barycentrics.y +
				 vertices[indices[vertId + 2]].pos * barycentrics.z;
    float2 uvs = vertices[indices[vertId]].uvs * barycentrics.x + vertices[indices[vertId + 1]].uvs * barycentrics.y +
				 vertices[indices[vertId + 2]].uvs * barycentrics.z;
    float3 norm = vertices[indices[vertId]].normal * barycentrics.x + vertices[indices[vertId + 1]].normal * barycentrics.y +
				 vertices[indices[vertId + 2]].normal * barycentrics.z;
    float3 tangent = vertices[indices[vertId]].tangent * barycentrics.x + vertices[indices[vertId + 1]].tangent * barycentrics.y +
				 vertices[indices[vertId + 2]].tangent * barycentrics.z;
    norm = normalize(mul(norm, (float3x3) gWorld));
    tangent = normalize(mul(tangent, (float3x3) gWorld));
    
    uint seed = initRand(DispatchRaysIndex().x * gFrameIndex, DispatchRaysIndex().y * gFrameIndex, 16);
    
    Material material = gMaterials[gMatIndex];
    
    //light
    Light gLights[MAX_LIGHTS];
#if USE_LIGHT == 0
    gLights[0].Strength = float3(1.0F, 1.0F, 1.0F);
#elif USE_LIGHT == 1
    gLights[0].Strength = float3(0.2F, 0.3F, 1.0F) * 2;
#elif USE_LIGHT == 2
    gLights[0].Strength = float3(1.0F, 0.8F, 0.5F);
#elif USE_LIGHT == 3
    gLights[0].Strength = float3(0.75F, 0.9F, 1.0F);
#endif
    gLights[0].Direction = float3(0.0F, -1.0F, 0.0F);
    gLights[0].Position = float3(-2.0F, 2.45F, -1.0F);
    gLights[0].FalloffStart = 0.01F;
    gLights[0].FalloffEnd = 10.0F;
    gLights[0].SpotPower = 1.0F;
    gLights[0].Radius = 0.15F;
    
    //global vars
    const float4 gAmbientLight = float4(0.2F, 0.2F, 0.2F, 1.0F);
    
    //diffuse and normal sampling
    float4 mapColor = float4(1, 1, 1, 1);
    
    float2 anisotropicDir = 0;
    if(gDiffuseIndex >= 0)
    {
        uint w, h, l;
        gDiffuseMap[gDiffuseIndex].GetDimensions(0, w, h, l);
        float LOD = computeTextureLOD(uint2(w, h), normRayDir, max(payload.colorAndDistance.a, 0.0F) + RayTCurrent(), anisotropicDir);
    #ifdef _ANISOTROPIC
        float2 offset = anisotropicDir * (nextRand(seed) * 2.0F - 1.0F) * float2(1.0F / w, 1.0F / h);
    #else
        float2 offset = 0.0F;
    #endif
        mapColor = gDiffuseMap[gDiffuseIndex].SampleLevel(gsamTrilinearWrap, uvs + offset, LOD);
    }
    if(gNormalIndex >= 0)
    {
        uint w, h, l;
        gNormalMap[gNormalIndex].GetDimensions(0, w, h, l);
        float LOD = computeTextureLOD(uint2(w, h), normRayDir, max(payload.colorAndDistance.a, 0.0F) + RayTCurrent(), anisotropicDir);
    #ifdef _ANISOTROPIC
        float2 offset = anisotropicDir * (nextRand(seed) * 2.0F - 1.0F) * float2(1.0F / w, 1.0F / h);
    #else
        float2 offset = 0.0F;
    #endif
        float3 normalSample = normalize(gNormalMap[gNormalIndex].SampleLevel(gsamTrilinearWrap, uvs + offset, LOD).rgb);
        norm = normalSampleToWorldSpace(normalSample, norm, tangent);
    }
    
    float4 diffuseAlbedo = material.diffuseAlbedo;
    diffuseAlbedo *= mapColor;
    
    //indirect back propagation
    if(payload.colorAndDistance.a < 0.0F)
    {
        int i;
    #if (NUM_DIR_LIGHTS > 0)
        for(i = 0; i < NUM_DIR_LIGHTS; ++i)
        {
            float ldot = diffuseAlbedo.a < 0.97F ? dot(-norm, -gLights[i].Direction) : dot(norm, -gLights[i].Direction);
            float lightPower = (1.0F - diffuseAlbedo.a) * max(dot(-norm, -gLights[i].Direction), 0.0F) + (1.0F - material.roughness) * max(ldot, 0.0F);

            if(material.metallic > 0.01F || diffuseAlbedo.a < 0.97F)
                lightPower *= 0.9F;
            else
                lightPower *= 0.75F;
            float3 color = diffuseAlbedo.rgb * gLights[i].Strength * lightPower;
            payload.colorAndDistance.rgb += color.rgb;
        }
    #endif

    #if (NUM_POINT_LIGHTS > 0)
        for(i = NUM_DIR_LIGHTS; i < NUM_DIR_LIGHTS + NUM_POINT_LIGHTS; ++i)
        {
            float3 lightDir = normalize(gLights[i].Position - worldOrigin);
            float ldot = diffuseAlbedo.a < 0.97F ? dot(-norm, lightDir) : dot(norm, lightDir);
            float lightPower = (1.0F - diffuseAlbedo.a) * max(dot(-norm, lightDir), 0.0F) + (1.0F - material.roughness) * max(ldot, 0.0F);

            if(material.metallic > 0.01F || diffuseAlbedo.a < 0.97F)
                lightPower *= 0.9F;
            else
                lightPower *= 0.75F;
            float3 color = diffuseAlbedo.rgb * gLights[i].Strength * lightPower;
            payload.colorAndDistance.rgb += color.rgb * min(3.5F / (RayTCurrent() + length(gLights[i].Position - worldOrigin)), 1.0F);
        }
    #endif

    #if (NUM_SPOT_LIGHTS > 0)
        for(i = NUM_DIR_LIGHTS + NUM_POINT_LIGHTS; i < NUM_DIR_LIGHTS + NUM_POINT_LIGHTS + NUM_SPOT_LIGHTS; ++i)
        {
            float3 lightDir = normalize(gLights[i].Position - worldOrigin);
            float ldot = diffuseAlbedo.a < 0.97F ? dot(-norm, lightDir) : dot(norm, lightDir);
            float spotFactor = pow(max(dot(-WorldRayDirection(), gLights[i].Direction), 0.0F), gLights[i].SpotPower);
            float lightPower = (1.0F - diffuseAlbedo.a) * max(dot(-norm, gLights[i].Direction), 0.0F) + (1.0F - material.roughness) * max(ldot, 0.0F);

            if(material.metallic > 0.01F || diffuseAlbedo.a < 0.97F)
                lightPower *= 0.9F;
            else
                lightPower *= 0.75F;
            float3 color = diffuseAlbedo.rgb * gLights[i].Strength * lightPower * spotFactor;
            payload.colorAndDistance.rgb += color.rgb * min(3.5F / (RayTCurrent() + length(gLights[i].Position - worldOrigin)), 1.0F);
        }
    #endif
        payload.colorAndDistance.a = 1.0F;
        return;
    }
    
    //indirect light
    float3 indirectLight = 0.0F;
    HitInfo reflPayload;
    reflPayload.colorAndDistance = float4(0, 0, 0, -1);
    reflPayload.recursionDepth = payload.recursionDepth + 1;
	
    float3 rv = float3(nextRand(seed), nextRand(seed), nextRand(seed)) * 2.0F - 1.0F;

    RayDesc reflRay;
    reflRay.Origin = worldOrigin;
    reflRay.Direction = normalize(rv);
    reflRay.TMin = gNearPlane;
    reflRay.TMax = 1.5F;
	
    TraceRay(SceneBVH, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, 0xFF, 0, 0, 0, reflRay, reflPayload);
    if(reflPayload.colorAndDistance.a > 0.0F)
        indirectLight = reflPayload.colorAndDistance.rgb;
    
    //ambient light
    float4 ambient = gAmbientLight * diffuseAlbedo;
    float4 hitColor = float4(ambient.rgb, RayTCurrent());
    
    //shadows
    float3 shadowFactor = float3(1.0F, 1.0F, 1.0F);
    
    int i;
#if (NUM_DIR_LIGHTS > 0)
    for(i = 0; i < NUM_DIR_LIGHTS; ++i)
    {
        float maxDist = DIRECTIONAL_LIGHT_DISTANCE;
        
        float3 lightSphereDirection = calcShadowDirectionDL(worldOrigin, gLights[i].Direction, gLights[i].Radius, seed);
        float d = length(lightSphereDirection);
    
        RayDesc ray;
        ray.Origin = worldOrigin;
        ray.Direction = normalize(lightSphereDirection);
        ray.TMin = gNearPlane;
        ray.TMax = d;
    
        ShadowHitInfo shadowPayload;
        shadowPayload.occlusion = 1.0F;
        shadowPayload.distance = 0.01F;
        if(dot(ray.Direction, norm) > 0.0F)
        {
            TraceRay(SceneBVH, RAY_FLAG_NONE, 0xFF, 1, 0, 1, ray, shadowPayload);
            if(shadowPayload.distance > 0.0F)
                shadowFactor[i] = 1.0F - (clamp(shadowPayload.distance / maxDist, shadowPayload.occlusion, 1.0F));
        }
    }
#endif
#if (NUM_POINT_LIGHTS > 0)
    for(i = NUM_DIR_LIGHTS; i < NUM_DIR_LIGHTS + NUM_POINT_LIGHTS; ++i)
    {
        float maxDist = gLights[i].FalloffEnd + gLights[i].FalloffStart;
    
        float3 lightSphereDirection = calcShadowDirection(worldOrigin, normalize(gLights[i].Position - worldOrigin), gLights[i].Position, gLights[i].Radius, seed);
        float d = length(lightSphereDirection);
    
        RayDesc ray;
        ray.Origin = worldOrigin;
        ray.Direction = normalize(lightSphereDirection);
        ray.TMin = gNearPlane;
        ray.TMax = d;
    
        ShadowHitInfo shadowPayload;
        shadowPayload.occlusion = 1.0F;
        shadowPayload.distance = 0.01F;
        if(dot(ray.Direction, norm) > 0.0F)
        {
            TraceRay(SceneBVH, RAY_FLAG_NONE, 0xFF, 1, 0, 1, ray, shadowPayload);
            if(shadowPayload.distance > 0.0F)
                shadowFactor[i] = 1.0F - (clamp(shadowPayload.distance / maxDist, shadowPayload.occlusion, 1.0F));
        }
    }
#endif
#if (NUM_SPOT_LIGHTS > 0)
    for(i = NUM_DIR_LIGHTS + NUM_POINT_LIGHTS; i < NUM_DIR_LIGHTS + NUM_POINT_LIGHTS + NUM_SPOT_LIGHTS; ++i)
    {
        float maxDist = gLights[i].FalloffEnd + gLights[i].FalloffStart;
        
        float3 lightSphereDirection = calcShadowDirection(worldOrigin, gLights[i].Direction, gLights[i].Position, gLights[i].Radius, seed);
        float d = length(lightSphereDirection);
        
        RayDesc ray;
        ray.Origin = worldOrigin;
        ray.Direction = normalize(lightSphereDirection);
        ray.TMin = gNearPlane;
        ray.TMax = d;
        
        ShadowHitInfo shadowPayload;
        shadowPayload.occlusion = 1.0F;
        shadowPayload.distance = 0.01F;
        if(dot(ray.Direction, norm) > 0.0F)
        {
            TraceRay(SceneBVH, RAY_FLAG_NONE, 0xFF, 1, 0, 1, ray, shadowPayload);
            if(shadowPayload.distance > 0.0F)
                shadowFactor[i] = 1.0F - (clamp(shadowPayload.distance / maxDist, shadowPayload.occlusion, 1.0F));
        }
    }
#endif
    
    //blinn phong
    const float shininess = max(1.0F - material.roughness, 0.01F);
    LightMaterial mat = { diffuseAlbedo, material.fresnelR0, shininess };
    float4 directLight = ComputeLighting(gLights, mat, worldOrigin, norm, -WorldRayDirection(), shadowFactor);
    
    hitColor.rgb += directLight.rgb + indirectLight;
    
    //reflections
    float rayNormDot = dot(norm, -WorldRayDirection());
    if(material.metallic > 0.01F && payload.recursionDepth < MAX_RECURSION_DEPTH)
    {
        float fresnelFactor;
        if(diffuseAlbedo.a < 0.97F)
            fresnelFactor = 1.0F - clamp(rayNormDot, 0.0F, 1.0F);
        else
            fresnelFactor = 1.0F - clamp(rayNormDot * material.fresnelPower, 0.0F, 1.0F);
        
        HitInfo reflPayload;
        reflPayload.colorAndDistance = float4(1, 1, 1, RayTCurrent());
        reflPayload.recursionDepth = payload.recursionDepth + 1;
        
        RayDesc reflRay;
        reflRay.Origin = worldOrigin;
        if((material.flags & 0x0000001) != 0)
            reflRay.Direction = reflect(WorldRayDirection(), norm);
        else
            reflRay.Direction = calcReflectionDirection(WorldRayDirection(), norm, material.roughness / 20.0F, seed);
        reflRay.TMin = gNearPlane;
        reflRay.TMax = 1200 * shininess;

        TraceRay(SceneBVH, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, 0xFF, 0, 0, 0, reflRay, reflPayload);
        
        float distanceFactor = 1.0F - clamp(reflPayload.colorAndDistance.w / (1200 * shininess), 0.0F, 1.0F);
        hitColor.rgb = lerp(hitColor.rgb, reflPayload.colorAndDistance.rgb, fresnelFactor * material.metallic * distanceFactor);
    }
    
    //refraction
    if(diffuseAlbedo.a < 0.97F && payload.recursionDepth < MAX_RECURSION_DEPTH)
    {
        float fresnelFactor;
        if(material.metallic > 0.01F)
            fresnelFactor = 1.0F - clamp((1.0F - rayNormDot), 0.0F, 1.0F);
        else
            fresnelFactor = 1.0F - clamp((1.0F - rayNormDot) * material.fresnelPower, 0.0F, 1.0F);
        float visibility = 1.0F - diffuseAlbedo.a;

        HitInfo refrPayload;
        refrPayload.colorAndDistance = float4(1, 1, 1, RayTCurrent());
        refrPayload.recursionDepth = payload.recursionDepth + 1;

        RayDesc refrRay;
        refrRay.Origin = worldOrigin;
        if((material.flags & 0x0000002) != 0)
            refrRay.Direction = refract(WorldRayDirection(), norm, material.refractionIndex);
        else
            refrRay.Direction = calcRefractionDirection(WorldRayDirection(), norm, material.refractionIndex, material.roughness / 20.0F, seed);
        refrRay.TMin = gNearPlane;
        refrRay.TMax = 500 * max(diffuseAlbedo.a, 40.0F);

        TraceRay(SceneBVH, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, 0xFF, 0, 0, 0, refrRay, refrPayload);
		
        float distanceFactor = 1.0F - clamp(refrPayload.colorAndDistance.w / max(500 * diffuseAlbedo.a, 40.0F), 0.0F, 1.0F);
        hitColor.rgb = lerp(hitColor.rgb, refrPayload.colorAndDistance.rgb, visibility * fresnelFactor * distanceFactor);
    }
            
    //ambient occlusion
    RayDesc aoRay;
    aoRay.Origin = worldOrigin;
    aoRay.Direction = calcRTAODirection(seed);
    aoRay.TMin = 0.001F;
    aoRay.TMax = 0.071F;

    AOHitInfo aoPayload;
    aoPayload.isHit = false;

    TraceRay(SceneBVH, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, 0xFF, 2, 0, 2, aoRay, aoPayload);
    if(aoPayload.isHit)
        hitColor.rgb *= 0.4F;
    
    payload.colorAndDistance = hitColor;
    payload.hPos = pos.xy;
}