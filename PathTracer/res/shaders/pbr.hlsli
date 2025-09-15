#define PI 3.14159265

struct MaterialPBR
{
    float roughness;
    float3 R0;
    float specular;
    bool divideByPdf;
};

float Dggx(float3 normal, float3 halfVec, float roughness)
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

float G1ggx(float3 x, float3 normal, float roughness)
{
    float xDotN = dot(x, normal);
    float alpha2 = roughness * roughness;
    
    if(xDotN > 0.0)
        return (2.0 * xDotN) / (xDotN + sqrt(alpha2 + (1.0 - alpha2) * xDotN * xDotN));
    else
        return 0.0;
}

float Gggx(float roughness, float3 normal, float3 toEye, float3 lightVec)
{
    return G1ggx(toEye, normal, roughness) * G1ggx(lightVec, normal, roughness);
}

float3 Fggx(float3 R0, float3 toEye, float3 halfVec)
{
    float vDotH = 1.0 - dot(toEye, halfVec);
    return R0 + (1.0 - R0) * (vDotH * vDotH * vDotH * vDotH * vDotH);
}

//GGX
float3 GGX(MaterialPBR mat, float3 lightVec, float3 normal, float3 toEye)
{
    if(gSpecular)
    {
        float3 halfVec = normalize(toEye + lightVec);
        float D = Dggx(normal, halfVec, mat.roughness);
        float G = Gggx(mat.roughness, normal, toEye, lightVec);
        float3 F = Fggx(mat.R0, toEye, halfVec);
        
        return (D * G * F) / (4 * dot(toEye, normal));
    }
    return 0.0;
}

float3 GGX_PDF(MaterialPBR mat, float3 halfVec, float3 normal, float3 toEye)
{
    if(gSpecular)
    {
        float3 lightVec = reflect(-toEye, halfVec);
        
        float NdotL = dot(normal, lightVec);
        float NdotV = dot(normal, toEye);
        float NdotH = dot(normal, halfVec);
        float VdotH = dot(toEye, halfVec);

        if(NdotL <= 0.0 || NdotH <= 0.0 || VdotH <= 0.0)
            return 0.0;
        
        float G = Gggx(mat.roughness, normal, toEye, lightVec);
        float3 F = Fggx(mat.R0, toEye, halfVec);
        
        return (G * F * VdotH) / (NdotV * NdotL * NdotH);
    }
    return 0.0;
}

//reflection GGX
float3 reflectionsGGX(float3 toEye, float3 reflectionDir, float3 normal, float roughness, float3 R0)
{
    float3 halfVec = normalize(toEye + reflectionDir);
    float D = Dggx(normal, halfVec, roughness);
    float G = Gggx(roughness, normal, toEye, halfVec);
    float3 F = Fggx(R0, toEye, halfVec);
    
    return (D * G * F) / (4 * dot(toEye, normal));
}

float3 reflectionsGGX_PDF(float3 toEye, float3 reflectionDir, float3 normal, float3 halfVec, float roughness, float3 R0)
{
    float G = Gggx(roughness, normal, toEye, reflectionDir);
    float3 F = Fggx(R0, reflectionDir, halfVec);
    
    float NdotL = dot(normal, reflectionDir);
    float NdotV = dot(normal, toEye);
    float NdotH = dot(normal, halfVec);
    float VdotH = dot(toEye, halfVec);

    if(NdotL <= 0.0 || NdotH <= 0.0 || VdotH <= 0.0)
        return 0.0;
    return (G * F * VdotH) / (NdotV * NdotL * NdotH);
}

float3 ComputeDirectionalLight(Light L, MaterialPBR mat, float3 vndfNormal, float3 cosWNormal, float3 normal, float3 toEye, out float3 Ls)
{
    float3 lightVec = -L.Direction;
    
    float ndotl = max(dot(lightVec, cosWNormal), 0.0);
    float3 lightStrength = L.Strength;
    
    if(mat.specular > 0.0)
    {
        if(mat.divideByPdf)
            Ls = lightStrength * GGX_PDF(mat, vndfNormal, normal, toEye);
        else
            Ls = lightStrength * GGX(mat, lightVec, vndfNormal, toEye);
    }
    return lightStrength * ndotl;
}

