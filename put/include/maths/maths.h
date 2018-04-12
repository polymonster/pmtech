#ifndef _maths_h
#define _maths_h

#include "types.h"
#include "util.h"
#include "vector.h"
#include "quat.h"
#include "matrix.h"

constexpr double M_PI_2 = 3.1415926535897932384626433832795 * 2.0;
constexpr double M_PI_OVER_180 = 3.1415926535897932384626433832795 * 180.0;
constexpr double M_180_OVER_PI = 180.0 / 3.1415926535897932384626433832795;

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
    vec3f   project_to_ndc(const vec3f& p, const mat4& view_projection);
    vec3f   project_to_sc(const vec3f& p, const mat4& view_projection, const vec2i& viewport);
	vec3f	unproject_ndc(const vec3f& p, const mat4& view_projection);
	vec3f	unproject_sc(const vec3f& p, const mat4& view_projection, const vec2i& viewport);

	// Plane
	f32     plane_distance(const vec3f& x0, const vec3f& xN);
	f32		point_plane_distance(const vec3f& p0, const vec3f& x0, const vec3f& xN);
	vec3f   ray_plane_intersect(const vec3f& r0, const vec3f& rV, const vec3f& x0, const vec3f& xN);
    u32     aabb_vs_plane(const vec3f& aabb_min, const vec3f& aabb_max, const vec3f& x0, const vec3f& xN);
    u32		sphere_vs_plane(const vec3f& s, f32 r, const vec3f& x0, const vec3f& xN);
    
    // Sphere
    bool    sphere_vs_sphere(const vec3f& s0, f32 r0, const vec3f& s1, f32 r1);
    bool    sphere_vs_aabb(const vec3f& s0, f32 r0, const vec3f& aabb_min, const vec3f& aabb_max);
    
	// Line Segment
    float   point_segment_distance(const vec3f &x0, const vec3f &x1, const vec3f &x2);
    float   point_triangle_distance(const vec3f &x0, const vec3f &x1, const vec3f &x2, const vec3f &x3);
	bool	line_vs_ray(const vec2f& l1, const vec2f& l2, const vec2f& s1, const vec2f& s2, vec3f& ip);
	bool	line_vs_line(const vec2f& l1, const vec2f& l2, const vec2f& s1, const vec2f& s2, vec3f& ip);

    vec3f   closest_point_on_aabb(const vec3f& s0, const vec3f& aabb_min, const vec3f& aabb_max);
    vec3f   closest_point_on_line(const vec3f& l1, const vec3f& l2, const vec3f& p);
	f32		distance_on_line(const vec3f& l1, const vec3f& l2, const vec3f& p);
    vec3f   closest_point_on_ray(const vec3f& r0, const vec3f& rV, const vec3f& p);

	// Traingle
	vec3f   get_normal(const vec3f& v1, const vec3f& v2, const vec3f& v3);
    bool    point_inside_triangle(const vec3f& p, const vec3f& v1, const vec3f& v2, const vec3f& v3);
	vec3f	closest_point_on_triangle(const vec3f& p, const vec3f& v1, const vec3f& v2, const vec3f& v3, f32& side);

	// AABB
	bool	point_inside_aabb(const vec3f& min, const vec3f& max, const vec3f& p0);
	bool	ray_vs_aabb(const vec3f& min, const vec3f& max, const vec3f& r1, const vec3f& rv, vec3f& ip); 
	bool	ray_vs_obb(const mat4& mat, const vec3f& r1, const vec3f& rv, vec3f& ip);

	// Inline functions ---------------------------------------------------------------------------------------------------

	inline f32 deg_to_rad(f32 degree_angle)
	{
		return(degree_angle * M_PI_OVER_180);
	}

	inline f32 rad_to_deg(f32 radian_angle)
	{
		return(radian_angle * M_180_OVER_PI);
	}
    
    // Project point p by view_projection to normalised device coordinates, perfroming homogenous divide
    inline vec3f project_to_ndc(const vec3f& p, const mat4& view_projection)
    {
        vec4f ndc = view_projection.transform_vector(vec4f(p, 1.0f));
        ndc /= ndc.w;
        return ndc.xyz;
    }
    
    // Project point p to screen coordinates of viewport
    inline vec3f project_to_sc(const vec3f& p, const mat4& view_projection, const vec2i& viewport)
    {
        vec3f ndc = project_to_ndc(p, view_projection);
        vec3f sc = ndc * 0.5f + 0.5f;
        sc.xy *= vec2f(viewport.x, viewport.y);
        return sc;
    }

	// Unproject normalised device coordinate p wih viewport using iverse view_projection
	inline vec3f unproject_ndc(const vec3f& p, const mat4& view_projection)
	{
		mat4 inv = view_projection.inverse4x4();

		f32 w = 1.0f;
		vec3f ppc = inv.transform_vector(p, &w);

		return ppc / w;
	}

	// Unproject screen coordinate p wih viewport using iverse view_projection
	inline vec3f unproject_sc(const vec3f& p, const mat4& view_projection, const vec2i& viewport)
	{
		vec2f ndc_xy = (p.xy / (vec2f)viewport) * vec2f(2.0) - vec2f(1.0);
		vec3f ndc = vec3f(ndc_xy, p.z);

		return unproject_ndc(p, view_projection);
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
	inline vec3f ray_plane_intersect(const vec3f& r0, const vec3f& rV, const vec3f& x0, const vec3f& xN)
	{
		f32 d = plane_distance(x0, xN);
		f32 t = -(dot(r0, xN) + d) / dot(rV, xN);

		return r0 + (rV * t);
	}

    // Returns the classification of an aabb vs a plane aabb defined by min and max
    // plane defined by point on plane x0 and normal of plane xN
	inline u32 aabb_vs_plane(const vec3f& aabb_min, const vec3f& aabb_max, const vec3f& x0, const vec3f& xN)
	{
        vec3f   e = (aabb_max - aabb_min) / 2.0f;
		vec3f   centre = aabb_min + e;
        f32     radius = abs(xN.x*e.x) + abs(xN.y*e.y) + abs(xN.z*e.z);
		f32     pd = plane_distance(xN, x0);
		f32     d = dot(xN, centre) + pd;

		if (d > radius)
			return INFRONT;

		if (d < -radius)
			return BEHIND;

		return INTERSECTS;
	}
    
    // Returns the classification of a sphere vs a plane
    // Sphere defined by centre s, and radius r
    // Plane defined by point on plane x0 and normal of plane xN
    inline u32 sphere_vs_plane(const vec3f& s, f32 r, const vec3f& x0, const vec3f& xN)
    {
        f32 pd = plane_distance(xN, x0);
        f32 d = dot(x0, s) + pd;
        
        if (d > r)
            return INFRONT;
        
        if (d < -r)
            return BEHIND;
        
        return INTERSECTS;
    }

    // Returns true is point p0 is inside aabb define by min and max
	inline bool point_inside_aabb(const vec3f& min, const vec3f& max, const vec3f& p0)
	{
		if (p0.x < min.x || p0.x > max.x)
			return false;

		if (p0.y < min.y || p0.y > max.y)
			return false;

		if (p0.z < min.z || p0.z > max.z)
			return false;

		return true;
	}
    
    // Returns true if sphere with centre s0 and radius r0 overlaps
    // Sphere with centre s1 and radius r1
    bool sphere_vs_sphere(const vec3f& s0, f32 r0, const vec3f& s1, f32 r1)
    {
        f32 rr = r0 + r1;
        f32 d = dist(s0, s1);
        
        if(d<rr)
            return true;
        
        return false;
    }
    
    // Returns true if sphere with centre s0 and radius r0 overlaps
    // AABB defined by aabb_min and aabb_max
    bool sphere_vs_aabb(const vec3f& s0, f32 r0, const vec3f& aabb_min, const vec3f& aabb_max)
    {
        vec3f cp = closest_point_on_aabb(s0, aabb_min, aabb_max);
        f32 d = dist(cp, s0);
        
        return d < r0;
    }
    
    vec3f closest_point_on_aabb(const vec3f& s0, const vec3f& aabb_min, const vec3f& aabb_max)
    {
        vec3f t1 = max_union(s0, aabb_min);
        vec3f t2 = min_union(t1, aabb_max);
        return t2;
    }
    
    // Returns the closest point to p on the line segment l1 to l2
    inline vec3f closest_point_on_line(const vec3f& l1, const vec3f& l2, const vec3f& p)
    {
        vec3f v1 = p - l1;
        vec3f v2 = normalised(l2 - l1);
    
        f32 d = dist(l1, l2);
        f32 t = dot(v2, v1);
        
        if (t <= 0)
            return l1;
        
        if (t >= d)
            return l2;
        
        return l1 + v2 * t;
    }

	// Returns the distance (t) of p along the line l1-l2
	inline f32 distance_on_line(const vec3f& l1, const vec3f& l2, const vec3f& p)
	{
		vec3f v1 = p - l1;
		vec3f v2 = normalised(l2 - l1);

		return dot(v2, v1);
	}

	bool line_vs_ray(const vec3f& l1, const vec3f& l2, const vec3f& r0, const vec3f& rV, vec3f& ip)
	{
		vec3f da = l2 - l1;
		vec3f db = rV;
		vec3f dc = r0 - l1;

		if (dot(dc, cross(da, db)) != 0.0) // lines are not coplanar
			return false;

		f32 s = dot(cross(dc, db), cross(da, db)) / mag2(cross(da, db));
		if (s >= 0.0 && s <= 1.0)
		{
			ip = l1 + da * s;
			return true;
		}

		return false;
	}

	bool line_vs_line(const vec3f& l1, const vec3f& l2, const vec3f& s1, const vec3f& s2, vec3f& ip)
	{
		vec3f da = l2 - l1;
		vec3f db = s2 - s1;
		vec3f dc = s1 - l1;

		if (dot(dc, cross(da, db)) != 0.0) // lines are not coplanar
			return false;

		f32 s = dot(cross(dc, db), cross(da, db)) / mag2(cross(da, db));
		if (s >= 0.0 && s <= 1.0)
		{
			ip = l1 + da * s;
			f32 t = distance_on_line(s1, s2, ip) / dist(s1, s2);
			if(t >= 0.0f && t <= 1.0f)
				return true;
		}

		return false;
	}

	// Returns the closest point to p on the line the ray r0 with diection rV
	inline vec3f closest_point_on_ray(const vec3f& r0, const vec3f& rV, const vec3f& p)
	{
		vec3f v1 = p - r0;
		f32 t = dot(v1, rV);

		return r0 + rV * t;
	}
    
    // find distance x0 is from segment x1-x2
    inline float point_segment_distance(const vec3f &x0, const vec3f &x1, const vec3f &x2)
    {
        Vec3f dx(x2-x1);
        double m2=mag2(dx);
        // find parameter value of closest point on segment
        float s12=(float)(dot(x2-x0, dx)/m2);
        if(s12<0){
            s12=0;
        }else if(s12>1){
            s12=1;
        }
        // and find the distance
        return dist(x0, s12*x1+(1-s12)*x2);
    }
    
    // find distance x0 is from triangle x1-x2-x3
    inline float point_triangle_distance(const vec3f &x0, const vec3f &x1, const vec3f &x2, const vec3f &x3)
    {
        // first find barycentric coordinates of closest point on infinite plane
        vec3f x13(x1-x3), x23(x2-x3), x03(x0-x3);
        float m13=mag2(x13), m23=mag2(x23), d=dot(x13,x23);
        float invdet=1.f/max(m13*m23-d*d,1e-30f);
        float a=dot(x13,x03), b=dot(x23,x03);
        // the barycentric coordinates themselves
        float w23=invdet*(m23*a-d*b);
        float w31=invdet*(m13*b-d*a);
        float w12=1-w23-w31;
        if(w23>=0 && w31>=0 && w12>=0){ // if we're inside the triangle
            return dist(x0, w23*x1+w31*x2+w12*x3);
        }else{ // we have to clamp to one of the edges
            if(w23>0) // this rules out edge 2-3 for us
                return min(point_segment_distance(x0,x1,x2), point_segment_distance(x0,x1,x3));
            else if(w31>0) // this rules out edge 1-3
                return min(point_segment_distance(x0,x1,x2), point_segment_distance(x0,x2,x3));
            else // w12 must be >0, ruling out edge 1-2
                return min(point_segment_distance(x0,x1,x3), point_segment_distance(x0,x2,x3));
        }
    }
    
    // Returns true is p is inside the triangle v1-v2-v3
    inline bool point_inside_triangle(const vec3f& p, const vec3f& v1, const vec3f& v2, const vec3f& v3)
    {
        vec3f cp1, cp2;
        
        //edge 1
        cp1 = cross(v2 - v1, v3 - v1);
        cp2 = cross(v2 - v1, p - v1);
        if (dot(cp1, cp2) < 0)
            return false;
        
        //edge 2
        cp1 = cross(v3 - v1, v2 - v1);
        cp2 = cross(v3 - v1, p - v1);
        if (dot(cp1, cp2) < 0)
            return false;
        
        //edge 3
        cp1 = cross(v3 - v2, v1 - v2);
        cp2 = cross(v3 - v2, p - v2);
        if (dot(cp1, cp2) < 0)
            return false;
        
        return true;
    }
    
    // Returns the cloest point on trianglt v1-v2-v3 to point p
    // side is 1 or -1 depending on whether the point is infront or behind the triangle
    vec3f closest_point_on_triangle(const vec3f& p, const vec3f& v1, const vec3f& v2, const vec3f& v3, f32& side)
    {
        vec3f n = normalised(cross(v3 - v1, v2 - v1));
        
        f32 d = point_plane_distance(p, v1, n);
        
        side = d <= 0.0f ? -1.0f : 1.0f;
        
        vec3f cp = p - n * d;
        
        if (put::maths::point_inside_triangle(v1, v2, v3, cp))
            return cp;
        
        vec3f cl[] =
        {
            closest_point_on_line(v1, v2, cp),
            closest_point_on_line(v2, v3, cp),
            closest_point_on_line(v1, v3, cp)
        };
        
        f32 ld = dist(p, cl[0]);
        cp = cl[0];
        
        for (int l = 1; l < 3; ++l)
        {
            f32 ldd = dist(p, cl[l]);
            
            if (ldd < ld)
            {
                cp = cl[l];
                ld = ldd;
            }
        }
        
        return cp;
    }
    
    // Get normal of triangle v1-v2-v3 with left handed winding
    vec3f get_normal(const vec3f& v1, const vec3f& v2, const vec3f& v3)
    {
        vec3f vA = v3 - v1;
        vec3f vB = v2 - v1;

        return normalised(cross(vB, vA));
    }
    
    // Returns true is ray with origin r1 and direction rv intersects the aabb defined by emin and emax
    // Intersection point is stored in ip
	inline bool ray_vs_aabb(const vec3f& emin, const vec3f& emax, const vec3f& r1, const vec3f& rv, vec3f& ip)
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
			return false;

		// if tmin > tmax, ray doesn't intersect AABB
		if (tmin > tmax)
			return false;

		t = tmin;
        ip = r1 + rv * t;
		return true;
	}

    // Returns true if there is an intersection bewteen ray with origin r1 and direction rv and obb defined by matrix mat 
	// mat will transform a cube centred at 0 with extents -1 to 1 into an obb
	inline bool ray_vs_obb(const mat4& mat, const vec3f& r1, const vec3f& rv, vec3f& ip)
	{
		mat4 invm = mat;
		invm = invm.inverse4x4();

		vec3f tr1 = invm.transform_vector(r1);

		invm.set_translation(vec3f::zero());
		vec3f trv = invm.transform_vector(rv);

		bool ii = ray_vs_aabb(-vec3f::one(), vec3f::one(), tr1, normalised(trv), ip);

		ip = mat.transform_vector(ip);
		return ii;
	}
}

#endif
