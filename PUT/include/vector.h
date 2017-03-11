#ifndef _VECTOR_H
#define _VECTOR_H

#include "definitions.h"
#include <math.h>

#include <vector>

typedef struct vec2i
{
public:
	vec2i()					: x( 0 ), y( 0 )	{ }
	vec2i( s32 ax, s32 ay )	: x( ax ), y( ay )	{ }
	vec2i( s32 f )	: x( f ), y( f ) { }
	~vec2i()					{ }

	const vec2i	operator =	( const vec2i &v )	{ x=v.x, y=v.y; return( *this ); }
	const bool		operator ==	( const vec2i &v )	{ return( x==v.x && y==v.y ); }
	const bool		operator !=	( const vec2i &v )	{ return( x!=v.x || y!=v.y ); }
	const vec2i	operator +	( const vec2i &v )	{ vec2i result( x+v.x, y+v.y ); return( result ); }
	const vec2i	operator -	( const vec2i &v )	{ vec2i result( x-v.x, y-v.y ); return( result ); }

	s32			x, y;
} vec2i;

typedef struct vec2f
{
public:
	vec2f()					: x( 0 ), y( 0 )	{ }
	vec2f( f32 ax, f32 ay )	: x( ax ), y( ay )	{ }
	~vec2f()					{ }

	inline vec2f	operator =	( const vec2f &v )		{ x=v.x, y=v.y; return( *this ); }
	inline bool		operator ==	( const vec2f &v )		{ return( x==v.x && y==v.y ); }
	inline bool		operator !=	( const vec2f &v )		{ return( x!=v.x || y!=v.y ); }
	inline vec2f	operator +	( const vec2f &v )		{ vec2f result( x+v.x, y+v.y ); return( result ); }
	inline vec2f	operator -	( const vec2f &v )		{ vec2f result( x-v.x, y-v.y ); return( result ); }
	inline vec2f	operator -	( const f32 &f )		{ vec2f result( x-f, y-f ); return( result ); }
	inline vec2f	operator *  (const f32 &a    )		{ vec2f result(x * a, y * a ); return ( result ); } 
	inline vec2f	operator *  (const vec2f &a    )	{ vec2f result(x * a.x, y * a.y ); return ( result ); } 
	inline vec2f	operator /  (const f32 &a    ) 
	{ 
		vec2f result( x / a, y / a ); 
		return result; 
	}

	inline vec2f  operator /  (const vec2f &v   ) 
	{
		return vec2f( x /v.x, y / v.y );
	}

	inline vec2f &operator += (const vec2f &v ) { x += v.x, y += v.y;  return ( *this ); } 
	inline vec2f &operator -= (const vec2f &v ) { x -= v.x, y -= v.y;  return ( *this ); } 

	inline vec2f &operator *= (const f32 &a    ) { x *= a, y *= a;  return ( *this ); } 
	inline vec2f &operator /= (const f32 &a    ) 
	{ 
		x /= a;
		y /= a;

		return ( *this ); 
	} 

	inline static vec2f zero() 
	{ 
		vec2f v(0.0f,0.0f);
		return v;
	}

	inline static bool almost_equal( vec2f a, vec2f b, f32 lEpsilon ) 
	{ 
		vec2f c = a - b;

		c.x = fabs( c.x );
		c.y = fabs( c.y );

		if( c.x <= lEpsilon && c.y <= lEpsilon )
		{
			return true;
		}

		return false;
	}

	inline vec2f floored()
	{
		return vec2f( floor( x ),  floor( y ) );
	}

	inline void clamp( f32 min, f32 max )
	{
		x = PEN_FMAX( x, min );
		x = PEN_FMIN( x, max );

		y = PEN_FMAX( y, min );
		y = PEN_FMIN( y, max );
	}

	f32		x, y;

} vec2f;

