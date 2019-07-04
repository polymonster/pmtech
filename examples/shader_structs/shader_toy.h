namespace shader_toy
{
    struct per_pass_view
    {
        float4x4 projection_matrix;
    };
    struct per_pass_params
    {
        float4 size_time;
        float4 test;
    };
}
