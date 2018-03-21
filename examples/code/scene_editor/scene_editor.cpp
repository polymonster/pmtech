#include "pen.h"
#include "renderer.h"
#include "timer.h"
#include "file_system.h"
#include "pen_string.h"
#include "loader.h"
#include "dev_ui.h"
#include "camera.h"
#include "debug_render.h"
#include "pmfx.h"
#include "pen_json.h"
#include "hash.h"
#include "str_utilities.h"
#include "input.h"
#include "ces/ces_scene.h"
#include "ces/ces_resources.h"
#include "ces/ces_editor.h"
#include "ces/ces_utilities.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

using namespace put;
using namespace put::ces;

pen::window_creation_params pen_window
{
    1280,					//width
    720,					//height
    4,						//MSAA samples
    "scene_editor"		    //window title / process name
};

namespace physics
{
    extern PEN_THREAD_RETURN physics_thread_main( void* params );
}

put::ces::entity_scene* main_scene;

// Volume Rasteriser WIP --------------------------------------------------------------------------------------------------------

static bool enable_volume_raster = false;
const u32	volume_dim = 256;
put::camera volume_raster_ortho;
u32			current_requestd_slice = -1;
s32			current_slice = 0;
s32			current_axis = 0;
bool		added_to_scene = false;

void*		volume_slices[6][volume_dim] = { 0 };

enum volume_raster_axis
{
	ZAXIS_POS,
	YAXIS_POS,
	XAXIS_POS,
	ZAXIS_NEG,
	YAXIS_NEG,
	XAXIS_NEG
};

inline u8* get_texel(u32 axis, u32 x, u32 y, u32 z )
{
	static u32 block_size = 4;
	static u32 row_pitch = volume_dim * 4;
	static u32 slice_pitch = volume_dim  * row_pitch;

	u32 invx = volume_dim - x - 1;
	u32 invy = volume_dim - y - 1;
	u32 invz = volume_dim - z - 1;
	u8* slice = nullptr;

	switch (axis)
	{
	case ZAXIS_POS:
	{
		//return nullptr;

		u32 offset_xpos = y * row_pitch + x * block_size;
		slice = (u8*)volume_slices[0][z];
		return &((u8*)volume_slices[0][z])[offset_xpos];
	}
	case ZAXIS_NEG:
	{
		//return nullptr;

		u32 offset_xneg = y * row_pitch + x * block_size;
		slice = (u8*)volume_slices[3][invz];
		return &slice[offset_xneg];
	}
	case YAXIS_POS:
	{
		//return nullptr;

		u32 offset_ypos = x * row_pitch + z * block_size;
		slice = (u8*)volume_slices[1][invy];
		return &slice[offset_ypos];
	}
	case YAXIS_NEG:
	{
		//return nullptr;

		u32 offset_yneg = x * row_pitch + z * block_size;
		slice = (u8*)volume_slices[4][y];
		return &slice[offset_yneg];
	}
	case XAXIS_POS:
	{
		//return nullptr;

		u32 offset_xpos = y * row_pitch + z * block_size;
		slice = (u8*)volume_slices[2][x];
		return &slice[offset_xpos];
	}
	default:
		return nullptr;
	}

	return nullptr;
}

void image_read_back(void* p_data, u32 row_pitch, u32 depth_pitch, u32 block_size)
{
	u32 w = row_pitch / block_size;
	u32 h = depth_pitch / row_pitch;

	//Str file_slice;
	//file_slice.appendf("slice_%i.bmp", current_slice);
	//stbi_write_bmp(file_slice.c_str(), w, h, 4, p_data);

	if (!volume_slices[current_axis][current_slice])
	{
		volume_slices[current_axis][current_slice] = pen::memory_alloc(depth_pitch);
		pen::memory_cpy(volume_slices[current_axis][current_slice], p_data, depth_pitch);
	}

	current_slice++;
}

