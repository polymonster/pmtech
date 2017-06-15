#ifndef _loader_h
#define _loader_h

#include "definitions.h"
#include "renderer.h"
#include "put_math.h"

namespace put
{
	struct animation
	{
		u32 bone_index;
		u32 num_times;
		f32* timeline;
		vec3f* translations;
		vec3f* euler_angles;
		Quaternion* rotations;
	};

	struct skeleton
	{
		u32				num_joints;
		u32				num_anims;
		u32*			parents;
		vec3f*			offsets;
		Quaternion*		rotations;		
		mat4*			matrix;
		animation*		animations;
		c8**			names;

	};
    
    u32             load_texture( const c8* filename );
	skeleton*       load_skeleton( const c8* filename );
}

#endif
