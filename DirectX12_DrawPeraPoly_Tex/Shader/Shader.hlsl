Texture2D Tex : register(t0);
SamplerState Smp : register(s0);
cbuffer TeansBuffer : register(b0)
{
	matrix TransMat;
};

struct Out
{
	float4 svpos : SV_POSITION;
	float4 pos : POSITION;
	float2 uv :TEXCOORD;
};

float4 BasicPS(Out o) : SV_Target
{
	float4 Col = Tex.Sample(Smp,o.uv);
	return Col;
}


//頂点シェーダ 
Out BasicVS(float4 pos : POSITION,float2 uv:TEXCOORD)
{
	Out o;
	o.svpos = o.pos =mul(TransMat,pos);
	o.uv = uv;

	return o;
}
