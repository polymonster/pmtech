struct high_pass
{
	float m_threshold;
	float4 m_padding;
};

struct depth_of_field
{
	float m_near_focus_start;
	float m_near_focus_end;
	float m_far_focus_start;
	float m_far_focus_end;
	float4x4 m_padding;
};

