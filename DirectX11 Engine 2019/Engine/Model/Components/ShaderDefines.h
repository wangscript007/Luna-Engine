#ifndef ___SAFE_GUARD__SHADER__DEFINES__
#define ___SAFE_GUARD__SHADER__DEFINES__    
    
    #ifndef __cplusplus                 // HLSL 11
        
        #define mfloat4x4 float4x4
        #define uint32_t uint
        #define TEXSLOT(x)   : register(t[x])
        #define SAMPLSLOT(x) : register(s[x])
        
    #else                               // C++
        
        #define Texture2D Texture* 
        #define SamplerState Sampler* 
        #define TEXSLOT(x) 
        #define SAMPLSLOT(x)
        
    #endif
    
#endif
