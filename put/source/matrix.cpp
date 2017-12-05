#include "matrix.h"
#include "put_math.h"

mat4 mat4::create_identity()
{
    mat4 m;
    
    m.m[ 0] = 1.0f; m.m[ 1] = 0.0f; m.m[ 2] = 0.0f; m.m[ 3] = 0.0f;
    m.m[ 4] = 0.0f; m.m[ 5] = 1.0f; m.m[ 6] = 0.0f; m.m[ 7] = 0.0f;
    m.m[ 8] = 0.0f; m.m[ 9] = 0.0f; m.m[10] = 1.0f; m.m[11] = 0.0f;
    m.m[12] = 0.0f; m.m[13] = 0.0f; m.m[14] = 0.0f; m.m[15] = 1.0f;
    
    return m;
}

mat4 mat4::operator *(const mat4 &b) const
{
    mat4 result;
    
    result.m[ 0] = m[ 0] * b.m[ 0] + m[ 1] * b.m[ 4] + m[ 2] * b.m[ 8] + m[ 3] * b.m[12];
    result.m[ 1] = m[ 0] * b.m[ 1] + m[ 1] * b.m[ 5] + m[ 2] * b.m[ 9] + m[ 3] * b.m[13];
    result.m[ 2] = m[ 0] * b.m[ 2] + m[ 1] * b.m[ 6] + m[ 2] * b.m[10] + m[ 3] * b.m[14];
    result.m[ 3] = m[ 0] * b.m[ 3] + m[ 1] * b.m[ 7] + m[ 2] * b.m[11] + m[ 3] * b.m[15];
    
    result.m[ 4] = m[ 4] * b.m[ 0] + m[ 5] * b.m[ 4] + m[ 6] * b.m[ 8] + m[ 7] * b.m[12];
    result.m[ 5] = m[ 4] * b.m[ 1] + m[ 5] * b.m[ 5] + m[ 6] * b.m[ 9] + m[ 7] * b.m[13];
    result.m[ 6] = m[ 4] * b.m[ 2] + m[ 5] * b.m[ 6] + m[ 6] * b.m[10] + m[ 7] * b.m[14];
    result.m[ 7] = m[ 4] * b.m[ 3] + m[ 5] * b.m[ 7] + m[ 6] * b.m[11] + m[ 7] * b.m[15];
    
    result.m[ 8] = m[ 8] * b.m[ 0] + m[ 9] * b.m[ 4] + m[10] * b.m[ 8] + m[11] * b.m[12];
    result.m[ 9] = m[ 8] * b.m[ 1] + m[ 9] * b.m[ 5] + m[10] * b.m[ 9] + m[11] * b.m[13];
    result.m[10] = m[ 8] * b.m[ 2] + m[ 9] * b.m[ 6] + m[10] * b.m[10] + m[11] * b.m[14];
    result.m[11] = m[ 8] * b.m[ 3] + m[ 9] * b.m[ 7] + m[10] * b.m[11] + m[11] * b.m[15];
    
    result.m[12] = m[12] * b.m[ 0] + m[13] * b.m[ 4] + m[14] * b.m[ 8] + m[15] * b.m[12];
    result.m[13] = m[12] * b.m[ 1] + m[13] * b.m[ 5] + m[14] * b.m[ 9] + m[15] * b.m[13];
    result.m[14] = m[12] * b.m[ 2] + m[13] * b.m[ 6] + m[14] * b.m[10] + m[15] * b.m[14];
    result.m[15] = m[12] * b.m[ 3] + m[13] * b.m[ 7] + m[14] * b.m[11] + m[15] * b.m[15];
    
    return result;
}

vec3f mat4::operator *(const vec3f &v) const
{
    vec3f result;
    
    result.x = v.x * m[ 0] + v.y * m[ 1] + v.z * m[ 2] + m[ 3];
    result.y = v.x * m[ 4] + v.y * m[ 5] + v.z * m[ 6] + m[ 7];
    result.z = v.x * m[ 8] + v.y * m[ 9] + v.z * m[10] + m[11];
    
    return result;
}

