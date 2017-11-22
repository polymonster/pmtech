#include "layer_controller.h"
#include "entry_point.h"
#include "component_entity.h"
#include "dev_ui.h"
#include "debug_render.h"
#include "pen_json.h"
#include "file_system.h"

extern pen::window_creation_params pen_window;

namespace put
{
	built_in_handles		k_built_in_handles;
	std::vector<layer>		k_layers;
    pen::json				k_shader_debug_settings;

	std::vector<const c8*>	k_shader_debug_names;
	std::vector<s32>		k_shader_debug_indices;
	static s32				k_shader_debug_selected = -1;

	struct debug_shader_settings
	{
		f32 debug_index = 0;
		f32 unused[3];
	};

	void create_debug_shader_settings()
	{
		Str dbg_shader_file = "data/pmfx/";
		dbg_shader_file.append(pen::renderer_get_shader_platform());
		dbg_shader_file.append("/debug_settings.json");
        
        k_shader_debug_settings = pen::json::load_from_file(dbg_shader_file.c_str());

        static const c8* none_str = "None";
		k_shader_debug_names.push_back(none_str);
		k_shader_debug_indices.push_back(-1);
        
        pen::json shader_settings = k_shader_debug_settings["debug_settings"];
        s32 num_settings = shader_settings.size();

        for( s32 i = 0; i < num_settings; ++i )
        {
            pen::json setting = shader_settings[i];
            Str setting_name = setting["name"].as_str();
            
            u32 len = setting_name.length();
            c8* c_name = (c8*)pen::memory_alloc(setting_name.length() + 1);
            pen::memory_cpy(c_name, setting_name.c_str(), setting_name.length());
            c_name[len] = '\0';
            
            k_shader_debug_names.push_back(c_name);
            
            u32 setting_index = setting["index"].as_u32();
            k_shader_debug_indices.push_back(setting_index);
        }
	}

	void create_built_in_blend_states()
	{
		//common blend options
		pen::render_target_blend rtb;
		rtb.blend_enable = 1;
		rtb.blend_op = PEN_BLEND_OP_ADD;
		rtb.blend_op_alpha = PEN_BLEND_OP_ADD;
		rtb.render_target_write_mask = 0x0F;

		pen::blend_creation_params blend_params;
		blend_params.alpha_to_coverage_enable = 0;
		blend_params.independent_blend_enable = 0;
		blend_params.render_targets = &rtb;
		blend_params.num_render_targets = 1;

		//disabled
		rtb.blend_enable = 0;
		k_built_in_handles.blend_disabled = pen::renderer_create_blend_state(blend_params);

		//src alpha inv src alpha
		rtb.blend_enable = 1;
		rtb.dest_blend = PEN_BLEND_INV_SRC_ALPHA;
		rtb.src_blend = PEN_BLEND_SRC_ALPHA;
		rtb.dest_blend_alpha = PEN_BLEND_INV_SRC_ALPHA;
		rtb.src_blend_alpha = PEN_BLEND_SRC_ALPHA;
		k_built_in_handles.blend_src_alpha_inv_src_alpha = pen::renderer_create_blend_state(blend_params);

		//additive
		rtb.blend_enable = 1;
		rtb.dest_blend = PEN_BLEND_ONE;
		rtb.src_blend = PEN_BLEND_ONE;
		rtb.dest_blend_alpha = PEN_BLEND_ONE;
		rtb.src_blend_alpha = PEN_BLEND_ONE;
		k_built_in_handles.blend_src_alpha_inv_src_alpha = pen::renderer_create_blend_state(blend_params);
	}

