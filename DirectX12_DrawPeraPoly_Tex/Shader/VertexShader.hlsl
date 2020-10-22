#include "Common.hlsli"

cbuffer TeansBuffer : register(b0)
{
    matrix TransMat;
};

Out BasicVS(float4 pos : POSITION, float2 uv : TEXCOORD)
{
    Out o;
    o.svpos = o.pos = mul(TransMat, pos);
    o.uv = uv;

    return o;
}
