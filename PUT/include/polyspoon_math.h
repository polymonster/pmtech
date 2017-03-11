/*=========================================================*\
|	polyspoon_math.h - main maths class
|-----------------------------------------------------------|
|				Project :	PolySpoon Math
|				Coder	:	ADixon
|				Date	:	26/04/09
|-----------------------------------------------------------|
|	Copyright (c) PolySpoon 2009. All rights reserved.		|
\*=========================================================*/

#ifndef _POLYSPOON_MATH_H
#define _POLYSPOON_MATH_H

/*======================== INCLUDES =======================*/

#include <math.h>
#include <float.h>
#include "vector.h"
#include "matrix.h"
#include "bounding_volumes.h"

/*======================== CONSTANTS ======================*/

#define	PI				3.1415926535897932f
#define	PI_2			3.1415926535897932f * 2.0f

#define	_PI_OVER_180	0.0174532925199433f
#define _180_OVER_PI	57.295779513082322f

typedef enum
{
	s32ERSECTS = 0,
	BEHIND = 1,
	INFRONT = 2,

}e_classifications;

typedef enum
{
	LEFT_HAND = 0,
	RIGHT_HAND = 1,

}e_handedness;


const vec3f unit_cube_vertices[8] =
{
	vec3f(1,1,1),
	vec3f(-1,-1,-1),
	vec3f(-1,1,-1),
	vec3f(1,-1,-1),
	vec3f(-1,-1,1),
	vec3f(1,1,-1),
	vec3f(1,-1,1),
	vec3f(-1,1,1)
};

