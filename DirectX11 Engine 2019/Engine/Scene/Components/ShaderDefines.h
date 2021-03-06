#ifndef ___SAFE_GUARD__SHADER__DEFINES__
#define ___SAFE_GUARD__SHADER__DEFINES__    
    
    #ifndef __cplusplus                 // HLSL 11
        
        #define mfloat4x4 float4x4
        #define mfloat3x3 float3x3
        #define uint32_t uint
        #define TEXSLOT(x)   : register(t[x]);
        #define SAMPLSLOT(x) : register(s[x]);
        #define _Shader_(x) uint x;
        #define _ShDefault_(type, name, def) type name;
        #define _Texture2D Texture2D
        
        static const float3 gGlobalUPVector = { 0.f, 1.f, 0.f };

        float4x4 mRotFromForward(float3 dir, float3 up) {
            //dir += .000001f; // Prevent division by 0

            float3 x = normalize(cross(up, dir));
            float3 y = normalize(cross(dir, x));

            return float4x4(
                x.x, x.y, x.z, 0.f,
                y.x, y.y, y.z, 0.f,
                dir.x, dir.y, dir.z, 0.f,
                0.f, 0.f, 0.f, 1.f
            );
        }

    #else                               // C++

        const float3 gUPVector = { 0.f, 1.f, 0.f };

        #define DXDRAW                         0u // yes
        #define DXDRAWAUTO                     1u // yes
        #define DXDRAWINDEXED                  2u // yes
        #define DXDRAWINDEXEDINSTANCED         3u // yes
        #define DXDRAWINDEXEDINSTANCEDINDIRECT 4u // no
        #define DXDRAWINSTANCED                5u // yes
        #define DXDRAWINSTANCEDINDIRECT        6u // no
        #define DXDRAWDISPATCH                 7u // no
        #define DXDRAWDISPATCHINDIRECT         8u // no
        
        #define _ShDefault_(type, name, def) type name = (def);
        #define _Shader_(x) Shader* x = 0;
        #define _Texture2D Texture* 
        #define SamplerState Sampler* 
        #define TEXSLOT(x)   = nullptr;
        #define SAMPLSLOT(x) = nullptr;
        
    #endif
    
#endif

