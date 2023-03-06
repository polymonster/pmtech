namespace forward_render
{
    struct light_data
    {
        float4 pos_radius;
        float4 dir_cutoff;
        float4 colour;
        float4 data;
    };
    struct distance_field_shadow
    {
        float4x4 world_matrix;
        float4x4 world_matrix_inv;
    };
    struct area_light_data
    {
        float4 corners[4];
        float4 colour;
    };
    struct skinning_info
    {
        float4x4 bones[85];
    };
    struct per_pass_view
    {
        float4x4 vp_matrix;
        float4x4 view_matrix;
        float4x4 vp_matrix_inverse;
        float4x4 view_matrix_inverse;
        float4 camera_view_pos;
        float4 camera_view_dir;
    };
    struct per_draw_call
    {
        float4x4 world_matrix;
        float4 user_data;
        float4 user_data2;
        float4x4 world_matrix_inv_transpose;
    };
    struct per_pass_lights
    {
        float4 light_info;
        light_data lights[100];
    };
    struct per_pass_shadow
    {
        float4x4 shadow_matrix[100];
    };
    struct per_pass_shadow_distance_fields
    {
        distance_field_shadow sdf_shadow;
    };
    struct per_pass_area_lights
    {
        float4 area_light_info;
        area_light_data area_lights[10];
    };
    struct cbuffer_single_light
    {
        light_data single_light;
    };
    struct cbuffer_gi_volume
    {
        float4 gi_scene_size;
        float4 gi_volume_size;
    };
    #define OMNI_SHADOW_SKINNED 2147483648
    #define OMNI_SHADOW_INSTANCED 1073741824
    #define DBG_POS_SKINNED 2147483648
    #define DBG_POS_INSTANCED 1073741824
    #define FORWARD_LIT_SKINNED 2147483648
    #define FORWARD_LIT_INSTANCED 1073741824
    #define FORWARD_LIT_UV_SCALE 2
    #define FORWARD_LIT_SDF_SHADOW 8
    #define FORWARD_LIT_GI 16
    struct forward_lit
    {
        float4 m_albedo;
        float m_roughness;
        float m_reflectivity;
        float2 m_padding;
    };
    struct forward_lit_skinned
    {
        float4 m_albedo;
        float m_roughness;
        float m_reflectivity;
        float2 m_padding;
    };
    struct forward_lit_instanced
    {
        float4 m_albedo;
        float m_roughness;
        float m_reflectivity;
        float2 m_padding;
    };
    struct forward_lit_instanced_skinned
    {
        float4 m_albedo;
        float m_roughness;
        float m_reflectivity;
        float2 m_padding;
    };
    struct forward_lit_uv_scale
    {
        float4 m_albedo;
        float2 m_uv_scale;
        float m_roughness;
        float m_reflectivity;
    };
    struct forward_lit_uv_scale_skinned
    {
        float4 m_albedo;
        float2 m_uv_scale;
        float m_roughness;
        float m_reflectivity;
    };
    struct forward_lit_uv_scale_instanced
    {
        float4 m_albedo;
        float2 m_uv_scale;
        float m_roughness;
        float m_reflectivity;
    };
    struct forward_lit_uv_scale_instanced_skinned
    {
        float4 m_albedo;
        float2 m_uv_scale;
        float m_roughness;
        float m_reflectivity;
    };
    struct forward_lit_sdf_shadow
    {
        float4 m_albedo;
        float m_roughness;
        float m_reflectivity;
        float m_surface_offset;
        float m_padding;
    };
    struct forward_lit_sdf_shadow_skinned
    {
        float4 m_albedo;
        float m_roughness;
        float m_reflectivity;
        float m_surface_offset;
        float m_padding;
    };
    struct forward_lit_sdf_shadow_instanced
    {
        float4 m_albedo;
        float m_roughness;
        float m_reflectivity;
        float m_surface_offset;
        float m_padding;
    };
    struct forward_lit_sdf_shadow_instanced_skinned
    {
        float4 m_albedo;
        float m_roughness;
        float m_reflectivity;
        float m_surface_offset;
        float m_padding;
    };
    struct forward_lit_sdf_shadow_uv_scale
    {
        float4 m_albedo;
        float2 m_uv_scale;
        float m_roughness;
        float m_reflectivity;
        float m_surface_offset;
        float3 m_padding;
    };
    struct forward_lit_sdf_shadow_uv_scale_skinned
    {
        float4 m_albedo;
        float2 m_uv_scale;
        float m_roughness;
        float m_reflectivity;
        float m_surface_offset;
        float3 m_padding;
    };
    struct forward_lit_sdf_shadow_uv_scale_instanced
    {
        float4 m_albedo;
        float2 m_uv_scale;
        float m_roughness;
        float m_reflectivity;
        float m_surface_offset;
        float3 m_padding;
    };
    struct forward_lit_sdf_shadow_uv_scale_instanced_skinned
    {
        float4 m_albedo;
        float2 m_uv_scale;
        float m_roughness;
        float m_reflectivity;
        float m_surface_offset;
        float3 m_padding;
    };
    struct forward_lit_gi
    {
        float4 m_albedo;
        float m_roughness;
        float m_reflectivity;
        float2 m_padding;
    };
    struct forward_lit_gi_skinned
    {
        float4 m_albedo;
        float m_roughness;
        float m_reflectivity;
        float2 m_padding;
    };
    struct forward_lit_gi_instanced
    {
        float4 m_albedo;
        float m_roughness;
        float m_reflectivity;
        float2 m_padding;
    };
    struct forward_lit_gi_instanced_skinned
    {
        float4 m_albedo;
        float m_roughness;
        float m_reflectivity;
        float2 m_padding;
    };
    struct forward_lit_gi_uv_scale
    {
        float4 m_albedo;
        float2 m_uv_scale;
        float m_roughness;
        float m_reflectivity;
    };
    struct forward_lit_gi_uv_scale_skinned
    {
        float4 m_albedo;
        float2 m_uv_scale;
        float m_roughness;
        float m_reflectivity;
    };
    struct forward_lit_gi_uv_scale_instanced
    {
        float4 m_albedo;
        float2 m_uv_scale;
        float m_roughness;
        float m_reflectivity;
    };
    struct forward_lit_gi_uv_scale_instanced_skinned
    {
        float4 m_albedo;
        float2 m_uv_scale;
        float m_roughness;
        float m_reflectivity;
    };
    struct forward_lit_gi_sdf_shadow
    {
        float4 m_albedo;
        float m_roughness;
        float m_reflectivity;
        float m_surface_offset;
        float m_padding;
    };
    struct forward_lit_gi_sdf_shadow_skinned
    {
        float4 m_albedo;
        float m_roughness;
        float m_reflectivity;
        float m_surface_offset;
        float m_padding;
    };
    struct forward_lit_gi_sdf_shadow_instanced
    {
        float4 m_albedo;
        float m_roughness;
        float m_reflectivity;
        float m_surface_offset;
        float m_padding;
    };
    struct forward_lit_gi_sdf_shadow_instanced_skinned
    {
        float4 m_albedo;
        float m_roughness;
        float m_reflectivity;
        float m_surface_offset;
        float m_padding;
    };
    struct forward_lit_gi_sdf_shadow_uv_scale
    {
        float4 m_albedo;
        float2 m_uv_scale;
        float m_roughness;
        float m_reflectivity;
        float m_surface_offset;
        float3 m_padding;
    };
    struct forward_lit_gi_sdf_shadow_uv_scale_skinned
    {
        float4 m_albedo;
        float2 m_uv_scale;
        float m_roughness;
        float m_reflectivity;
        float m_surface_offset;
        float3 m_padding;
    };
    struct forward_lit_gi_sdf_shadow_uv_scale_instanced
    {
        float4 m_albedo;
        float2 m_uv_scale;
        float m_roughness;
        float m_reflectivity;
        float m_surface_offset;
        float3 m_padding;
    };
    struct forward_lit_gi_sdf_shadow_uv_scale_instanced_skinned
    {
        float4 m_albedo;
        float2 m_uv_scale;
        float m_roughness;
        float m_reflectivity;
        float m_surface_offset;
        float3 m_padding;
    };
    #define SIMPLE_LIGHTING_SKINNED 2147483648
    #define SIMPLE_LIGHTING_INSTANCED 1073741824
    #define SIMPLE_LIGHTING_SSS 4
    struct simple_lighting
    {
        float4 m_albedo;
        float m_roughness;
        float m_reflectivity;
        float2 m_padding;
    };
    struct simple_lighting_skinned
    {
        float4 m_albedo;
        float m_roughness;
        float m_reflectivity;
        float2 m_padding;
    };
    struct simple_lighting_instanced
    {
        float4 m_albedo;
        float m_roughness;
        float m_reflectivity;
        float2 m_padding;
    };
    struct simple_lighting_instanced_skinned
    {
        float4 m_albedo;
        float m_roughness;
        float m_reflectivity;
        float2 m_padding;
    };
    struct simple_lighting_sss
    {
        float4 m_albedo;
        float m_roughness;
        float m_reflectivity;
        float m_sss_scale;
        float m_padding;
    };
    struct simple_lighting_sss_skinned
    {
        float4 m_albedo;
        float m_roughness;
        float m_reflectivity;
        float m_sss_scale;
        float m_padding;
    };
    struct simple_lighting_sss_instanced
    {
        float4 m_albedo;
        float m_roughness;
        float m_reflectivity;
        float m_sss_scale;
        float m_padding;
    };
    struct simple_lighting_sss_instanced_skinned
    {
        float4 m_albedo;
        float m_roughness;
        float m_reflectivity;
        float m_sss_scale;
        float m_padding;
    };
    #define GBUFFER_SKINNED 2147483648
    #define GBUFFER_INSTANCED 1073741824
    #define GBUFFER_UV_SCALE 2
    struct gbuffer
    {
        float4 m_albedo;
        float m_roughness;
        float m_reflectivity;
        float2 m_padding;
    };
    struct gbuffer_skinned
    {
        float4 m_albedo;
        float m_roughness;
        float m_reflectivity;
        float2 m_padding;
    };
    struct gbuffer_instanced
    {
        float4 m_albedo;
        float m_roughness;
        float m_reflectivity;
        float2 m_padding;
    };
    struct gbuffer_instanced_skinned
    {
        float4 m_albedo;
        float m_roughness;
        float m_reflectivity;
        float2 m_padding;
    };
    struct gbuffer_uv_scale
    {
        float4 m_albedo;
        float2 m_uv_scale;
        float m_roughness;
        float m_reflectivity;
    };
    struct gbuffer_uv_scale_skinned
    {
        float4 m_albedo;
        float2 m_uv_scale;
        float m_roughness;
        float m_reflectivity;
    };
    struct gbuffer_uv_scale_instanced
    {
        float4 m_albedo;
        float2 m_uv_scale;
        float m_roughness;
        float m_reflectivity;
    };
    struct gbuffer_uv_scale_instanced_skinned
    {
        float4 m_albedo;
        float2 m_uv_scale;
        float m_roughness;
        float m_reflectivity;
    };
    #define ZONLY_SKINNED 2147483648
    #define ZONLY_INSTANCED 1073741824
    struct single_light_directional
    {
        float4 m_albedo;
        float m_roughness;
        float m_reflectivity;
        float2 m_padding;
    };
    #define LAMBERT_SKINNED 2147483648
    #define LAMBERT_INSTANCED 1073741824
    struct lambert
    {
        float4 m_albedo;
    };
    struct lambert_skinned
    {
        float4 m_albedo;
    };
    struct lambert_instanced
    {
        float4 m_albedo;
    };
    struct lambert_instanced_skinned
    {
        float4 m_albedo;
    };
    #define GI_SKINNED 2147483648
    #define GI_INSTANCED 1073741824
    struct gi
    {
        float4 m_albedo;
        float m_roughness;
        float m_reflectivity;
        float2 m_padding;
    };
    struct gi_skinned
    {
        float4 m_albedo;
        float m_roughness;
        float m_reflectivity;
        float2 m_padding;
    };
    struct gi_instanced
    {
        float4 m_albedo;
        float m_roughness;
        float m_reflectivity;
        float2 m_padding;
    };
    struct gi_instanced_skinned
    {
        float4 m_albedo;
        float m_roughness;
        float m_reflectivity;
        float2 m_padding;
    };
}