typedef struct Quaternion
{
public:

	const Quaternion operator *( const f32 &scale) const
	{
		Quaternion out_quat; 
		out_quat.x = x * scale;
		out_quat.y = y * scale;
		out_quat.z = z * scale;
		out_quat.w = w * scale;

		return out_quat;
	}

	const Quaternion operator /(const f32 &scale) const
	{
		Quaternion out_quat;
		out_quat.x = x / scale;
		out_quat.y = y / scale;
		out_quat.z = z / scale;
		out_quat.w = w / scale;

		return out_quat;
	}

	const Quaternion operator +(const Quaternion &q) const
	{
		Quaternion out_quat;

		out_quat.x = x + q.x;
		out_quat.y = y + q.y;
		out_quat.z = z + q.w;
		out_quat.w = w + q.z;

		return out_quat;
	}

	const Quaternion operator =(const vec4f &v) const
	{
		Quaternion out_quat;

		out_quat.x = v.x;
		out_quat.y = v.y;
		out_quat.z = v.z;
		out_quat.w = v.w;

		return out_quat;
	}

	const Quaternion operator -() const
	{
		Quaternion out_quat;

		out_quat.x = -x;
		out_quat.y = -y;
		out_quat.z = -z;
		out_quat.w = -w;

		return out_quat;
	}


	inline void euler_angles( f32 z_theta, f32 y_theta, f32 x_theta )
	{
		f32 cos_z_2 = cosf( 0.5f * z_theta );
		f32 cos_y_2 = cosf( 0.5f * y_theta );
		f32 cos_x_2 = cosf( 0.5f * x_theta );

		f32 sin_z_2 = sinf( 0.5f * z_theta );
		f32 sin_y_2 = sinf( 0.5f * y_theta );
		f32 sin_x_2 = sinf( 0.5f * x_theta );

		// compute quaternion
		w = cos_z_2*cos_y_2*cos_x_2 + sin_z_2*sin_y_2*sin_x_2;
		x = cos_z_2*cos_y_2*sin_x_2 - sin_z_2*sin_y_2*cos_x_2;
		y = cos_z_2*sin_y_2*cos_x_2 + sin_z_2*cos_y_2*sin_x_2;
		z = sin_z_2*cos_y_2*cos_x_2 - cos_z_2*sin_y_2*sin_x_2;

		normalise();
	}

	inline void normalise( )
	{
		f32 mag = sqrt( w * w + x * x + y * y + z * z );

		w /= mag;
		x /= mag;
		y /= mag;
		z /= mag;
	}

	static inline f32 dot( const Quaternion &l, const Quaternion &r )
	{
		return l.x * r.x + l.y * r.y + l.z * r.z + l.w * r.w;
	}

	static inline Quaternion lerp( const Quaternion &l, const Quaternion &r, f32 t )
	{
		Quaternion lerped = ( l * ( 1.0f - t ) + r * t );
		lerped.normalise();

		return lerped;
	}

	static inline Quaternion slerp( const Quaternion &l, const Quaternion &r, f32 t )
	{
		Quaternion out_quat;

		f32 dotproduct = l.x * r.x + l.y * r.y + l.z * r.z + l.w * r.w;
		f32 theta, st, sut, sout, coeff1, coeff2;

		theta = ( float ) acosf( dotproduct );
		if (theta < 0.0) theta = -theta;

		st = ( f32 ) sinf( theta );
		sut = ( f32 ) sinf( t*theta );
		sout = ( f32 ) sinf( (1.0f - t)*theta );
		coeff1 = sout / st;
		coeff2 = sut / st;

		out_quat.x = coeff1 * l.x + coeff2 * r.x;
		out_quat.y = coeff1 * l.y + coeff2 * r.y;
		out_quat.z = coeff1 * l.z + coeff2 * r.z;
		out_quat.w = coeff1 * l.w + coeff2 * r.w;

		out_quat.normalise( );

		return out_quat;

// 		Quaternion out_quat;
// 		f32 dp = Quaternion::dot( l, r );
// 
// 		/*  dot = cos(theta)
// 			if (dot < 0), q1 and q2 are more than 90 degrees apart,
// 			so we can invert one to reduce spinning	*/
// 		if (dp < 0.0f)
// 		{
// 			dp = -dp;
// 			out_quat = -r;
// 		}
// 		else 
// 		{
// 			out_quat = r;
// 		}
// 		
// 		if (dp < 0.95f)
// 		{
// 			float angle = acosf(dp);
// 
// 			return ( l * sinf( angle * ( 1.0f - t ) ) + out_quat * sinf( angle * t ) ) / sinf( angle );
// 		} 
// 		else // if the angle is small, use linear interpolation
// 		{
// 			return lerp(l,out_quat,t);
// 		}
	}

	inline void axis_angle( vec3f axis, f32 w )
	{
		axis_angle( axis.x, axis.y, axis.z, w );
	}

	inline void axis_angle( f32 lx, f32 ly, f32 lz, f32 lw )
	{
		f32 half_angle = lw * 0.5f;

		w = cosf( half_angle );
		x = lx * sinf( half_angle );
		y = ly * sinf( half_angle );
		z = lz * sinf( half_angle );

		normalise();
	}

	inline void axis_angle( vec4f v )
	{
		axis_angle( v.x, v.y, v.z, v.w );
	}

	inline void get_matrix( mat4 &lmatrix )
	{
		normalise( );

		lmatrix.create_identity();

		/*
		1 - 2*qy2 - 2*qz2	2*qx*qy - 2*qz*qw	2*qx*qz + 2*qy*qw
		2*qx*qy + 2*qz*qw	1 - 2*qx2 - 2*qz2	2*qy*qz - 2*qx*qw
		2*qx*qz - 2*qy*qw	2*qy*qz + 2*qx*qw	1 - 2*qx2 - 2*qy2
		*/

		lmatrix.m[ 0 ] = 1.0f - 2.0f * y * y - 2.0f * z * z;
		lmatrix.m[ 1 ] = 2.0f * x * y - 2.0f * z * w;
		lmatrix.m[ 2 ] = 2.0f * x * z + 2.0f * y * w;

		lmatrix.m[ 4 ] = 2.0f * x * y + 2.0f * z * w;
		lmatrix.m[ 5 ] = 1.0f - 2.0f * x * x - 2.0f * z * z;
		lmatrix.m[ 6 ] = 2.0f * y * z - 2.0f * x * w;

		lmatrix.m[ 8 ] = 2.0f * x * z - 2.0f * y * w;
		lmatrix.m[ 9 ] = 2.0f * y * z + 2.0f * x * w;
		lmatrix.m[ 10 ] = 1.0f - 2.0f * x * x - 2.0f * y * y;

		//lmatrix.m[ 0  ] = w * w + x * x - y * y - z * z; lmatrix.m[ 1  ] = 2 * x * y - 2 * w * z; lmatrix.m[ 2  ] = 2 * x * z + 2 * w * y; 
		//lmatrix.m[ 4  ] = 2 * x * y + 2 * w * z; lmatrix.m[ 5  ] = w * w - x * x + y * y - z * z; lmatrix.m[ 6  ] = 2 * y * z + 2 * w * x;
		//lmatrix.m[ 8  ] = 2 * x * z - 2 * w * y; lmatrix.m[ 9  ] = 2 * y * z - 2 * w * x; lmatrix.m[ 10 ] = w * w - x * x - y * y + z * z;
	}

	//non commutative multiply
	inline Quaternion operator *( const Quaternion &rhs)
	{
		Quaternion res;

		res.w = w * rhs.w - x * rhs.x - y * rhs.y - z * rhs.z; 
		res.x = w * rhs.x + x * rhs.w + y * rhs.z - z * rhs.y; 
		res.y = w * rhs.y - x * rhs.z + y * rhs.w + z * rhs.z; 
		res.z = w * rhs.z + x * rhs.y - y * rhs.x + z * rhs.w; 

		return res;
	}

	f32 x, y, z, w;
} Quaternion;

