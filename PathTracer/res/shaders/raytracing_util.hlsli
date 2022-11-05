#include "common.hlsl"

const static float3 wRight = float3(1.0F, 0.0F, 0.0F);

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