#ifndef _put_math_h
#define _put_math_h

#include <math.h>
#include <float.h>
#include "vector.h"
#include "matrix.h"
#include "bounding_volumes.h"

#define	PI				3.1415926535897932f
#define	PI_2			3.1415926535897932f * 2.0f
#define	_PI_OVER_180	0.0174532925199433f
#define _180_OVER_PI	57.295779513082322f

typedef enum
{
	INTERSECTS = 0,
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

		lmatrix.m[ 0 ] = 1.0f - 2.0f * y * y - 2.0f * z * z;
		lmatrix.m[ 1 ] = 2.0f * x * y - 2.0f * z * w;
		lmatrix.m[ 2 ] = 2.0f * x * z + 2.0f * y * w;
        lmatrix.m[ 3 ] = 0.0f;

		lmatrix.m[ 4 ] = 2.0f * x * y + 2.0f * z * w;
		lmatrix.m[ 5 ] = 1.0f - 2.0f * x * x - 2.0f * z * z;
		lmatrix.m[ 6 ] = 2.0f * y * z - 2.0f * x * w;
        lmatrix.m[ 7 ] = 0.0f;

		lmatrix.m[ 8 ] = 2.0f * x * z - 2.0f * y * w;
		lmatrix.m[ 9 ] = 2.0f * y * z + 2.0f * x * w;
		lmatrix.m[ 10 ] = 1.0f - 2.0f * x * x - 2.0f * y * y;
        lmatrix.m[ 11 ] = 0.0f;
        
        lmatrix.m[ 12 ] = 0.0f;
        lmatrix.m[ 13 ] = 0.0f;
        lmatrix.m[ 14 ] = 0.0f;
        lmatrix.m[ 15 ] = 1.0f;
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

	inline void from_matrix(mat4 m) 
	{
		w = sqrt(1.0 + m.m[0] + m.m[5] + m.m[10]) / 2.0;
		
		double w4 = (4.0 * w);
		x = (m.m[9] - m.m[6]) / w4;
		y = (m.m[2] - m.m[8]) / w4;
		z = (m.m[4] - m.m[1]) / w4;
	}

	inline vec3f to_euler(  )
	{
		vec3f euler;

		// roll (x-axis rotation)
		double sinr = +2.0 * (w * x + y * z);
		double cosr = +1.0 - 2.0 * (x * x + y * y);
		euler.x = atan2(sinr, cosr);

		// pitch (y-axis rotation)
		double sinp = +2.0 * (w * y - z * x );
		if (fabs(sinp) >= 1)
			euler.y = copysign(PI / 2, sinp); // use 90 degrees if out of range
		else
			euler.y = asin(sinp);

		// yaw (z-axis rotation)
		double siny = +2.0 * (w * z + x * y);
		double cosy = +1.0 - 2.0 * (y * y + z * z);
		euler.z = atan2(siny, cosy);

		return euler;
	}

	f32 x, y, z, w;
} Quaternion;

typedef Quaternion quat;

namespace put
{
	namespace maths
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
		vec3f unproject(vec3f scrren_space_pos, mat4 view, mat4 proj, vec2i viewport);
		vec3f unproject(vec3f screen_space_pos, mat4 view, mat4 proj, vec2i viewport);

		f32 cross(vec2f v1, vec2f v2);

		f32 dot(vec3f v1, vec3f v2);
		f32 dot2f(vec2f v1, vec2f v2);

		f32 magnitude(vec3f v);
		f32 magnitude(vec2f v);
		f32 distance(vec3f p1, vec3f p2);

		inline f32 plane_distance(vec3f normal, vec3f pos)
		{
			f32 distance = 0.0f;

			distance = dot(normal, pos) * -1.0f;

			return distance;
		}

		inline vec3f ray_vs_plane(vec3f vray, vec3f pray, vec3f nplane, vec3f pplane)
		{
			vec3f v = vray;
			v.normalise();

			vec3f p = pray;
			vec3f n = nplane;

			f32 d = plane_distance(n, pplane);
			f32 t = -(dot(p, n) + d) / dot(v, n);

			vec3f point_on_plane = p + (v * t);

			return point_on_plane;
		}

		inline f32 distance(const vec2f p1, const vec2f p2)
		{
			f32 d = sqrt((p2.x - p1.x) * (p2.x - p1.x) +
				(p2.y - p1.y) * (p2.y - p1.y));

			return d;
		}

		inline f32 distance_fast(const vec2f p1, const vec2f p2)
		{
			f32 d = ((p2.x - p1.x) * (p2.x - p1.x) +
				(p2.y - p1.y) * (p2.y - p1.y)) * 0.5f;

			return d;
		}

		inline f32 dot2f(vec2f v1, vec2f v2)
		{
			return v1.x * v2.x + v1.y * v2.y;
		}

		f32     distance_squared(vec2f p1, vec2f p2);

		f32     distance_on_line(vec3f l1, vec3f l2, vec3f p, bool clamp = true);
		vec3f   closest_point_on_line(vec3f l1, vec3f l2, vec3f p, bool clamp = true);
                
#if 0
		//todo - move the primitves over and port these functions from old engine.
		void get_axes_from_OBB(OBB3D b1, vec3f *axes);

		//polygons / triangles
		vec3f get_normal(TRIANGLE t1);
		vec3f get_normal(vec3f v1, vec3f v2, vec3f v3);

		bool point_inside_triangle(vec3f v1, vec3f v2, vec3f v3, vec3f p);

		void compute_tangents(vec3f v1, vec3f v2, vec3f v3, vec2f t1, vec2f t2, vec2f t3, vec3f *tangent, vec3f *bitangent, bool normalise);

		//planes
		s32 classify_sphere(SPHERE s1, vec3f p, vec3f normal, f32 *distance);
		s32 classify_sphere(SPHERE s1, PLANE p1);
		f32 plane_distance(vec3f normal, vec3f point);
		vec3f RAY_vs_PLANE(RAY_3D ray, PLANE plane);

		//closest point
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

		//point tests
		bool POINT_inside_TRIANGLE(TRIANGLE t1, vec3f p);
		bool POINT_inside_AA_ELLIPSOID(AA_ELLIPSOID e1, vec3f p);
		bool POINT_inside_SPHERE(SPHERE s1, vec3f p);
		bool POINT_inside_AABB3D(AABB3D b1, vec3f p);

		//ray / line tests tests
		bool SPHERE_vs_LINE();
		bool SPHERE_vs_RAY();
		bool AA_ELLIPSOID_vs_LINE();
		bool AA_ELLIPSOID_vs_RAY();

#endif
	}
};

#endif
