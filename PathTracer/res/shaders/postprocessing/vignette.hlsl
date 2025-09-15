RWTexture2D<float4> gOutput: register(u0);

cbuffer passData: register(b0)
{
    float invWidth;
    float invHeight;
}

#define SQRT2 1.414213

[numthreads(32, 32, 1)]
void main(uint3 threadID: SV_DispatchThreadID )
{
    float2 distance = threadID.xy * float2(invWidth, invHeight);
    distance = (distance * 2.0) - 1.0;
    
    float l = length(distance);
    float factor = saturate(1.2 - l / SQRT2);
    
    gOutput[threadID.xy] *= factor;
}