vec4f mat4::transform_vector(vec4f v) const
{
    vec4f result;
    
    result.x = v.x * m[ 0] + v.y * m[ 1] + v.z * m[ 2] + v.w * m[ 3];
    result.y = v.x * m[ 4] + v.y * m[ 5] + v.z * m[ 6] + v.w * m[ 7];
    result.z = v.x * m[ 8] + v.y * m[ 9] + v.z * m[10] + v.w * m[11];
    
    result.w = v.x * m[12] + v.y * m[13] + v.z * m[14] + v.w * m[15];
    
    return result;
}

vec3f mat4::transform_vector(vec3f v ) const
{
    vec3f result;
    
    result.x = v.x * m[ 0] + v.y * m[ 1] + v.z * m[ 2] + m[ 3];
    result.y = v.x * m[ 4] + v.y * m[ 5] + v.z * m[ 6] + m[ 7];
    result.z = v.x * m[ 8] + v.y * m[ 9] + v.z * m[10] + m[11];
    
    return result;
}

vec3f mat4::transform_vector(vec3f v, f32 *w) const
{
    vec3f result;
    
    result.x = v.x * m[ 0] + v.y * m[ 1] + v.z * m[ 2] + m[ 3];
    result.y = v.x * m[ 4] + v.y * m[ 5] + v.z * m[ 6] + m[ 7];
    result.z = v.x * m[ 8] + v.y * m[ 9] + v.z * m[10] + m[11];
    
    f32 w_result = v.x * m[12] + v.y * m[13] + v.z * m[14] + *w * m[15];
    
    *w = w_result;
    
    return result;
}

mat4 mat4::create_translation(vec3f t)
{
    mat4 m;
    
    m.m[ 0] = 1.0f; m.m[ 1] = 0.0f; m.m[ 2] = 0.0f; m.m[ 3] = t.x;
    m.m[ 4] = 0.0f; m.m[ 5] = 1.0f; m.m[ 6] = 0.0f; m.m[ 7] = t.y;
    m.m[ 8] = 0.0f; m.m[ 9] = 0.0f; m.m[10] = 1.0f; m.m[11] = t.z;
    m.m[12] = 0.0f; m.m[13] = 0.0f; m.m[14] = 0.0f; m.m[15] = 1.0f;
    
    return m;
}

mat4 mat4::create_x_rotation(f32 theta)
{
    mat4 m;
    
    //get sin / cos theta once
    f32 theta_rad = theta;
    f32 sin_theta = sin(theta_rad);
    f32 cos_theta = cos(theta_rad);
    
    m.m[ 0] = 1.0f;   m.m[ 1] = 0.0f;           m.m[ 2] = 0.0f;           m.m[ 3] = 0.0f;
    m.m[ 4] = 0.0f;   m.m[ 5] = cos_theta;      m.m[ 6] = -sin_theta;     m.m[ 7] = 0.0f;
    m.m[ 8] = 0.0f;   m.m[ 9] = sin_theta;      m.m[10] = cos_theta;      m.m[11] = 0.0f;
    m.m[12] = 0.0f;   m.m[13] = 0.0f;           m.m[14] = 0.0f;           m.m[15] = 1.0f;
    
    return m;
}

mat4 mat4::create_y_rotation(f32 theta)
{
    mat4 m;
    
    //get sin / cos theta once
    f32 theta_rad = theta;
    f32 sin_theta = sin(theta_rad);
    f32 cos_theta = cos(theta_rad);
    
    m.m[ 0] = cos_theta;   m.m[ 1] = 0.0f; m.m[ 2] = sin_theta;    m.m[ 3] = 0.0f;
    m.m[ 4] = 0.0f;        m.m[ 5] = 1.0f; m.m[ 6] = 0.0f;         m.m[ 7] = 0.0f;
    m.m[ 8] = -sin_theta;  m.m[ 9] = 0.0f; m.m[10] = cos_theta;    m.m[11] = 0.0f;
    m.m[12] = 0.0f;        m.m[13] = 0.0f; m.m[14] = 0.0f;         m.m[15] = 1.0f;
    
    return m;
}

