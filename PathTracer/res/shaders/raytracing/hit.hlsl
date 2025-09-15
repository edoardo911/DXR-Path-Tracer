#include "common.hlsli"
#include "path_tracing_utils.hlsli"

#define NRD_NORMAL_ENCODING 2
#define NRD_ROUGHNESS_ENCODING 1
#include "include/NRD.hlsli"

StructuredBuffer<Vertex> vertices: register(t0);
StructuredBuffer<int> indices: register(t1);
StructuredBuffer<Material> gMaterials: register(t0, space1);
StructuredBuffer<ObjectData> gData: register(t1, space1);

Texture2DArray gTextures: register(t0, space2);
Texture2DArray gNormalMaps: register(t1, space2);
Texture2DArray gRoughnessMaps: register(t2, space2);
Texture2DArray gHeightMaps: register(t3, space2);
Texture2DArray gAOMaps: register(t4, space2);
Texture2DArray gEmissiveMaps: register(t5, space2);
Texture2DArray gMetallicMaps: register(t6, space2);

RaytracingAccelerationStructure SceneBVH: register(t2);

Texture2D gBlueNoise: register(t3);

SamplerState gPointWrap: register(s0);
SamplerState gBilinearWrap: register(s1);
SamplerState gTrilinearWrap: register(s2);

#define AO_CLIP 15.0
#define SHADOWS_CLIP 35.0
#define INDIRECT_CLIP 15.0
#define REFLECTIONS_CLIP 60.0
#define REFRACTIONS_CLIP 40.0
#define SPECULAR_CLIP 20.0
#define HEIGHT_CLIP 10.0

#include "../utils.hlsli"
#include "../pbr.hlsli"
#include "restir_utils.hlsli"

