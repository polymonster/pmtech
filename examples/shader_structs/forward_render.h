struct forward_lit
{
	float4 m_albedo;
	float m_roughness;
	float m_reflectivity;
	float3 m_padding;
};

struct forward_lit_sdf_shadow
{
	float4 m_albedo;
	float m_roughness;
	float m_reflectivity;
	float m_surface_offset;
	float2 m_padding;
};

struct forward_lit_instanced
{
	float4 m_albedo;
	float m_roughness;
	float m_reflectivity;
	float3 m_padding;
};

struct forward_lit_skinned
{
	float4 m_albedo;
	float m_roughness;
	float m_reflectivity;
	float3 m_padding;
};

struct gbuffer
{
	float4 m_albedo;
	float m_roughness;
	float m_reflectivity;
	float3 m_padding;
};

struct gbuffer_skinned
{
	float4 m_albedo;
	float m_roughness;
	float m_reflectivity;
	float3 m_padding;
};

