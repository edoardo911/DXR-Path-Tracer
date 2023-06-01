Texture2D gInput: register(t0);
Texture2D gHistory: register(t1);
Texture2D gDepth: register(t2);
Texture2D gMotionVector: register(t3);

RWTexture2D<float4> gOutput: register(u0);

SamplerState gLinearSampler: register(s1);

//TODO remove a and b
float Mitchell(float distance, float b = 1, float c = 0)
{
    float ax = abs(distance);
    if(ax < 1)
        return ((12 - 9 * b - 6 * c) * ax * ax * ax + (-18 + 12 * b + 6 * c) * ax * ax + (6 - 2 * b)) / 6.0;
    else if(ax >= 1 && ax < 2)
        return ((-b - 6 * c) * ax * ax * ax + (6 * b + 30 * c) * ax * ax + (-12 * b - 48 * c) * ax + (8 * b + 24 * c)) / 6.0;
    else
        return 0;
}

// The following code is licensed under the MIT license: https://gist.github.com/TheRealMJP/bc503b0b87b643d3505d41eab8b332ae

// Samples a texture with Catmull-Rom filtering, using 9 texture fetches instead of 16.
// See http://vec3.ca/bicubic-filtering-in-fewer-taps/ for more details
float4 SampleTextureCatmullRom(in Texture2D<float4> tex, in SamplerState linearSampler, in float2 uv, in float2 texSize)
{
    // We're going to sample a a 4x4 grid of texels surrounding the target UV coordinate. We'll do this by rounding
    // down the sample location to get the exact center of our "starting" texel. The starting texel will be at
    // location [1, 1] in the grid, where [0, 0] is the top left corner.
    float2 samplePos = uv * texSize;
    float2 texPos1 = floor(samplePos - 0.5f) + 0.5f;

    // Compute the fractional offset from our starting texel to our original sample location, which we'll
    // feed into the Catmull-Rom spline function to get our filter weights.
    float2 f = samplePos - texPos1;

    // Compute the Catmull-Rom weights using the fractional offset that we calculated earlier.
    // These equations are pre-expanded based on our knowledge of where the texels will be located,
    // which lets us avoid having to evaluate a piece-wise function.
    float2 w0 = f * (-0.5f + f * (1.0f - 0.5f * f));
    float2 w1 = 1.0f + f * f * (-2.5f + 1.5f * f);
    float2 w2 = f * (0.5f + f * (2.0f - 1.5f * f));
    float2 w3 = f * f * (-0.5f + 0.5f * f);

    // Work out weighting factors and sampling offsets that will let us use bilinear filtering to
    // simultaneously evaluate the middle 2 samples from the 4x4 grid.
    float2 w12 = w1 + w2;
    float2 offset12 = w2 / (w1 + w2);

    // Compute the final UV coordinates we'll use for sampling the texture
    float2 texPos0 = texPos1 - 1;
    float2 texPos3 = texPos1 + 2;
    float2 texPos12 = texPos1 + offset12;

    texPos0 /= texSize;
    texPos3 /= texSize;
    texPos12 /= texSize;

    float4 result = 0.0f;
    result += tex.SampleLevel(linearSampler, float2(texPos0.x, texPos0.y), 0.0f) * w0.x * w0.y;
    result += tex.SampleLevel(linearSampler, float2(texPos12.x, texPos0.y), 0.0f) * w12.x * w0.y;
    result += tex.SampleLevel(linearSampler, float2(texPos3.x, texPos0.y), 0.0f) * w3.x * w0.y;

    result += tex.SampleLevel(linearSampler, float2(texPos0.x, texPos12.y), 0.0f) * w0.x * w12.y;
    result += tex.SampleLevel(linearSampler, float2(texPos12.x, texPos12.y), 0.0f) * w12.x * w12.y;
    result += tex.SampleLevel(linearSampler, float2(texPos3.x, texPos12.y), 0.0f) * w3.x * w12.y;

    result += tex.SampleLevel(linearSampler, float2(texPos0.x, texPos3.y), 0.0f) * w0.x * w3.y;
    result += tex.SampleLevel(linearSampler, float2(texPos12.x, texPos3.y), 0.0f) * w12.x * w3.y;
    result += tex.SampleLevel(linearSampler, float2(texPos3.x, texPos3.y), 0.0f) * w3.x * w3.y;

    return result;
}

