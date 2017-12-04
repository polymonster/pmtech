#ifdef GLSL

#define TEXTURE_2D( sampler_name, sampler_index ) uniform sampler2D sampler_name
#define SAMPLE_TEXTURE_2D( sampler_name, uv ) texture( sampler_name, uv.xy )
#define mul( A, B ) A * B
#define saturate( A ) clamp( A, 0.0, 1.0 );

#define to_3x3( M4 ) float3x3(M4)

#else
	
#define TEXTURE_2D( name, sampler_index ) Texture2D name : register(t##sampler_index); ; SamplerState sampler_##name : register(s##sampler_index); 
#define SAMPLE_TEXTURE_2D( name, uv ) name.Sample( sampler_##name, uv )

#define to_3x3( M4 ) (float3x3)M4

#endif
