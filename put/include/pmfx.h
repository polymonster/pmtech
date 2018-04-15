#ifndef _pmfx_h
#define _pmfx_h

#include "pen.h"
#include "str/Str.h"

namespace pen
{
    struct viewport;
}

namespace put
{
    namespace pmfx
    {
        typedef u32 shader_program_handle;
        typedef u32 shader_handle;
    }

    namespace ces
    {
        struct entity_scene;
    }

    struct camera;
}

namespace put
{
    enum e_update_order
    {
        PRE_UPDATE = 0,
        MAIN_UPDATE,
        POST_UPDATE,

        UPDATES_NUM
    };

    struct scene_view
    {
        u32 cb_view = PEN_INVALID_HANDLE;
        u32 cb_2d_view = PEN_INVALID_HANDLE;
        u32 render_flags = 0;
        u32 depth_stencil_state = 0;
        u32 blend_state_state = 0;
        u32 raster_state = 0;
        camera* camera = nullptr;
        pen::viewport* viewport = nullptr;
        pmfx::shader_handle pmfx_shader = PEN_INVALID_HANDLE;
        hash_id technique = 0;
        ces::entity_scene* scene = nullptr;
    };

    struct scene_controller
    {
        Str                     name;
        hash_id                 id_name = 0;
        ces::entity_scene*      scene = nullptr;
        camera*                 camera = nullptr;
        e_update_order          order = MAIN_UPDATE;

        void(*update_function)(scene_controller*) = nullptr;
    };

    struct scene_view_renderer
    {
        Str name;
        hash_id id_name = 0;

        void(*render_function)(const scene_view&) = nullptr; 
    };
}

namespace put
{
    namespace pmfx
    {
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

        void                    register_scene_controller(const scene_controller& controller);
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
		u32						get_technique_index(shader_handle handle, hash_id id_technique, hash_id id_sub_type);

        void					poll_for_changes();
    }
}

#endif
