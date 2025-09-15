RWTexture2D<float4> gOutput: register(u0);

Texture2D gInput: register(t0);

SamplerState gLinearSampler: register(s1);

float rgb2luma(float3 rgb)
{
    return sqrt(dot(rgb, float3(0.299, 0.587, 0.114)));
}

float quality(int i)
{
    if(i < 6)
        return 1.0;
    else if(i == 6)
        return 1.5;
    else if(i > 6 && i < 11)
        return 2.0;
    else if(i == 11)
        return 4.0;
    else
        return 8.0;
}

#define EDGE_THRESHOLD_MIN 0.0312
#define EDGE_THRESHOLD_MAX 0.125
#define ITERATIONS 12
#define SUBPIXEL_QUALITY 0.75

[numthreads(32, 32, 1)]
void main(uint3 threadID: SV_DispatchThreadID)
{
    int w, h, l;
    gInput.GetDimensions(0, w, h, l);
    w -= 1;
    h -= 1;
    
    float invW = 1.0 / w;
    float invH = 1.0 / h;
    float2 uv = (threadID.xy + 0.5) / float2(w, h);
    
    float3 colorCenter = gInput.SampleLevel(gLinearSampler, uv, 0).rgb;
    
    float lumaCenter = rgb2luma(colorCenter);
    
    float lumaDown = rgb2luma(gInput[threadID.xy + int2(0, -1)].rgb);
    float lumaUp = rgb2luma(gInput[threadID.xy + int2(0, 1)].rgb);
    float lumaLeft = rgb2luma(gInput[threadID.xy + int2(-1, 0)].rgb);
    float lumaRight = rgb2luma(gInput[threadID.xy + int2(1, 0)].rgb);
    
    float lumaMin = min(lumaCenter, min(min(lumaDown, lumaUp), min(lumaLeft, lumaRight)));
    float lumaMax = max(lumaCenter, max(max(lumaDown, lumaUp), max(lumaLeft, lumaRight)));
    
    float lumaRange = lumaMax - lumaMin;
    
    if(lumaRange < max(EDGE_THRESHOLD_MIN, lumaMax * EDGE_THRESHOLD_MAX))
        gOutput[threadID.xy] = float4(colorCenter, 1.0);
    else
    {
        float lumaDownLeft = rgb2luma(gInput.SampleLevel(gLinearSampler, uv, 0, int2(-1, -1)).rgb);
        float lumaUpRight = rgb2luma(gInput.SampleLevel(gLinearSampler, uv, 0, int2(1, 1)).rgb);
        float lumaUpLeft = rgb2luma(gInput.SampleLevel(gLinearSampler, uv, 0, int2(-1, 1)).rgb);
        float lumaDownRight = rgb2luma(gInput.SampleLevel(gLinearSampler, uv, 0, int2(1, -1)).rgb);
        
        float lumaDownUp = lumaDown + lumaUp;
        float lumaLeftRight = lumaLeft + lumaRight;
        
        float lumaLeftCorners = lumaDownLeft + lumaUpLeft;
        float lumaDownCorners = lumaDownLeft + lumaDownRight;
        float lumaRightCorners = lumaDownRight + lumaUpRight;
        float lumaUpCorners = lumaUpRight + lumaUpLeft;
        
        float edgeHorizontal = abs(-2.0 * lumaLeft + lumaLeftCorners) + abs(-2.0 * lumaCenter + lumaDownUp) + abs(-2.0 * lumaRight + lumaRightCorners);
        float edgeVertical = abs(-2.0 * lumaUp + lumaUpCorners) + abs(-2.0 * lumaCenter + lumaLeftRight) + abs(-2.0 * lumaDown + lumaDownCorners);
        
        bool isHorizontal = (edgeHorizontal >= edgeVertical);
        
        float luma1 = isHorizontal ? lumaDown : lumaLeft;
        float luma2 = isHorizontal ? lumaUp : lumaRight;
        
        float gradient1 = luma1 - lumaCenter;
        float gradient2 = luma2 - lumaCenter;
        
        bool is1Steepest = abs(gradient1) >= abs(gradient2);
        
        float gradientScaled = 0.25 * max(abs(gradient1), abs(gradient2));
        
        float stepLength = isHorizontal ? invH : invW;
        
        float lumaLocalAverage = 0.0;
        if(is1Steepest)
        {
            stepLength = -stepLength;
            lumaLocalAverage = 0.5 * (luma1 + lumaCenter);
        }
        else
            lumaLocalAverage = 0.5 * (luma2 + lumaCenter);
        
        float2 currentUv = uv;
        if(isHorizontal)
            currentUv.y += stepLength * 0.5;
        else
            currentUv.x += stepLength * 0.5;
        
        float2 offset = isHorizontal ? float2(invW, 0.0) : float2(0.0, invH);
        
        float2 uv1 = currentUv - offset;
        float2 uv2 = currentUv + offset;
        
        float lumaEnd1 = rgb2luma(gInput.SampleLevel(gLinearSampler, uv1, 0).rgb);
        float lumaEnd2 = rgb2luma(gInput.SampleLevel(gLinearSampler, uv2, 0).rgb);
        lumaEnd1 -= lumaLocalAverage;
        lumaEnd2 -= lumaLocalAverage;
        
        bool reached1 = abs(lumaEnd1) >= gradientScaled;
        bool reached2 = abs(lumaEnd2) >= gradientScaled;
        bool reachedBoth = reached1 && reached2;
        
        if(!reached1)
            uv1 -= offset;
        if(!reached2)
            uv2 += offset;
        
        if(!reachedBoth)
        {
            for(int i = 2; i < ITERATIONS; ++i)
            {
                if(!reached1)
                {
                    lumaEnd1 = rgb2luma(gInput.SampleLevel(gLinearSampler, uv1, 0).rgb);
                    lumaEnd1 -= lumaLocalAverage;
                }
                
                if(!reached2)
                {
                    lumaEnd2 = rgb2luma(gInput.SampleLevel(gLinearSampler, uv2, 0).rgb);
                    lumaEnd2 -= lumaLocalAverage;
                }
                
                reached1 = abs(lumaEnd1) >= gradientScaled;
                reached2 = abs(lumaEnd2) >= gradientScaled;
                reachedBoth = reached1 && reached2;
                
                if(!reached1)
                    uv1 -= offset * quality(i);
                if(!reached2)
                    uv2 += offset * quality(i);
                if(reachedBoth)
                    break;
            }
        }
        
        float distance1 = isHorizontal ? (uv.x - uv1.x) : (uv.y - uv1.y);
        float distance2 = isHorizontal ? (uv2.x - uv.x) : (uv2.y - uv.y);
        
        bool isDirection1 = distance1 < distance2;
        float distanceFinal = min(distance1, distance2);
        
        float edgeThickness = distance1 + distance2;
        
        float pixelOffset = -distanceFinal / edgeThickness + 0.5;
        
        bool isLumaCenterSmaller = lumaCenter < lumaLocalAverage;
        bool correctVariation = ((isDirection1 ? lumaEnd1 : lumaEnd2) < 0.0) != isLumaCenterSmaller;
        float finalOffset = correctVariation ? pixelOffset : 0.0;
        
        float lumaAverage = (1.0 / 12.0) * (2.0 * (lumaDownUp + lumaLeftRight) + lumaLeftCorners + lumaRightCorners);
        
        float subPixelOffset1 = clamp(abs(lumaAverage - lumaCenter) / lumaRange, 0.0, 1.0);
        float subPixelOffset2 = (-2.0 * subPixelOffset1 + 3.0) * subPixelOffset1 * subPixelOffset1;
        
        float subPixeLOffsetFinal = subPixelOffset2 * subPixelOffset2 * SUBPIXEL_QUALITY;
        finalOffset = max(finalOffset, subPixeLOffsetFinal);
        
        float2 finalUv = uv;
        if(isHorizontal)
            finalUv.y += finalOffset * stepLength;
        else
            finalUv.x += finalOffset * stepLength;
        
        gOutput[threadID.xy] = gInput.SampleLevel(gLinearSampler, finalUv, 0);
    }
}