#ifndef _put_math_h
#define _put_math_h

#include <math.h>
#include <float.h>
#include "vector.h"
#include "quat.h"
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

namespace put
{
	namespace maths
	{
		//functions decls----------------------------------------------------------------------------------------------------

		//generic
		f32     absolute(f32 value);
		f32     absolute_smallest_of(f32 value_1, f32 value_2);

		//angles
		f32     deg_to_rad(f32 degree_angle);
		f32     rad_to_deg(f32 radian_angle);
		f32     angle_between_vectors(vec3f v1, vec3f v2);
		vec3f   azimuth_altitude_to_xyz(f32 azimuth, f32 altitude);

		//vectors
		vec3f   cross(vec3f v1, vec3f v2);
		f32     cross(vec2f v1, vec2f v2);
		vec2f   perp_lh(vec2f v1);
		vec2f   perp_rh(vec2f v1);
		vec3f   normalise(vec3f v);
		vec2f   normalise(vec2f v);
		f32     dot(vec3f v1, vec3f v2);
		f32     dot2f(vec2f v1, vec2f v2);
		f32     magnitude(vec3f v);
		f32     magnitude(vec2f v);
		f32     distance(vec3f p1, vec3f p2);
		f32     distance_fast(const vec2f p1, const vec2f p2);
		f32     distance_squared(vec2f p1, vec2f p2);

		//projection
		vec3f   project(vec3f v, mat4 view, mat4 proj, vec2i viewport = vec2i(0, 0), bool normalise_coordinates = false);
		vec3f   unproject(vec3f scrren_space_pos, mat4 view, mat4 proj, vec2i viewport);
		vec3f   unproject(vec3f screen_space_pos, mat4 view, mat4 proj, vec2i viewport);

		//planes
		f32     plane_distance(vec3f normal, vec3f pos);
		f32     point_vs_plane(const vec3f& pos, const vec3f& p, const vec3f& normal);
		vec3f   ray_vs_plane(vec3f vray, vec3f pray, vec3f nplane, vec3f pplane);

		u32		aabb_vs_plane(vec3f aabb_min, vec3f aabb_max, vec3f nplane, vec3f pplane);
		u32		sphere_vs_plane(vec3f sphere_cente, f32 radius, vec3f nplane, vec3f pplane);

		//lines
		f32     distance_on_line(vec3f l1, vec3f l2, vec3f p, bool clamp = true);
		vec3f   closest_point_on_line(vec3f l1, vec3f l2, vec3f p, bool clamp = true);

		//traingles
		vec3f   get_normal(vec3f v1, vec3f v2, vec3f v3);
		bool    point_inside_triangle(vec3f v1, vec3f v2, vec3f v3, vec3f p);

		//inline functions---------------------------------------------------------------------------------------------------

		inline f32 deg_to_rad(f32 degree_angle)
		{
			return(degree_angle * _PI_OVER_180);
		}

		inline f32 rad_to_deg(f32 radian_angle)
		{
			return(radian_angle * _180_OVER_PI);
		}

		inline f32 absolute(f32 value)
		{
			if (value < 0.0f) value *= -1;

			return value;
		}

		inline f32 absolute_smallest_of(f32 value_1, f32 value_2)
		{
			if (absolute(value_1) < absolute(value_2)) return value_1;
			else return value_2;
		}

		inline vec3f cross(vec3f v1, vec3f v2)
		{
			vec3f result;

			result.x = ((v1.y * v2.z) - (v1.z * v2.y));
			result.y = ((v1.x * v2.z) - (v1.z * v2.x));
			result.z = ((v1.x * v2.y) - (v1.y * v2.x));

			result.y *= -1;

			return result;
		}

		inline f32 cross(vec2f v1, vec2f v2)
		{
			return v1.x * v2.y - v1.y * v2.x;
		}

		inline f32 dot(vec3f v1, vec3f v2)
		{
			return ((v1.x * v2.x) + (v1.y * v2.y) + (v1.z * v2.z));
		}

		inline vec2f perp_lh(vec2f v1)
		{
			return vec2f(v1.y, -v1.x);
		}

		inline vec2f perp_rh(vec2f v1)
		{
			return vec2f(-v1.y, v1.x);
		}

		inline f32 magnitude(vec3f v)
		{
			return sqrtf((v.x * v.x) + (v.y * v.y) + (v.z * v.z));
		}

		inline f32 magnitude(vec2f v)
		{
			return sqrtf((v.x * v.x) + (v.y * v.y));
		}

		inline f32 distance(vec3f p1, vec3f p2)
		{
			f32 d = sqrtf((p2.x - p1.x) * (p2.x - p1.x) +
				(p2.y - p1.y) * (p2.y - p1.y) +
				(p2.z - p1.z) * (p2.z - p1.z));

			return d;
		}

		inline f32 distance_squared(vec2f p1, vec2f p2)
		{
			f32 d = (p2.x - p1.x) * (p2.x - p1.x) + (p2.y - p1.y) * (p2.y - p1.y);
			return  d;
		}

		inline vec3f normalise(vec3f v)
		{
			f32 r_mag = 1.0f / magnitude(v);
			return v * r_mag;
		}

		inline vec2f normalise(vec2f v)
		{
			f32 r_mag = 1.0f / magnitude(v);
			return v * r_mag;
		}

		inline vec3f azimuth_altitude_to_xyz(f32 azimuth, f32 altitude)
		{
			f32 z = sin(altitude);
			f32 hyp = cos(altitude);
			f32 y = hyp * cos(azimuth);
			f32 x = hyp * sin(azimuth);

			return vec3f(x, z, y);
		}

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

		inline f32 point_vs_plane(const vec3f& pos, const vec3f& p, const vec3f& normal)
		{
			f32 d = plane_distance(normal, p);

			return  (normal.x * pos.x + normal.y * pos.y + normal.z * pos.z + d);
		}

		inline u32 aabb_vs_plane(vec3f aabb_min, vec3f aabb_max, vec3f pplane, vec3f nplane)
		{
			vec3f e = (aabb_max - aabb_min) / 2.0f;

			vec3f centre = aabb_min + e;

			f32 radius = abs(nplane.x*e.x) + abs(nplane.y*e.y) + abs(nplane.z*e.z);

			f32 pd = plane_distance(nplane, pplane);

			f32 d = dot(nplane, centre) + pd;

			if (d > radius)
				return INFRONT;

			if (d < -radius)
				return BEHIND;

			return INTERSECTS;
		}

		inline u32 sphere_vs_plane(vec3f sphere_cente, f32 radius, vec3f nplane, vec3f pplane)
		{
			f32 pd = plane_distance(nplane, pplane);

			f32 d = dot(nplane, sphere_cente) + pd;

			if (d > radius)
				return INFRONT;

			if (d < -radius)
				return BEHIND;

			return INTERSECTS;
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
	}
};

#endif

//todo - move the primitves over and port these functions from old engine.
//the code is knocking around somewhere in the pjected ifdefed out but might not compile :)
#if 0
void get_axes_from_OBB(OBB3D b1, vec3f *axes);

//polygons / triangles
vec3f get_normal(TRIANGLE t1);


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
