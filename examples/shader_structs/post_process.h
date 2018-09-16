struct high_pass
{
	float m_threshold;
	float4 m_padding;
};

struct depth_of_field
{
	float m_focus_centre;
	float m_centre_range;
	float m_focus_width;
	float m_width_range;
	float4x4 m_padding;
};