mat4 mat4::create_z_rotation(f32 theta)
{
    mat4 m;
    
    //get sin / cos theta once
    f32 theta_rad = theta;
    f32 sin_theta = sin(theta_rad);
    f32 cos_theta = cos(theta_rad);
    
    m.m[ 0] = cos_theta;    m.m[ 1] = -sin_theta;   m.m[ 2] = 0.0f;   m.m[ 3] = 0.0f;
    m.m[ 4] = sin_theta;    m.m[ 5] = cos_theta;    m.m[ 6] = 0.0f;   m.m[ 7] = 0.0f;
    m.m[ 8] = 0.0f;         m.m[ 9] = 0.0f;         m.m[10] = 1.0f;   m.m[11] = 0.0f;
    m.m[12] = 0.0f;         m.m[13] = 0.0f;         m.m[14] = 0.0f;   m.m[15] = 1.0f;
    
    return m;
}

mat4 mat4::create_arbitrary_rotation( vec3f axis, f32 theta )
{
    mat4 m;
    
    f32 theta_rad = theta;
    f32 sin_theta = sin(theta_rad);
    f32 cos_theta = cos(theta_rad);
    f32 inv_cos_theta = 1.0f - cos(theta_rad);
    
    m.m[ 0] = inv_cos_theta * axis.x * axis.x + cos_theta;
    m.m[ 1] = inv_cos_theta * axis.x * axis.y - sin_theta * axis.z;
    m.m[ 2] = inv_cos_theta * axis.x * axis.z + sin_theta * axis.y;
    m.m[ 3] = 0.0f;
    
    m.m[ 4] = inv_cos_theta * axis.x * axis.y + sin_theta * axis.z;
    m.m[ 5] = inv_cos_theta * axis.y * axis.y + cos_theta;
    m.m[ 6] = inv_cos_theta * axis.y * axis.z - sin_theta * axis.x;
    m.m[ 7] = 0.0f;
    
    m.m[ 8] = inv_cos_theta * axis.x * axis.z - sin_theta * axis.y;
    m.m[ 9] = inv_cos_theta * axis.y * axis.z + sin_theta * axis.x;
    m.m[10] = inv_cos_theta * axis.z * axis.z + cos_theta;
    m.m[11] = 0.0f;
    
    m.m[12] = 0.0f; m.m[13] = 0.0f; m.m[14] = 0.0f; m.m[15] = 1.0f;
    
    return m;
}

mat4 mat4::create_scale(vec3f s)
{
    mat4 m;
    
    m.m[ 0] = s.x ; m.m[ 1] = 0.0f; m.m[ 2] = 0.0f; m.m[ 3] = 0.0f;
    m.m[ 4] = 0.0f; m.m[ 5] = s.y ; m.m[ 6] = 0.0f; m.m[ 7] = 0.0f;
    m.m[ 8] = 0.0f; m.m[ 9] = 0.0f; m.m[10] = s.z ; m.m[11] = 0.0f;
    m.m[12] = 0.0f; m.m[13] = 0.0f; m.m[14] = 0.0f; m.m[15] = 1.0f;
    
    return m;
}

mat4 mat4::create_bias()
{
    mat4 m;
    
    m.m[ 0] = 0.5f; m.m[ 1] = 0.0f; m.m[ 2] = 0.0f; m.m[ 3] = 0.5f;
    m.m[ 4] = 0.0f;	m.m[ 5] = 0.5f;	m.m[ 6] = 0.0f;	m.m[ 7] = 0.5f;
    m.m[ 8] = 0.0f;	m.m[ 9] = 0.0f;	m.m[10] = 0.5f;	m.m[11] = 0.5f;
    m.m[12] = 0.0f;	m.m[13] = 0.0f;	m.m[14] = 0.0f;	m.m[15] = 1.0f;
    
    return m;
}

