#define FORWARD_LIT_SKINNED 2147483648
#define FORWARD_LIT_INSTANCED 1073741824
#define FORWARD_LIT_UV_SCALE 2
#define FORWARD_LIT_SSS 4
#define FORWARD_LIT_SDF_SHADOW 8

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

struct forward_lit_sss
{
    float4 m_albedo;
    float m_roughness;
    float m_reflectivity;
    float m_sss_scale;
    float m_padding;
};

struct forward_lit_sss_skinned
{
    float4 m_albedo;
    float m_roughness;
    float m_reflectivity;
    float m_sss_scale;
    float m_padding;
};

struct forward_lit_sss_instanced
{
    float4 m_albedo;
    float m_roughness;
    float m_reflectivity;
    float m_sss_scale;
    float m_padding;
};

struct forward_lit_sss_instanced_skinned
{
    float4 m_albedo;
    float m_roughness;
    float m_reflectivity;
    float m_sss_scale;
    float m_padding;
};

struct forward_lit_sss_uv_scale
{
    float4 m_albedo;
    float2 m_uv_scale;
    float m_roughness;
    float m_reflectivity;
    float m_sss_scale;
    float3 m_padding;
};

struct forward_lit_sss_uv_scale_skinned
{
    float4 m_albedo;
    float2 m_uv_scale;
    float m_roughness;
    float m_reflectivity;
    float m_sss_scale;
    float3 m_padding;
};

struct forward_lit_sss_uv_scale_instanced
{
    float4 m_albedo;
    float2 m_uv_scale;
    float m_roughness;
    float m_reflectivity;
    float m_sss_scale;
    float3 m_padding;
};

struct forward_lit_sss_uv_scale_instanced_skinned
{
    float4 m_albedo;
    float2 m_uv_scale;
    float m_roughness;
    float m_reflectivity;
    float m_sss_scale;
    float3 m_padding;
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

struct forward_lit_sdf_shadow_sss
{
    float4 m_albedo;
    float m_roughness;
    float m_reflectivity;
    float m_sss_scale;
    float m_surface_offset;
};

struct forward_lit_sdf_shadow_sss_skinned
{
    float4 m_albedo;
    float m_roughness;
    float m_reflectivity;
    float m_sss_scale;
    float m_surface_offset;
};

struct forward_lit_sdf_shadow_sss_instanced
{
    float4 m_albedo;
    float m_roughness;
    float m_reflectivity;
    float m_sss_scale;
    float m_surface_offset;
};

struct forward_lit_sdf_shadow_sss_instanced_skinned
{
    float4 m_albedo;
    float m_roughness;
    float m_reflectivity;
    float m_sss_scale;
    float m_surface_offset;
};

struct forward_lit_sdf_shadow_sss_uv_scale
{
    float4 m_albedo;
    float2 m_uv_scale;
    float m_roughness;
    float m_reflectivity;
    float m_sss_scale;
    float m_surface_offset;
    float2 m_padding;
};

struct forward_lit_sdf_shadow_sss_uv_scale_skinned
{
    float4 m_albedo;
    float2 m_uv_scale;
    float m_roughness;
    float m_reflectivity;
    float m_sss_scale;
    float m_surface_offset;
    float2 m_padding;
};

struct forward_lit_sdf_shadow_sss_uv_scale_instanced
{
    float4 m_albedo;
    float2 m_uv_scale;
    float m_roughness;
    float m_reflectivity;
    float m_sss_scale;
    float m_surface_offset;
    float2 m_padding;
};

struct forward_lit_sdf_shadow_sss_uv_scale_instanced_skinned
{
    float4 m_albedo;
    float2 m_uv_scale;
    float m_roughness;
    float m_reflectivity;
    float m_sss_scale;
    float m_surface_offset;
    float2 m_padding;
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