typedef struct vec3f
{

public:

	vec3f() 
	{

	}

	vec3f( f32 ax, f32 ay, f32 az ) : x( ax ), y( ay ), z( az ) { }
	vec3f( f32 f ) : x( f ), y( f ), z( f ) { }
	~vec3f()					{ }

	inline vec3f operator =  (const vec3f &v ) {x = v.x, y = v.y, z = v.z; return (*this ); }
	inline bool	  operator == (const vec3f &v ) { return( x==v.x && y==v.y && z==v.z); }
	inline bool	  operator != (const vec3f &v ) { return( x!=v.x || y!=v.y || z!=v.z); }
	inline vec3f operator +  (const vec3f &v ) { vec3f result(x + v.x, y + v.y, z + v.z); return ( result ); } 
	inline vec3f operator -  (const vec3f &v ) { vec3f result(x - v.x, y - v.y, z - v.z); return ( result ); }
	inline vec3f operator *  (const f32 &a    ) { vec3f result(x * a, y * a, z * a); return ( result ); } 
	inline vec3f operator *  (const vec3f &v ) { vec3f result(x * v.x, y * v.y, z * v.z); return ( result ); } 
	inline vec3f operator /  (const f32 &a    ) 
	{ 
		f32 one_over_a = 1.0f / a;
		vec3f result(x * one_over_a, y * one_over_a, z * one_over_a); 
		return ( result ); 
	}

	inline vec3f operator / (const vec3f &v ) 
	{
		vec3f result(x / v.x, y / v.y, z / v.z); 
			
		return ( result );
	}

	inline vec3f &operator += (const vec3f &v ) { x += v.x, y += v.y, z += v.z;  return ( *this ); } 
	inline vec3f &operator -= (const vec3f &v ) { x -= v.x, y -= v.y, z -= v.z ; return ( *this ); } 

	inline vec3f &operator *= (const f32 &a    ) { x *= a, y *= a, z *= a;  return ( *this ); } 
	inline vec3f &operator /= (const f32 &a    ) 
	{ 
		f32 one_over_a = 1.0f / a;
		x *= one_over_a; y *= one_over_a; z *= one_over_a;  return ( *this ); 
	} 

	inline vec3f &operator /= (const vec3f &v)
	{
		x /= v.x; y /= v.y; z /= v.z;
		return *this;
	}

	inline vec3f &operator *= (const vec3f &v)
	{
		x *= v.x; y *= v.y; z *= v.z;
		return *this;
	}

	inline void vfloor()
	{
		x = (f32)floor((double)x);
		y = (f32)floor((double)y);
		z = (f32)floor((double)z);
	}

	inline static vec3f flt_max()
	{
		vec3f v( FLT_MAX, FLT_MAX, FLT_MAX );
		return v;
	}

	inline static vec3f flt_min( )
	{
		vec3f v( -FLT_MAX, -FLT_MAX, -FLT_MAX );
		return v;
	}

	inline static vec3f zero() 
	{ 
		vec3f v(0.0f,0.0f,0.0f);
		return v;
	}

	inline static vec3f epsilon() 
	{ 
		vec3f v( 0.0000001f, 0.0000001f, 0.0000001f );
		return v;
	}

	inline static vec3f iRGB( s32 r, s32 g, s32 b )
	{
		return vec3f( (f32)r / 255.0f, (f32)g / 255.0f, (f32)b / 255.0f );
	}

	inline static bool almost_equal( vec3f a, vec3f b, f32 epsilon = 0.001f ) 
	{ 
		vec3f c = a - b;

		c.x = c.x < 0.0f ? c.x * -1.0f : c.x;
		c.y = c.y < 0.0f ? c.y * -1.0f : c.y;
		c.z = c.z < 0.0f ? c.z * -1.0f : c.z;

		if( c.x < epsilon && c.y < epsilon && c.z < epsilon )
		{
			return true;
		}

		return false;
	}

	inline static vec3f one()
	{ 
		vec3f v(1.0f,1.0f,1.0f);
		return v;
	}

	inline static vec3f unit_x() 
	{ 
		vec3f v(1.0f,0.0f,0.0f);
		return v;
	}

	inline static vec3f unit_y()
	{ 
		vec3f v(0.0f,1.0f,0.0f);
		return v;
	}

	inline static vec3f unit_z()
	{ 
		vec3f v(0.0f,0.0f,1.0f);
		return v;
	}

	inline static vec3f yellow()
	{
		vec3f v( 1.0f, 1.0f, 0.0f );
		return v;
	}

	inline static vec3f cyan()
	{
		vec3f v( 0.0f, 1.0f, 1.0f );
		return v;
	}

	inline static vec3f magenta()
	{
		vec3f v( 1.0f, 0.0f, 1.0f );
		return v;
	}

	inline void normalise ()
	{
		f32 mag_sq = x * x + y * y + z * z;

		//if(mag_sq > 0.0f)
		{
			f32 one_over_mag = 1.0f / sqrt(mag_sq);

			x *= one_over_mag;
			y *= one_over_mag;
			z *= one_over_mag;
		}
	}

	f32 x, y, z;
} vec3f;

typedef struct vec4f
{
public:

	f32 x, y, z, w;

	vec4f()
	{

	}

	vec4f(f32 _x, f32 _y, f32 _z, f32 _w)
	{
		x = _x;
		y = _y;
		z = _z;
		w = _w;
	}

	vec4f(vec3f vec3, f32 aw)
	{
		x = vec3.x;
		y = vec3.y;
		z = vec3.z;
		w = aw;
	}

	inline static vec4f zero() 
	{ 
		vec4f v(0.0f,0.0f,0.0f,0.0f);
		return v;
	}

	inline vec3f xyz()
	{
		return vec3f( x, y, z );
	}
} vec4f;

typedef std::vector< vec3f > Vector3fArray;
typedef std::vector< vec4f > Vector4fArray;
typedef std::vector< vec2f > Vector2fArray;
typedef std::vector< vec2i > Vector2iArray;

typedef std::vector< vec3f* > Vector3fArray_p;
typedef std::vector< vec2f* > Vector2fArray_p;
typedef std::vector< vec2i* > Vector2iArray_p;

#endif //_VECTOR_H