[shader("closesthit")]
void ClosestHit(inout HitInfo payload, Attributes attrib)
{    
    ObjectData objectData = gData[InstanceID()];
    Material material = gMaterials[objectData.materialIndex];
    
    uint vertId = 3 * PrimitiveIndex();
    float3 worldOrigin = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
    float3 bary = float3(1.0 - attrib.bary.x - attrib.bary.y, attrib.bary.x, attrib.bary.y);
    float3 normRayDir = normalize(WorldRayDirection());
    
    //vertex data
    float2 uvs = vertices[indices[vertId]].uvs * bary.x + vertices[indices[vertId + 1]].uvs * bary.y + vertices[indices[vertId + 2]].uvs * bary.z;
    float3 norm = vertices[indices[vertId]].norm * bary.x + vertices[indices[vertId + 1]].norm * bary.y + vertices[indices[vertId + 2]].norm * bary.z;
    float3 tangent = vertices[indices[vertId]].tangent * bary.x + vertices[indices[vertId + 1]].tangent * bary.y + vertices[indices[vertId + 2]].tangent * bary.z;
    
    uint seed = initRand(DispatchRaysIndex().x * gFrameIndex, DispatchRaysIndex().y * gFrameIndex, 16);
    
    uint2 posStatic = uint2(DispatchRaysIndex().x % 128, DispatchRaysIndex().y % 128);
    uint posX = floor(nextRand(seed) * 128);
    uint posY = floor(nextRand(seed) * 128);
    uint2 pos = uint2(posX, posY);
    
    float totDistanceMipmaps = RayTCurrent() + max(payload.colorAndDistance.a, 0.0);
    float totDistance = totDistanceMipmaps / objectData.world[0][0];
    
    //uvs transformation
    float4 transUvs = mul(float4(uvs, 0.0, 1.0), objectData.texTransform);
    uvs = mul(transUvs, material.matTransform).xy;
    
    //world normalization
    norm = normalize(mul(norm, (float3x3) objectData.world).xyz);
    tangent = normalize(mul(tangent, (float3x3) objectData.world).xyz);
    
    //height mapping
    if(gHeightMapping && payload.recursionDepth == 1 && objectData.heightIndex >= 0 && payload.colorAndDistance.a >= 0.0 && totDistance < HEIGHT_CLIP)
    {
        uint w, h, e, n;
        gHeightMaps.GetDimensions(0, w, h, e, n);
        float LOD = 0.0;
        
        if(gMipmaps)
        {
            float2 s = float2(w, h);
            s *= objectData.texTransform[0][0];
            s *= material.matTransform[0][0];
            
            LOD = computeTextureLOD(s, normRayDir, totDistanceMipmaps);
        }
                
        float3 N = norm;
        float3 T = normalize(tangent - dot(tangent, N) * N);
        float3 B = normalize(cross(N, T));
        float3x3 TBN = float3x3(T, B, N);
        
        float3 viewDir = normalize(mul(TBN, -normRayDir));
        
        float numLayers = lerp(POM_MIN_LAYERS, POM_MAX_LAYERS, abs(dot(float3(0.0, 0.0, 1.0), viewDir)));
        float layerHeight = 1.0 / numLayers;
        float currentLayerHeight = 0.0;
        
        float2 p = viewDir.xy * HEIGHT_SCALE;
        float2 deltaUV = p / numLayers;
        
        float2 currentUV = uvs;
        float currentHeight = 1.0 - sampleTextureLOD(LOD, currentUV, gHeightMaps, objectData.heightIndex).r;
        
        while(currentLayerHeight < currentHeight)
        {
            currentUV -= deltaUV;
            currentHeight = 1.0 - sampleTextureLOD(LOD, currentUV, gHeightMaps, objectData.heightIndex).r;
            currentLayerHeight += layerHeight;
        }
        
        float2 prevUV = currentUV + deltaUV;
        
        float afterHeight = currentHeight - currentLayerHeight;
        float beforeHeight = 1.0 - sampleTextureLOD(LOD, prevUV, gHeightMaps, objectData.heightIndex).r - currentLayerHeight + layerHeight;
        
        float weight = afterHeight / (afterHeight - beforeHeight);
        uvs = prevUV * weight + currentUV * (1.0 - weight);
    }
    //texturing
    float4 mapColor = float4(1, 1, 1, 1);
    if(gTexturing && objectData.textureIndex >= 0)
    {
        uint w, h, e, n;
        gTextures.GetDimensions(0, w, h, e, n);
        float LOD = 0.0;
        
        if(gMipmaps)
        {
            float2 s = float2(w, h);
            s *= objectData.texTransform[0][0];
            s *= material.matTransform[0][0];
            
            LOD = computeTextureLOD(s, normRayDir, totDistanceMipmaps);
        }
        
        mapColor = sampleTextureLOD(LOD, uvs, gTextures, objectData.textureIndex);
    }
    //normal mapping
    if(gNormalMapping && objectData.normalIndex >= 0)
    {
        uint w, h, e, n;
        gNormalMaps.GetDimensions(0, w, h, e, n);
        float LOD = 0.0;
        
        if(gMipmaps)
        {
            float2 s = float2(w, h);
            s *= objectData.texTransform[0][0];
            s *= material.matTransform[0][0];
            
            LOD = computeTextureLOD(s, normRayDir, totDistanceMipmaps);
        }
        
        float3 sampleNorm = sampleTextureLOD(LOD, uvs, gNormalMaps, objectData.normalIndex).xyz * 2.0 - 1.0;
        sampleNorm.z = sqrt(saturate(1.0 - dot(sampleNorm.xy, sampleNorm.xy)));
        norm = normalSampleToWorldSpace(sampleNorm, norm, tangent, true);
    }
    //roughness mapping
    float roughness = material.roughness;
    if(gRoughMapping && objectData.roughIndex >= 0 && totDistance < INDIRECT_CLIP)
    {
        uint w, h, e, n;
        gRoughnessMaps.GetDimensions(0, w, h, e, n);
        float LOD = 0.0;
        
        if(gMipmaps)
        {
            float2 s = float2(w, h);
            s *= objectData.texTransform[0][0];
            s *= material.matTransform[0][0];
            
            LOD = computeTextureLOD(s, normRayDir, totDistanceMipmaps);
        }
        
        roughness = sampleTextureLOD(LOD, uvs, gRoughnessMaps, objectData.roughIndex).r;
        roughness *= roughness;
    }
    if(gAOMapping && objectData.aoIndex >= 0 && payload.recursionDepth == 1 && payload.colorAndDistance.a >= 0.0 && totDistance < HEIGHT_CLIP)
    {
        uint w, h, e, n;
        gAOMaps.GetDimensions(0, w, h, e, n);
        float LOD = 0.0;
        
        if(gMipmaps)
        {
            float2 s = float2(w, h);
            s *= objectData.texTransform[0][0];
            s *= material.matTransform[0][0];
            
            LOD = computeTextureLOD(s, normRayDir, totDistanceMipmaps);
        }
        
        float ao = sampleTextureLOD(LOD, uvs, gAOMaps, objectData.aoIndex).r;
        mapColor *= max(ao, 0.15);
    }
    //emissive
    float3 emissive = float3(0.0, 0.0, 0.0);
    if(objectData.emissiveIndex >= 0)
    {
        uint w, h, e, n;
        gEmissiveMaps.GetDimensions(0, w, h, e, n);
        float LOD = 0.0;
        
        if(gMipmaps)
        {
            float2 s = float2(w, h);
            s *= objectData.texTransform[0][0];
            s *= material.matTransform[0][0];
            
            LOD = computeTextureLOD(s, normRayDir, totDistanceMipmaps);
        }
        
        emissive = material.emission * sampleTextureLOD(LOD, uvs, gEmissiveMaps, objectData.emissiveIndex).r;
    }
    //metallic mapping
    float matMetallic = material.metallic;
    if(gMetallicMapping && objectData.metallicIndex >= 0 && totDistance < HEIGHT_CLIP)
    {
        uint w, h, e, n;
        gMetallicMaps.GetDimensions(0, w, h, e, n);
        float LOD = 0.0;
        
        if(gMipmaps)
        {
            float2 s = float2(w, h);
            s *= objectData.texTransform[0][0];
            s *= material.matTransform[0][0];
            
            LOD = computeTextureLOD(s, normRayDir, totDistanceMipmaps);
        }
        
        matMetallic = sampleTextureLOD(LOD, uvs, gMetallicMaps, objectData.metallicIndex).r;
    }
    
    float p = payload.recursionDepth == 1 ? roughness : payload.roughAndZ.x;
    int maxBounces = p > 0.3 ? 2 : 3;
    
    //restir
    Reservoir reservoir = (Reservoir) 0.0;
    if(gLightCount > 0)
    {
        for(int i = 0; i < 8; ++i)
        {
            int chosenLight = min(uint(nextRand(seed) * gLightCount), gLightCount - 1);
            computeLightSample(reservoir, gLights[chosenLight], worldOrigin, norm, chosenLight, material.fresnelR0, roughness, -normRayDir, seed);
        }
    }
    
    Reservoir shadowReservoir = reservoir;
    if(gLightCount > 1)
        reservoir = mergeReservoir(reservoir, payload.candidate, worldOrigin, norm, material.fresnelR0, roughness, -normRayDir, seed);
    float sampleWeight = reservoir.W;

    //color calculation
    const float4 gAmbientLight = float4(0.2, 0.2, 0.2, 1.0);
    
    //diffuse albedo
    float4 diffuseAlbedo = material.diffuseAlbedo * float4(mapColor.rgb, 1.0);
    
    float4 hitColor = float4(0, 0, 0, 1e7);
    
    //shadows
    float chance = 1.0;
    if(gShadowsRT && payload.recursionDepth < maxBounces && material.castsShadows && totDistance < SHADOWS_CLIP && gLightCount > 0)
    {
        float w = 0.0;
        float occlusion = 1.0;
        float shadowDistance = calcShadow(gLights[shadowReservoir.sampleIndex], worldOrigin, norm, occlusion, pos, w);
            
        if(payload.recursionDepth == 1)
        {
            if(shadowDistance >= NRD_FP16_MAX)
                occlusion = 0;
            else
                chance = max(1.0 - occlusion, 0.2);
                
            if(!gRayReconstruction)
            {
                float lightRadius = gLights[shadowReservoir.sampleIndex].radius;
                if(gLights[shadowReservoir.sampleIndex].type == LIGHT_TYPE_DIRECTIONAL)
                    lightRadius /= 200.0;
                
                float factor = shadowReservoir.W;
                float penumbraRadius = SIGMA_FrontEnd_PackPenumbra(shadowDistance, tan(lightRadius * 0.5));
                payload.shadow = float4(occlusion.rrr, penumbraRadius * w * factor);
            }
            else
                payload.shadow.a = 1.0 - occlusion;
        }
        else if(shadowDistance < NRD_FP16_MAX)
        {
            float occlusionFactor = 0.0;
            if (occlusion > 0.5)
                occlusionFactor = occlusion * occlusion;
            else
                occlusionFactor = occlusion * 0.43;
                
            hitColor.rgb *= min(0.7 + (shadowDistance / (60.0 * occlusionFactor)), 1.0);
        }
    }
    
    //indirect + rtao
    float3 indirectLight = float3(0, 0, 0);
    bool occluded = false;
    if((gIndirect || gRTAO) && objectData.emissiveIndex < 0 && payload.recursionDepth == 1 && totDistance < INDIRECT_CLIP && nextRand(seed) < chance)
    {
        IndirectInfo indirectPayload;
        indirectPayload.colorAndDistance = float4(0, 0, 0, RayTCurrent());
        
        float3 rv = cosWeight(gBlueNoise, pos, norm, 1.0);
        
        RayDesc indirectRay;
        indirectRay.Origin = worldOrigin;
        indirectRay.Direction = normalize(rv);
        indirectRay.TMin = 0.01;
        indirectRay.TMax = 8.5;
        TraceRay(SceneBVH, RAY_FLAG_NONE, 0xFF, 2, 0, 2, indirectRay, indirectPayload);
        indirectLight = indirectPayload.colorAndDistance.rgb * gLightCount * saturate(1.0 - indirectPayload.colorAndDistance.a / 5.5);
        if(gRayReconstruction)
            indirectLight *= 0.65;
        if(gRTAO && indirectPayload.colorAndDistance.a < 0.3)
        {
            if(gRayReconstruction)
                occluded = true;
            hitColor.a = indirectPayload.colorAndDistance.a;
        }
    }
    
    if(totDistanceMipmaps > 5.5 * objectData.world[0][0])
    {
        chance = 0.7;
        if(roughness > 0.2)
            chance = 0.65;
        if(roughness > 0.25)
            chance = 0.5;
        if(roughness > 0.3)
            chance = 0.35;
        if(roughness > 0.45)
            chance = 0.125;
    }
    
    //ambient
    float4 ambient = gAmbientLight * diffuseAlbedo;
    hitColor.rgb = ambient.rgb * !occluded;
    
    //diffuse
    const float shininess = 1.0 - roughness;
    
    float3 specular = 0.0;
    float3 cosWNormal = norm;
    float3 vndfNormal = norm;
    float3 localSpec = 0.0;
    
    if(payload.recursionDepth == 1)
    {
        cosWNormal = cosWeight(gBlueNoise, pos, norm, saturate((roughness + gLights[reservoir.sampleIndex].radius) * 0.5));
        vndfNormal = VNDF(-normRayDir, norm, roughness, gBlueNoise, pos);
    }
        
    MaterialPBR matPBR = { roughness, material.fresnelR0, material.specular, true };
    if(totDistance > SPECULAR_CLIP)
        matPBR.specular = 0.0;
    float3 directLight = 0.0;
    if(gLightCount > 0)
        directLight = ComputeLighting(gLights[reservoir.sampleIndex], matPBR, worldOrigin, objectData.isWater ? norm : vndfNormal, cosWNormal, norm, -normRayDir, localSpec);
        
    float3 L = directLight * diffuseAlbedo.rgb;
    hitColor.rgb += L * sampleWeight;
    if(gSpecular)
        specular += localSpec * sampleWeight;
    
    if(gLightCount > 0)
        specular /= gLightCount;
    
    if(gIndirect)
        hitColor.rgb += indirectLight * diffuseAlbedo.rgb;
    hitColor.rgb += emissive;
    
    if(payload.specAndDistance.a < 0.0 && dot(normRayDir, norm) > 0.0)
        norm = -norm;
    
    //reflections
    float metallic = 0.0;
    float specDist = 0.0;
    if(gReflectionsRT && payload.recursionDepth < maxBounces && matMetallic > 0.0 && totDistance < REFLECTIONS_CLIP * matMetallic && nextRand(seed) < chance)
    {
        HitInfo reflPayload;
        reflPayload.colorAndDistance = float4(0, 0, 0, RayTCurrent());
        reflPayload.recursionDepth = payload.recursionDepth + 1;
        reflPayload.roughAndZ.x = roughness;
        reflPayload.candidate = reservoir;
        
        RayDesc reflRay;
        reflRay.Origin = worldOrigin;
        reflRay.Direction = reflect(normRayDir, vndfNormal);
        reflRay.TMin = 0.01;
        reflRay.TMax = 50.0 * shininess;
        
        if(dot(reflRay.Direction, norm) > 0.0)
        {
            TraceRay(SceneBVH, RAY_FLAG_NONE, 0xFF, 0, 0, 0, reflRay, reflPayload);
            
            float3 ggx = reflectionsGGX_PDF(-normRayDir, reflRay.Direction, norm, vndfNormal, roughness, material.fresnelR0);
            specular += reflPayload.colorAndDistance.rgb * ggx * matMetallic;
            specDist = reflPayload.colorAndDistance.a;
            metallic = luma(ggx) * matMetallic;
        }
    }
    
    //refractions
    float visibility = 1.0 - material.diffuseAlbedo.a;
    if(payload.recursionDepth < maxBounces && material.diffuseAlbedo.a < 1.0 && nextRand(seed) < chance)
    {
        float f = Fggx(material.fresnelR0, -normRayDir, vndfNormal);
        
        HitInfo refrPayload;
        refrPayload.colorAndDistance = float4(0, 0, 0, RayTCurrent());
        refrPayload.recursionDepth = payload.recursionDepth + 1;
        refrPayload.roughAndZ.x = roughness;
        refrPayload.candidate = reservoir;
        
        RayDesc refrRay;
        refrRay.Origin = worldOrigin;
        if(gRefractionsRT && totDistance < REFRACTIONS_CLIP * visibility)
            refrRay.Direction = refract(normRayDir, vndfNormal, material.refractionIndex);
        else
            refrRay.Direction = normRayDir;
        refrRay.TMin = 0.001;
        refrRay.TMax = 50.0 * visibility;
        TraceRay(SceneBVH, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, 0xFF, 0, 0, 0, refrRay, refrPayload);

        float m = (1.0 - f) * (1.0 - metallic) * visibility;
        specular += refrPayload.colorAndDistance.rgb * m;
        metallic += m;
        if(matMetallic == 0.0)
            specDist = refrPayload.colorAndDistance.a;
    }
    
    hitColor.rgb *= max(1.0 - metallic, 0.0);
    if(payload.recursionDepth > 1)
    {
        hitColor.rgb += specular;
        hitColor.a = RayTCurrent();
    }
    
    if(payload.recursionDepth == 1 && !gRayReconstruction)
    {
        float3 diffFactor;
        float3 specFactor;
        NRD_MaterialFactors(norm, normRayDir, diffuseAlbedo.rgb, material.fresnelR0, roughness, diffFactor, specFactor);
        
        hitColor.rgb /= diffFactor;
        specular /= specFactor;
    }
    
    payload.colorAndDistance = hitColor;
    payload.specAndDistance = float4(specular, specDist);
    payload.normal = packDirection(norm);
    payload.roughAndZ = float2(roughness, mul(float4(worldOrigin, 1.0), gView).z);
    payload.albedo = packColorLDR(diffuseAlbedo.rgb);
    payload.cosW = packDirection(cosWNormal);
    payload.vndf = packDirection(vndfNormal);
    payload.candidate = reservoir;
    
    if(gRayReconstruction && payload.recursionDepth == 1)
    {   
        float3 lightVec;
        if(gLights[reservoir.sampleIndex].type == LIGHT_TYPE_DIRECTIONAL)
            lightVec = -gLights[reservoir.sampleIndex].Direction;
        else
            lightVec = normalize(gLights[reservoir.sampleIndex].Position - worldOrigin);

        float3 h = normalize(lightVec - normRayDir);
        float3 F = Fggx(material.fresnelR0, -normRayDir, h);
        
        payload.colorAndDistance.rgb += specular;
        payload.colorAndDistance.rgb *= clamp(payload.shadow.a, 0.3, 1.0);
        payload.shadow.rgb = EnvBRDFApprox2(F, roughness * roughness, dot(norm, -normRayDir));
        payload.shadow.a = diffuseAlbedo.a;
        
        if(gLightCount == 0)
            payload.albedo = packColorLDR(0.0);
    }
    
    payload.recursionDepth = objectData.materialIndex;
}