float3 ComputePointLight(Light L, MaterialPBR mat, float3 pos, float3 vndfNormal, float3 cosWNormal, float3 normal, float3 toEye, out float3 Ls)
{
    float3 lightVec = L.Position - pos;
    float d = length(lightVec);
    
    if(d > L.FalloffEnd)
        return 0.0;
    
    lightVec /= d;
    
    float ndotl = max(dot(lightVec, cosWNormal), 0.0);
    float3 lightStrength = L.Strength;
    
    float att = CalcAttenuation(d, L.FalloffStart, L.FalloffEnd);
    lightStrength *= att;
    
    if(mat.specular > 0.0)
    {
        if(mat.divideByPdf)
            Ls = lightStrength * GGX_PDF(mat, vndfNormal, normal, toEye);
        else
            Ls = lightStrength * GGX(mat, lightVec, vndfNormal, toEye);
    }
    return lightStrength * ndotl;
}

float3 ComputeSpotLight(Light L, MaterialPBR mat, float3 pos, float3 vndfNormal, float3 cosWNormal, float3 normal, float3 toEye, out float3 Ls)
{
    float3 lightVec = L.Position - pos;
    float d = length(lightVec);
    
    if(d > L.FalloffEnd)
        return 0.0;
    
    lightVec /= d;
    
    float ndotl = max(dot(lightVec, cosWNormal), 0.0);
    float3 lightStrength = L.Strength;
    
    float att = CalcAttenuation(d, L.FalloffStart, L.FalloffEnd);
    lightStrength *= att;
    
    float spotFactor = pow(max(dot(-lightVec, L.Direction), 0.0), L.SpotPower);
    lightStrength *= spotFactor;
    
    if(mat.specular > 0.0)
    {
        if(mat.divideByPdf)
            Ls = lightStrength * GGX_PDF(mat, vndfNormal, normal, toEye);
        else
            Ls = lightStrength * GGX(mat, lightVec, vndfNormal, toEye);
    }
    return lightStrength * ndotl;
}

float3 ComputeLighting(Light light, MaterialPBR mat, float3 pos, float3 vndfNormal, float3 cosWNormal, float3 normal, float3 toEye, out float3 specAlbedo)
{
    float3 Ld = 0.0;
    float3 Ls = 0.0;
    
    if(light.type == LIGHT_TYPE_DIRECTIONAL)
        Ld = ComputeDirectionalLight(light, mat, vndfNormal, cosWNormal, normal, toEye, Ls);
    else if(light.type == LIGHT_TYPE_POINTLIGHT)
        Ld = ComputePointLight(light, mat, pos, vndfNormal, cosWNormal, normal, toEye, Ls);
    else if(light.type == LIGHT_TYPE_SPOTLIGHT)
        Ld = ComputeSpotLight(light, mat, pos, vndfNormal, cosWNormal, normal, toEye, Ls);
    
    float s = mat.specular;
    
    specAlbedo = Ls * s;
    return Ld * max(1.0 - length(Ls) * s, 0.0);
}

float3 EnvBRDFApprox2(float3 SpecularColor, float alpha, float NoV)
{
    NoV = abs(NoV);
 // [Ray Tracing Gems, Chapter 32]
    float4 X;
    X.x = 1.f;
    X.y = NoV;
    X.z = NoV * NoV;
    X.w = NoV * X.z;
    float4 Y;
    Y.x = 1.f;
    Y.y = alpha;
    Y.z = alpha * alpha;
    Y.w = alpha * Y.z;
    float2x2 M1 = float2x2(0.99044f, -1.28514f, 1.29678f, -0.755907f);
    float3x3 M2 = float3x3(1.f, 2.92338f, 59.4188f, 20.3225f, -27.0302f,
222.592f, 121.563f, 626.13f, 316.627f);
    float2x2 M3 = float2x2(0.0365463f, 3.32707, 9.0632f, -9.04756);
    float3x3 M4 = float3x3(1.f, 3.59685f, -1.36772f, 9.04401f, -16.3174f,
9.22949f, 5.56589f, 19.7886f, -20.2123f);
    float bias = dot(mul(M1, X.xy), Y.xy) * rcp(dot(mul(M2, X.xyw), Y.xyw));
    float scale = dot(mul(M3, X.xy), Y.xy) * rcp(dot(mul(M4, X.xzw), Y.xyw));
 // This is a hack for specular reflectance of 0
    bias *= saturate(SpecularColor.g * 50);
    return mad(SpecularColor, max(0, scale), max(0, bias));
}