void volume_raster_completed()
{
	if (added_to_scene)
		return;

	added_to_scene = true;

	material_resource* default_material = get_material_resource(PEN_HASH("default_material"));
	geometry_resource* cube = get_geometry_resource(PEN_HASH("cube"));

	u32 new_prim = get_new_node(main_scene);
	main_scene->names[new_prim] = "sphere";
	main_scene->names[new_prim].appendf("%i", new_prim);
	main_scene->transforms[new_prim].rotation = quat();
	main_scene->transforms[new_prim].scale = vec3f(10.0f);
	main_scene->transforms[new_prim].translation = vec3f::zero();
	main_scene->entities[new_prim] |= CMP_TRANSFORM;
	main_scene->parents[new_prim] = new_prim;
	instantiate_geometry(cube, main_scene, new_prim);
	instantiate_material(default_material, main_scene, new_prim);
	instantiate_model_cbuffer(main_scene, new_prim);

	//create a simple 3d texture
	u32 block_size = 4;
	u32 data_size = volume_dim * volume_dim * volume_dim * block_size;

	u8* volume_data = (u8*)pen::memory_alloc(data_size);
	u32 row_pitch = volume_dim * block_size;
	u32 slice_pitch = volume_dim  * row_pitch;

	for (u32 z = 0; z < volume_dim; ++z)
	{
		u8* slice_mem[6] = { 0 };
		for (u32 a = 0; a < 6; ++a)
		{
			slice_mem[a] = (u8*)volume_slices[a][z];
		}

		for (u32 y = 0; y < volume_dim; ++y)
		{
			for (u32 x = 0; x < volume_dim; ++x)
			{
				u32 offset = z * slice_pitch + y * row_pitch + x * block_size;

				u8 rgba[4] = { 0 };

				for (u32 a = 0; a < 6; ++a)
				{
					u8* tex = get_texel(a, x, y, z);
					
					if (!tex)
						continue;

					if (tex[3] > 16)
						for( u32 p = 0; p < 4; ++p )
							rgba[p] = tex[p];
				}

				volume_data[offset + 0] = rgba[2];
				volume_data[offset + 1] = rgba[1];
				volume_data[offset + 2] = rgba[0];
				volume_data[offset + 3] = rgba[3];
			}
		}
	}

	pen::texture_creation_params tcp;
	tcp.collection_type = pen::TEXTURE_COLLECTION_VOLUME;

	tcp.width = volume_dim;
	tcp.height = volume_dim;
	tcp.format = PEN_TEX_FORMAT_BGRA8_UNORM;
	tcp.num_mips = 1;
	tcp.num_arrays = volume_dim;
	tcp.sample_count = 1;
	tcp.sample_quality = 0;
	tcp.usage = PEN_USAGE_DEFAULT;
	tcp.bind_flags = PEN_BIND_SHADER_RESOURCE;
	tcp.cpu_access_flags = 0;
	tcp.flags = 0;
	tcp.block_size = block_size;
	tcp.pixels_per_block = 1;
	tcp.data = volume_data;
	tcp.data_size = data_size;

	u32 volume_texture = pen::renderer_create_texture(tcp);

	//set material for basic volume texture
	scene_node_material& mat = main_scene->materials[new_prim];
	mat.texture_id[4] = volume_texture;
	mat.default_pmfx_shader = pmfx::load_shader("pmfx_utility");
	mat.id_default_shader = PEN_HASH("pmfx_utility");
	mat.id_default_technique = PEN_HASH("volume_texture");
}

void volume_rasteriser_update(put::camera_controller* cc)
{
#if 1
	if (!enable_volume_raster)
		return;

	if (current_requestd_slice == current_slice)
		return;

	if (current_slice >= volume_dim)
	{
		current_axis++;
		current_slice = 0;
	}

	if (current_axis > 5)
	{
		volume_raster_completed();
		return;
	}
#endif

	static mat4 axis_swaps[] =
	{
		mat4::create_axis_swap(vec3f::unit_x(), vec3f::unit_y(), vec3f::unit_z()),
		mat4::create_axis_swap(vec3f::unit_x(), -vec3f::unit_z(), vec3f::unit_y()),
		mat4::create_axis_swap(vec3f::unit_z(), vec3f::unit_y(), vec3f::unit_x()),

		mat4::create_axis_swap(vec3f::unit_x(), vec3f::unit_y(), -vec3f::unit_z()),
		mat4::create_axis_swap(vec3f::unit_x(), -vec3f::unit_z(), -vec3f::unit_y()),
		mat4::create_axis_swap(vec3f::unit_z(), vec3f::unit_y(), -vec3f::unit_x())
	};

	vec3f min = main_scene->renderable_extents.min;
	vec3f max = main_scene->renderable_extents.max;

	vec3f dim = max - min;
	f32 texel_boarder = dim.max_component() / 64;
	min -= texel_boarder;
	max += texel_boarder;

	vec3f smin[] =
	{
		vec3f(min.x, min.y, min.z),
		vec3f(min.x, min.z, min.y),
		vec3f(min.z, min.y, min.x),

		vec3f(min.x, min.y, max.z),
		vec3f(min.x, min.z, max.y),
		vec3f(min.z, min.y, max.x)
	};

	vec3f smax[] =
	{
		vec3f(max.x, max.y, max.z),
		vec3f(max.x, max.z, max.y),
		vec3f(max.z, max.y, max.x),

		vec3f(max.x, max.y, min.z),
		vec3f(max.x, max.z, min.y),
		vec3f(max.z, max.y, min.x)
	};

	vec3f mmin = smin[current_axis];
	vec3f mmax = smax[current_axis];

	//ImGui::InputInt("axis", &current_axis);
	//ImGui::InputInt("slice", &current_slice);

	f32 slice_thickness = (mmax.z - mmin.z) / volume_dim;
	f32 near_slice = mmin.z + slice_thickness * current_slice;

	put::camera_create_orthographic(&volume_raster_ortho, mmin.x, mmax.x, mmin.y, mmax.y, near_slice, near_slice + slice_thickness);
	volume_raster_ortho.view = axis_swaps[current_axis];

	static hash_id id_volume_raster = PEN_HASH("volume_raster");
	const pmfx::render_target* rt = pmfx::get_render_target(id_volume_raster);

	pen::resource_read_back_params rrbp;
	rrbp.block_size = 4;
	rrbp.row_pitch = volume_dim * rrbp.block_size;
	rrbp.depth_pitch = volume_dim * rrbp.row_pitch;
	rrbp.data_size = rrbp.depth_pitch;
	rrbp.resource_index = rt->handle;
	rrbp.format = PEN_TEX_FORMAT_BGRA8_UNORM;
	rrbp.call_back_function = image_read_back;

	pen::renderer_read_back_resource(rrbp);
	current_requestd_slice = current_slice;
}

