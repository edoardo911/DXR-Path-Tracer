float CalcAttenuation(float d, float falloffStart, float falloffEnd)
{
    return saturate((falloffEnd - d) / (falloffEnd - falloffStart));
}

float3 normalSampleToWorldSpace(float3 normalMapSample, float3 unitNormalW, float3 tangentW, bool blueBias = false)
{
    float3 normalT = blueBias ? normalMapSample : (2.0 * normalMapSample - 1.0);
    
    float3 N = unitNormalW;
    float3 T = normalize(tangentW - dot(tangentW, N) * N);
    float3 B = cross(N, T);
    
    float3x3 TBN = float3x3(T, B, N);
    return mul(normalT, TBN);
}

float2 encodeDirOct(float3 v)
{
    if(all(abs(v - float3(0, 0, -1)) < 1e-4))
        return float2(1, 1);
    
    v /= abs(v.x) + abs(v.y) + abs(v.z);
    float2 enc = v.xy;
    if(v.z < 0.0)
        enc = (1.0 - abs(enc.yx)) * float2(sign(enc.x), sign(enc.y));
    return enc * 0.5 + 0.5;
}

float3 decodeDirOct(float2 f)
{
    if(all(abs(f - float2(1, 1)) < 1e-4))
        return float3(0, 0, -1);
    
    f = f * 2.0 - 1.0;
    float3 n = float3(f.xy, 1.0 - abs(f.x) - abs(f.y));
    if(n.z < 0.0)
        n.xy = (1.0 - abs(n.yx)) * float2(sign(n.x), sign(n.y));
    return normalize(n);
}

uint packDirection(float3 dir)
{
    float2 enc = encodeDirOct(normalize(dir));
    uint2 uenc = uint2(saturate(enc) * 255.0 + 0.5);
    return (uenc.x << 8) | uenc.y;
}

float3 unpackDirection(uint packed)
{
    float2 enc;
    enc.x = float((packed >> 8) & 0xFF) / 255.0;
    enc.y = float(packed & 0xFF) / 255.0;
    return decodeDirOct(enc);
}

uint packColorLDR(float3 color)
{
    uint r = (uint) (saturate(color.r) * 255.0);
    uint g = (uint) (saturate(color.g) * 255.0);
    uint b = (uint) (saturate(color.b) * 255.0);
    return (r << 16) | (g << 8) | b;
}

float3 unpackColorLDR(uint packed)
{
    float r = ((packed >> 16) & 0xFF) / 255.0;
    float g = ((packed >> 8) & 0xFF) / 255.0;
    float b = (packed & 0xFF) / 255.0;
    return float3(r, g, b);
}

float luma(float3 color)
{
    return dot(color, float3(0.2126, 0.7152, 0.0722));
}

#ifndef COMMON_ONLY
#ifdef RT_RAYTRACING
float computeTextureLOD(float2 size, float3 d, float t)
{
    uint vertId = 3 * PrimitiveIndex();
    float2 dims = float2(DispatchRaysDimensions().xy - 1);
    
    ObjectData objectData = gData[InstanceID()];
    
    float3 pos0 = mul((float3x3) objectData.world, vertices[indices[vertId]].pos);
    float3 pos1 = mul((float3x3) objectData.world, vertices[indices[vertId + 1]].pos);
    float3 pos2 = mul((float3x3) objectData.world, vertices[indices[vertId + 2]].pos);
    
    float3 e1 = pos1 - pos0;
    float3 e2 = pos2 - pos0;
    
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
    return log2(ceil(p));
}

