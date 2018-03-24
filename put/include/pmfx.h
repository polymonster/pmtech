#ifndef _pmfx_h
#define _pmfx_h

#include "pen.h"
#include "str/Str.h"

namespace put
{
	struct scene_controller;
	struct scene_view_renderer;
	struct camera_controller;
	struct camera;
}

namespace put
{
    namespace pmfx
    {
        typedef u32 shader_program_handle;
        typedef u32 shader_handle;

        struct shader_program
        {
            hash_id id_name;
            hash_id id_sub_type;
            
            u32 stream_out_shader;
            u32 vertex_shader;
            u32 pixel_shader;
            u32 input_layout;
            u32 program_index;
        };

		struct render_target
		{
			hash_id id_name;

			s32     width = 0;
			s32     height = 0;
			f32     ratio = 0;
			s32     num_mips;
			u32     format;
			u32     handle;
			u32     samples = 1;
			Str     name;
		};

		// pmfx renderer ------------------------------------------------------------------------------------------

		void					init(const c8* filename);
		void					shutdown();
		void					release_script_resources();
		void					update();
		void					render();

		void					register_scene(const scene_controller& scene);
		void					register_camera(const camera_controller& cam);
		void					register_scene_view_renderer(const scene_view_renderer& svr);

		const camera*			get_camera(hash_id id_name);

		const render_target*	get_render_target(hash_id h);
		void					get_render_target_dimensions(const render_target* rt, f32& w, f32& h);
		void					resize_render_target(hash_id target, u32 width, u32 height, const c8* format = nullptr );

		void					resize_viewports();

		u32						get_render_state_by_name(hash_id id_name);

		void					show_dev_ui();

		// pmfx shader ------------------------------------------------------------------------------------------

		shader_handle			load_shader(const c8* pmfx_name);
        void					release_shader(shader_handle handle );
        void					set_technique(shader_handle handle, u32 index );
        bool					set_technique(shader_handle handle, hash_id id_technique, hash_id id_sub_type );
        void					poll_for_changes();
    }
}

#endif
