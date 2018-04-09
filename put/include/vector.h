#ifndef _vector_h
#define _vector_h

#include "pen.h"
#include <math.h>

#if 1

#include "maths/vec.h"

typedef Vec2i vec2i;
typedef Vec2f vec2f;

typedef Vec3f vec3f;

typedef Vec4f vec4f;
typedef Vec4i vec4i;

struct lw_vec3f
{
	f32 x, y, z;

	inline lw_vec3f& operator = (const vec3f &v)
	{
		x = v.x; y = v.y; z = v.z;

		return *this;
	}
};

#else

struct vec2f;
struct vec2i;
struct vec3f;
struct vec4f;
struct lw_vec3f;

struct vec2i
{
public:
    vec2i()					: x( 0 ), y( 0 )	{ }
    vec2i( s32 ax, s32 ay )	: x( ax ), y( ay )	{ }
    vec2i( s32 f )	: x( f ), y( f ) { }
    ~vec2i()					{ }
    
    const vec2i	operator =	( const vec2i &v )	{ x=v.x, y=v.y; return( *this ); }
    const bool	operator ==	( const vec2i &v )	{ return( x==v.x && y==v.y ); }
    const bool	operator !=	( const vec2i &v )	{ return( x!=v.x || y!=v.y ); }
    const vec2i	operator +	( const vec2i &v )	{ vec2i result( x+v.x, y+v.y ); return( result ); }
    const vec2i	operator -	( const vec2i &v )	{ vec2i result( x-v.x, y-v.y ); return( result ); }
    
    s32			x, y;
};

struct vec2f
{
public:
    vec2f()					: x( 0 ), y( 0 )	{ }
    vec2f( f32 ax, f32 ay )	: x( ax ), y( ay )	{ }
    ~vec2f()					{ }
    
    inline vec2f    operator -  ( void )                    { return vec2f(-x, -y); };
    inline vec2f	operator =	( const vec2f &v )		    { x=v.x, y=v.y; return( *this ); }
    inline bool		operator ==	( const vec2f &v )		    { return( x==v.x && y==v.y ); }
    inline bool		operator !=	( const vec2f &v )		    { return( x!=v.x || y!=v.y ); }
    inline vec2f	operator +	( const vec2f &v )	const   { vec2f result( x+v.x, y+v.y ); return( result ); }
    inline vec2f	operator -	( const vec2f &v )  const   { vec2f result( x-v.x, y-v.y ); return( result ); }
    inline vec2f	operator -	( const f32 &f )    const   { vec2f result( x-f, y-f ); return( result ); }
    inline vec2f	operator *  (const f32 &a    )  const   { vec2f result(x * a, y * a ); return ( result ); }
    inline vec2f	operator *  (const vec2f &a  )  const   { vec2f result(x * a.x, y * a.y ); return ( result ); }
    inline vec2f	operator /  (const f32 &a    )  const
    {
        vec2f result( x / a, y / a );
        return result;
    }
    
    inline vec2f  operator /  (const vec2f &v   )   const
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

	inline void vfloor()
	{
		x = (f32)floor((double)x);
		y = (f32)floor((double)y);
	}

	inline static vec2f vmax(vec2f a, vec2f b)
	{
		vec2f v(PEN_FMAX(a.x, b.x), PEN_FMAX(a.y, b.y));
		return v;
	}

	inline static vec2f vmin(vec2f a, vec2f b)
	{
		vec2f v(PEN_FMIN(a.x, b.x), PEN_FMIN(a.y, b.y));
		return v;
	}

	inline static vec2f flt_max()
	{
		vec2f v(FLT_MAX, FLT_MAX);
		return v;
	}

	inline static vec2f flt_min()
	{
		vec2f v(-FLT_MAX, -FLT_MAX);
		return v;
	}
    
	inline f32 max_component()
	{
		return fmax(x, y);
	}

	inline f32 min_component()
	{
		return fmin(x, y);
	}

    f32	x, y;  
};

struct vec3f
{
public:
    
