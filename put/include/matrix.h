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
    static mat4 identity();
    
    void create_identity();
    void create_translation(vec3f t);
    void create_cardinal_rotation_deg(s32 axis, f32 theta);
    void create_cardinal_rotation(s32 axis, f32 theta);
    void create_arbitrary_rotation_deg(vec3f axis, f32 theta);
    void create_arbitrary_rotation(vec3f axis, f32 theta);
    void create_scale(vec3f s);
    void create_axis_swap(vec3f x, vec3f y, vec3f z);
    void create_perspective_projection(f32 left, f32 right, f32 bottom, f32 top, f32 znear, f32 zfar);
    void create_orthographic_projection(f32 left, f32 right, f32 bottom, f32 top, f32 znear, f32 zfar);
    void create_bias();
    
    mat4    transpose();
    mat4    inverse3x3();
    mat4    inverse3x4();
    mat4    inverse4x4();
    f32     determinant4x4();
    
    vec3f   get_translation();
    mat4    get_orientation();
    vec3f   get_right( );
    vec3f   get_up( );
    vec3f   get_fwd( );
    
    void set_vectors( vec3f right, vec3f up, vec3f at, vec3f pos );
    
    void set_matrix_from_raw(f32 *entries);
    
    //multiplication / transformation
    vec3f homogeneous_multiply(vec3f v, f32 *w);
    vec3f operator *(const vec3f &p);
    mat4 operator *(const mat4 &b);
    void multiply_by_scalar(f32 value);
    
} mat4;

#endif // _MATRIX_H 

