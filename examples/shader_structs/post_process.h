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