void volume_rasteriser_init()
{
	put::camera_controller cc;
	cc.camera = &volume_raster_ortho;
	cc.update_function = &volume_rasteriser_update;
	cc.name = "volume_rasteriser_camera";
	cc.id_name = PEN_HASH(cc.name.c_str());

	pmfx::register_camera(cc);
}

PEN_THREAD_RETURN pen::game_entry( void* params )
{
    //unpack the params passed to the thread and signal to the engine it ok to proceed
    pen::job_thread_params* job_params = (pen::job_thread_params*)params;
    pen::job_thread* p_thread_info = job_params->job_thread_info;
    pen::threads_semaphore_signal(p_thread_info->p_sem_continue, 1);
    
    pen::threads_create_job( physics::physics_thread_main, 1024*10, nullptr, pen::THREAD_START_DETACHED );
    
	put::dev_ui::init();
	put::dbg::init();
    
	//create main camera and controller
	put::camera main_camera;
	put::camera_create_perspective( &main_camera, 60.0f, (f32)pen_window.width / (f32)pen_window.height, 0.1f, 1000.0f );
    
    put::camera_controller cc;
    cc.camera = &main_camera;
    cc.update_function = &ces::update_model_viewer_camera;
    cc.name = "model_viewer_camera";
    cc.id_name = PEN_HASH(cc.name.c_str());
    
    //create the main scene and controller
    main_scene = put::ces::create_scene("main_scene");
    put::ces::editor_init( main_scene );
    
    put::scene_controller sc;
    sc.scene = main_scene;
    sc.update_function = &ces::update_model_viewer_scene;
    sc.name = "main_scene";
	sc.camera = &main_camera;
    sc.id_name = PEN_HASH(sc.name.c_str());
    
    //create view renderers
    put::scene_view_renderer svr_main;
    svr_main.name = "ces_render_scene";
    svr_main.id_name = PEN_HASH(svr_main.name.c_str());
    svr_main.render_function = &ces::render_scene_view;
    
    put::scene_view_renderer svr_editor;
    svr_editor.name = "ces_render_editor";
    svr_editor.id_name = PEN_HASH(svr_editor.name.c_str());
    svr_editor.render_function = &ces::render_scene_editor;
    
	volume_rasteriser_init();

    pmfx::register_scene_view_renderer(svr_main);
    pmfx::register_scene_view_renderer(svr_editor);

    pmfx::register_scene(sc);
    pmfx::register_camera(cc);
    
    pmfx::init("data/configs/editor_renderer.json");
    
    bool enable_dev_ui = true;
    
    f32 frame_time = 0.0f;
    
    while( 1 )
    {
        static u32 frame_timer = pen::timer_create("frame_timer");
        pen::timer_start(frame_timer);
        
		put::dev_ui::new_frame();
        
        pmfx::update();
        
        pmfx::render();
        
        pmfx::show_dev_ui();
        
        if( enable_dev_ui )
        {
            put::dev_ui::console();
            put::dev_ui::render();
        }
        
        if( pen::input_is_key_held(PENK_MENU) && pen::input_is_key_pressed(PENK_D) )
            enable_dev_ui = !enable_dev_ui;
        
        frame_time = pen::timer_elapsed_ms(frame_timer);
        
        pen::renderer_present();
        pen::renderer_consume_cmd_buffer();
        
		pmfx::poll_for_changes();
		put::poll_hot_loader();

        //msg from the engine we want to terminate
        if( pen::threads_semaphore_try_wait( p_thread_info->p_sem_exit ) )
            break;
    }

	ces::destroy_scene(main_scene);
	ces::editor_shutdown();
    
    //clean up mem here
	put::pmfx::shutdown();
	put::dbg::shutdown();
	put::dev_ui::shutdown();

    pen::renderer_consume_cmd_buffer();
    
    //signal to the engine the thread has finished
    pen::threads_semaphore_signal( p_thread_info->p_sem_terminated, 1);
    
    return PEN_THREAD_OK;
}
