//forward_render forward_lit_instanced
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


float3 cook_torrence(
	float4 light_pos_radius,
	float3 light_colour, 
	float3 n, 
	float3 world_pos, 
	float3 view_pos, 
	float3 albedo, 
	float3 metalness, 
	float roughness,
	float reflectivity
)
{
	float3 l = normalize( light_pos_radius.xyz - world_pos.xyz );
	float n_dot_l = dot( n, l );
	
	if( n_dot_l > 0.0f )
	{
		float roughness_sq = roughness * roughness;
		float k = reflectivity;
	
		float3 v_view = normalize( (view_pos.xyz - world_pos.xyz) );
		float3 hv = normalize( v_view + l );
		
		float n_dot_v	= dot( n, v_view );
		float n_dot_h = dot( n, hv );
		float v_dot_h = dot( v_view, hv );
		
		// geometric attenuation
 float n_dot_h_2 = 2.0f * n_dot_h;
 float g1 = (n_dot_h_2 * n_dot_v) / v_dot_h;
 float g2 = (n_dot_h_2 * n_dot_l) / v_dot_h;
 float geom_atten = min(1.0, min(g1, g2));
		
		// roughness (or: microfacet distribution function)
 // beckmann distribution function
 float r1 = 1.0f / ( 4.0f * roughness_sq * pow(n_dot_h, 4.0f));
 float r2 = (n_dot_h * n_dot_h - 1.0) / (roughness_sq * n_dot_h * n_dot_h);
 float roughness_atten = r1 * exp(r2);
		
		// fresnel
 // Schlick approximation
 float fresnel = pow(1.0 - v_dot_h, 5.0);
 fresnel *= roughness;
 fresnel += reflectivity;
		
 float specular = (fresnel * geom_atten * roughness_atten) / (n_dot_v * n_dot_l * 3.1419);
 
 //specular
 float3 lit_colour = metalness * light_colour * n_dot_l * ( k + specular * ( 1.0 - k ) );
 
 return saturate(lit_colour);	
	}
		
	return float3( 0.0, 0.0, 0.0 );
}




float3 oren_nayar(
	float4 light_pos_radius,
	float3 light_colour,
	float3 n,
	float3 world_pos,
	float3 view_pos,
	float roughness,
	float3 albedo) 
{
 
 float3 v = normalize(view_pos-world_pos);
 float3 l = normalize(light_pos_radius.xyz-world_pos);
 
 float l_dot_v = dot(l, v);
 float n_dot_l = dot(n, l);
 float n_dot_v = dot(n, v);

 float s = l_dot_v - n_dot_l * n_dot_v;
 float t = lerp(1.0, max(n_dot_l, n_dot_v), step(0.0, s));

 float lum = length( albedo );
 
 float sigma2 = roughness * roughness;
 float A = 1.0 + sigma2 * (lum / (sigma2 + 0.13) + 0.5 / (sigma2 + 0.33));
 float B = 0.45 * sigma2 / (sigma2 + 0.09);

 return ( albedo * light_colour * max(0.0, n_dot_l) * (A + B * s / t) / 3.14159265 );
}




float point_light_attenuation(
	float4 light_pos_radius,
	float3 world_pos)
{
	float d = length( world_pos.xyz - light_pos_radius.xyz );
	float r = light_pos_radius.w;
	
	float denom = d/r + 1.0;
	float attenuation = 1.0 / (denom*denom);
	
	return attenuation;
}




float3 transform_ts_normal( float3 t, float3 b, float3 n, float3 ts_normal )
{
	float3x3 tbn;
	tbn[0] = float3(t.x, b.x, n.x);
	tbn[1] = float3(t.y, b.y, n.y);
	tbn[2] = float3(t.z, b.z, n.z);
	
	return normalize( mul_tbn( tbn, ts_normal ) );
}



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
		
	float4 albedo = sample_texture( diffuse_texture, _input.texcoord.xy );
		
	float3 normal_sample = sample_texture( normal_texture, _input.texcoord.xy ).rgb;
	normal_sample = normal_sample * 2.0 - 1.0;
	
	float4 specular_sample = sample_texture( specular_texture, _input.texcoord.xy );
		
	float3 n = transform_ts_normal( 
		_input.tangent, 
		_input.bitangent, 
		_input.normal, 
		normal_sample );
		
	albedo *= _input.colour;
	
	float3 lit_colour = float3( 0.0, 0.0, 0.0 );
		
	//todo these need to be passed from vs for instancing
	float single_roughness = saturate(user_data.y); 
	float reflectivity = saturate(user_data.z);
	
	//for directional lights
	for( int i = 0; i < light_info.x; ++i )
	{
		float3 light_col = float3( 0.0, 0.0, 0.0 );
		
		light_col += cook_torrence( 
			lights[i].pos_radius, 
			lights[i].colour.rgb,
			n,
			_input.world_pos.xyz,
			camera_view_pos.xyz,
			albedo.rgb,
			specular_sample.rgb,
			single_roughness,
			reflectivity
		);	
		
		light_col += oren_nayar( 
			lights[i].pos_radius, 
			lights[i].colour.rgb,
			n,
			_input.world_pos.xyz,
			camera_view_pos.xyz,
			single_roughness,
			albedo.rgb
		);		
		
		if( lights[i].colour.a == 0.0 )
		{
			lit_colour += light_col;
			continue;
		}
		else
		{
			float4 sp = mul( _input.world_pos, shadow_matrix );
			sp.xyz /= sp.w;
			sp.y *= -1.0;
			sp.xy = sp.xy * 0.5 + 0.5;
			sp.z = remap_depth(sp.z);
			
			float d = sample_texture( shadowmap_texture, sp.xy ).r;
			float shadow = sp.z < d ? 1.0 : 0.0;
			lit_colour += light_col * shadow;
		}
	}
	
	//for point lights
	int point_start = int(light_info.x);
	int point_end = int(light_info.x) + int(light_info.y);
	for( int i = point_start; i < point_end; ++i )
	{
		float3 light_col = float3( 0.0, 0.0, 0.0 );
		
		light_col += cook_torrence( 
			lights[i].pos_radius, 
			lights[i].colour.rgb,
			n,
			_input.world_pos.xyz,
			camera_view_pos.xyz,
			albedo.rgb,
			specular_sample.rgb,
			single_roughness,
			reflectivity
		);	
		
		light_col += oren_nayar( 
			lights[i].pos_radius, 
			lights[i].colour.rgb,
			n,
			_input.world_pos.xyz,
			camera_view_pos.xyz,
			single_roughness,
			albedo.rgb
		);		
			
		float a = point_light_attenuation( lights[i].pos_radius, _input.world_pos.xyz );	
		light_col *= a;
		
		lit_colour += light_col;
	}

	_output.colour.rgb = lit_colour.rgb;
	_output.colour.a = albedo.a;

	//assign glsl global outputs from structs
	colour_ps_output = _output.colour;
}
 