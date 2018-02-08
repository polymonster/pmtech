#ifndef _MATRIX_H
#define _MATRIX_H

#include <math.h>
#include "vector.h"

typedef enum
{
    X_AXIS = 0,
    Y_AXIS = 1,
    Z_AXIS = 2,
    
}e_cardinal_axes;

struct mat4;
struct mat3;

//Matrix 4 - Row Major

/*
 m[ 0] m[ 1] m[ 2] m[ 3]
 m[ 4] m[ 5] m[ 6] m[ 7]
 m[ 8] m[ 9] m[10] m[11]
 m[12] m[13] m[14] m[15]
 */

typedef struct mat3
{
    mat3(mat4 extract);
    mat3(){};
    
    f32 m[9];
    
} mat3;

typedef struct mat4
{
    f32 m[16];
    
    //static identity fucntions
    static mat4 create_identity();
    static mat4 create_translation(vec3f t);
    static mat4 create_x_rotation(f32 theta);
    static mat4 create_y_rotation(f32 theta);
    static mat4 create_z_rotation(f32 theta);
    static mat4 create_arbitrary_rotation(vec3f axis, f32 theta);
    static mat4 create_scale(vec3f s);
    static mat4 create_axis_swap(vec3f x, vec3f y, vec3f z);
    static mat4 create_perspective_projection(f32 left, f32 right, f32 bottom, f32 top, f32 znear, f32 zfar);
    static mat4 create_orthographic_projection(f32 left, f32 right, f32 bottom, f32 top, f32 znear, f32 zfar);
    static mat4 create_orthographic_off_centre_projection(f32 left, f32 right, f32 bottom, f32 top, f32 znear, f32 zfar);
    static mat4 create_bias();
    
    mat4    transpose();
    mat4    inverse3x3();
    mat4    inverse3x4();
    mat4    inverse4x4();
    f32     determinant4x4();
    
    vec3f   get_translation() const;
    mat4    get_orientation() const;
    vec3f   get_right( ) const;
    vec3f   get_up( ) const;
    vec3f   get_fwd( ) const;
    
    void set_vectors( vec3f right, vec3f up, vec3f at, vec3f pos );
    void set_translation( vec3f pos );
    
    void set_matrix_from_raw(f32 *entries);
    
    //multiplication / transformation
    vec4f transform_vector(vec4f v ) const;
    vec3f transform_vector(vec3f v, f32 *w) const;
    vec3f transform_vector(vec3f v ) const;
    
    vec3f operator *(const vec3f &p) const;
    mat4 operator *(const mat4 &b) const;
	
	mat4& operator *=(const mat4 &b);

    void multiply_by_scalar(f32 value);
    
} mat4;

#endif // _MATRIX_H

