cbuffer bGlobal : register(b0) {
    float2 _TanAspect;  // dtan(fov * .5) * aspect, - dtan(fov / 2)
    float2 _Texel;      // 1 / target width, 1 / target height
    float1 _Far;        // Far
    float1 PADDING0;    // 
    float4 _ProjValues; // 1 / m[0][0], 1 / m[1][1], m[3][2], -m[2][2]
    float4x4 _mInvView; // 
    float4 vCameraPos;  // Camera position
}

cbuffer bLightData : register(b1) {
    float3 _LightDiffuse;
    float1 PADDING1;
    float2 _LightData; // Empty, Intensity
    float4 vPosition; // Light pos, w - unused
};

Texture2D<half2> _NormalTexture : register(t0);
SamplerState _NormalSampler     : register(s0);

Texture2D _DepthTexture    : register(t1);
SamplerState _DepthSampler : register(s1);

// https://aras-p.info/texts/CompactNormalStorage.html#method03spherical
// Spherical Coordinates
#define kPI 3.1415926536f
half3 NormalDecode(half2 enc) {
    half2 scth, ang = enc * 2.f - 1.f;
    sincos(ang.x * kPI, scth.x, scth.y);

    half2 scphi = half2(sqrt(1.f - ang.y * ang.y), ang.y);
    return half3(scth.y * scphi.x, scth.x * scphi.x, scphi.y);
}

float Depth2Linear(float z) {
    return _ProjValues.z / (z + _ProjValues.w);
}

float3 GetWorldPos(float2 ClipSpace, float lDepth) {
    float4 p = float4(ClipSpace * _ProjValues.xy * lDepth, lDepth, 1.);
    return mul(_mInvView, p).xyz;
}

struct PS {
    float4 Position : SV_Position0;
    float2 Texcoord : TEXCOORD0;
    float4 LightPos : TEXCOORD1;
    
};

half3 PointLight(float3 p, float3 n) {
    float3 lDir = vPosition  - p;
    float3 eyeD = vCameraPos - p;
    float dist = length(lDir);

    // Lambertian
    return dot(n, lDir / dist) * _LightDiffuse * _LightData.y;

    // Specular

}

half4 main(PS In) : SV_Target0 {
    // Unpack GBuffer
    float LinDepth = Depth2Linear(_DepthTexture.Sample(_DepthSampler, In.Texcoord).x);
    //half4 Diffuse = ;
    half3 Normal = NormalDecode(_NormalTexture.Sample(_NormalSampler, In.Texcoord));

    // Reconstruct world position
    float3 WorldPos = GetWorldPos(In.Texcoord, LinDepth);

    // Calculate point light
    half4 Final = half4(PointLight(In.Position.xyz, Normal), 1.);



    return Final;
}