	void layer_controller_init()
	{
		//clear state
		static pen::clear_state cs =
		{
			0.5f, 0.5, 0.5f, 1.0f, 1.0f, PEN_CLEAR_COLOUR_BUFFER | PEN_CLEAR_DEPTH_BUFFER,
		};

		k_built_in_handles.default_clear_state = pen::renderer_create_clear_state(cs);

		//raster state
		pen::rasteriser_state_creation_params rcp;
		pen::memory_zero(&rcp, sizeof(pen::rasteriser_state_creation_params));
		rcp.fill_mode = PEN_FILL_SOLID;
		rcp.cull_mode = PEN_CULL_NONE;
		rcp.depth_bias_clamp = 0.0f;
		rcp.sloped_scale_depth_bias = 0.0f;

		k_built_in_handles.raster_state_fill_cull_back = pen::renderer_create_rasterizer_state(rcp);

		//viewport
		k_built_in_handles.back_buffer_vp =
		{
			0.0f, 0.0f,
			(f32)pen_window.width, (f32)pen_window.height,
			0.0f, 1.0f
        };
        
		k_built_in_handles.back_buffer_scissor_rect = { 0.0f,  0.0f, (f32)pen_window.width, (f32)pen_window.height };

		//buffers
		//create vertex buffer for a quad
		textured_vertex quad_vertices[] =
		{
			0.0f, 0.0f, 0.5f, 1.0f,         //p1
			0.0f, 0.0f,                     //uv1

			0.0f, 1.0f, 0.5f, 1.0f,         //p2
			0.0f, 1.0f,                     //uv2

			1.0f, 1.0f, 0.5f, 1.0f,         //p3
			1.0f, 1.0f,                     //uv3

			1.0f, 0.0f, 0.5f, 1.0f,         //p4
			1.0f, 0.0f,                     //uv4
		};

		pen::buffer_creation_params bcp;
		bcp.usage_flags = PEN_USAGE_DEFAULT;
		bcp.bind_flags = PEN_BIND_VERTEX_BUFFER;
		bcp.cpu_access_flags = 0;

		bcp.buffer_size = sizeof(textured_vertex) * 4;
		bcp.data = (void*)&quad_vertices[0];

		k_built_in_handles.screen_quad_vb = pen::renderer_create_buffer(bcp);

		//create index buffer
		u16 indices[] =
		{
			0, 1, 2,
			2, 3, 0
		};

		bcp.usage_flags = PEN_USAGE_IMMUTABLE;
		bcp.bind_flags = PEN_BIND_INDEX_BUFFER;
		bcp.cpu_access_flags = 0;
		bcp.buffer_size = sizeof(u16) * 6;
		bcp.data = (void*)&indices[0];

		k_built_in_handles.screen_quad_ib = pen::renderer_create_buffer(bcp);

		//sampler states
		//create a sampler object so we can sample a texture
		pen::sampler_creation_params scp;
		pen::memory_zero(&scp, sizeof(pen::sampler_creation_params));
		scp.filter = PEN_FILTER_MIN_MAG_MIP_LINEAR;
		scp.address_u = PEN_TEXTURE_ADDRESS_CLAMP;
		scp.address_v = PEN_TEXTURE_ADDRESS_CLAMP;
		scp.address_w = PEN_TEXTURE_ADDRESS_CLAMP;
		scp.comparison_func = PEN_COMPARISON_ALWAYS;
		scp.min_lod = 0.0f;
		scp.max_lod = PEN_F32_MAX;

		k_built_in_handles.sampler_linear_clamp = pen::renderer_create_sampler(scp);

		scp.filter = PEN_FILTER_MIN_MAG_MIP_POINT;
		k_built_in_handles.sampler_point_clamp = pen::renderer_create_sampler(scp);

		scp.address_u = PEN_TEXTURE_ADDRESS_WRAP;
		scp.address_v = PEN_TEXTURE_ADDRESS_WRAP;
		scp.address_w = PEN_TEXTURE_ADDRESS_WRAP;
		k_built_in_handles.sampler_point_wrap = pen::renderer_create_sampler(scp);

		scp.filter = PEN_FILTER_MIN_MAG_MIP_LINEAR;
		k_built_in_handles.sampler_linear_wrap = pen::renderer_create_sampler(scp);

		//depth stencil state
		pen::depth_stencil_creation_params depth_stencil_params = { 0 };

		// Depth test parameters
		depth_stencil_params.depth_enable = true;
		depth_stencil_params.depth_write_mask = 1;
		depth_stencil_params.depth_func = PEN_COMPARISON_LESS;

		k_built_in_handles.depth_stencil_state_write_less = pen::renderer_create_depth_stencil_state(depth_stencil_params);

		//constant buffer
		bcp.usage_flags = PEN_USAGE_DYNAMIC;
		bcp.bind_flags = PEN_BIND_CONSTANT_BUFFER;
		bcp.cpu_access_flags = PEN_CPU_ACCESS_WRITE;
		bcp.buffer_size = sizeof(float) * 16;
		bcp.data = (void*)nullptr;

		k_built_in_handles.default_view_cbuffer = pen::renderer_create_buffer(bcp);

		//debug shader c buffer
		bcp.usage_flags = PEN_USAGE_DYNAMIC;
		bcp.bind_flags = PEN_BIND_CONSTANT_BUFFER;
		bcp.cpu_access_flags = PEN_CPU_ACCESS_WRITE;
		bcp.buffer_size = sizeof(debug_shader_settings);
		bcp.data = (void*)nullptr;

		k_built_in_handles.debug_shader_cbuffer = pen::renderer_create_buffer(bcp);

		create_built_in_blend_states();

		create_debug_shader_settings();
	}

