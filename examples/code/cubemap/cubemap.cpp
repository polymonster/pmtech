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

using namespace put;
using namespace ces;

pen::window_creation_params pen_window
{
	1280,					    //width
	720,					    //height
	4,						    //MSAA samples
	"cubemap"					//window title / process name
};

namespace physics
{
	extern PEN_THREAD_RETURN physics_thread_main(void* params);
}

void create_scene_objects(ces::entity_scene* scene)
{
	clear_scene(scene);

	material_resource* default_material = get_material_resource(PEN_HASH("default_material"));
	geometry_resource* sphere = get_geometry_resource(PEN_HASH("sphere"));

	u32 new_prim = get_new_node(scene);
	scene->names[new_prim] = "sphere";
	scene->names[new_prim].appendf("%i", new_prim);
	scene->transforms[new_prim].rotation = quat();
	scene->transforms[new_prim].scale = vec3f(10.0f);
	scene->transforms[new_prim].translation = vec3f::zero();
	scene->entities[new_prim] |= CMP_TRANSFORM;
	scene->parents[new_prim] = new_prim;
	instantiate_geometry(sphere, scene, new_prim);
	instantiate_material(default_material, scene, new_prim);
	instantiate_model_cbuffer(scene, new_prim);

	//set material for basic cubemap
	scene_node_material& mat = scene->materials[new_prim];
	mat.texture_id[3] = put::load_texture("data/textures/cubemap.dds");
	mat.default_pmfx_shader = pmfx::load_shader("pmfx_utility");
	mat.id_default_shader = PEN_HASH("pmfx_utility");
	mat.id_default_technique = PEN_HASH("cubemap");
}

PEN_THREAD_RETURN pen::game_entry(void* params)
{
	//unpack the params passed to the thread and signal to the engine it ok to proceed
	pen::job_thread_params* job_params = (pen::job_thread_params*)params;
	pen::job_thread* p_thread_info = job_params->job_thread_info;
	pen::threads_semaphore_signal(p_thread_info->p_sem_continue, 1);

	pen::threads_create_job(physics::physics_thread_main, 1024 * 10, nullptr, pen::THREAD_START_DETACHED);

	put::dev_ui::init();
	put::dbg::init();

	//create main camera and controller
	put::camera main_camera;
	put::camera_create_perspective(&main_camera, 60.0f, (f32)pen_window.width / (f32)pen_window.height, 0.1f, 1000.0f);

	put::camera_controller cc;
	cc.camera = &main_camera;
	cc.update_function = &ces::update_model_viewer_camera;
	cc.name = "model_viewer_camera";
	cc.id_name = PEN_HASH(cc.name.c_str());

	//create the main scene and controller
	put::ces::entity_scene* main_scene = put::ces::create_scene("main_scene");
	put::ces::editor_init(main_scene);

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

	pmfx::register_scene_view_renderer(svr_main);

	pmfx::register_scene(sc);
	pmfx::register_camera(cc);

	pmfx::init("data/configs/basic_renderer.json");

	create_scene_objects(main_scene);
	
	bool enable_dev_ui = true;
	f32 frame_time = 0.0f;

	while (1)
	{
		static u32 frame_timer = pen::timer_create("frame_timer");
		pen::timer_start(frame_timer);

		put::dev_ui::new_frame();

		pmfx::update();

		pmfx::render();

		pmfx::show_dev_ui();

		if (enable_dev_ui)
		{
			put::dev_ui::console();
			put::dev_ui::render();
		}

		if (pen::input_is_key_held(PENK_MENU) && pen::input_is_key_pressed(PENK_D))
			enable_dev_ui = !enable_dev_ui;

		frame_time = pen::timer_elapsed_ms(frame_timer);

		pen::renderer_present();
		pen::renderer_consume_cmd_buffer();

		pmfx::poll_for_changes();
		put::poll_hot_loader();

		//msg from the engine we want to terminate
		if (pen::threads_semaphore_try_wait(p_thread_info->p_sem_exit))
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
	pen::threads_semaphore_signal(p_thread_info->p_sem_terminated, 1);

	return PEN_THREAD_OK;
}
