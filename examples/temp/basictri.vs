#ifdef GLSL

#else
	
#define TEXTURE_2D( name, sampler_index ) Texture2D name; SamplerState sampler_##name : register(s##sampler_index); 
#define SAMPLE_TEXTURE_2D( name, uv ) name.Sample( sampler_##name, uv )

#endif

struct vs_input
{
	float4 position : POSITION;
};

struct vs_output
{
	float4 position : SV_POSITION0;
};


vs_output main( vs_input input )
{
	vs_output output;
	
	output.position = input.position;
	
	return output;
}

