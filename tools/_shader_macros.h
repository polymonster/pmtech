#ifdef GLSL
    // texture
    #ifdef BINDING_POINTS
    #define _tex_binding(sampler_index) layout(binding = sampler_index)
    #else
    #define _tex_binding(sampler_index)  
    #endif
    #define texture_2d( sampler_name, sampler_index ) _tex_binding(sampler_index) uniform sampler2D sampler_name
    #define texture_3d( sampler_name, sampler_index ) _tex_binding(sampler_index) uniform sampler3D sampler_name
    #define texture_cube( sampler_name, sampler_index ) _tex_binding(sampler_index) uniform samplerCube sampler_name
    #define texture_2d_array( sampler_name, sampler_index ) _tex_binding(sampler_index) uniform sampler2DArray sampler_name
    #ifdef GLES
    #define sample_texture_2dms( sampler_name, x, y, fragment ) texture( sampler_name, vec2(0.0, 0.0) )
    #define texture_2dms( type, samples, sampler_name, sampler_index ) uniform sampler2D sampler_name
    #else
    #define sample_texture_2dms( sampler_name, x, y, fragment ) texelFetch( sampler_name, ivec2( x, y ), fragment )
    #define texture_2dms( type, samples, sampler_name, sampler_index ) _tex_binding(sampler_index) uniform sampler2DMS sampler_name
    #endif
    // sampler
    #define sample_texture( sampler_name, V ) texture( sampler_name, V )
    #define sample_texture_level( sampler_name, V, l ) textureLod( sampler_name, V, l )
    #define sample_texture_grad( sampler_name, V, vddx, vddy ) textureGrad( sampler_name, V, vddx, vddy )
    #define sample_texture_array( sampler_name, V, a ) texture( sampler_name, vec3(V, a) )
    #define sample_texture_array_level( sampler_name, V, a, l ) textureLod( sampler_name, vec3(V, a), l )
    // matrix
    #define to_3x3( M4 ) float3x3(M4)
    #define from_columns_3x3(A, B, C) (transpose(float3x3(A, B, C)))
    #define from_rows_3x3(A, B, C) (float3x3(A, B, C))
    #define unpack_vb_instance_mat( mat, r0, r1, r2, r3 ) mat[0] = r0; mat[1] = r1; mat[2] = r2; mat[3] = r3;
    #define to_data_matrix(mat) mat
    // clip
    #define remap_z_clip_space( d ) d // gl clip space is -1 to 1, and this is normalised device coordinate
    #define remap_depth( d ) (d = d * 0.5 + 0.5)
    #define remap_ndc_ray( r ) float2(r.x, r.y)  
    #define depth_ps_output gl_FragDepth
    // def
    #define float4x4 mat4
    #define float3x3 mat3
    #define float2x2 mat2
    #define float4 vec4
    #define float3 vec3
    #define float2 vec2
    #define modf mod
    #define frac fract
    #define lerp mix
    #define mul( A, B ) ((A) * (B))
    #define mul_tbn( A, B ) ((B) * (A))
    #define saturate( A ) (clamp( A, 0.0, 1.0 ))
    #define ddx dFdx
    #define ddy dFdy
    #define _pmfx_unroll
#endif //GLSL

#ifdef HLSL
    // texture
    #define texture2d_rw( name, index ) RWTexture2D<float4> name : register(u##index)
    #define texture2d_r( name, index ) Texture2D<float4> name : register(t##index)
    #define texture2d_w( name, index ) texture2d_rw( name, index )
    #define read_texture( name, gid ) name[gid]
    #define write_texture( name, val, gid ) name[gid] = val
    #define texture_2d( name, sampler_index ) Texture2D name : register(t##sampler_index); ; SamplerState sampler_##name : register(s##sampler_index); 
    #define texture_3d( name, sampler_index ) Texture3D name : register(t##sampler_index); ; SamplerState sampler_##name : register(s##sampler_index); 
    #define texture_2dms( type, samples, name, sampler_index ) Texture2DMS<type, samples> name : register(t##sampler_index); ; SamplerState sampler_##name : register(s##sampler_index); 
    #define texture_cube( name, sampler_index )    TextureCube name : register(t##sampler_index); ; SamplerState sampler_##name : register(s##sampler_index); 
    #define texture_2d_array( name, sampler_index ) Texture2DArray name : register(t##sampler_index); ; SamplerState sampler_##name : register(s##sampler_index); 
    // structured buffer
    #define structured_buffer_rw( type, name, index ) RWStructuredBuffer<type> name : register(u##index);
    #define structured_buffer( type, name, index ) StructuredBuffer<type> name : register(u##index);
    // sampler
    #define sample_texture_2dms( name, x, y, fragment ) name.Load( uint2(x, y), fragment )
    #define sample_texture( name, V ) name.Sample(sampler_##name, V)
    #define sample_texture_level( name, V, l ) name.SampleLevel(sampler_##name, V, l)
    #define sample_texture_grad( name, V, vddx, vddy ) name.SampleGrad(sampler_##name, V, vddx, vddy )
    #define sample_texture_array( name, V, a ) name.Sample(sampler_##name, float3(V.xy, a) )
    #define sample_texture_array_level( name, V, a, l ) name.SampleLevel(sampler_##name, float3(V.xy, a), l)
    // matrix
    #define to_3x3( M4 ) (float3x3)M4
    #define from_columns_3x3(A, B, C) (float3x3(A, B, C))
    #define from_rows_3x3(A, B, C) (transpose(float3x3(A, B, C)))
    #define mul_tbn( A, B ) mul(A, B)
    #define unpack_vb_instance_mat( mat, r0, r1, r2, r3 ) mat[0] = r0; mat[1] = r1; mat[2] = r2; mat[3] = r3; mat = transpose(mat)
    #define to_data_matrix(mat) transpose(mat)
    // clip
    #define remap_z_clip_space( d ) (d = d * 0.5 + 0.5)
    #define remap_depth( d ) (d)
    #define remap_ndc_ray( r ) float2(r.x, r.y * -1.0)
    // defs
    #define mod(x, y) (x - y * floor(x/y))
    #define fract frac
    #define _pmfx_unroll [unroll]
