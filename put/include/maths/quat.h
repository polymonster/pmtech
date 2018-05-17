#ifndef _quat_h
#define _quat_h

#include "maths/mat.h"
#include "maths/vec.h"
#include <float.h>
#include <math.h>

struct Quaternion
{
    f32 x, y, z, w;

    // Constructor
    Quaternion()
    {
        x = 0.0f;
        y = 0.0f;
        z = 0.0f;
        w = 1.0f;
    };

    // Operators
    Quaternion operator*(const f32& scale) const;
    Quaternion operator/(const f32& scale) const;
    Quaternion operator+(const Quaternion& q) const;
    Quaternion operator=(const vec4f& v) const;
    Quaternion operator-() const;

    Quaternion operator*(const Quaternion& rhs) const;
    Quaternion& operator*=(const Quaternion& rhs);

    // Computation Functions
    void euler_angles(f32 z_theta, f32 y_theta, f32 x_theta);
    void normalise();
    f32 dot(const Quaternion& l, const Quaternion& r);

    Quaternion lerp(const Quaternion& l, const Quaternion& r, f32 t);
    Quaternion slerp(const Quaternion& l, const Quaternion& r, f32 t);

    void axis_angle(vec3f axis, f32 w);
    void axis_angle(f32 lx, f32 ly, f32 lz, f32 lw);
    void axis_angle(vec4f v);
    void get_matrix(mat4& lmatrix);
    void from_matrix(mat4 m);
    vec3f to_euler();
};

inline Quaternion Quaternion::operator*(const f32& scale) const
{
    Quaternion out_quat;
    out_quat.x = x * scale;
    out_quat.y = y * scale;
    out_quat.z = z * scale;
    out_quat.w = w * scale;

    return out_quat;
}

inline Quaternion Quaternion::operator/(const f32& scale) const
{
    Quaternion out_quat;
    out_quat.x = x / scale;
    out_quat.y = y / scale;
    out_quat.z = z / scale;
    out_quat.w = w / scale;

    return out_quat;
}

inline Quaternion Quaternion::operator+(const Quaternion& q) const
{
    Quaternion out_quat;

    out_quat.x = x + q.x;
    out_quat.y = y + q.y;
    out_quat.z = z + q.w;
    out_quat.w = w + q.z;

    return out_quat;
}

inline Quaternion Quaternion::operator=(const vec4f& v) const
{
    Quaternion out_quat;

    out_quat.x = v.x;
    out_quat.y = v.y;
    out_quat.z = v.z;
    out_quat.w = v.w;

    return out_quat;
}

inline Quaternion Quaternion::operator-() const // Unary minus
{
    Quaternion out_quat;

    out_quat.x = -x;
    out_quat.y = -y;
    out_quat.z = -z;
    out_quat.w = -w;

    return out_quat;
}

// non commutative multiply
inline Quaternion Quaternion::operator*(const Quaternion& rhs) const
{
    Quaternion res;

    res.w = w * rhs.w - x * rhs.x - y * rhs.y - z * rhs.z;
    res.x = w * rhs.x + x * rhs.w + y * rhs.z - z * rhs.y;
    res.y = w * rhs.y - x * rhs.z + y * rhs.w + z * rhs.z;
    res.z = w * rhs.z + x * rhs.y - y * rhs.x + z * rhs.w;

    return res;
}

inline Quaternion& Quaternion::operator*=(const Quaternion& rhs)
{
    Quaternion res;

    res.w = w * rhs.w - x * rhs.x - y * rhs.y - z * rhs.z;
    res.x = w * rhs.x + x * rhs.w + y * rhs.z - z * rhs.y;
    res.y = w * rhs.y - x * rhs.z + y * rhs.w + z * rhs.z;
    res.z = w * rhs.z + x * rhs.y - y * rhs.x + z * rhs.w;

    *this = res;
    return *this;
}

inline void Quaternion::euler_angles(f32 z_theta, f32 y_theta, f32 x_theta)
{
    f32 cos_z_2 = cosf(0.5f * z_theta);
    f32 cos_y_2 = cosf(0.5f * y_theta);
    f32 cos_x_2 = cosf(0.5f * x_theta);

    f32 sin_z_2 = sinf(0.5f * z_theta);
    f32 sin_y_2 = sinf(0.5f * y_theta);
    f32 sin_x_2 = sinf(0.5f * x_theta);

    // compute quaternion
    w = cos_z_2 * cos_y_2 * cos_x_2 + sin_z_2 * sin_y_2 * sin_x_2;
    x = cos_z_2 * cos_y_2 * sin_x_2 - sin_z_2 * sin_y_2 * cos_x_2;
    y = cos_z_2 * sin_y_2 * cos_x_2 + sin_z_2 * cos_y_2 * sin_x_2;
    z = sin_z_2 * cos_y_2 * cos_x_2 - cos_z_2 * sin_y_2 * sin_x_2;

    normalise();
}

