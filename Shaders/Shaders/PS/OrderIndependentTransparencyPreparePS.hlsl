cbuffer cbMaterial : register(b0) {
    #include "Material.h"
};

#include "../Common/OITCommon.h"

struct PS {
    float4 Position : SV_Position;
    float3 Normal   : NORMAL0;
    float2 Texcoord : TEXCOORD0;
    float4 WorldPos : TEXCOORD1;
};

#include "MaterialTextures.h"

float4 ComputeTransparentColor(PS In, bool front) {
    //abs(In.Position.x - In.Position.y - front * 2.f).xxx * 10.f
    //return front ? float4(1.f, 0.f, 0.f, 1.f) : float4(0.f, 1.f, 0.f, 1.f);
    
    //return front ? float4(.9f, 0.f, 0.f, .5f) : float4(.9f, .9f, 0.f, .5f);
    
    float3 Albedo = 1.f;
    [flatten] if( _Alb ) Albedo = _AlbedoTex.Sample(_AlbedoSampl, In.Texcoord).rgb * _AlbedoMul;
    
    return float4(Albedo, _Alpha);
    //return front ? float4(.7f, .9f, 0.f, .1f) : float4(0.f, 0.f, 1.f, 0.f);
    
    //return float4(_Texture.Sample(_Sampler, In.Texcoord).rgb, .1f);
    //return float4(1.f, 0.f, 0.f, 1.f);
}

[earlydepthstencil]
float4 main(PS In, uint coverage : SV_Coverage, bool front : SV_IsFrontFace) : SV_Target0 {
	uint head = sbLinkedLists.IncrementCounter();
	if( head == 0xFFFFFFFF ) return 0.f;
    
    float4 color = ComputeTransparentColor(In, front);
    
	uint oldHeadBuffVal;
	InterlockedExchange(rwListHead[uint2(In.Position.xy)], head, oldHeadBuffVal);
	
    float Depth = In.WorldPos.w;
    
    // Store
	ListItem node;
	    node.uColor    = PackColor(color.rgba);
	    node.fDepth    = (Depth);
	    node.uNext     = oldHeadBuffVal;
        node.uCoverage = coverage;
	sbLinkedLists[head] = node;
    
    return 0.f;
}
