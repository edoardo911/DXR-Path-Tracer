Texture2D gInput: register(t0);

RWTexture2D<float4> gOutput: register(u0);

[numthreads(16, 16, 1)]
void main(uint3 DTid: SV_DispatchThreadID)
{
}