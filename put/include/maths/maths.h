#ifndef _maths_h
#define _maths_h

#include "types.h"
#include "util.h"
#include "vector.h"
#include "quat.h"
#include "matrix.h"

#define	M_PI_2			M_PI * 2.0
#define	M_PI_OVER_180	M_PI / 180.0
#define M_180_OVER_PI	180.0 / M_PI

namespace maths2
{
	typedef enum
	{
		INTERSECTS = 0,
		BEHIND = 1,
		INFRONT = 2,

	}e_classifications;

	// Collection of tests and useful maths functions, see inline implementation below or cpp file for comments

	// Angles
	f32     deg_to_rad(f32 degree_angle);
	f32     rad_to_deg(f32 radian_angle);
	vec3f   azimuth_altitude_to_xyz(f32 azimuth, f32 altitude);

	// Projection
	vec3f   project(vec3f v, mat4 view, mat4 proj, vec2i viewport = vec2i(0, 0), bool normalise_coordinates = false);
	vec3f   unproject(vec3f scrren_space_pos, mat4 view, mat4 proj, vec2i viewport);
	vec3f   unproject(vec3f screen_space_pos, mat4 view, mat4 proj, vec2i viewport);

	// Planes
	f32     plane_distance(const vec3f& x0, const vec3f& xN);
	f32		point_plane_distance(const vec3f& p0, const vec3f& x0, const vec3f& xN);


	vec3f   ray_plane_intersect(const vec3f r0, vec3f rV, vec3f x0, vec3f xN);

	u32		aabb_vs_plane(vec3f aabb_min, vec3f aabb_max, vec3f nplane, vec3f pplane);
	u32		sphere_vs_plane(vec3f sphere_cente, f32 radius, vec3f nplane, vec3f pplane);

	// Lines
	f32     distance_on_line(vec3f l1, vec3f l2, vec3f p, bool clamp = true);
	vec3f   closest_point_on_line(vec3f l1, vec3f l2, vec3f p, bool clamp = true);

	// Traingles
	vec3f   get_normal(vec3f v1, vec3f v2, vec3f v3);
	bool    point_inside_triangle(vec3f v1, vec3f v2, vec3f v3, vec3f p);
	vec3f	closest_point_on_triangle(vec3f v1, vec3f v2, vec3f v3, vec3f p, f32& side);

	// Aabb
	bool	point_inside_aabb(vec3f min, vec3f max, vec3f p);
	bool	ray_vs_aabb(const vec3f& min, const vec3f& max, const vec3f& r1, const vec3f& rv, vec3f& intersection);
	bool	ray_vs_obb(const vec3f& min, const vec3f& max, const mat4& mat, const vec3f& r1, const vec3f& rv, vec3f& intersection);

	// Inline functions ---------------------------------------------------------------------------------------------------

	inline f32 deg_to_rad(f32 degree_angle)
	{
		return(degree_angle * M_PI_OVER_180);
	}

	inline f32 rad_to_deg(f32 radian_angle)
	{
		return(radian_angle * M_180_OVER_PI);
	}

	// Convert azimuth / altitude to vec3f xyz
	inline vec3f azimuth_altitude_to_xyz(f32 azimuth, f32 altitude)
	{
		f32 z = sin(altitude);
		f32 hyp = cos(altitude);
		f32 y = hyp * cos(azimuth);
		f32 x = hyp * sin(azimuth);

		return vec3f(x, z, y);
	}

	// Get distance to plane x defined by point on plane x0 and normal of plane xN
	inline f32 plane_distance(const vec3f& x0, const vec3f& xN)
	{
		return dot(xN, x0) * -1.0f;
	}

	// Get distance from point p0 to plane defined by point x0 and normal xn 
	inline f32 point_plane_distance(const vec3f& p0, const vec3f& x0, const vec3f& xN)
	{
		f32 d = plane_distance(x0, xN);
		return dot(p0, xN) + d;
	}

	// Returns the intersection point of ray defined by origin r0 direction rV, 
	// with plane defined by point on plane x0 normal of plane xN
	inline vec3f ray_plane_intersect(const vec3f r0, vec3f rV, vec3f x0, vec3f xN)
	{
		vec3f rVn = normalised(rV);

		f32 d = plane_distance(x0, xN);
		f32 t = -(dot(r0, xN) + d) / dot(rV, xN);

		return r0 + (rV * t);
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

	inline bool point_inside_aabb(vec3f min, vec3f max, vec3f p)
	{
		if (p.x < min.x || p.x > max.x)
			return false;

		if (p.y < min.y || p.y > max.y)
			return false;

		if (p.z < min.z || p.z > max.z)
			return false;

		return true;
	}

	inline bool ray_vs_aabb(const vec3f& emin, const vec3f& emax, const vec3f& r1, const vec3f& rv, vec3f& intersection)
	{
		vec3f dirfrac = vec3f(1.0f) / rv;

		f32 t1 = (emin.x - r1.x)*dirfrac.x;
		f32 t2 = (emax.x - r1.x)*dirfrac.x;
		f32 t3 = (emin.y - r1.y)*dirfrac.y;
		f32 t4 = (emax.y - r1.y)*dirfrac.y;
		f32 t5 = (emin.z - r1.z)*dirfrac.z;
		f32 t6 = (emax.z - r1.z)*dirfrac.z;

		f32 tmin = max(max(min(t1, t2), min(t3, t4)), min(t5, t6));
		f32 tmax = min(min(max(t1, t2), max(t3, t4)), max(t5, t6));

		f32 t = 0.0f;

		// if tmax < 0, ray (line) is intersecting AABB, but the whole AABB is behind us
		if (tmax < 0)
		{
			t = tmax;
			return false;
		}

		// if tmin > tmax, ray doesn't intersect AABB
		if (tmin > tmax)
		{
			t = tmax;
			return false;
		}

		t = tmin;

		intersection = r1 + rv * t;

		return true;
	}

	inline bool ray_vs_obb(const vec3f& min, const vec3f& max, const mat4& mat, const vec3f& r1, const vec3f& rv, vec3f& intersection)
	{
		mat4 invm = mat;
		invm = invm.inverse4x4();

		vec3f tr1 = invm.transform_vector(r1);

		invm.set_translation(vec3f::zero());
		vec3f trv = invm.transform_vector(rv);

		return ray_vs_aabb(min, max, tr1, normalised(trv), intersection);
	}
}

#endif