[shader("anyhit")]
void AnyHit(inout HitInfo payload, in Attributes attrib)
{    
    payload.specAndDistance.a = -1;
    
    ObjectData objectData = gData[InstanceID()];
    if(objectData.textureIndex >= 0)
    {
        uint w, h, e, n;
        float LOD = 0.0;
        uint vertId = 3 * PrimitiveIndex();
        float3 barycentrics = float3(1.0 - attrib.bary.x - attrib.bary.y, attrib.bary.x, attrib.bary.y);
    
        float2 uvs = vertices[indices[vertId]].uvs * barycentrics.x + vertices[indices[vertId + 1]].uvs * barycentrics.y + vertices[indices[vertId + 2]].uvs * barycentrics.z;
        
        float4 mapColor = float4(1, 1, 1, 1);
        gTextures.GetDimensions(0, w, h, e, n);
        if(gMipmaps)
        {
            float2 s = float2(w, h);
            s *= objectData.texTransform[0][0];
        
            LOD = computeTextureLOD(s, normalize(WorldRayDirection()), max(payload.colorAndDistance.a, 0.0) + RayTCurrent());
        }
        
        mapColor = sampleTextureLOD(LOD, uvs, gTextures, objectData.textureIndex);
        if(mapColor.a < 0.1)
            IgnoreHit();
    }
}