namespace trace
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
        float4 viewport_correction;
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
}