typedef Quaternion quat;

/*========================= NAMESPACE =======================*/
namespace psmath
{
	//generic
	f32 absolute(f32 value);
	f32 absolute_smallest_of(f32 value_1, f32 value_2);

	//angles
	f32 deg_to_rad(f32 degree_angle);
	f32 rad_to_deg(f32 radian_angle);
	f32 angle_between_vectors(vec3f v1, vec3f v2);

	//vectors
	vec3f cross(vec3f v1, vec3f v2);
	vec2f perp(vec2f v1, s32 hand);
	vec3f normalise(vec3f v);
	vec2f normalise(vec2f v);
	vec3f project(vec3f v, mat4 view, mat4 proj, vec2i viewport, bool normalise_coordinates = false);
	vec3f unproject( vec3f scrren_space_pos, mat4 view, mat4 proj, vec2i viewport);
	vec3f unproject( vec3f screen_space_pos, mat4 view, mat4 proj, vec2i viewport);
		
	f32 cross( vec2f v1, vec2f v2 );
		
	f32 dot( vec3f v1, vec3f v2 );
	f32 dot2f(vec2f v1, vec2f v2);

	f32 magnitude(vec3f v);
	f32 magnitude(vec2f v);
	f32 distance(vec3f p1, vec3f p2);

	inline f32 plane_distance( vec3f normal, vec3f pos )
	{
		f32 distance = 0.0f;

		//Use the plane equation to find the distance D
		//negative dot product between normal vector and pos32 (p)
		distance = dot( normal, pos ) * -1.0f;

		return distance;
	}

	inline vec3f ray_vs_plane( vec3f vray, vec3f pray, vec3f nplane, vec3f pplane )
	{
		vec3f v = vray;
		v.normalise( );

		vec3f p = pray;
		vec3f n = nplane;

		f32 d = plane_distance( n, pplane );
		f32 t = -(dot( p, n ) + d) / dot( v, n );

		vec3f pos32_on_plane = p + (v * t);

		return pos32_on_plane;
	}

	inline f32 distance( const vec2f p1, const vec2f p2)
	{
		f32 d = sqrt( (p2.x - p1.x) * (p2.x - p1.x) + 
			(p2.y - p1.y) * (p2.y - p1.y));

		return d;
	}

