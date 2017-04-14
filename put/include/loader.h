#ifndef _loader_h
#define _loader_h

#include "definitions.h"
#include "renderer.h"
#include "polyspoon_math.h"

namespace put
{
	struct managed_shader;

	struct shader_program
	{
		u32 vertex_shader;
		u32 pixel_shader;
		u32 input_layout;
        u32 program_index;
	};

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

    shader_program& loader_load_shader_program( const c8* shader_name, managed_shader* ms = nullptr );
    void            loader_release_shader_program( put::shader_program& program );
    
    u32             loader_load_texture( const c8* filename );
	skeleton*       loader_load_skeleton( const c8* filename );
    void            loader_poll_for_changes();
}

#endif