    vec3f(){}
	vec3f( vec2f v2, f32 az ) : x(v2.x), y(v2.y), z(az) { }
    vec3f( f32 ax, f32 ay, f32 az ) : x( ax ), y( ay ), z( az ) { }
    vec3f( f32 f ) : x( f ), y( f ), z( f ) { }
    ~vec3f()					{ }
    
    inline vec3f operator - ( void ) { return vec3f(-x, -y, -z); };
    
    inline vec3f operator =  (const vec3f &v ) {x = v.x, y = v.y, z = v.z; return (*this ); }
    inline bool  operator == (const vec3f &v ) const { return( x==v.x && y==v.y && z==v.z); }
    inline bool	 operator != (const vec3f &v ) const { return( x!=v.x || y!=v.y || z!=v.z); }
    inline vec3f operator +  (const vec3f &v ) const { vec3f result(x + v.x, y + v.y, z + v.z); return ( result ); }
    inline vec3f operator -  (const vec3f &v ) const { vec3f result(x - v.x, y - v.y, z - v.z); return ( result ); }
    inline vec3f operator *  (const f32 &a   ) const { vec3f result(x * a, y * a, z * a); return ( result ); }
    inline vec3f operator *  (const vec3f &v ) const { vec3f result(x * v.x, y * v.y, z * v.z); return ( result ); }
    inline vec3f operator /  (const f32 &a   ) const
    {
        f32 one_over_a = 1.0f / a;
        vec3f result(x * one_over_a, y * one_over_a, z * one_over_a);
        return ( result );
    }
    
    inline vec3f operator / (const vec3f &v ) const
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
    
    inline static vec3f vmax( vec3f a, vec3f b )
    {
        vec3f v( PEN_FMAX(a.x, b.x), PEN_FMAX(a.y, b.y), PEN_FMAX(a.z, b.z) );
        return v;
    }
    
    inline static vec3f vmin( vec3f a, vec3f b )
    {
        vec3f v( PEN_FMIN(a.x, b.x), PEN_FMIN(a.y, b.y), PEN_FMIN(a.z, b.z) );
        return v;
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

	inline f32 max_component()
	{
		return fmax(fmax(x, y), z);
	}

	inline f32 min_component()
	{
		return fmin(fmin(x, y), z);
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
    
    inline vec2f xy() const
    {
        return vec2f( x, y );
    }
    
    inline vec2f yx() const
    {
        return vec2f( y, x );
    }
    
    inline static vec3f irgb( s32 r, s32 g, s32 b )
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
        
        f32 one_over_mag = 1.0f / sqrt(mag_sq);
        
        x *= one_over_mag;
        y *= one_over_mag;
        z *= one_over_mag;
    }
    
    f32 x, y, z;
};

struct vec4f
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
    
    inline vec4f operator - ( void ) { return vec4f(-x, -y, -z, -w); };
    inline vec4f operator =  (const vec4f &v ) {x = v.x, y = v.y, z = v.z; w = v.w; return (*this ); }
    inline bool  operator == (const vec4f &v ) const { return( x==v.x && y==v.y && z==v.z && w ==v.w); }
    inline bool  operator != (const vec4f &v ) const { return( x!=v.x || y!=v.y || z!=v.z || w!=v.w); }
    inline vec4f operator +  (const vec4f &v ) const { vec4f result(x + v.x, y + v.y, z + v.z, w + v.w); return ( result ); }
    inline vec4f operator -  (const vec4f &v ) const { vec4f result(x - v.x, y - v.y, z - v.z, w - v.w); return ( result ); }
    inline vec4f operator *  (const f32 &a ) const { vec4f result(x * a, y * a, z * a, w * a); return ( result ); }
    inline vec4f operator *  (const vec4f &v ) const { vec4f result(x * v.x, y * v.y, z * v.z, w * v.w); return ( result ); }
    inline vec4f &operator += (const vec4f &v ) { x += v.x, y += v.y, z += v.z;  return ( *this ); }
    inline vec4f &operator -= (const vec4f &v ) { x -= v.x, y -= v.y, z -= v.z ; return ( *this ); }
    inline vec4f &operator *= (const f32 &a    ) { x *= a, y *= a, z *= a;  return ( *this ); }
    