mat4 mat4::create_axis_swap( vec3f x, vec3f y, vec3f z )
{
    mat4 m;
    
    m.m[ 0] = x.x;  m.m[ 1] = y.x;  m.m[ 2] = z.x;  m.m[ 3] = 0.0f;
    m.m[ 4] = x.y;  m.m[ 5] = y.y;  m.m[ 6] = z.y;	m.m[ 7] = 0.0f;
    m.m[ 8] = x.z;  m.m[ 9] = y.z;  m.m[10] = z.z;	m.m[11] = 0.0f;
    m.m[12] = 0.0f; m.m[13] = 0.0f; m.m[14] = 0.0f;	m.m[15] = 1.0f;
    
    return m;
}

mat4 mat4::create_perspective_projection(f32 left, f32 right, f32 bottom, f32 top, f32 znear, f32 zfar)
{
    mat4 m;
    
    m.m[ 0] = (2.0f * znear) / (right - left);
    m.m[ 1] = 0.0f;
    m.m[ 2] = (right + left) / (right - left);
    m.m[ 3] = 0.0f;
    m.m[ 4] = 0.0f;
    m.m[ 5] = (2.0f * znear) / (top - bottom);
    m.m[ 6] = (top + bottom) / (top - bottom);
    m.m[ 7] = 0.0f;
    m.m[ 8] = 0.0f;
    m.m[ 9] = 0.0f;
    m.m[10] = (-zfar - znear) / (zfar - znear);
    m.m[11] = (-(2.0f * znear) * zfar) / (zfar - znear);
    m.m[12] = 0.0f;
    m.m[13] = 0.0f;
    m.m[14] = -1.0f;
    m.m[15] = 0.0f;
    
    return m;
}

mat4 mat4::create_orthographic_projection( f32 left, f32 right, f32 bottom, f32 top, f32 znear, f32 zfar )
{
    mat4 m;
    
    f32 tx = -((right + left) / (right - left));
    f32 ty = -((top + bottom) / (top - bottom));
    f32 tz = -((zfar + znear) / (zfar - znear));
    
    m.m[ 0] = (2.0f / (right - left)); m.m[ 1] = 0.0f; m.m[ 2] = 0.0f; m.m[ 3] = tx;
    m.m[ 4] = 0.0f; m.m[ 5] = (2.0f / (top - bottom)); m.m[ 6] = 0.0f; m.m[ 7] = ty;
    m.m[ 8] = 0.0f; m.m[ 9] = 0.0f; m.m[10] = (-2.0f / (zfar - znear)); m.m[11] = tz;
    m.m[12] = 0.0f; m.m[13] = 0.0f; m.m[14] = 0.0f; m.m[15] = 1.0f;
    
    return m;
}

void mat4::multiply_by_scalar( f32 value )
{
    m[ 0] *= value; m[ 1] *= value; m[ 2] *= value; m[ 3] *= value;
    m[ 4] *= value; m[ 5] *= value; m[ 6] *= value; m[ 7] *= value;
    m[ 8] *= value; m[ 9] *= value; m[10] *= value; m[11] *= value;
    m[12] *= value; m[13] *= value; m[14] *= value; m[15] *= value;
}

void mat4::set_matrix_from_raw(f32 *entries)
{
    m[ 0] = entries[0]; m[ 4] = entries[4]; m[ 8] = entries[8];  m[12] = entries[12];
    m[ 1] = entries[1]; m[ 5] = entries[5]; m[ 9] = entries[9];  m[13] = entries[13];
    m[ 2] = entries[2]; m[ 6] = entries[6]; m[10] = entries[10]; m[14] = entries[14];
    m[ 3] = entries[3]; m[ 7] = entries[7]; m[11] = entries[11]; m[15] = entries[15];
}

vec3f mat4::get_translation() const
{
    return vec3f(m[ 3],m[ 7],m[11]);
}

