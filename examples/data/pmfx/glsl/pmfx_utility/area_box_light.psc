//pmfx_utility area_box_light
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

out float4 colour_ps_output;

struct vs_output
{
	float4 position;
	float4 world_pos;
	float3 normal;
	float3 tangent;
	float3 bitangent;
	float4 texcoord;
};

struct ps_output
{
	float4 colour;
};


texture_2d( diffuse_texture, 0 );
texture_2d( normal_texture, 1 );
texture_2d( specular_texture, 2 );

texture_cube( cubemap_texture, 3 );
texture_3d( volume_texture, 4 );

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

bool ray_vs_aabb(float3 emin, float3 emax, float3 r1, float3 rv, out float3 intersection)
{
	float3 dirfrac = float3(1.0, 1.0, 1.0) / rv;

	float t1 = (emin.x - r1.x)*dirfrac.x;
	float t2 = (emax.x - r1.x)*dirfrac.x;
	float t3 = (emin.y - r1.y)*dirfrac.y;
	float t4 = (emax.y - r1.y)*dirfrac.y;
	float t5 = (emin.z - r1.z)*dirfrac.z;
	float t6 = (emax.z - r1.z)*dirfrac.z;

	float tmin = max(max(min(t1, t2), min(t3, t4)), min(t5, t6));
	float tmax = min(min(max(t1, t2), max(t3, t4)), max(t5, t6));

	float t = 0.0f;

	// if tmax < 0, ray (line) is intersecting AABB, but the whole AABB is behind us
	if (tmax < 0)
	{
		t = tmax;
		return false;
	}
	

	// if tmin > tmax, ray doesn't intersect AABB
	if (tmin > tmax)
	{
		t = tmax;
		return false;
	}

	t = tmin;

	intersection = r1 + rv * t;

	return true;
}




float3 closest_point_on_ray(float3 r0, float3 rV, float3 p)
{
	float3 v1 = p - r0;
	float t = dot(v1, rV);

	return r0 + rV * t;
}




float3 closest_point_on_aabb(float3 emin, float3 emax, float3 p)
{
	float3 t1 = float3(max(p.x, emin.x), max(p.y, emin.y), max(p.z, emin.z));
	float3 t2 = float3(min(t1.x, emax.x), min(t1.y, emax.y), min(t1.z, emax.z));
	
 return t2;
}




bool ray_vs_obb_ex(float4x4 obb_mat, float4x4 obb_inv_mat, float3 r1, float3 rv, 
					out float3 intersection, out float3 closest_point_obb, out float3 closest_point_ray)
{
	float4x4 inv_rot = obb_inv_mat;
	inv_rot[0].w = 0.0;
	inv_rot[1].w = 0.0;
	inv_rot[2].w = 0.0;
	inv_rot[3].xyzw = float4(0.0, 0.0, 0.0, 1.0 );
	
	//transform into obb
	float3 trv = mul( float4(rv, 1.0), inv_rot ).xyz;
	float3 tr1 = mul( float4(r1, 1.0), obb_inv_mat ).xyz;
	
	float3 ip;
	bool hit = ray_vs_aabb(float3(-1.0, -1.0, -1.0), float3(1.0, 1.0, 1.0), tr1, trv, ip);
	
	float3 cpr = closest_point_on_ray(tr1, trv, float3(0.0, 0.0, 0.0));
	float3 cpb = closest_point_on_aabb(float3(-1.0, -1.0, -1.0), float3(1.0, 1.0, 1.0), cpr);
	
	intersection = mul(float4(ip, 1.0), obb_mat).xyz;
	
	closest_point_obb = mul(float4(cpr, 1.0), obb_mat).xyz;
	closest_point_ray = mul(float4(cpb, 1.0), obb_mat).xyz;
	
	return hit;
}




float3 closest_point_on_obb(float4x4 mat, float4x4 inv_mat, float3 p)
{	
	float3 tp = mul(float4(p, 1.0), inv_mat).xyz;

	float3 cp = closest_point_on_aabb(float3(-1.0, -1.0, -1.0), float3(1.0, 1.0, 1.0), tp);

	float3 tcp = mul(float4(cp, 1.0f), mat).xyz;

	return tcp;
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

	//main body from ../assets/shaders/pmfx_utility.shp
	ps_output _output;
	
	_output.colour = float4(0.0, 0.0, 0.0, 1.0);
	
	float3 albedo = float3( 1.0, 1.0, 1.0 );
	
	float max_samples = 64.0;
	
	float3 v = _input.texcoord.xyz;
	float3 chebyshev_norm = chebyshev_normalize(v);
	
	float3 light_pos = lights[0].pos_radius.xyz;
	
	float3 view_v = normalize(_input.world_pos.xyz - camera_view_pos.xyz);
	float3 refl = reflect(view_v, _input.normal);
	
	float3 cp = closest_point_on_obb(area_lights.world_matrix, area_lights.world_matrix_inv, _input.world_pos.xyz);
	
	float diff = saturate(dot(-normalize(_input.world_pos.xyz-cp), _input.normal));
	
	float3 rv = normalize(refl);
	float3 r1 = _input.world_pos.xyz;
	
	float3 ip, bp, rp;
	bool hit = ray_vs_obb_ex(area_lights.world_matrix, area_lights.world_matrix_inv, r1, rv, ip, bp, rp);
	
	float wd = length(_input.world_pos.xyz-rp)/100;
	
	float dp2 = dot(normalize(_input.world_pos.xyz-cp), refl);
	
	float n_dot_v = dot(_input.normal, -view_v);
	
	float n_dot_l = saturate( dot(normalize(cp-_input.world_pos.xyz), _input.normal) );
		
	float s = lerp(0.01, 10.0, wd);
	
	float d = 1.0 - length(bp-rp) / s;
	
	if(n_dot_l <= 0.0)
		d = 0.0;

	float mask = 1.0 - saturate(dp2);
	
	float light = diff; //saturate(d + diff);
		
	_output.colour = float4( light, light, light, 1.0);

	//assign glsl global outputs from structs
	colour_ps_output = _output.colour;
}
 