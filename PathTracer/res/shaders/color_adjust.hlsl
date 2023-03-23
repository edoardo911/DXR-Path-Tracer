RWTexture2D<float4> image: register(u0);

cbuffer passData: register(b0)
{
	float exposure;
	float brightness;
	float contrast;
	float saturation;
	float gamma;
}

float3 uncharted2_tonemapping(float3 color)
{
	float a = 0.15F;
	float b = 0.5F;
	float c = 0.1F;
	float d = 0.2F;
	float e = 0.02F;
	float f = 0.3F;
	return ((color * (a * color + c * b) + d * e) / (color * (a * color + b) + d * f)) - e / f;
}

#define TEST 0

[numthreads(16, 16, 1)]
void main(uint3 pixel: SV_DispatchThreadID)
{
#if !TEST
	float4 baseColor = image[pixel.xy] * exposure;
	float3 bcColor = contrast * (baseColor.rgb - 0.5F) + 0.5F + brightness;

	float luma = dot(bcColor, float3(0.299F, 0.587F, 0.144F));

	float3 saturatedColor = lerp(luma, bcColor, saturation);
	float3 tonemapped = uncharted2_tonemapping(saturatedColor * 3.5F);
	float3 whiteScale = 1.0F / uncharted2_tonemapping(11.2F);
	tonemapped *= whiteScale;
	
	float3 finalColor;
	if(gamma == 1.0F)
		finalColor = tonemapped;
	else
		finalColor = pow(tonemapped, gamma);
    image[pixel.xy] = float4(pow(baseColor, gamma).rgb, 1.0F); //TODO
#else
    image[pixel.xy] = clamp(image[pixel.xy], 0, 1);
#endif
}