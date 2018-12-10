#ifdef GLSL

//pmfx custom macros
#define texture_2d( sampler_name, sampler_index ) uniform sampler2D sampler_name
#define texture_3d( sampler_name, sampler_index ) uniform sampler3D sampler_name
#define texture_cube( sampler_name, sampler_index )	uniform samplerCube sampler_name

#ifdef GLES
#define sample_texture_2dms( sampler_name, x, y, fragment ) texture( sampler_name, vec2(0.0, 0.0) )
#define texture_2dms( type, samples, sampler_name, sampler_index ) uniform sampler2D sampler_name
#else
#define sample_texture_2dms( sampler_name, x, y, fragment ) texelFetch( sampler_name, ivec2( x, y ), fragment )
#define texture_2dms( type, samples, sampler_name, sampler_index ) uniform sampler2DMS sampler_name
#endif

#define sample_texture( sampler_name, V ) texture( sampler_name, V )
#define sample_texture_level( sampler_name, V, l ) textureLod( sampler_name, V, l )
#define sample_texture_grad( sampler_name, V, vddx, vddy ) textureGrad( sampler_name, V, vddx, vddy )

#define to_3x3( M4 ) float3x3(M4)
#define unpack_vb_instance_mat( mat, r0, r1, r2, r3 ) mat[0] = r0; mat[1] = r1; mat[2] = r2; mat[3] = r3;
#define to_data_matrix(mat) mat

#define remap_z_clip_space( d ) d // gl clip space is -1 to 1, and this is normalised device coordinate
#define remap_depth( d ) d = d * 0.5 + 0.5
#define remap_ndc_ray( r ) float2(r.x, r.y)  

#define depth_ps_output gl_FragDepth

//hlsl / glsl portability macros
#define float4x4 mat4
#define float3x3 mat3
#define float2x2 mat2

#define float4 vec4
#define float3 vec3
#define float2 vec2

// hlsl style mod
#define modf mod
#define frac fract
#define lerp mix

#define mul( A, B ) (A * B)
#define mul_tbn( A, B ) (B * A)
#define saturate( A ) (clamp( A, 0.0, 1.0 ))
	
#define ddx dFdx
#define ddy dFdy

#define __pmfx_unroll

#else

//pmfx custom macros
#define texture_2d( name, sampler_index ) Texture2D name : register(t##sampler_index); ; SamplerState sampler_##name : register(s##sampler_index); 
#define texture_3d( name, sampler_index ) Texture3D name : register(t##sampler_index); ; SamplerState sampler_##name : register(s##sampler_index); 
#define texture_2dms( type, samples, name, sampler_index ) Texture2DMS<type, samples> name : register(t##sampler_index); ; SamplerState sampler_##name : register(s##sampler_index); 
#define texture_cube( name, sampler_index )	TextureCube name : register(t##sampler_index); ; SamplerState sampler_##name : register(s##sampler_index); 

#define sample_texture_2dms( name, x, y, fragment ) name.Load( uint2(x, y), fragment )
#define sample_texture( name, V ) name.Sample( sampler_##name, V )
#define sample_texture_level( name, V, l ) name.SampleLevel( sampler_##name, V, l )
#define sample_texture_grad( name, V, vddx, vddy ) name.SampleGrad( sampler_##name, V, vddx, vddy )

#define to_3x3( M4 ) (float3x3)M4
#define mul_tbn( A, B ) mul(A, B)

#define unpack_vb_instance_mat( mat, r0, r1, r2, r3 ) mat[0] = r0; mat[1] = r1; mat[2] = r2; mat[3] = r3; mat = transpose(mat)
#define to_data_matrix(mat) transpose(mat)

// d3d clip pace z is 0 to 1 and ndc z is -1 to 1
#define remap_z_clip_space( d ) d = d * 0.5 + 0.5;
#define remap_depth( d ) (d)
#define remap_ndc_ray( r ) float2(r.x, r.y * -1.0)

#define mod(x, y) x - y * floor(x/y) // glsl style mod

#define pmfx_unroll [unroll]

#endif

//platform agnostic pmfx macros
#define chebyshev_normalize( V ) (V.xyz / max( max(abs(V.x), abs(V.y)), abs(V.z) ))	



