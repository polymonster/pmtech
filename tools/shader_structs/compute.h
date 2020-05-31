namespace compute
{
    struct per_pass
    {
        float4 output_buffer_dimension;
    };
    struct gi_info
    {
        float4 scene_size;
        float4 volume_size;
        float4 shadow_map_size;
        float4x4 inv_mat;
    };
}