float3 calcIndirectLight(Reservoir reservoir, float4 diffuseAlbedo, float3 norm, float3 worldOrigin, float roughness, float metallic, float refractionIndex)
{
    Light light = gLights[reservoir.sampleIndex];

    float3 color = 0;
    float3 refrNorm = refract(normalize(WorldRayDirection()), norm, refractionIndex);
    float3 weights = float3(30.0, 0.9, 1.2); //artistic weights for refraction, reflection and opaque materials

    if(light.type == LIGHT_TYPE_DIRECTIONAL)
    {
        float3 lightDir = -light.Direction;

        float3 radiance = light.Strength * diffuseAlbedo.rgb * reservoir.W;
        float3 cRefr = radiance * pow(max(dot(refrNorm, lightDir), 0.0), 5.5) * (1.0 - diffuseAlbedo.a);
        float3 cRefl = radiance * max(dot(norm, normalize(lightDir - WorldRayDirection())), 0.0) * max(metallic * 3.0 - roughness * 1.1, 0.0);
        float3 cNorm = radiance * max(dot(norm, lightDir), 0.0) * (1.0 - roughness);
        color = cRefr * weights.x + cRefl * weights.y + cNorm * weights.z;
    }
    else if(light.type == LIGHT_TYPE_POINTLIGHT)
    {
        float3 lightDir = light.Position - worldOrigin;
        float dist = length(lightDir);
        lightDir /= dist;
        float lightPower = CalcAttenuation(dist, light.FalloffStart, light.FalloffEnd);

        float3 radiance = light.Strength * diffuseAlbedo.rgb * lightPower * reservoir.W;
        float3 cRefr = radiance * pow(max(dot(refrNorm, lightDir), 0.0), 5.5) * (1.0 - diffuseAlbedo.a);
        float3 cRefl = radiance * max(dot(norm, normalize(lightDir - WorldRayDirection())), 0.0) * max(metallic * 3.0 - roughness * 1.1, 0.0);
        float3 cNorm = radiance * max(dot(norm, lightDir), 0.0) * (1.0 - roughness);
        color = cRefr * weights.x + cRefl * weights.y + cNorm * weights.z;
    }
    else if(light.type == LIGHT_TYPE_SPOTLIGHT)
    {
        float3 lightDir = light.Position - worldOrigin;
        float dist = length(lightDir);
        lightDir /= dist;
        float spotFactor = pow(max(dot(-lightDir, light.Direction), 0.0), light.SpotPower);
        float lightPower = CalcAttenuation(dist, light.FalloffStart, light.FalloffEnd);
        
        float3 radiance = light.Strength * diffuseAlbedo.rgb * lightPower * spotFactor * reservoir.W;
        float3 cRefr = radiance * pow(max(dot(refrNorm, lightDir), 0.0), 2.0) * (1.0 - diffuseAlbedo.a);
        float3 cRefl = radiance * max(dot(norm, normalize(lightDir - WorldRayDirection())), 0.0) * max(metallic * 3.0 - roughness * 1.1, 0.0);
        float3 cNorm = radiance * max(dot(norm, lightDir), 0.0) * (1.0 - roughness);
        color = cRefr * weights.x + cRefl * weights.y + cNorm * weights.z;
    }
    
    return color;
}