    inline vec4f operator /  (const f32 &a   ) const
    {
        f32 one_over_a = 1.0f / a;
        vec4f result(x * one_over_a, y * one_over_a, z * one_over_a, w * one_over_a);
        return ( result );
    }
    
    inline vec4f operator / (const vec4f &v ) const
    {
        vec4f result(x / v.x, y / v.y, z / v.z, w / v.w);
        return ( result );
    }
    
    inline vec4f &operator /= (const f32 &a    )
    {
        f32 one_over_a = 1.0f / a;
        x *= one_over_a; y *= one_over_a; z *= one_over_a;  return ( *this );
    }
    
    inline vec4f &operator /= (const vec4f &v)
    {
        x /= v.x; y /= v.y; z /= v.z; w /= v.w;
        return *this;
    }
    
    inline vec4f &operator *= (const vec4f &v)
    {
        x *= v.x; y *= v.y; z *= v.z; w *= w;
        return *this;
    }
    
    inline void vfloor()
    {
        x = (f32)floor((double)x);
        y = (f32)floor((double)y);
        z = (f32)floor((double)z);
        w = (f32)floor((double)w);
    }
    
    inline static vec4f vmax( vec4f a, vec4f b )
    {
        vec4f v( PEN_FMAX(a.x, b.x), PEN_FMAX(a.y, b.y), PEN_FMAX(a.z, b.z), PEN_FMAX(a.z, b.z) );
        return v;
    }
    
    inline static vec3f vmin( vec3f a, vec3f b )
    {
        vec3f v( PEN_FMIN(a.x, b.x), PEN_FMIN(a.y, b.y), PEN_FMIN(a.z, b.z) );
        return v;
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

	inline f32 max_component()
	{
		return fmax(fmax(fmax(x, y), z ), w);
	}

	inline f32 min_component()
	{
		return fmin(fmin(fmin(x, y), z), w);
	}
    
    inline static vec4f zero() 
    {
        return vec4f(0.0f,0.0f,0.0f,0.0f);
    }
    
    inline static vec4f one()
    {
        return vec4f (1.0f, 1.0f, 1.0f, 1.0);;
    }
    
    inline vec3f xyz()
    {
        return vec3f( x, y, z );
    }
    
    inline static vec4f yellow()
    {
        return vec4f( 1.0f, 1.0f, 0.0f, 1.0f );
    }
    
    inline static vec4f cyan()
    {
        return vec4f( 0.0f, 1.0f, 1.0f, 1.0f );
    }
    
    inline static vec4f magenta()
    {
        return vec4f( 1.0f, 0.0f, 1.0f, 1.0f );
    }
    
    inline static vec4f red()
    {
        return vec4f( 1.0f, 0.0f, 0.0f, 1.0f );
    }
    
    inline static vec4f green()
    {
        return vec4f( 0.0f, 1.0f, 0.0f, 1.0f );
    }
    
    inline static vec4f blue()
    {
        return vec4f( 0.0f, 0.0f, 1.0f, 1.0f );
    }
    
	inline static vec4f orange()
	{
		return vec4f(1.0f, 0.5f, 0.0f, 1.0f);
	}


    inline static vec4f black()
    {
        return vec4f::zero();
    }
    
    inline static vec4f white()
    {
        return vec4f::one();
    }
    
    inline vec3f xyz() const
    {
        return vec3f( x, y, z );
    }
    
    inline vec2f xy() const
    {
        return vec2f( x, y );
    }
};

struct lw_vec3f
{
    f32 x, y, z;
    
    inline lw_vec3f& operator = (const vec3f &v)
    {
        x = v.x; y = v.y; z = v.z;
        
        return *this;
    }
};

inline float component_wise_min(vec2f v)
{
	return v.min_component();
}
inline float component_wise_min(vec3f v)
{
	return v.min_component();
}
inline float component_wise_min(vec4f v)
{
	return v.min_component();
}

inline float component_wise_max(vec2f v)
{
	return v.max_component();
}
inline float component_wise_max(vec3f v)
{
	return v.max_component();
}
inline float component_wise_max(vec4f v)
{
	return v.max_component();
}


#endif //_vector_h

#endif
