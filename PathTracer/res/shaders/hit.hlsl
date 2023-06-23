#include "common.hlsli"
#include "path_tracing_utils.hlsli"

#define NRD_NORMAL_ENCODING 2
#define NRD_ROUGHNESS_ENCODING 1
#include "include/NRD.hlsli"

StructuredBuffer<Vertex> vertices: register(t0);
StructuredBuffer<int> indices: register(t1);
StructuredBuffer<Material> gMaterials: register(t0, space1);
StructuredBuffer<ObjectData> gData: register(t1, space1);

RaytracingAccelerationStructure SceneBVH: register(t2);

Texture2D gDiffuseMap[1]: register(t3);
Texture2D gNormalMap[2]: register(t4);

SamplerState gsamPointWrap: register(s0);
SamplerState gsamBilinearWrap: register(s1);
SamplerState gsamTrilinearWrap: register(s2);

cbuffer cbPass: register(b0)
{
    float4x4 gView;
    float4x4 gViewProjPrev;
    float4x4 gInvView;
    float4x4 gInvProj;
    float gFov;
    float gAspectRatio;
    float gNearPlane;
    float gFarPlane;
    float gLODOffset;
    uint gFrameIndex;
    float2 jitter;
}

/*
    0: fake light
    1: xenon light
    2: warm light
    3: neon light
*/
#define USE_LIGHT 0

#define _USE_MIPMAPS

float computeTextureLOD(uint2 size, float3 d, float t)
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
    
    float p = max(sqrt(dsdx * dsdx + dtdx * dtdx), sqrt(dsdy * dsdy + dtdy * dtdy));
    return log2(ceil(p)) + gLODOffset - 0.5;
#else
    return 0;
#endif
}

float calcShadow(Light light, float3 worldOrigin, float3 normal, uint seed, out float occlusion)
{
    float shadowDistance = NRD_FP16_MAX;
    float distanceToLight = 0;
    
    if(light.Type == LIGHT_TYPE_DIRECTIONAL)
    {
        float maxDist = DIRECTIONAL_LIGHT_DISTANCE;
        
        float3 lightSphereDirection = calcShadowDirectionDL(worldOrigin, light.Direction, light.Radius, seed);
        distanceToLight = length(lightSphereDirection);
    
        RayDesc ray;
        ray.Origin = worldOrigin;
        ray.Direction = normalize(lightSphereDirection);
        ray.TMin = gNearPlane;
        ray.TMax = distanceToLight;
    
        ShadowHitInfo shadowPayload;
        shadowPayload.occlusion = 1.0F;
        shadowPayload.distance = 0.01F;
        if(dot(ray.Direction, normal) > 0.0F)
        {
            TraceRay(SceneBVH, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, 0xFF, 1, 0, 1, ray, shadowPayload);
            if(shadowPayload.distance >= 0.0F)
            {
                shadowDistance = shadowPayload.distance;
                occlusion = shadowPayload.occlusion;
            }
        }
        else
            shadowDistance = NRD_FP16_MAX;
    }
    else if(light.Type == LIGHT_TYPE_POINTLIGHT)
    {
        float maxDist = light.FalloffEnd + light.FalloffStart;
    
        float3 lightSphereDirection = calcShadowDirection(worldOrigin, normalize(light.Position - worldOrigin), light.Position, light.Radius, seed);
        distanceToLight = length(lightSphereDirection);
    
        RayDesc ray;
        ray.Origin = worldOrigin;
        ray.Direction = normalize(lightSphereDirection);
        ray.TMin = gNearPlane;
        ray.TMax = distanceToLight;
    
        ShadowHitInfo shadowPayload;
        shadowPayload.occlusion = 1.0F;
        shadowPayload.distance = 0.01F;
        if(dot(ray.Direction, normal) > 0.0F)
        {
            TraceRay(SceneBVH, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, 0xFF, 1, 0, 1, ray, shadowPayload);
            if(shadowPayload.distance >= 0.0F)
            {
                shadowDistance = shadowPayload.distance;
                occlusion = shadowPayload.occlusion;
            }
        }
        else
            shadowDistance = NRD_FP16_MAX;
    }
    else if(light.Type == LIGHT_TYPE_SPOTLIGHT)
    {
        float maxDist = light.FalloffEnd + light.FalloffStart;
        
        float3 lightSphereDirection = calcShadowDirection(worldOrigin, light.Direction, light.Position, light.Radius, seed);
        distanceToLight = length(lightSphereDirection);
        
        RayDesc ray;
        ray.Origin = worldOrigin;
        ray.Direction = normalize(lightSphereDirection);
        ray.TMin = gNearPlane;
        ray.TMax = distanceToLight;
        
        ShadowHitInfo shadowPayload;
        shadowPayload.occlusion = 1.0F;
        shadowPayload.distance = 0.01F;
        if(dot(ray.Direction, normal) > 0.0F)
        {
            TraceRay(SceneBVH, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, 0xFF, 1, 0, 1, ray, shadowPayload);
            if(shadowPayload.distance >= 0.0F)
            {
                shadowDistance = shadowPayload.distance;
                occlusion = shadowPayload.occlusion;
            }
        }
        else
            shadowDistance = NRD_FP16_MAX;
    }
    
    return shadowDistance;
}

