#include "common.hlsl"
#include "raytracing_util.hlsli"

StructuredBuffer<Vertex> vertices: register(t0);
StructuredBuffer<int> indices: register(t1);
StructuredBuffer<MaterialData> gMaterials: register(t0, space1);

RaytracingAccelerationStructure SceneBVH: register(t2);

Texture2D gDiffuseMap[1]: register(t3);
Texture2D gNormalMap[2]: register(t4);

SamplerState gsamPointWrap: register(s0);
SamplerState gsamBilinearWrap: register(s1);

cbuffer cbPass: register(b0)
{
	float4x4 gInvView;
	float4x4 gInvProj;
	uint gFrameIndex;
}

cbuffer objPass: register(b1)
{
	float4x4 gWorld;
	int gDiffuseIndex;
	int gNormalIndex;
	uint gMatIndex;
}

[shader("closesthit")]
void ClosestHit(inout HitInfo payload, Attributes attrib)
{
	uint vertId = 3 * PrimitiveIndex();
	float3 worldOrigin = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
	float3 barycentrics = float3(1.0F - attrib.bary.x - attrib.bary.y, attrib.bary.x, attrib.bary.y);

	float2 uvs = vertices[indices[vertId]].uvs * barycentrics.x + vertices[indices[vertId + 1]].uvs * barycentrics.y +
				 vertices[indices[vertId + 2]].uvs * barycentrics.z;
	float3 tangent = vertices[indices[vertId]].tangent * barycentrics.x + vertices[indices[vertId + 1]].tangent * barycentrics.y +
					 vertices[indices[vertId + 2]].tangent * barycentrics.z;
	float3 norm = vertices[indices[vertId]].normal * barycentrics.x + vertices[indices[vertId + 1]].normal * barycentrics.y +
				  vertices[indices[vertId + 2]].normal * barycentrics.z;
	norm = normalize(mul(norm, (float3x3) gWorld));
	tangent = normalize(tangent);

	MaterialData data = gMaterials[gMatIndex];

	//******** TEMP ********//

	//global var
	const float4 gAmbientLight = float4(0.2F, 0.2F, 0.2F, 1.0F);

	//lights
	Light gLights[16];
	gLights[0].Strength = float3(1.0F, 1.0F, 1.0F);
	gLights[0].Direction = float3(0.0F, -1.0F, 0.0F);
	gLights[0].Position = float3(-2.0F, 2.45F, -1.0F);
	gLights[0].FalloffStart = 0.01F;
	gLights[0].FalloffEnd = 10.0F;
	gLights[0].SpotPower = 1.0F;

	//******** TEMP ********//
	
	//diffuse albedo
	float4 mapColor = float4(1.0F, 1.0F, 1.0F, 1.0F);
	float4 normalSample;

	if(gDiffuseIndex >= 0)
		mapColor = gDiffuseMap[gDiffuseIndex].SampleLevel(gsamBilinearWrap, uvs, 0.0F);
	if(gNormalIndex >= 0)
	{
		normalSample = gNormalMap[gNormalIndex].SampleLevel(gsamBilinearWrap, uvs, 0.0F);
	
		norm = normalSampleToWorldSpace(normalSample.rgb, norm, tangent);
	}

	uint seed = initRand(DispatchRaysIndex().x * gFrameIndex, DispatchRaysIndex().y * gFrameIndex, 16);
	float3 shadowFactor = float3(1.0F, 1.0F, 1.0F);
	float3 lightDir = normalize(gLights[0].Position - worldOrigin);

	float4 diffuseAlbedo = data.diffuseAlbedo;
	diffuseAlbedo *= mapColor;
	
	//indirect back propagation
	if(payload.colorAndDistance.a < 0.0F)
	{
		float ldot = dot(norm, lightDir);
		float spotFactor = pow(max(dot(-lightDir, gLights[0].Direction), 0.0F), gLights[0].SpotPower);
		float lightPower = (1.0F - diffuseAlbedo.a) * max(dot(-norm, lightDir), 0.0F) + (1.0F - data.roughness) * max(ldot, 0.0F);

		if(data.metallic > 0.01F || data.diffuseAlbedo.a < 0.97F)
			lightPower *= 0.7F;
		else
			lightPower *= 0.55F;
		float3 color = diffuseAlbedo * gLights[0].Strength * lightPower * spotFactor;
		payload.colorAndDistance = float4(color, RayTCurrent() + length(gLights[0].Position - worldOrigin));
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
	reflRay.TMin = 0.01F;
	reflRay.TMax = 1.5F;
	
    TraceRay(SceneBVH, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, 0xFF, 0, 0, 0, reflRay, reflPayload);
	if(reflPayload.colorAndDistance.a > 0.0F)
		indirectLight = reflPayload.colorAndDistance.rgb * min(3.5F / reflPayload.colorAndDistance.a, 1.0F);

	//ambient light
	float4 ambient = gAmbientLight * diffuseAlbedo;
	float4 hitColor = float4(ambient.rgb, RayTCurrent());
	const float lightRadius = 0.5F;

	//shadows
	float maxDist = gLights[0].FalloffEnd + gLights[0].FalloffStart;

	float3 lightSphereDirection = calcShadowDirectionSL(worldOrigin, gLights[0].Direction, gLights[0].Position, lightRadius, seed);
	float d = length(lightSphereDirection);

	RayDesc ray;
	ray.Origin = worldOrigin;
	ray.Direction = normalize(lightSphereDirection);
	ray.TMin = 0.01F;
	ray.TMax = d;

	ShadowHitInfo shadowPayload;
	shadowPayload.occlusion = 1.0F;
	shadowPayload.distance = 0.01F;
	if(dot(ray.Direction, norm) > 0.0F)
	{
		TraceRay(SceneBVH, RAY_FLAG_NONE, 0xFF, 2, 0, 2, ray, shadowPayload);
		if(shadowPayload.distance > 0.0F)
			shadowFactor[0] = 1.0F - shadowPayload.occlusion;
	}

	//blinn phong
	const float shininess = 1.0F - data.roughness;
	Material mat = { diffuseAlbedo, data.fresnelR0, shininess };
	float4 directLight = ComputeLighting(gLights, mat, worldOrigin, norm, -WorldRayDirection(), shadowFactor);

	hitColor.rgb += directLight.rgb + indirectLight;

	//reflections
	float rayNormDot = dot(norm, -WorldRayDirection());
	if(data.metallic > 0.01F && payload.recursionDepth < 3)
	{
		float fresnelFactor;
		if(data.diffuseAlbedo.a < 0.97F)
			fresnelFactor = 1.0F - clamp(rayNormDot, 0.0F, 1.0F);
		else
			fresnelFactor = 1.0F - clamp(rayNormDot * data.fresnelPower, 0.0F, 1.0F);

		HitInfo reflPayload;
		reflPayload.colorAndDistance = float4(1, 1, 1, 1);
		reflPayload.recursionDepth = payload.recursionDepth + 1;

		RayDesc reflRay;
		reflRay.Origin = worldOrigin;
		if((data.flags & 0x0000001) != 0)
			reflRay.Direction = reflect(WorldRayDirection(), norm);
		else
			reflRay.Direction = calcReflectionDirection(WorldRayDirection(), norm, data.roughness, seed);
		reflRay.TMin = 0.01F;
		reflRay.TMax = 1200 * shininess;

		TraceRay(SceneBVH, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, 0xFF, 0, 0, 0, reflRay, reflPayload);
		
		float distanceFactor = 1.0F - clamp(reflPayload.colorAndDistance.w / (1200 * shininess), 0.0F, 1.0F);
		hitColor.rgb = lerp(hitColor.rgb, reflPayload.colorAndDistance.rgb, fresnelFactor * (1.0F - data.metallic) * distanceFactor);
	}

	//refraction
	if(data.diffuseAlbedo.a < 0.97F && payload.recursionDepth < 3)
	{
		float fresnelFactor;
		if(data.metallic > 0.01F)
			fresnelFactor = 1.0F - clamp((1.0F - rayNormDot), 0.0F, 1.0F);
		else
			fresnelFactor = 1.0F - clamp((1.0F - rayNormDot) * data.fresnelPower, 0.0F, 1.0F);
		float visibility = 1.0F - diffuseAlbedo.a;

		HitInfo refrPayload;
		refrPayload.colorAndDistance = float4(1, 1, 1, 1);
		refrPayload.recursionDepth = payload.recursionDepth + 1;

		RayDesc refrRay;
		refrRay.Origin = worldOrigin;
		if((data.flags & 0x0000002) != 0)
			refrRay.Direction = refract(WorldRayDirection(), norm, data.refractionIndex);
		else
			refrRay.Direction = calcRefractionDirection(WorldRayDirection(), norm, data.refractionIndex, data.roughness, seed);
		refrRay.TMin = 0.01F;
		refrRay.TMax = 500 * max(diffuseAlbedo.a, 40.0F);

		TraceRay(SceneBVH, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, 0xFF, 0, 0, 0, refrRay, refrPayload);
		
		float distanceFactor = 1.0F - clamp(refrPayload.colorAndDistance.w / max(500 * diffuseAlbedo.a, 40.0F), 0.0F, 1.0F);
		hitColor.rgb = lerp(hitColor.rgb, refrPayload.colorAndDistance.rgb, visibility * fresnelFactor * distanceFactor);
	}

	//ambient occlusion
	float occlusion = 1.0F;

	RayDesc aoRay;
	aoRay.Origin = worldOrigin;
    aoRay.Direction = calcRTAODirection(norm, tangent, seed);
	aoRay.TMin = 0.001F;
	aoRay.TMax = 0.071F;

	AOHitInfo aoPayload;
	aoPayload.isHit = false;

    TraceRay(SceneBVH, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, 0xFF, 1, 0, 1, aoRay, aoPayload);
	if(aoPayload.isHit)
		occlusion = 0.1F;

	hitColor.rgb *= occlusion;

	payload.colorAndDistance = hitColor;
}