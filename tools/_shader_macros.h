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

#define OPTION_OUTPUT 0
#define DEBUG_SETTINGS_START
#define DEBUG_DIFFUSE_MAP 0 == debug_render_options[OPTION_OUTPUT] 			
#define DEBUG_SPECULAR_MAP 1 == debug_render_options[OPTION_OUTPUT]			
#define DEBUG_NORMAL_MAP 2 == debug_render_options[OPTION_OUTPUT]			
#define DEBUG_VERTEX_NORMALS 3 == debug_render_options[OPTION_OUTPUT]		
#define DEBUG_VERTEX_TANGENTS 4 == debug_render_options[OPTION_OUTPUT]		
#define DEBUG_VERTEX_BITANGENTS 5 == debug_render_options[OPTION_OUTPUT]	
#define DEBUG_PER_PIXEL_NORMALS 6 == debug_render_options[OPTION_OUTPUT]	
#define DEBUG_COOK_TORRENCE 7 == debug_render_options[OPTION_OUTPUT]	
#define DEBUG_SETTINGS_END