// From "Temporal Reprojection Anti-Aliasing"
// https://github.com/playdeadgames/temporal
float3 ClipAABB(float3 aabbMin, float3 aabbMax, float3 prevSample)
{
    // note: only clips towards aabb center (but fast!)
    float3 p_clip = 0.5 * (aabbMax + aabbMin);
    float3 e_clip = 0.5 * (aabbMax - aabbMin);

    float3 v_clip = prevSample - p_clip;
    float3 v_unit = v_clip.xyz / e_clip;
    float3 a_unit = abs(v_unit);
    float ma_unit = max(a_unit.x, max(a_unit.y, a_unit.z));

    if(ma_unit > 1.0)
        return p_clip + v_clip / ma_unit;
    else
        return prevSample; // point inside aabb
}

#define TEST 0

[numthreads(16, 16, 1)]
void main(uint3 pixel: SV_DispatchThreadID)
{
#if TEST
    gOutput[pixel.xy] = gInput[pixel.xy];
#else
    float3 sourceSampleTotal = float3(0, 0, 0);
    float sourceSampleWeight = 0.0;
    float3 neighborhoodMin = 10000;
    float3 neighborhoodMax = -10000;
    float3 m1 = float3(0, 0, 0);
    float3 m2 = float3(0, 0, 0);
    float closestDepth = 1.0;
    int2 closestDepthPixelPosition = int2(0, 0);
    
    uint w, h, l;
    gInput.GetDimensions(0, w, h, l);
    
    [unroll]
    for(int x = -1; x <= 1; ++x)
    {
        [unroll]
        for(int y = -1; y <= 1; ++y)
        {
            int2 pixelPosition = pixel.xy + int2(x, y);
            pixelPosition = clamp(pixelPosition, 0, int2(w - 1, h - 1));
            
            float3 neighbor = max(0, gInput[pixelPosition].rgb);
            float subSampleDistance = length(float2(x, y));
            float subSampleWeight = Mitchell(subSampleDistance);
            
            sourceSampleTotal += neighbor * subSampleWeight;
            sourceSampleWeight += subSampleWeight;
            
            neighborhoodMin = min(neighborhoodMin, neighbor);
            neighborhoodMax = max(neighborhoodMax, neighbor);
            
            m1 += neighbor;
            m2 += neighbor * neighbor;
            
            float currentDepth = gDepth[pixelPosition].r;
            if(currentDepth < closestDepth)
            {
                closestDepth = currentDepth;
                closestDepthPixelPosition = pixelPosition;
            }
        }
    }
    
    float2 motionVector = gMotionVector[closestDepthPixelPosition].xy;
    float2 historyTexCoord = (pixel.xy + motionVector) / float2(w, h);
    float3 sourceSample = sourceSampleTotal / sourceSampleWeight;
    
    if(any(historyTexCoord != saturate(historyTexCoord)))
        gOutput[pixel.xy] = float4(sourceSample, 1);
    else
    {
        float3 historySample = SampleTextureCatmullRom(gHistory, gLinearSampler, historyTexCoord, float2(w, h)).rgb;
    
        float oneDividedBySampleCount = 1.0 / 9.0;
        float gamma = 1.0;
        float3 mu = m1 * oneDividedBySampleCount;
        float3 sigma = sqrt(abs((m2 * oneDividedBySampleCount) - (mu * mu)));
        float3 minc = mu - gamma * sigma;
        float3 maxc = mu + gamma * sigma;
    
        historySample = ClipAABB(minc, maxc, clamp(historySample, neighborhoodMin, neighborhoodMax));
    
        float sourceWeight = 0.05;
        float historyWeight = 1.0 - sourceWeight;
        float3 compressedSource = sourceSample * rcp(max(max(sourceSample.r, sourceSample.g), sourceSample.b) + 1.0);
        float3 compressedHistory = historySample * rcp(max(max(historySample.r, historySample.g), historySample.b) + 1.0);
        float luminanceSource = dot(compressedSource, float3(0.299F, 0.587F, 0.144F));
        float luminanceHistory = dot(compressedHistory, float3(0.299F, 0.587F, 0.144F));
    
        sourceWeight *= 1.0 / (1.0 + luminanceSource);
        historyWeight *= 1.0 / (1.0 + luminanceHistory);
    
        float3 result = (sourceSample * sourceWeight + historySample * historyWeight) / max(sourceWeight + historyWeight, 0.00001);
        gOutput[pixel.xy] = float4(result, 1.0);
    }
#endif
}