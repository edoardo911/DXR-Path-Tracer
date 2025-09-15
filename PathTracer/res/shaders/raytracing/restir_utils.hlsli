float RESTIR_Dggx(float3 normal, float3 halfVec, float roughness)
{
    float nDotH = dot(normal, halfVec);
    float alpha2 = roughness * roughness;
    
    if(nDotH > 0.0)
    {
        float denom = nDotH * nDotH * (alpha2 - 1) + 1;
        return alpha2 / (PI * denom * denom);
    }
    else
        return 0.0;
}

float RESTIR_G1ggx(float3 x, float3 normal, float roughness)
{
    float xDotN = dot(x, normal);
    float alpha2 = roughness * roughness;
    
    if(xDotN > 0.0)
        return (2.0 * xDotN) / (xDotN + sqrt(alpha2 + (1.0 - alpha2) * xDotN * xDotN));
    else
        return 0.0;
}

float RESTIR_Gggx(float roughness, float3 normal, float3 toEye, float3 lightVec)
{
    return RESTIR_G1ggx(toEye, normal, roughness) * RESTIR_G1ggx(lightVec, normal, roughness);
}

float3 RESTIR_Fggx(float3 R0, float3 toEye, float3 halfVec)
{
    float vDotH = 1.0 - dot(toEye, halfVec);
    return R0 + (1.0 - R0) * (vDotH * vDotH * vDotH * vDotH * vDotH);
}

//GGX
float3 RESTIR_GGX(float3 R0, float roughness, float3 lightVec, float3 normal, float3 toEye)
{
    float3 halfVec = normalize(toEye + lightVec);
    float D = RESTIR_Dggx(normal, halfVec, roughness);
    float G = RESTIR_Gggx(roughness, normal, toEye, lightVec);
    float3 F = RESTIR_Fggx(R0, toEye, halfVec);
    
    float VdotN = dot(toEye, normal);
    if(VdotN <= 0.0)
        return 0.0;
    
    return ((D * G * F) / (4 * VdotN));
}

float restirWeight(Light light, float3 hitPos, float3 normal, float3 R0, float roughness, float3 toEye)
{
    float3 lightDir;
    float distance;
    float3 radiance = light.Strength;

    if(light.type == LIGHT_TYPE_DIRECTIONAL)
    {
        lightDir = -light.Direction;
        distance = 1.0;
    }
    else
    {
        lightDir = light.Position - hitPos;
        distance = length(lightDir);
        lightDir /= distance;

        radiance *= CalcAttenuation(distance, light.FalloffStart, light.FalloffEnd);
        if(light.type == LIGHT_TYPE_SPOTLIGHT)
            radiance *= pow(max(dot(-lightDir, light.Direction), 0.0), light.SpotPower);
    }

    radiance /= distance * distance + NRD_EPS;
    float ndotl = saturate(dot(normal, lightDir));
    float3 brdf = max(RESTIR_GGX(R0, roughness, lightDir, normal, toEye), 1e-7);
    float pdf = 1.0 / gLightCount;

    return length(radiance * brdf * ndotl) / pdf;
}

void computeLightSample(inout Reservoir reservoir, Light light, float3 hitPos, float3 normal, int index, float3 R0, float roughness, float3 toEye, uint seed)
{
    float weight = restirWeight(light, hitPos, normal, R0, roughness, toEye);
    
    reservoir.M += 1.0;
    reservoir.weightSum += weight;
    
    if(nextRand(seed) < weight / reservoir.weightSum)
    {
        reservoir.sampleIndex = index;
        reservoir.W = (1.0 / (weight * (1.0 / gLightCount))) * (reservoir.weightSum / reservoir.M);
    }
}

Reservoir mergeReservoir(Reservoir a, Reservoir b, float3 hitPos, float3 normal, float3 R0, float roughness, float3 toEye, uint seed)
{
    Reservoir s;
    s.M = a.M + b.M;
    s.weightSum = 0.0;
    
    float totW = 0.0;
    
    float pdf = 1.0 / gLightCount;
    float r = nextRand(seed);
    float weightA = restirWeight(gLights[a.sampleIndex], hitPos, normal, R0, roughness, toEye) * pdf * a.W * a.M;
    totW += weightA;
    if(r < weightA / max(totW, 1e-6))
        s.sampleIndex = a.sampleIndex;
    
    float weightB = restirWeight(gLights[b.sampleIndex], hitPos, normal, R0, roughness, toEye) * pdf * b.W * b.M;
    totW += weightB;
    if(r < weightB / totW)
        s.sampleIndex = b.sampleIndex;
    
    s.weightSum = totW;
    float weightS = max(restirWeight(gLights[s.sampleIndex], hitPos, normal, R0, roughness, toEye), 1e-6);
    if(s.M == 0.0)
        s.W = 0.0;
    else
        s.W = (1.0 / (weightS * pdf)) * (s.weightSum / s.M);
    return s;
}

Reservoir mergeReservoirSpatial(Reservoir r[9], float3 hitPos, float3 normal, float3 R0, float roughness, float3 toEye, uint seed)
{
    Reservoir merged;
    merged.sampleIndex = r[0].sampleIndex;
    merged.M = 0;
    merged.weightSum = 0.0;
    merged.W = 1.0;
    
    float pdf = 1.0 / gLightCount;
    float totW = 0.0;
    
    [unroll]
    for(int i = 0; i < 9; ++i)
    {
        Reservoir ri = r[i];
        
        if(ri.M == 0 || ri.weightSum == 0.0)
            continue;
        
        float w = restirWeight(gLights[ri.sampleIndex], hitPos, normal, R0, roughness, toEye);
        totW += w;
        
        if(nextRand(seed) < w / max(totW, 1e-6))
            merged.sampleIndex = ri.sampleIndex;
        
        merged.M += ri.M;
    }

    merged.weightSum = totW;
    
    float finalWeight = restirWeight(gLights[merged.sampleIndex], hitPos, normal, R0, roughness, toEye);
    float denom = max(finalWeight * pdf, 1e-6);
    merged.W = (1.0 / denom) * (merged.weightSum / max(merged.M, 1));
    return merged;
}