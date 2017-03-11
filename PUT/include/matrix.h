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

/*=========================================================*\
|	Matrix4 - 4 x 4 Matrix - Row Major Format
\*=========================================================*/

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
	void gl_compliant_matrix(f32 *entries);

	f32 m[9];

} mat3;

typedef struct mat4
{
	f32 m[16];

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

	//static creation fucntions
	static mat4 identity();

	void multiply_by_scalar(f32 value);

	mat4 transpose();
	mat4 inverse3x3();
	mat4 inverse3x4();
	mat4 inverse4x4();
	f32	determinant4x4();

	void create_bias();

	void gl_compliant_matrix(f32 *entries);
	void gl_compliant_matrix(double *entries);
	vec3f get_translation();
	mat4 get_orientation();

	vec3f get_right( );
	vec3f get_up( );
	vec3f get_fwd( );

	void set_matrix_from_gl(f32 *entries);
	void set_matrix_from_gl(double *entries);
	void set_matrix_from_raw(f32 *entries);

	void multiply_with_gl_matrix();

	void set_vectors( vec3f right, vec3f up, vec3f at, vec3f pos );

	vec3f homogeneous_multiply(vec3f v, f32 *w);
	const vec3f operator *(const vec3f &p);
	mat4 operator *(mat4 &b);

} mat4;

#endif // _MATRIX_H 