#ifndef NO_BLUE_NOISE
float calcShadow(Light light, float3 worldOrigin, float3 normal, out float occlusion, uint2 pos, out float w)
{
    const float minDistance = 0.0001;
    
    float shadowDistance = NRD_FP16_MAX;
    float distanceToLight = 0;
    w = 0;
    
    if(light.type == LIGHT_TYPE_DIRECTIONAL)
    {
        float maxDist = DIRECTIONAL_LIGHT_DISTANCE;
        
        float3 lightSphereDirection = calcShadowDirectionDL(worldOrigin, light.Direction, light.radius / 200.0, gBlueNoise, pos, w);
        distanceToLight = length(lightSphereDirection);
                
        RayDesc ray;
        ray.Origin = worldOrigin;
        ray.Direction = lightSphereDirection / distanceToLight;
        ray.TMin = minDistance;
        ray.TMax = distanceToLight;
        
        ShadowInfo shadowPayload;
        shadowPayload.occlusion = 1.0;
        shadowPayload.distance = minDistance;
        if(dot(ray.Direction, normal) > 0.0)
        {
            TraceRay(SceneBVH, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, 0xFE, 1, 0, 1, ray, shadowPayload);
            if(shadowPayload.distance >= 0.0)
            {
                shadowDistance = shadowPayload.distance;
                occlusion = shadowPayload.occlusion * 0.5;
            }
        }
    }
    else if(light.type == LIGHT_TYPE_POINTLIGHT)
    {
        float maxDist = light.FalloffEnd + light.FalloffStart;
        
        float3 lightSphereDirection = calcShadowDirection(worldOrigin, normalize(light.Position - worldOrigin), light.Position, light.radius, gBlueNoise, pos, w);
        distanceToLight = length(lightSphereDirection);
        
        RayDesc ray;
        ray.Origin = worldOrigin;
        ray.Direction = lightSphereDirection / distanceToLight;
        ray.TMin = minDistance;
        ray.TMax = distanceToLight;
        
        ShadowInfo shadowPayload;
        shadowPayload.occlusion = 1.0;
        shadowPayload.distance = minDistance;
        if(dot(ray.Direction, normal) > 0.0)
        {
            TraceRay(SceneBVH, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, 0xFE, 1, 0, 1, ray, shadowPayload);
            if(shadowPayload.distance >= 0.0)
            {
                shadowDistance = shadowPayload.distance;
                occlusion = shadowPayload.occlusion;
            }
        }
    }
    else if(light.type == LIGHT_TYPE_SPOTLIGHT)
    {
        float maxDist = light.FalloffEnd + light.FalloffStart;
        
        float3 lightSphereDirection = calcShadowDirection(worldOrigin, light.Direction, light.Position, light.radius, gBlueNoise, pos, w);
        distanceToLight = length(lightSphereDirection);
        
        RayDesc ray;
        ray.Origin = worldOrigin;
        ray.Direction = lightSphereDirection / distanceToLight;
        ray.TMin = minDistance;
        ray.TMax = distanceToLight;
        
        ShadowInfo shadowPayload;
        shadowPayload.occlusion = 1.0;
        shadowPayload.distance = minDistance;
        if(dot(ray.Direction, normal) > 0.0)
        {
            TraceRay(SceneBVH, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, 0xFE, 1, 0, 1, ray, shadowPayload);
            if(shadowPayload.distance >= 0.0)
            {
                shadowDistance = shadowPayload.distance;
                occlusion = shadowPayload.occlusion;
            }
        }
    }
    
    return shadowDistance;
}
#endif
#else
float4 sampleTexture(float2 uvs, Texture2DArray tex, int index)
{
    switch(gTextureFilter)
    {
        case TEX_FILTER_NEAREST:
            return tex.Sample(gPointWrap, float3(uvs, index));
        case TEX_FILTER_BILINEAR:
            return tex.Sample(gBilinearWrap, float3(uvs, index));
        case TEX_FILTER_TRILINEAR:
            return tex.Sample(gTrilinearWrap, float3(uvs, index));
    }
    
    return float4(0, 0, 0, 0);
}

float4 sampleTexture(float2 uvs, Texture2D tex)
{    
    switch(gTextureFilter)
    {
    case TEX_FILTER_NEAREST:
        return tex.Sample(gPointWrap, uvs);
    case TEX_FILTER_BILINEAR:
        return tex.Sample(gBilinearWrap, uvs);
    case TEX_FILTER_TRILINEAR:
        return tex.Sample(gTrilinearWrap, uvs);
    }
    
    return float4(0, 0, 0, 0);
}

float calculateLODRast(Texture2DArray tex, float2 uvs)
{    
    switch(gTextureFilter)
    {
    case TEX_FILTER_NEAREST:
        return tex.CalculateLevelOfDetail(gPointWrap, uvs);
    case TEX_FILTER_BILINEAR:
        return tex.CalculateLevelOfDetail(gBilinearWrap, uvs);
    case TEX_FILTER_TRILINEAR:
        return tex.CalculateLevelOfDetail(gTrilinearWrap, uvs);
    }
    
    return 0.0;
}
#endif

float4 sampleTextureLOD(float LOD, float2 uvs, Texture2D tex)
{
    switch(gTextureFilter)
    {
        case TEX_FILTER_NEAREST:
            return tex.SampleLevel(gPointWrap, uvs, LOD);
        case TEX_FILTER_BILINEAR:
            return tex.SampleLevel(gBilinearWrap, uvs, LOD);
        case TEX_FILTER_TRILINEAR:
            return tex.SampleLevel(gTrilinearWrap, uvs, LOD);
    }
    
    return float4(0, 0, 0, 0);
}

float4 sampleTextureLOD(float LOD, float2 uvs, Texture2DArray tex, int index)
{
    switch(gTextureFilter)
    {
        case TEX_FILTER_NEAREST:
            return tex.SampleLevel(gPointWrap, float3(uvs, index), LOD);
        case TEX_FILTER_BILINEAR:
            return tex.SampleLevel(gBilinearWrap, float3(uvs, index), LOD);
        case TEX_FILTER_TRILINEAR:
            return tex.SampleLevel(gTrilinearWrap, float3(uvs, index), LOD);
    }
    
    return float4(0, 0, 0, 0);
}
#endif