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
	float a = 0.15;
	float b = 0.5;
	float c = 0.1;
	float d = 0.2;
	float e = 0.02;
	float f = 0.3;
	return ((color * (a * color + c * b) + d * e) / (color * (a * color + b) + d * f)) - e / f;
}

[numthreads(16, 16, 1)]
void main(uint3 pixel: SV_DispatchThreadID)
{
	float4 baseColor = image[pixel.xy] * exposure;
	float3 bcColor = contrast * (baseColor.rgb - 0.5) + 0.5 + brightness;

	float luma = dot(bcColor, float3(0.299, 0.587, 0.144));

	float3 saturatedColor = lerp(luma, bcColor, saturation);
	float3 tonemapped = uncharted2_tonemapping(saturatedColor * 3);
	float3 whiteScale = 1.0 / uncharted2_tonemapping(11.2);
	tonemapped *= whiteScale;
	
	float3 finalColor;
	if(gamma == 1.0)
		finalColor = tonemapped;
	else
		finalColor = pow(tonemapped, gamma);

    image[pixel.xy] = float4(finalColor.rgb, 1.0);
}