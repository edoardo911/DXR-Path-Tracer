#include "common.hlsli"

[shader("closesthit")]
void PosClosestHit(inout PosPayload payload, Attributes attrib)
{
    payload.hPosAndT = float4(WorldRayOrigin() + RayTCurrent() * WorldRayDirection(), RayTCurrent());
    payload.instanceID = InstanceID();
}

[shader("miss")]
void PosMiss(inout PosPayload payload: SV_RayPayload)
{
    payload.hPosAndT = float4(WorldRayOrigin() + 1e5 * WorldRayDirection(), RayTCurrent());
}