	void layer_controller_shutdown()
	{
		//release states
		pen::renderer_release_clear_state(k_built_in_handles.default_clear_state);
		pen::renderer_release_raster_state(k_built_in_handles.raster_state_fill_cull_back);
		pen::renderer_release_depth_stencil_state(k_built_in_handles.depth_stencil_state_write_less);

		//release buffers
		pen::renderer_release_buffer(k_built_in_handles.screen_quad_vb);
		pen::renderer_release_buffer(k_built_in_handles.screen_quad_ib);
		pen::renderer_release_buffer(k_built_in_handles.default_view_cbuffer);

		//release sampler states
		pen::renderer_release_sampler(k_built_in_handles.sampler_linear_clamp);
		pen::renderer_release_sampler(k_built_in_handles.sampler_linear_wrap);
		pen::renderer_release_sampler(k_built_in_handles.sampler_point_clamp);
		pen::renderer_release_sampler(k_built_in_handles.sampler_point_wrap);
	}

	void layer_controller_add_layer(const layer& layer)
	{
		k_layers.push_back(layer);
	}

	void show_ui()
	{
		static bool open = true;
		ImGui::Begin("Viewer", &open);

		static s32 debug_shader_combo_index = 0;
		if (ImGui::Combo("Debug Shader", &debug_shader_combo_index, (const c8**)&k_shader_debug_names[0], k_shader_debug_names.size()))
		{
			k_shader_debug_selected = k_shader_debug_indices[debug_shader_combo_index];

            debug_shader_settings set;
            set.debug_index = (f32)k_shader_debug_selected;
			pen::renderer_update_buffer(k_built_in_handles.debug_shader_cbuffer, &set, sizeof(debug_shader_settings), 0);
		};

		ImGui::End();
	}

	void layer_controller_update()
	{
		//bind debug cbuffer
		//pen::renderer_set_constant_buffer(k_built_in_handles.debug_shader_cbuffer, 13, PEN_SHADER_TYPE_PS);

		//update layers
		u32 num_layers = k_layers.size();
		for (u32 i = 0; i < num_layers; ++i)
		{
			if (k_layers[i].update_function)
			{
				k_layers[i].update_function(&k_layers[i]);
			}
		}

		for (u32 i = 0; i < num_layers; ++i)
		{
            pen::viewport vp =
            {
                0.0f, 0.0f,
                (f32)pen_window.width, (f32)pen_window.height,
                0.0f, 1.0f
            };
            pen::rect sr = { 0.0f,  0.0f, (f32)pen_window.width, (f32)pen_window.height };
    
			//pen::renderer_set_viewport(vp);
			//pen::renderer_set_scissor_rect(sr);
            
			pen::renderer_set_depth_stencil_state(k_layers[i].depth_stencil_state);
			pen::renderer_set_blend_state(k_layers[i].blend_state);

            /*
			if ( k_layers[i].num_colour_targets == 1 )
			{
				pen::renderer_set_targets( k_layers[i].colour_targets[0], k_layers[i].depth_target );
			}
            else
            {
                pen::renderer_set_targets( PEN_DEFAULT_RT, PEN_DEFAULT_DS );
            }

			pen::renderer_clear(k_layers[i].clear_state);

            put::ces::render_scene_view( k_layers[i].view, k_shader_debug_selected == -1 ? ces::SN_RENDER_LIT : ces::SN_RENDER_DEBUG );

            put::ces::render_scene_debug(k_layers[i].view.scene, k_layers[i].view);
            
            if( k_layers[i].debug_dispatch & LAYER_DEBUG_3D )
                put::dbg::render_3d( k_layers[i].view.cb_view );
            
            if( k_layers[i].debug_dispatch & LAYER_DEBUG_2D )
                put::dbg::render_2d( );
             */
		}
	}

	void layer_controller_render()
	{

	}

	const built_in_handles& layer_controller_built_in_handles()
	{
		return k_built_in_handles;
	}
}