	inline f32 distanceFast( const vec2f p1, const vec2f p2)
	{
		f32 d = ( (p2.x - p1.x) * (p2.x - p1.x) + 
					(p2.y - p1.y) * (p2.y - p1.y) ) * 0.5f;

		return d;
	}

	inline f32 dot2f( vec2f v1, vec2f v2 )
	{
		return v1.x * v2.x + v1.y * v2.y;
	}

	f32 distanceSq(vec2f p1, vec2f p2);

	f32    distance_on_line( vec3f l1, vec3f l2, vec3f p, bool clamp = true );
	vec3f closest_point_on_line( vec3f l1, vec3f l2, vec3f p, bool clamp = true );


#if 0
	void get_axes_from_OBB(OBB3D b1, vec3f *axes);
		
	//billboards
	void billboard_spherical_begin();
	void billboard_cylindrical_begin();
	void billboard_end();

	//polygons / triangles
	vec3f get_normal(TRIANGLE t1);
	vec3f get_normal(vec3f v1, vec3f v2, vec3f v3);

	bool pos32_inside_triangle(vec3f v1, vec3f v2, vec3f v3, vec3f p);

	void compute_tangents(vec3f v1, vec3f v2, vec3f v3, vec2f t1, vec2f t2, vec2f t3, vec3f *tangent,  vec3f *bitangent, bool normalise);

	//planes
	s32 classify_sphere(SPHERE s1, vec3f p, vec3f normal, f32 *distance);
	s32 classify_sphere(SPHERE s1, PLANE p1);
	f32 plane_distance(vec3f normal, vec3f pos32);
	vec3f RAY_vs_PLANE(RAY_3D ray, PLANE plane);

	//closest pos32
	vec3f closest_point_on_AABB3D(AABB3D b1, vec3f p);
	void find_extents(vec3f axis, vec3f *vertices, u32 vertex_count, f32 *min, f32 *max);
	void find_extents(vec3f axis, Vector3fArray vertices, f32 *min, f32 *max);
	void find_extents(vec3f axis, vec3f *vertices, u32 vertex_count, vec3f *min_position, vec3f *max_position);

	//bounding volume vs bounding volume tests
	bool SPHERE_vs_SPHERE(SPHERE *s1, SPHERE *s2);
	bool AA_ELLIPSOID_vs_SPHERE(AA_ELLIPSOID *e1, SPHERE *s1);
	bool AA_ELLIPSOID_vs_AA_ELLIPSOID(AA_ELLIPSOID *e1, AA_ELLIPSOID *e2);
	bool SPHERE_vs_TRIANGLE(SPHERE *s1, TRIANGLE *t1);
	bool AA_ELLIPSOID_vs_TRIANGLE(AA_ELLIPSOID *e1, TRIANGLE *t1);
	bool AABB3D_vs_AABB3D(AABB3D *b1, AABB3D *b2);
	bool AABB3D_vs_SPHERE(AABB3D *b1, SPHERE *s1);
	bool AABB3D_vs_AA_ELLIPSOID(AABB3D *b1, AA_ELLIPSOID *e1);
	bool OBB3D_vs_OBB3D(OBB3D *b1, OBB3D *b2);

	bool CONVEX_HULL_vs_CONVEX_HULL(CONVEX_HULL *h1, CONVEX_HULL *h2);

	//pos32 tests
	bool POs32_inside_TRIANGLE(TRIANGLE t1, vec3f p);
	bool POs32_inside_AA_ELLIPSOID(AA_ELLIPSOID e1, vec3f p);
	bool POs32_inside_SPHERE(SPHERE s1, vec3f p);
	bool POs32_inside_AABB3D(AABB3D b1, vec3f p);

	//ray / line tests tests
	bool SPHERE_vs_LINE();
	bool SPHERE_vs_RAY();
	bool AA_ELLIPSOID_vs_LINE();
	bool AA_ELLIPSOID_vs_RAY();

#endif
};


/*================== EXTERNAL VARIABLES ===================*/


#endif //_POLYSPOON_MATH_H