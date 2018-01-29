#ifdef GLSL

#define TEXTURE_2D( sampler_name, sampler_index ) uniform sampler2D sampler_name
#define SAMPLE_TEXTURE_2D( sampler_name, uv ) texture( sampler_name, uv.xy )

#define TEXTURE_2DMS( type, samples, sampler_name, sampler_index ) uniform sampler2DMS sampler_name
#define SAMPLE_TEXTURE_2DMS( sampler_name, x, y, fragment ) texelFetch( sampler_name, ivec2( x, y ), fragment )

#define mul( A, B ) A * B
#define mul_tbn( A, B ) B * A
#define saturate( A ) clamp( A, 0.0, 1.0 );

#define to_3x3( M4 ) float3x3(M4)

#define unpack_vb_instance_mat( r1, r2, r3, r4 ) mat[0] = r0; mat[1] = r1; mat[2] = r2; mat[3] = r3;

#else
	
#define TEXTURE_2D( name, sampler_index ) Texture2D name : register(t##sampler_index); ; SamplerState sampler_##name : register(s##sampler_index); 
#define SAMPLE_TEXTURE_2D( name, uv ) name.Sample( sampler_##name, uv )

#define TEXTURE_2DMS( type, samples, name, sampler_index ) Texture2DMS<type, samples> name : register(t##sampler_index); ; SamplerState sampler_##name : register(s##sampler_index); 
#define SAMPLE_TEXTURE_2DMS( name, x, y, fragment ) name.Load( uint2(x, y), fragment )

#define to_3x3( M4 ) (float3x3)M4
#define mul_tbn( A, B ) mul(A, B)

#define unpack_vb_instance_mat( mat, r0, r1, r2, r3 ) mat[0] = r0; mat[1] = r1; mat[2] = r2; mat[3] = r3; mat = transpose(mat)

#endif
