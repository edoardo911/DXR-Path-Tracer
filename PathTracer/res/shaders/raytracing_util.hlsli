#pragma once

//random
uint initRand(uint val0, uint val1, uint backoff = 16)
{
    uint v0 = val0, v1 = val1, s0 = 0;

    [unroll]
    for (uint n = 0; n < backoff; n++)
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

//constants
const static float3 wRight = float3(1.0F, 0.0F, 0.0F);

//path tracing directions
float3 calcShadowDirectionSL(float3 worldOrigin, float3 lightDir, float3 lightPos, float lightRadius, uint seed)
{
	float3 lFront = cross(lightDir, wRight);
	lFront = normalize(lFront - dot(lFront, lightDir) * lightDir);
	float3 lRight = cross(lightDir, lFront);

	float2 offset = float2(nextRand(seed), nextRand(seed)) * 2.0F - 1.0F;
	float3 offsetPos = lightPos + (lRight * offset.x * lightRadius) + (lFront * offset.y * lightRadius);

	return offsetPos - worldOrigin;
}

float3 calcReflectionDirection(float3 rayDir, float3 normal, float roughness, uint seed)
{
	float3 up = reflect(rayDir, normal);
	float3 front = cross(up, wRight);
	front = normalize(front - dot(front, up) * up);
	float3 right = cross(up, front);
	float3x3 RFU = float3x3(right, up, front);

	float3 rv = float3(nextRand(seed) * 2.0F - 1.0F, nextRand(seed), nextRand(seed) * 2.0F - 1.0F);
	rv *= float3(roughness, 1.0F, roughness);

	return mul(rv, RFU);
}

float3 calcRefractionDirection(float3 rayDir, float3 normal, float refractionIndex, float roughness, uint seed)
{
	float3 up = refract(rayDir, normal, refractionIndex);
	float3 front = cross(up, wRight);
	front = normalize(front - dot(front, up) * up);
	float3 right = cross(up, front);
	float3x3 RFU = float3x3(right, up, front);

	float3 rv = float3(nextRand(seed) * 2.0F - 1.0F, nextRand(seed), nextRand(seed) * 2.0F - 1.0F);
	rv *= float3(roughness / 10.0F, 1.0F, roughness / 10.0F);

	return mul(rv, RFU);
}

float3 calcRTAODirection(float3 norm, float3 tangent, uint seed)
{
    float3 N = norm;
    float3 T = normalize(tangent - dot(tangent, N) * N);
    float3 B = cross(N, T);
    float3x3 TNB = float3x3(T, N, B);
	
    return normalize(mul(TNB, float3(nextRand(seed) * 2.0F - 1.0F, 1.0F, nextRand(seed) * 2.0F - 1.0F)));
}