mat4 mat4::get_orientation() const
{
    mat4 om = *this;
    
    // zero pos
    om.m[ 3] = 0.f; om.m[ 7] = 0.f; om.m[11] = 0.f;
    
    return om;
}

mat4 mat4::inverse3x3()
{
    //determinant
    f32 det =
    (m[ 0] * (m[ 5] * m[10] - m[ 9] * m[ 6])) -
    (m[ 4] * (m[ 1] * m[10] - m[ 9] * m[ 2])) +
    (m[ 8] * (m[ 1] * m[ 6] - m[ 5] * m[ 2]));
    
    f32 one_over_det = 1.0f / det;
    
    mat4 inverse;
    inverse.create_identity();
    
    //find the adjoint matrix (transposed) and multiply by 1/det to get the inverse
    inverse.m[ 0] =  (m[ 5] * m[10] - m[ 6] * m[ 9]) * one_over_det;
    inverse.m[ 1] = -(m[ 1] * m[10] - m[ 2] * m[ 9]) * one_over_det;
    inverse.m[ 2] =  (m[ 1] * m[ 6] - m[ 2] * m[ 5]) * one_over_det;
    
    inverse.m[ 4] = -(m[ 4] * m[10] - m[ 6] * m[ 8]) * one_over_det;
    inverse.m[ 5] =  (m[ 0] * m[10] - m[ 2] * m[ 8]) * one_over_det;
    inverse.m[ 6] = -(m[ 0] * m[ 6] - m[ 2] * m[ 4]) * one_over_det;
    
    inverse.m[ 8] =  (m[ 4] * m[ 9] - m[ 5] * m[ 8]) * one_over_det;
    inverse.m[ 9] = -(m[ 0] * m[ 9] - m[ 1] * m[ 8]) * one_over_det;
    inverse.m[10] =  (m[ 0] * m[ 5] - m[ 1] * m[ 4]) * one_over_det;
    
    return inverse;
}

mat4 mat4::inverse3x4()
{
    //determinant
    f32 det =
    (m[ 0] * (m[ 5] * m[10] - m[ 9] * m[ 6])) -
    (m[ 4] * (m[ 1] * m[10] - m[ 9] * m[ 2])) +
    (m[ 8] * (m[ 1] * m[ 6] - m[ 5] * m[ 2]));
    
    f32 one_over_det = 1.0f / det;
    
    mat4 inverse = mat4::create_identity();
    
    //find the adjoint matrix (transposed) and multiply by 1/det to get the inverse
    inverse.m[ 0] =  (m[ 5] * m[10] - m[ 6] * m[ 9]) * one_over_det;
    inverse.m[ 1] = -(m[ 1] * m[10] - m[ 2] * m[ 9]) * one_over_det;
    inverse.m[ 2] =  (m[ 1] * m[ 6] - m[ 2] * m[ 5]) * one_over_det;
    
    inverse.m[ 4] = -(m[ 4] * m[10] - m[ 6] * m[ 8]) * one_over_det;
    inverse.m[ 5] =  (m[ 0] * m[10] - m[ 2] * m[ 8]) * one_over_det;
    inverse.m[ 6] = -(m[ 0] * m[ 6] - m[ 2] * m[ 4]) * one_over_det;
    
    inverse.m[ 8] =  (m[ 4] * m[ 9] - m[ 5] * m[ 8]) * one_over_det;
    inverse.m[ 9] = -(m[ 0] * m[ 9] - m[ 1] * m[ 8]) * one_over_det;
    inverse.m[10] =  (m[ 0] * m[ 5] - m[ 1] * m[ 4]) * one_over_det;
    
    //take into account inverse the translation portion (inverse translation portion * inverse rotation)
    vec3f t(-m[ 3],-m[ 7],-m[11]);
    inverse.m[ 3] = t.x * inverse.m[ 0] + t.y * inverse.m[ 1] + t.z * inverse.m[ 2];
    inverse.m[ 7] = t.x * inverse.m[ 4] + t.y * inverse.m[ 5] + t.z * inverse.m[ 6];
    inverse.m[11] = t.x * inverse.m[ 8] + t.y * inverse.m[ 9] + t.z * inverse.m[10];
    
    return inverse;
}

