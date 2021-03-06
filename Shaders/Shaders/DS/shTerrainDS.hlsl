cbuffer MeshBuffer : register(b0) {
    #include "Transform.h"
}

cbuffer MatrixBuffer : register(b1) {
    #include "Camera.h"
};

struct TS {
    float edges[3] : SV_TessFactor;
    float inside : SV_InsideTessFactor;
};

struct DS {
    float3 Position : POSITION0;
    float3 Normal   : NORMAL0;
    float2 Texcoord : TEXCOORD0;
};

struct PS {
    float4 Position : SV_Position;
    float3 Normal   : NORMAL0;
    float2 Texcoord : TEXCOORD0;
};

[domain("tri")]
PS main(TS In, float3 uvwCoord : SV_DomainLocation, const OutputPatch<DS, 3> path) {
    PS Out;
    float3 Vertex, Normal;

    Vertex = uvwCoord.x * path[0].Position +
             uvwCoord.y * path[1].Position +
             uvwCoord.z * path[2].Position;

    Normal = uvwCoord.x * path[0].Normal +
             uvwCoord.y * path[1].Normal +
             uvwCoord.z * path[2].Normal;

    Out.Position = mul(mWorld, float4(Vertex, 1.));
    Out.Position = mul(mView0, Out.Position);
    Out.Position = mul(mProj0, Out.Position);

    Out.Normal = mul(mWorld, float4(Normal, 0.)).xyz;
    Out.Texcoord = path[0].Texcoord;

    return Out;
}
