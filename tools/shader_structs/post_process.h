namespace post_process
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
    struct src_info
    {
        float4 inv_texel_size[8];
    };
    struct filter_kernel
    {
        float4 filter_info;
        float4 filter_offset_weight[16];
    };
    struct pp_info
    {
        float4 frame_jitter;
    };
    struct taa_cbuffer
    {
        float4x4 frame_inv_view_projection;
        float4x4 prev_view_projection;
        float4 jitter;
    };
    struct high_pass
    {
        float m_threshold;
        float m_smoothness;
        float2 m_padding;
    };
    struct bloom_upsample
    {
        float m_intensity;
        float3 m_padding;
    };
    struct depth_of_field
    {
        float m_focus_centre;
        float m_centre_range;
        float m_focus_width;
        float m_width_range;
    };
}