float3 calcIndirectLight(Light light, float4 diffuseAlbedo, float3 norm, Material material, float3 worldOrigin)
{
    float3 color = 0;
    if(light.Type == LIGHT_TYPE_DIRECTIONAL)
    {
        float ldot = diffuseAlbedo.a < 1.0 ? dot(-norm, -light.Direction) : dot(norm, -light.Direction);
        float lightPower = (1.0 - diffuseAlbedo.a) * max(dot(-norm, -light.Direction), 0.0) + (1.0 - material.roughness) * max(ldot, 0.0);

        if(material.metallic > 0.0 || diffuseAlbedo.a < 1.0)
            lightPower *= 0.9;
        else
            lightPower *= 0.75;
        color = lerp(light.Strength, diffuseAlbedo.rgb, min(diffuseAlbedo.a * 3, 1.0)) * lightPower * 2;
    }
    else if(light.Type == LIGHT_TYPE_POINTLIGHT)
    {
        float3 lightDir = normalize(light.Position - worldOrigin);
        float ldot = diffuseAlbedo.a < 1.0 ? dot(-norm, lightDir) : dot(norm, lightDir);
        float lightPower = (1.0 - diffuseAlbedo.a) * max(dot(-norm, lightDir), 0.0) + (1.0 - material.roughness) * max(ldot, 0.0);

        if(material.metallic > 0.0 || diffuseAlbedo.a < 1.0)
            lightPower *= 0.9;
        else
            lightPower *= 0.75;
        float3 c = lerp(light.Strength, diffuseAlbedo.rgb, min(diffuseAlbedo.a * 3, 1.0)) * lightPower * 2;
        color = c.rgb * min(3.5F / (RayTCurrent() + length(light.Position - worldOrigin)), 1.0);
    }
    else if(light.Type == LIGHT_TYPE_SPOTLIGHT)
    {
        
        float3 lightDir = normalize(light.Position - worldOrigin);
        float ldot = diffuseAlbedo.a < 1.0 ? dot(-norm, lightDir) : dot(norm, lightDir);
        float spotFactor = pow(max(dot(-WorldRayDirection(), light.Direction), 0.0F), light.SpotPower);
        float lightPower = (1.0F - diffuseAlbedo.a) * max(dot(-norm, light.Direction), 0.0F) + (1.0F - material.roughness) * max(ldot, 0.0F);
            
        if(material.metallic > 0.0 || diffuseAlbedo.a < 1.0)
            lightPower *= 0.9;
        else
            lightPower *= 0.75;
        float3 c = lerp(light.Strength, diffuseAlbedo.rgb, min(diffuseAlbedo.a * 3, 1.0F)) * lightPower * spotFactor * 2;
        color = c.rgb * min(2.5F / (RayTCurrent() + length(light.Position - worldOrigin)), 1.0F);
    }
    
    return color;
}

