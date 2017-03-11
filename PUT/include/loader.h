#ifndef _loader_h
#define _loader_h

#include "definitions.h"
#include "renderer.h"
#include "polyspoon_math.h"

namespace put
{
	pen::texture_creation_params* loader_load_texture( const c8* filename );
	void						  loader_free_texture( pen::texture_creation_params** tcp );

	typedef struct shader_program
	{
		u32 vertex_shader;
		u32 pixel_shader;
		u32 input_layout;
	} shader_program;

	shader_program  loader_load_shader_program( const c8* vs_filename, const c8* ps_filename, const c8* input_layout_filename );

	typedef struct animation
	{
		u32 bone_index;
		u32 num_times;
		f32* timeline;
		vec3f* translations;
		vec3f* euler_angles;
		Quaternion* rotations;
	}animation;

	typedef struct skeleton
	{
		u32				num_joints;
		u32				num_anims;
		u32*			parents;
		vec3f*			offsets;
		Quaternion*		rotations;		
		mat4*			matrix;
		animation*		animations;
		c8**			names;

	} skeleton;

	skeleton*	loader_load_skeleton( const c8* filename );
}

#endif
