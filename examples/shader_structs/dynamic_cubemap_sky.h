namespace dynamic_cubemap_sky
{
    struct per_pass_view
    {
        float4x4 vp_matrix;
        float4x4 view_matrix;
        float4x4 vp_matrix_inverse;
        float4x4 view_matrix_inverse;
        float4 camera_view_pos;
        float4 camera_view_dir;
    };
}
