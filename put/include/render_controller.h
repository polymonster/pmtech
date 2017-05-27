#ifndef _renderer_controller_h
#define _renderer_controller_h

#include "pen.h"
#include "renderer.h"
#include "component_entity.h"
#include "camera.h"

#define MAX_MRT_TARGETS 4

namespace put
{
	struct textured_vertex
	{
		float x, y, z, w;
		float u, v;
	};

	struct layer
	{
		ces::scene_view view;
		put::camera		camera;

		pen::viewport	viewport;
		pen::rect		scissor_rect;
		u32				raster_state;
		u32				clear_state;
		u32				depth_stencil_state;
		u32				blend_state;
		u32				colour_targets[MAX_MRT_TARGETS];
		u32				num_colour_targets;
		u32				depth_target;

		void(*update_function)(layer*) = nullptr;
	};

	struct built_in_handles
	{
		//states
		u32 default_clear_state;
		u32 raster_state_fill_cull_back;
		u32 depth_stencil_state_write_less;

		//blend modes
		u32 blend_src_alpha_inv_src_alpha;
		u32 blend_additive;
		u32 blend_disabled;

		//samplers
		u32 sampler_linear_clamp;
		u32 sampler_linear_wrap;
		u32 sampler_point_clamp;
		u32 sampler_point_wrap;

		//buffers
		u32 screen_quad_vb;
		u32 screen_quad_ib;

		//cbuffers
		u32 default_view_cbuffer;
		u32 debug_shader_cbuffer;

		//backbuffer vp and scissor rect
		pen::viewport	back_buffer_vp;
		pen::rect		back_buffer_scissor_rect;
	};

	void render_controller_init();
	void render_controller_shutdown();
	void render_controller_render();
	void render_controller_update();

	void render_controller_add_layer( const layer& layer );

	const built_in_handles& render_controller_built_in_handles();
}

#endif

