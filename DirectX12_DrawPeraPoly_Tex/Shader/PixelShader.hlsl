#include "Common.hlsli"

Texture2D Tex : register(t0);
SamplerState Smp : register(s0);

float4 BasicPS(Out o) : SV_Target
{
    float4 Col = Tex.Sample(Smp, o.uv);
    return Col;
}