inline void Quaternion::normalise()
{
    f32 mag = sqrt(w * w + x * x + y * y + z * z);

    w /= mag;
    x /= mag;
    y /= mag;
    z /= mag;
}

inline f32 Quaternion::dot(const Quaternion& l, const Quaternion& r)
{
    return l.x * r.x + l.y * r.y + l.z * r.z + l.w * r.w;
}

inline Quaternion Quaternion::lerp(const Quaternion& l, const Quaternion& r, f32 t)
{
    Quaternion lerped = (l * (1.0f - t) + r * t);
    lerped.normalise();

    return lerped;
}

inline Quaternion Quaternion::slerp(const Quaternion& l, const Quaternion& r, f32 t)
{
    Quaternion out_quat;

    f32 dotproduct = l.x * r.x + l.y * r.y + l.z * r.z + l.w * r.w;
    f32 theta, st, sut, sout, coeff1, coeff2;

    theta = (float)acosf(dotproduct);
    if (theta < 0.0)
        theta = -theta;

    st = (f32)sinf(theta);
    sut = (f32)sinf(t * theta);
    sout = (f32)sinf((1.0f - t) * theta);
    coeff1 = sout / st;
    coeff2 = sut / st;

    out_quat.x = coeff1 * l.x + coeff2 * r.x;
    out_quat.y = coeff1 * l.y + coeff2 * r.y;
    out_quat.z = coeff1 * l.z + coeff2 * r.z;
    out_quat.w = coeff1 * l.w + coeff2 * r.w;

    out_quat.normalise();

    return out_quat;
}

inline void Quaternion::axis_angle(vec3f axis, f32 w)
{
    axis_angle(axis.x, axis.y, axis.z, w);
}

inline void Quaternion::axis_angle(f32 lx, f32 ly, f32 lz, f32 lw)
{
    f32 half_angle = lw * 0.5f;

    w = cosf(half_angle);
    x = lx * sinf(half_angle);
    y = ly * sinf(half_angle);
    z = lz * sinf(half_angle);

    normalise();
}

inline void Quaternion::axis_angle(vec4f v)
{
    axis_angle(v.x, v.y, v.z, v.w);
}

inline void Quaternion::get_matrix(mat4& lmatrix)
{
    normalise();

    lmatrix.m[0] = 1.0f - 2.0f * y * y - 2.0f * z * z;
    lmatrix.m[1] = 2.0f * x * y - 2.0f * z * w;
    lmatrix.m[2] = 2.0f * x * z + 2.0f * y * w;
    lmatrix.m[3] = 0.0f;

    lmatrix.m[4] = 2.0f * x * y + 2.0f * z * w;
    lmatrix.m[5] = 1.0f - 2.0f * x * x - 2.0f * z * z;
    lmatrix.m[6] = 2.0f * y * z - 2.0f * x * w;
    lmatrix.m[7] = 0.0f;

    lmatrix.m[8] = 2.0f * x * z - 2.0f * y * w;
    lmatrix.m[9] = 2.0f * y * z + 2.0f * x * w;
    lmatrix.m[10] = 1.0f - 2.0f * x * x - 2.0f * y * y;
    lmatrix.m[11] = 0.0f;

    lmatrix.m[12] = 0.0f;
    lmatrix.m[13] = 0.0f;
    lmatrix.m[14] = 0.0f;
    lmatrix.m[15] = 1.0f;
}

inline void Quaternion::from_matrix(mat4 m)
{
    w = sqrt(1.0 + m.m[0] + m.m[5] + m.m[10]) / 2.0;

    double w4 = (4.0 * w);
    x = (m.m[9] - m.m[6]) / w4;
    y = (m.m[2] - m.m[8]) / w4;
    z = (m.m[4] - m.m[1]) / w4;
}

inline vec3f Quaternion::to_euler()
{
    vec3f euler;

    // roll (x-axis rotation)
    double sinr = +2.0 * (w * x + y * z);
    double cosr = +1.0 - 2.0 * (x * x + y * y);
    euler.x = atan2(sinr, cosr);

    // pitch (y-axis rotation)
    double sinp = +2.0 * (w * y - z * x);
    if (fabs(sinp) >= 1)
        euler.y = copysign(3.1415926535897932f / 2.0f, sinp); // use 90 degrees if out of range
    else
        euler.y = asin(sinp);

    // yaw (z-axis rotation)
    double siny = +2.0 * (w * z + x * y);
    double cosy = +1.0 - 2.0 * (y * y + z * z);
    euler.z = atan2(siny, cosy);

    return euler;
}

typedef Quaternion quat;

#endif //_quat_h
