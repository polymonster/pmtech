#ifdef GLSL

#else
	
#define TEXTURE_2D( name, sampler_index ) Texture2D name; SamplerState sampler_##name : register(s##sampler_index); 
#define SAMPLE_TEXTURE_2D( name, uv ) name.Sample( sampler_##name, uv )

#endif