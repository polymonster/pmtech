//forward_render constant_colour
#version 330 core
#define GLSL
#ifdef GLSL

//pmfx custom macros
#define texture_2d( sampler_name, sampler_index ) uniform sampler2D sampler_name
#define texture_3d( sampler_name, sampler_index ) uniform sampler3D sampler_name
#define texture_2dms( type, samples, sampler_name, sampler_index ) uniform sampler2DMS sampler_name
#define texture_cube( sampler_name, sampler_index )	uniform samplerCube sampler_name

#define sample_texture_2dms( sampler_name, x, y, fragment ) texelFetch( sampler_name, ivec2( x, y ), fragment )
#define sample_texture( sampler_name, V ) texture( sampler_name, V )
#define sample_texture_level( sampler_name, V, l ) textureLod( sampler_name, V, l )
#define sample_texture_grad( sampler_name, V, vddx, vddy ) textureGrad( sampler_name, V, vddx, vddy )

#define to_3x3( M4 ) float3x3(M4)
#define unpack_vb_instance_mat( mat, r0, r1, r2, r3 ) mat[0] = r0; mat[1] = r1; mat[2] = r2; mat[3] = r3;

#define remap_depth( d ) d = d * 0.5 + 0.5

#define depth_ps_output gl_FragDepth

//hlsl / glsl portability macros
#define float4x4 mat4
#define float3x3 mat3
#define float2x2 mat2

#define float4 vec4
#define float3 vec3
#define float2 vec2

#define lerp mix
#define modf mod

#define mul( A, B ) (A * B)
#define mul_tbn( A, B ) (B * A)
#define saturate( A ) (clamp( A, 0.0, 1.0 ))
	
#define ddx dFdx
#define ddy dFdy

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

#define remap_depth( d ) (d)

#endif

//platform agnostic pmfx macros
#define chebyshev_normalize( V ) (V.xyz / max( max(abs(V.x), abs(V.y)), abs(V.z) ))	



struct forward_light
{
	float4 pos_radius;
	float4 colour;
};
struct distance_field_shadow
{
	float4x4 	world_matrix;
	float4x4 	world_matrix_inv;
};
struct area_light
{
	float4x4 	world_matrix;
	float4x4 	world_matrix_inv;
};


in float4 world_pos_vs_output;
in float3 normal_vs_output;
in float3 tangent_vs_output;
in float3 bitangent_vs_output;
in float4 texcoord_vs_output;
in float4 colour_vs_output;

out float4 colour_ps_output;

struct vs_output
{
	float4 position;
	float4 world_pos;
	float3 normal;
	float3 tangent;
	float3 bitangent;
	float4 texcoord;
	float4 colour;
};

struct ps_output
{
	float4 colour;
};


texture_2d( diffuse_texture, 0 );
texture_2d( normal_texture, 1 );
texture_2d( specular_texture, 2 );

texture_2d( shadowmap_texture, 15 );

layout (std140) uniform per_pass_view 
{
	float4x4 vp_matrix;
	float4x4 view_matrix;
	float4x4 view_matrix_inverse;
	
	float4 camera_view_pos;
	float4 camera_view_dir;
};

layout (std140) uniform per_draw_call 
{
	float4x4 world_matrix;
	float4 user_data; 	//x = id, y roughness, z reflectivity
	float4 user_data2; 	//rgba colour
	
	float4x4 world_matrix_inv_transpose;
};

layout (std140) uniform per_pass_lights 
{
	forward_light lights[8];
	float4 		 light_info;
};

layout (std140) uniform per_pass_shadow 
{
	float4x4 shadow_matrix; 
};

layout (std140) uniform per_pass_shadow_distance_fields 
{
	distance_field_shadow sdf_shadow; 
};

layout (std140) uniform per_pass_area_lights 
{
	area_light area_lights; 
	float4	area_light_info;
};

layout (std140) uniform skinning_info 
{
	float4x4 bones[85];
};

void main()
{
	//assign vs_output struct from glsl inputs
	vs_output _input;
	_input.world_pos = world_pos_vs_output;
	_input.normal = normal_vs_output;
	_input.tangent = tangent_vs_output;
	_input.bitangent = bitangent_vs_output;
	_input.texcoord = texcoord_vs_output;
	_input.colour = colour_vs_output;

	//main body from ../assets/shaders/forward_render.shp
	ps_output _output;
	_output.colour = float4( 1.0, 1.0, 1.0, 1.0 );

	//assign glsl global outputs from structs
	colour_ps_output = _output.colour;
}
 