mat4 mat4::transpose()
{
    mat4 t;
    
    t.m[ 0] = m[ 0]; t.m[ 1] = m[ 4]; t.m[ 2] = m[ 8]; t.m[ 3] = m[12];
    t.m[ 4] = m[ 1]; t.m[ 5] = m[ 5]; t.m[ 6] = m[ 9]; t.m[ 7] = m[13];
    t.m[ 8] = m[ 2]; t.m[ 9] = m[ 6]; t.m[10] = m[10]; t.m[11] = m[14];
    t.m[12] = m[ 3]; t.m[13] = m[ 7]; t.m[14] = m[11]; t.m[15] = m[15];
    
    return t;
}

mat4 mat4::inverse4x4()
{
    //laplace expansion theorum
    
    //some test data
    /*m[ 0] = 5.6f; m[ 1] = 7.0f;  m[ 2] = 9.0f; m[ 3] = -2.4f;
     m[ 4] = 1.884f; m[ 5] = -9.0f;  m[ 6] = 0.667f;  m[ 7] = 0.0f;
     m[ 8] = 2.0f; m[ 9] = 4.0f;  m[10] = -0.531f;  m[11] =  3.0f;
     m[12] = 0.0f; m[13] = 0.0f;  m[14] = 2.0f;  m[15] = 1.0f;*/
    
    //calculate determinants of sub 2x2 matrices
    f32 s0 = ((m[0] * m[5]) - (m[1] * m[4]));
    f32 s1 = ((m[0] * m[6]) - (m[2] * m[4]));
    f32 s2 = ((m[0] * m[7]) - (m[3] * m[4]));
    f32 s3 = ((m[1] * m[6]) - (m[2] * m[5]));
    f32 s4 = ((m[1] * m[7]) - (m[3] * m[5]));
    f32 s5 = ((m[2] * m[7]) - (m[3] * m[6]));
    
    f32 c5 = ((m[10] * m[15]) - (m[11] * m[14]));
    f32 c4 = ((m[9] * m[15]) - (m[11] * m[13]));
    f32 c3 = ((m[9] * m[14]) - (m[10] * m[13]));
    f32 c2 = ((m[8] * m[15]) - (m[11] * m[12]));
    f32 c1 = ((m[8] * m[14]) - (m[10] * m[12]));
    f32 c0 = ((m[8] * m[13]) - (m[9] * m[12]));
    
    f32 det = (s0 * c5) - (s1 * c4) + (s2 * c3) + (s3 * c2) - (s4 * c1) + (s5 * c0);
    
    f32 one_over_det = 1.0f / det;
    
    mat4 inverse;
    
    inverse.m[ 0] = + (m[ 5] * c5 - m[ 6] * c4 + m[ 7] * c3) * one_over_det;
    inverse.m[ 1] = - (m[ 1] * c5 - m[ 2] * c4 + m[ 3] * c3) * one_over_det;
    inverse.m[ 2] = + (m[13] * s5 - m[14] * s4 + m[15] * s3) * one_over_det;
    inverse.m[ 3] = - (m[ 9] * s5 - m[10] * s4 + m[11] * s3) * one_over_det;
    
    inverse.m[ 4] = - (m[ 4] * c5 - m[ 6] * c2 + m[ 7] * c1) * one_over_det;
    inverse.m[ 5] = + (m[ 0] * c5 - m[ 2] * c2 + m[ 3] * c1) * one_over_det;
    inverse.m[ 6] = - (m[12] * s5 - m[14] * s2 + m[15] * s1) * one_over_det;
    inverse.m[ 7] = + (m[ 8] * s5 - m[10] * s2 + m[11] * s1) * one_over_det;
    
    inverse.m[ 8] = + (m[ 4] * c4 - m[ 5] * c2 + m[ 7] * c0) * one_over_det;
    inverse.m[ 9] = - (m[ 0] * c4 - m[ 1] * c2 + m[ 3] * c0) * one_over_det;
    inverse.m[10] = + (m[12] * s4 - m[13] * s2 + m[15] * s0) * one_over_det;
    inverse.m[11] = - (m[ 8] * s4 - m[ 9] * s2 + m[11] * s0) * one_over_det;
    
    inverse.m[12] = - (m[ 4] * c3 - m[ 5] * c1 + m[ 6] * c0) * one_over_det;
    inverse.m[13] = + (m[ 0] * c3 - m[ 1] * c1 + m[ 2] * c0) * one_over_det;
    inverse.m[14] = - (m[12] * s3 - m[13] * s1 + m[14] * s0) * one_over_det;
    inverse.m[15] = + (m[ 8] * s3 - m[ 9] * s1 + m[10] * s0) * one_over_det;
    
    return inverse;
}