[shader("closesthit")]
void ClosestHit(inout HitInfo payload, Attributes attrib)
{
    ObjectData objectData = gData[InstanceID()];
    
    uint vertId = 3 * PrimitiveIndex();
    float3 worldOrigin = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
    float3 barycentrics = float3(1.0F - attrib.bary.x - attrib.bary.y, attrib.bary.x, attrib.bary.y);
    float3 normRayDir = normalize(WorldRayDirection());
    
    float2 uvs = vertices[indices[vertId]].uvs * barycentrics.x + vertices[indices[vertId + 1]].uvs * barycentrics.y +
				 vertices[indices[vertId + 2]].uvs * barycentrics.z;
    float3 norm = vertices[indices[vertId]].normal * barycentrics.x + vertices[indices[vertId + 1]].normal * barycentrics.y +
				 vertices[indices[vertId + 2]].normal * barycentrics.z;
    float3 tangent = vertices[indices[vertId]].tangent * barycentrics.x + vertices[indices[vertId + 1]].tangent * barycentrics.y +
				 vertices[indices[vertId + 2]].tangent * barycentrics.z;
    norm = normalize(mul(norm, (float3x3) objectData.world));
    tangent = normalize(mul(tangent, (float3x3) objectData.world));
    
    uint seed = initRand(DispatchRaysIndex().x * gFrameIndex, DispatchRaysIndex().y * gFrameIndex, 16);
    uint chosenLight = floor(nextRand(seed) * (NUM_DIR_LIGHTS + NUM_POINT_LIGHTS + NUM_SPOT_LIGHTS)); //TODO importance sampling
    
    Material material = gMaterials[objectData.matIndex];
    
    //light
    Light gLights[MAX_LIGHTS];
#if USE_LIGHT == 0
    gLights[0].Strength = float3(1.0, 1.0, 1.0);
#elif USE_LIGHT == 1
    gLights[0].Strength = float3(0.2, 0.3, 1.0) * 2;
#elif USE_LIGHT == 2
    gLights[0].Strength = float3(1.0, 0.8, 0.5);
#elif USE_LIGHT == 3
    gLights[0].Strength = float3(0.75, 0.9, 1.0);
#endif
    gLights[0].Direction = float3(0.0, -1.0, 0.0);
    gLights[0].Position = float3(-2.0, 2.45, -1.0);
    gLights[0].FalloffStart = 0.01;
    gLights[0].FalloffEnd = 10.0;
    gLights[0].SpotPower = 1.0;
    gLights[0].Radius = 0.15;
    gLights[0].Type = LIGHT_TYPE_SPOTLIGHT;
    
    gLights[1].Strength = float3(1.0, 0.0, 1.0);
    gLights[1].Direction = float3(0.0, -1.0, 0.0);
    gLights[1].Position = float3(1.7, 2.45, -1.0);
    gLights[1].FalloffStart = 0.01;
    gLights[1].FalloffEnd = 10.0;
    gLights[1].SpotPower = 1.0;
    gLights[1].Radius = 0.3;
    gLights[1].Type = LIGHT_TYPE_SPOTLIGHT;
    
    //global vars
    const float4 gAmbientLight = float4(0.2F, 0.2F, 0.2F, 1.0F);
    
    //diffuse and normal sampling
    float4 mapColor = float4(1, 1, 1, 1);
    if(objectData.diffuseIndex >= 0)
    {
        uint w, h, l;
        gDiffuseMap[objectData.diffuseIndex].GetDimensions(0, w, h, l);
        float LOD = computeTextureLOD(uint2(w, h), normRayDir, max(payload.colorAndDistance.a, 0.0F) + RayTCurrent());
        mapColor = gDiffuseMap[objectData.diffuseIndex].SampleLevel(gsamTrilinearWrap, uvs, 0);
    }
    if(objectData.normalIndex >= 0)
    {
        uint w, h, l;
        gNormalMap[objectData.normalIndex].GetDimensions(0, w, h, l);
        float LOD = computeTextureLOD(uint2(w, h), normRayDir, max(payload.colorAndDistance.a, 0.0F) + RayTCurrent());
        float3 normalSample = normalize(gNormalMap[objectData.normalIndex].SampleLevel(gsamTrilinearWrap, uvs, LOD).rgb);
        norm = normalSampleToWorldSpace(normalSample, norm, tangent);
    }
    
    float4 diffuseAlbedo = material.diffuseAlbedo;
    if(payload.recursionDepth > 1)
        diffuseAlbedo *= mapColor;
    
    //indirect back propagation
    if(payload.colorAndDistance.a < 0.0F)
    {
        payload.colorAndDistance.rgb = calcIndirectLight(gLights[chosenLight], diffuseAlbedo, norm, material, worldOrigin);
        payload.colorAndDistance.a = RayTCurrent();
        return;
    }
    
    float3 indirectLight = 0.0F;
    HitInfo indirectPayload;
    indirectPayload.colorAndDistance = float4(0, 0, 0, -1);
    indirectPayload.recursionDepth = payload.recursionDepth + 1;
	
    float3 rv = float3(nextRand(seed), nextRand(seed), nextRand(seed)) * 2.0F - 1.0F;

    RayDesc reflRay;
    reflRay.Origin = worldOrigin;
    reflRay.Direction = normalize(rv);
    reflRay.TMin = gNearPlane;
    reflRay.TMax = 2.5F;
	
    TraceRay(SceneBVH, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, 0xFF, 0, 0, 0, reflRay, indirectPayload);
    indirectLight = indirectPayload.colorAndDistance.rgb;
    
    //ambient light
    float4 ambient = gAmbientLight * diffuseAlbedo;
    float4 hitColor = float4(ambient.rgb, 1e7);
    payload.specularAndDistance.a = 1e7;
    
    //ambient occlusion
    RayDesc aoRay;
    aoRay.Origin = worldOrigin;
    aoRay.Direction = calcRTAODirection(seed);
    aoRay.TMin = 0.001;
    aoRay.TMax = 0.2;

    AOHitInfo aoPayload;
    aoPayload.isHit = false;
    aoPayload.hitT = 1e7;

    TraceRay(SceneBVH, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, 0xFF, 2, 0, 2, aoRay, aoPayload);
    if(aoPayload.isHit)
        hitColor.a = aoPayload.hitT;
    
    //blinn phong
    const float shininess = max(1.0F - material.roughness, 0.01F);
    LightMaterial mat = { diffuseAlbedo, material.fresnelR0, shininess };
    float3 specAlbedo;
    float4 directLight = ComputeLighting(gLights[chosenLight], mat, worldOrigin, norm, -WorldRayDirection(), specAlbedo);
    
    hitColor.rgb += directLight.rgb * diffuseAlbedo.rgb;
    hitColor.rgb += indirectLight * diffuseAlbedo.rgb;
    
    //shadows
    float3 LSum = 0;
    bool shadowed = false;
    SIGMA_MULTILIGHT_DATATYPE shadowData = SIGMA_FrontEnd_MultiLightStart();
    for(int i = 0; i < NUM_DIR_LIGHTS + NUM_POINT_LIGHTS + NUM_SPOT_LIGHTS; ++i)
    {
        float w = 1;
        float3 L = 1;
        LSum += L;
        
        float occlusion;
        float shadowDistance = calcShadow(gLights[i], worldOrigin, norm, seed, occlusion);
        if(payload.recursionDepth == 1)
        {
            if(shadowDistance == NRD_FP16_MAX)
                shadowed = true;
            SIGMA_FrontEnd_MultiLightUpdate(L, shadowDistance, tan(gLights[i].Radius * 0.5), w, shadowData);
        }
        else if(shadowDistance < NRD_FP16_MAX)
        {
            float occlusionFactor = 0;
            if(occlusion >= 0.5)
                occlusionFactor = occlusion * occlusion;
            else
                occlusionFactor = occlusion * 0.43;
            hitColor *= min(0.7 + (shadowDistance / (60.0 * occlusionFactor)), 1.0);
        }
    }
    
    if(shadowed)
        payload.specularAndDistance.rgb = specAlbedo;
    
    //reflections
    float metallic = 0;
    float rayNormDot = dot(norm, -WorldRayDirection());
    if(material.metallic > 0.0 && payload.recursionDepth < MAX_RECURSION_DEPTH)
    {
        float fresnelFactor;
        if(diffuseAlbedo.a < 1.0)
            fresnelFactor = 1.0F - clamp(rayNormDot, 0.0F, 1.0F);
        else
            fresnelFactor = 1.0F - clamp(rayNormDot * material.fresnelPower, 0.0F, 1.0F);
        
        HitInfo reflPayload;
        reflPayload.colorAndDistance = float4(1, 1, 1, RayTCurrent());
        reflPayload.recursionDepth = payload.recursionDepth + 1;
        reflPayload.virtualZ = -1;
        reflPayload.albedoAndZ.xyz = WorldRayDirection();
        
        RayDesc reflRay;
        reflRay.Origin = worldOrigin;
        if((material.flags & 0x0000001) != 0)
            reflRay.Direction = reflect(WorldRayDirection(), norm);
        else
            reflRay.Direction = calcReflectionDirection(WorldRayDirection(), norm, material.roughness / 10.0F, seed);
        reflRay.TMin = gNearPlane;
        reflRay.TMax = 50 * shininess;

        TraceRay(SceneBVH, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, 0xFF, 0, 0, 0, reflRay, reflPayload);
        
        float distanceFactor;
        if(reflPayload.colorAndDistance.a < 1e7) //hit
            distanceFactor = 1.0 - clamp(reflPayload.colorAndDistance.a / (50 * shininess), 0.0F, 1.0F);
        else //miss
            distanceFactor = min(shininess * 3, 1.0F);
        
        metallic = fresnelFactor * material.metallic * distanceFactor;
        payload.specularAndDistance.rgb += reflPayload.colorAndDistance.rgb * metallic;
        payload.specularAndDistance.a = reflPayload.colorAndDistance.a;
        if(payload.recursionDepth == 2)
            payload.virtualZ = reflPayload.virtualZ;
    }
        
    //refraction
    if(material.diffuseAlbedo.a < 1.0 && payload.recursionDepth < MAX_RECURSION_DEPTH)
    {
        float fresnelFactor;
        if(material.metallic > 0.0)
            fresnelFactor = 1.0 - clamp((1.0 - rayNormDot), 0.0, 1.0);
        else
            fresnelFactor = 1.0 - clamp((1.0 - rayNormDot) * material.fresnelPower, 0.0, 1.0);
        float visibility = 1.0 - material.diffuseAlbedo.a;

        HitInfo refrPayload;
        refrPayload.colorAndDistance = float4(1, 1, 1, RayTCurrent());
        refrPayload.recursionDepth = payload.recursionDepth + 1;
        refrPayload.virtualZ = -1;
        refrPayload.albedoAndZ.xyz = WorldRayDirection();

        RayDesc refrRay;
        refrRay.Origin = worldOrigin;
        if((material.flags & 0x0000002) != 0)
            refrRay.Direction = refract(WorldRayDirection(), norm, material.refractionIndex);
        else
            refrRay.Direction = calcRefractionDirection(WorldRayDirection(), norm, material.refractionIndex, material.roughness / 10.0, seed);
        refrRay.TMin = gNearPlane;
        refrRay.TMax = 50 * visibility;

        TraceRay(SceneBVH, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, 0xFF, 0, 0, 0, refrRay, refrPayload);
		
        if(refrPayload.colorAndDistance.a < 1e7) //hit
        {
            float distanceFactor = 1.0 - saturate(refrPayload.colorAndDistance.a / (50 * visibility));
            metallic = fresnelFactor * visibility * distanceFactor;
            payload.specularAndDistance.rgb += refrPayload.colorAndDistance.rgb * metallic;
        }
        else //miss
        {
            metallic = fresnelFactor * visibility;
            payload.specularAndDistance.rgb += refrPayload.colorAndDistance.rgb * metallic;
        }
        
        payload.specularAndDistance.a = refrPayload.colorAndDistance.a;
        if(payload.recursionDepth == 2)
            payload.virtualZ = refrPayload.virtualZ;
    }
    
    if(payload.recursionDepth > 1)
    {
        float3 virtualPos = WorldRayOrigin() + RayTCurrent() * payload.albedoAndZ.xyz;
        payload.virtualZ = mul(float4(virtualPos, 1.0), gView).z;
    }
    
    hitColor.rgb *= 1.0 - metallic;
    if(payload.recursionDepth > 1)
    {
        hitColor.rgb += payload.specularAndDistance.rgb;
        hitColor.a = RayTCurrent();
    }
    
    if(material.metallic == 1.0 || material.diffuseAlbedo.a == 0.0)
        hitColor.a = 0;
    payload.colorAndDistance = hitColor;
    payload.normalAndRough = float4(norm, material.roughness);
    payload.albedoAndZ = float4(mapColor.rgb, mul(float4(worldOrigin, 1.0), gView).z);
    if(material.roughness >= 0.05)
        payload.virtualZ = payload.albedoAndZ.w;
    payload.shadow = SIGMA_FrontEnd_MultiLightEnd(payload.albedoAndZ.w, shadowData, LSum, payload.shadowTranslucency);
}