#endif //HLSL

#ifdef METAL
    // texture
    #define texture2d_rw( name, index ) texture2d<float, access::read_write> name [[texture(index)]]
    #define texture2d_r( name, index ) texture2d<float, access::read> name [[texture(index)]]
    #define texture2d_w( name, index ) texture2d<float, access::write> name [[texture(index)]]
    #define read_texture( name, gid ) name.read(gid)
    #define write_texture( name, val, gid ) name.write(val, gid)
    #define texture_2d( name, sampler_index ) texture2d<float> name [[texture(sampler_index)]], sampler sampler_##name [[sampler(sampler_index)]]
    #define texture_3d( name, sampler_index ) texture3d<float> name [[texture(sampler_index)]], sampler sampler_##name [[sampler(sampler_index)]]
    #define texture_2dms( type, samples, name, sampler_index ) texture2d_ms<float> name [[texture(sampler_index)]], sampler sampler_##name [[sampler(sampler_index)]]
    #define texture_cube( name, sampler_index ) texturecube<float> name [[texture(sampler_index)]], sampler sampler_##name [[sampler(sampler_index)]]
    #define texture_2d_array( name, sampler_index ) texture2d_array<float> name [[texture(sampler_index)]], sampler sampler_##name [[sampler(sampler_index)]]
    #define texture_2d_arg(name) thread texture2d<float>& name, thread sampler& sampler_##name
    #define texture_3d_arg(name) thread texture3d<float>& name, thread sampler& sampler_##name
    #define texture_2dms_arg(name) thread texture2d_ms<float>& name, thread sampler& sampler_##name
    #define texture_cube_arg(name) thread texturecube<float>& name, thread sampler& sampler_##name
    #define texture_2d_array_arg(name) thread texture2d_array<float>& name, thread sampler& sampler_##name
    // structured buffers
    #define structured_buffer_rw( type, name, index ) device type* name [[buffer(index)]]
    #define structured_buffer_rw_arg( type, name, index ) device type* name [[buffer(index)]]
    #define structured_buffer( type, name, index ) constant type& name [[buffer(index)]]
    #define structured_buffer_arg( type, name, index ) constant type& name [[buffer(index)]]
    // sampler
    #define sample_texture( name, tc ) name.sample(sampler_##name, tc)
    #define sample_texture_2dms( name, x, y, fragment ) name.read(uint2(x, y), fragment)
    #define sample_texture_level( name, tc, l ) name.sample(sampler_##name, tc, level(l))
    #define sample_texture_grad( name, tc, vddx, vddy ) name.sample(sampler_##name, tc, gradient3d(vddx, vddy))
    #define sample_texture_array( name, tc, a ) name.sample(sampler_##name, tc, uint(a))
    #define sample_texture_array_level( name, tc, a, l ) name.sample(sampler_##name, tc, uint(a), level(l))
    // matrix
    #define to_3x3( M4 ) float3x3(M4[0].xyz, M4[1].xyz, M4[2].xyz)
    #define from_columns_3x3(A, B, C) (transpose(float3x3(A, B, C)))
    #define from_rows_3x3(A, B, C) (float3x3(A, B, C))
    #define mul( A, B ) ((A) * (B))
    #define mul_tbn( A, B ) ((B) * (A))
    #define unpack_vb_instance_mat( mat, r0, r1, r2, r3 ) mat[0] = r0; mat[1] = r1; mat[2] = r2; mat[3] = r3;
    #define to_data_matrix(mat) mat
    // clip
    #define remap_z_clip_space( d ) (d = d * 0.5 + 0.5)
    #define remap_ndc_ray( r ) float2(r.x, r.y)  
    #define remap_depth( d ) (d)
    // defs
    #define ddx dfdx
    #define ddy dfdy
    #define discard discard_fragment
    #define lerp mix
    #define frac fract
    #define mod(x, y) (x - y * floor(x/y)) 
    #define _pmfx_unroll
#endif //METAL
//GENERIC MACROS
#define chebyshev_normalize( V ) (V.xyz / max( max(abs(V.x), abs(V.y)), abs(V.z) ))    
#define max3(v) max(max(v.x, v.y),v.z)
#define max4(v) max(max(max(v.x, v.y),v.z), v.w)