f32 mat4::determinant4x4()
{
    //calculate the 4x4 determinant
    //4 3x3 determinants
    f32 sub_det0 = + ( m[5] * ( (m[10] * m[15]) - (m[11] * m[14]) ) )
    - ( m[6] * ( (m[ 9] * m[15]) - (m[11] * m[13]) ) )
    + ( m[7] * ( (m[ 9] * m[14]) - (m[10] * m[13]) ) );
    
    f32 sub_det1 = + ( m[4] * ( (m[10] * m[15]) - (m[11] * m[14]) ) )
    - ( m[6] * ( (m[ 8] * m[15]) - (m[11] * m[12]) ) )
    + ( m[7] * ( (m[ 8] * m[14]) - (m[10] * m[12]) ) );
    
    f32 sub_det2 = + ( m[4] * ( (m[ 9] * m[15]) - (m[11] * m[13]) ) )
    - ( m[5] * ( (m[ 8] * m[15]) - (m[11] * m[12]) ) )
    + ( m[7] * ( (m[ 8] * m[13]) - (m[ 9] * m[12]) ) );
    
    f32 sub_det3 = + ( m[4] * ( (m[ 9] * m[14]) - (m[10] * m[13]) ) )
    - ( m[5] * ( (m[ 8] * m[14]) - (m[10] * m[12]) ) )
    + ( m[6] * ( (m[ 8] * m[13]) - (m[ 9] * m[12]) ) );
    
    //add the expanded 3x3 determinant
    return m[0] * sub_det0 - m[1] * sub_det1 + m[2] * sub_det2 - m[3] * sub_det3;
}

void mat4::set_vectors( vec3f right, vec3f up, vec3f at, vec3f pos )
{
    m[ 0] = right.x; m[ 1] = right.y; m[ 2] = right.z; m[ 3] = pos.x;
    m[ 4] = up.x; m[ 5] = up.y; m[ 6] = up.z; m[ 7] = pos.y;
    m[ 8] = at.x; m[ 9] = at.y; m[10] = at.z; m[11] = pos.z;
    m[12] = 0.0f; m[13] = 0.0f; m[14] = 0.0f;    m[15] = 1.0f;
}

vec3f mat4::get_right( ) const
{
    return vec3f( m[ 0 ], m[ 1 ], m[ 2 ] );
}

vec3f mat4::get_up( ) const
{
    return vec3f( m[ 4 ], m[ 5 ], m[ 6 ] );
}

vec3f mat4::get_fwd( ) const
{
    return vec3f( m[ 8 ], m[ 9 ], m[ 10 ] );
}

mat3::mat3( mat4 extract )
{
    m[ 0] = extract.m[ 0]; m[ 1] = extract.m[ 1]; m[ 2] = extract.m[ 2];
    m[ 3] = extract.m[ 4]; m[ 4] = extract.m[ 5]; m[ 5] = extract.m[ 6];
    m[ 6] = extract.m[ 8]; m[ 7] = extract.m[ 6]; m[ 8] = extract.m[10];
}
