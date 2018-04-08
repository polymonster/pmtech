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
#include "maths/maths.h"

using namespace put;
using namespace ces;

pen::window_creation_params pen_window
{
	1280,					    //width
	720,					    //height
	4,						    //MSAA samples
	"maths_test_functions"		//window title / process name
};

void draw_plane(const vec3f* plane_points, const vec3f& plane_normal)
{
	vec3f plane_centre = vec3f::zero();

	for (u32 i = 0; i < 3; ++i)
	{
		dbg::add_point(plane_points[i], 0.1f, vec4f::white());

		u32 next = (i + 1) % 3;
		dbg::add_line(plane_points[i], plane_points[next], vec4f::white());

		plane_centre += plane_points[i];
	}

	plane_centre /= 3.0f;

	dbg::add_line(plane_centre, plane_centre + plane_normal, vec4f::blue());
}

void test_ray_plane_intersect(entity_scene* scene, bool initialise)
{
	static u32 ray[2] = { 0 };
	static u32 plane_point[3] = { 0 };

	static vec3f default_plane[] =
	{
		vec3f(-10.0f, -0.1f, -10.0f),
		vec3f(0.0f, 0.0f, 8.0f),
		vec3f(10.0f, 0.1f, -10.0f)
	};

	static vec3f default_ray[] =
	{
		vec3f(-3.0f, 20.0f, 0.0f),
		vec3f(0.0f, -20.0f, 0.0f),
	};

	if (initialise)
	{
		ces::clear_scene(scene);

		//a ray defined by 3 points
		for (u32 i = 0; i < 2; ++i)
		{
			ray[i] = ces::get_new_node(scene);
			scene->names[ray[i]] = "ray";
			scene->transforms[ray[i]].translation = default_ray[i];
		}

		//a plane defined by 3 points
		for (u32 i = 0; i < 3; ++i)
		{
			plane_point[i] = ces::get_new_node(scene);
			scene->names[plane_point[i]] = "plane_point";
			scene->names[plane_point[i]].appendf("%i", i);

			scene->transforms[plane_point[i]].translation = default_plane[i];
		}
	}

	vec3f ray_pos[2];
	vec3f plane_pos[3];

	for (u32 i = 0; i< 2; ++i)
		ray_pos[i] = scene->transforms[ray[i]].translation;

	for (u32 i = 0; i< 3; ++i)
		plane_pos[i] = scene->transforms[plane_point[i]].translation;

	vec3f plane_normal = cross(normalised(plane_pos[1] - plane_pos[0]), normalised(plane_pos[2] - plane_pos[0]));

	vec3f ip = maths2::ray_plane_intersect(ray_pos[0], ray_pos[1] - ray_pos[0], plane_pos[0], plane_normal);

	//debug output
	dbg::add_point(ip, 0.1f, vec4f::red());

	draw_plane(plane_pos, plane_normal);

	dbg::add_line(ray_pos[0], ray_pos[1], vec4f::green());
}

void test_point_plane_distance(entity_scene* scene, bool initialise)
{
	static u32 point = 0;
	static u32 plane_point[3] = { 0 };

	static vec3f default_plane[] =
	{
		vec3f(-10.0f, 0.01f, -10.0f),
		vec3f( 0.0f, -0.01f, 10.0f),
		vec3f( 7.0f, 0.0f, -10.0f)
	};

	static vec3f default_point = vec3f(0.0f, 1.0f, 0.0f);

	if (initialise)
	{
		ces::clear_scene(scene);

		//we need a point and a plane
		point = ces::get_new_node(scene);
		scene->names[point] = "point";
		scene->transforms[point].translation = default_point;

		//a plane defined by 3 points
		for (u32 i = 0; i < 3; ++i)
		{
			plane_point[i] = ces::get_new_node(scene);
			scene->names[plane_point[i]] = "plane_point";
			scene->names[plane_point[i]].appendf("%i", i);

			scene->transforms[plane_point[i]].translation = default_plane[i];
		}
	}

	//get data to test
	vec3f point_pos = scene->transforms[point].translation;
	vec3f plane_pos[3];

	for(u32 i = 0; i< 3; ++i)
		plane_pos[i] = scene->transforms[plane_point[i]].translation;

	vec3f plane_normal = cross(normalised(plane_pos[1] - plane_pos[0]), normalised(plane_pos[2] - plane_pos[0]));
	
	f32 distance = maths2::point_plane_distance(point_pos, plane_pos[0], plane_normal);

	//debug output
	ImGui::Text("Distance %f", distance);
	
	dbg::add_point(point_pos, 0.1f, vec4f::green());

	draw_plane(plane_pos, plane_normal);
}

const c8* test_names[]
{
	"Point Plane Distance",
	"Ray Plane Intersect",
};

typedef void(*maths_test_function)(entity_scene*, bool);

maths_test_function test_functions[] =
{
	test_point_plane_distance,
	test_ray_plane_intersect
};

void maths_test_ui( entity_scene* scene )
{
	static s32 test_index = 0;
	static bool initialise = true;
	if (ImGui::Combo("Test", &test_index, test_names, PEN_ARRAY_SIZE(test_names)))
		initialise = true;

	test_functions[test_index](scene, initialise);

	initialise = false;
}

namespace physics
{
	extern PEN_TRV physics_thread_main(void* params);
}

PEN_TRV pen::user_entry(void* params)
{
	//unpack the params passed to the thread and signal to the engine it ok to proceed
	pen::job_thread_params* job_params = (pen::job_thread_params*)params;
	pen::job* p_thread_info = job_params->job_info;
	pen::thread_semaphore_signal(p_thread_info->p_sem_continue, 1);

	pen::thread_create_job(physics::physics_thread_main, 1024 * 10, nullptr, pen::THREAD_START_DETACHED);

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
	pmfx::register_scene_view_renderer(svr_editor);

	pmfx::register_scene(sc);
	pmfx::register_camera(cc);

	pmfx::init("data/configs/editor_renderer.json");
	
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

		maths_test_ui(main_scene);

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
		if (pen::thread_semaphore_try_wait(p_thread_info->p_sem_exit))
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
	pen::thread_semaphore_signal(p_thread_info->p_sem_terminated, 1);

	return PEN_THREAD_OK;
}
