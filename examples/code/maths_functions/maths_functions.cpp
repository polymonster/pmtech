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
	"maths_functions"		    //window title / process name
};

// Small structs for debug maths rendering and test primitives
struct debug_ray
{
    u32 node;
    vec3f origin;
    vec3f direction;
};

struct debug_triangle
{
    u32 node;
    vec3f t0;
    vec3f t1;
    vec3f t2;
};

struct debug_line
{
    u32 node;
    vec3f l1;
    vec3f l2;
};

struct debug_plane
{
    u32   node;
    vec3f point;
    vec3f normal;
};

struct debug_aabb
{
    u32   node;
    vec3f min;
    vec3f max;
};

struct debug_sphere
{
    u32   node;
    vec3f pos;
    f32   radius;
};

struct debug_point
{
    u32   node;
    vec3f point;
};

struct debug_extents
{
    vec3f min;
    vec3f max;
};

// Randomise vector in range of extents
vec3f random_vec_range( const debug_extents& extents )
{
    vec3f range = extents.max - extents.min;
    range *= 100.0f;
    
    f32 rx = range.x == 0.0 ? 0.0 : rand()%(u32)range.x;
    f32 ry = range.y == 0.0 ? 0.0 : rand()%(u32)range.y;
    f32 rz = range.z == 0.0 ? 0.0 : rand()%(u32)range.z;
    
    vec3f random = vec3f( rx, ry, rz );
    random /= 100.0f;

    return extents.min + random;
}

// Spawn a randomised point within range of extents
void add_debug_point( const debug_extents& extents, entity_scene* scene, debug_point& point )
{
    u32 node = ces::get_new_node(scene);
    scene->names[node] = "point";
    scene->transforms[node].translation = random_vec_range(extents);
    scene->entities[node] |= CMP_TRANSFORM;
    scene->parents[node] = node;
    
    point.node = node;
    point.point = scene->transforms[node].translation;
}

// Spawn a random plane which starts at a point within extents
void add_debug_plane( const debug_extents& extents, entity_scene* scene, debug_plane& plane )
{
    u32 node = ces::get_new_node(scene);
    scene->names[node] = "plane";
    scene->transforms[node].translation = random_vec_range(extents);
    scene->entities[node] |= CMP_TRANSFORM;
    scene->parents[node] = node;
    
    plane.normal = normalised(random_vec_range({-vec3f::one(), vec3f::one()}));
    plane.point = scene->transforms[node].translation;
}

//Spawn a random ray which contains point within extents
void add_debug_ray( const debug_extents& extents, entity_scene* scene, debug_ray& ray )
{
    u32 node = ces::get_new_node(scene);
    scene->names[node] = "ray";
    scene->transforms[node].translation = random_vec_range(extents);
    scene->entities[node] |= CMP_TRANSFORM;
    scene->parents[node] = node;
    
    ray.direction = normalised(random_vec_range({-vec3f::one(), vec3f::one()}));
    ray.origin = scene->transforms[node].translation;
}

//Spawn a random line which contains both points within extents
void add_debug_line( const debug_extents& extents, entity_scene* scene, debug_line& line )
{
    u32 node = ces::get_new_node(scene);
    scene->names[node] = "line";
    scene->transforms[node].translation = random_vec_range(extents);
    scene->entities[node] |= CMP_TRANSFORM;
    scene->parents[node] = node;
    
    line.l1 = random_vec_range(extents);
    line.l2 = random_vec_range(extents);
}

//Spawn a random ray which contains point within extents
void add_debug_aabb( const debug_extents& extents, entity_scene* scene, debug_aabb& aabb )
{
    u32 node = ces::get_new_node(scene);
    scene->names[node] = "aabb";
    scene->transforms[node].translation = random_vec_range(extents);
    scene->entities[node] |= CMP_TRANSFORM;
    scene->parents[node] = node;
    
    vec3f size = random_vec_range(extents);
    
    aabb.min = scene->transforms[node].translation - size;
    aabb.max = scene->transforms[node].translation + size;
}

// Add debug sphere with randon radius within range extents and at random position within extents
void add_debug_sphere( const debug_extents& extents, entity_scene* scene, debug_sphere& sphere )
{
    material_resource* default_material = get_material_resource( PEN_HASH( "default_material" ) );
    geometry_resource* sphere_res = get_geometry_resource( PEN_HASH( "sphere" ) );
    
    vec3f size = random_vec_range(extents);
    
    u32 node = ces::get_new_node(scene);
    scene->names[node] = "sphere";
    
    scene->transforms[node].rotation = quat();
    scene->transforms[node].scale = vec3f(size.x);
    scene->transforms[node].translation = random_vec_range(extents);
    
    scene->entities[node] |= CMP_TRANSFORM;
    scene->parents[node] = node;
    
    instantiate_geometry(sphere_res, scene, node);
    instantiate_material(default_material, scene, node );
    instantiate_model_cbuffer(scene, node);
    
    sphere.pos = scene->transforms[node].translation;
    sphere.radius = size.x;
    sphere.node = node;
}

//Spawn a random ray which contains point within extents
void add_debug_triangle( const debug_extents& extents, entity_scene* scene, debug_triangle& tri )
{
    u32 node = ces::get_new_node(scene);
    scene->names[node] = "triangle";
    scene->transforms[node].translation = random_vec_range(extents);
    scene->entities[node] |= CMP_TRANSFORM;
    scene->parents[node] = node;
    
    tri.t0 = random_vec_range(extents);
    tri.t1 = random_vec_range(extents);
    tri.t2 = random_vec_range(extents);
}

void test_ray_plane_intersect(entity_scene* scene, bool initialise)
{
    static debug_plane plane;
    static debug_ray ray;
    
    static debug_extents e =
    {
        vec3f(-10.0, -10.0, -10.0),
        vec3f(10.0, 10.0, 10.0)
    };
    
    bool randomise = ImGui::Button("Randomise");
    
	if (initialise || randomise)
	{
		ces::clear_scene(scene);

        add_debug_ray(e, scene, ray);
        add_debug_plane(e, scene, plane);
	}

	vec3f ip = maths2::ray_plane_intersect(ray.origin, ray.direction, plane.point, plane.normal);

	//debug output
	dbg::add_point(ip, 0.3f, vec4f::red());

    dbg::add_plane(plane.point, plane.normal);

	dbg::add_line(ray.origin, ray.direction, vec4f::green());
}

void test_point_plane_distance(entity_scene* scene, bool initialise)
{
    static debug_point point;
    static debug_plane plane;
    
    static debug_extents e =
    {
        vec3f(-10.0, -10.0, -10.0),
        vec3f(10.0, 10.0, 10.0)
    };

    bool randomise = ImGui::Button("Randomise");
    
	if (initialise || randomise)
	{
		ces::clear_scene(scene);

        add_debug_point(e, scene, point);
        add_debug_plane(e, scene, plane);
	}

	f32 distance = maths2::point_plane_distance(point.point, plane.point, plane.normal);

	//debug output
	ImGui::Text("Distance %f", distance);
	
	dbg::add_point(point.point, 0.3f, vec4f::green());
    
    dbg::add_plane(plane.point, plane.normal);
}

const c8* classifications[]
{
    "Intersects",
    "Behind",
    "Infront",
};

const vec4f classification_colours[] =
{
    vec4f::red(),
    vec4f::cyan(),
    vec4f::green()
};

void test_aabb_vs_plane(entity_scene* scene, bool initialise)
{
    static debug_aabb aabb;
    static debug_plane plane;
    
    static debug_extents e =
    {
        vec3f(-10.0, -10.0, -10.0),
        vec3f(10.0, 10.0, 10.0)
    };
    
    bool randomise = ImGui::Button("Randomise");
    
    if (initialise || randomise)
    {
        ces::clear_scene(scene);
        
        add_debug_plane(e, scene, plane);
        add_debug_aabb(e, scene, aabb);
    }

    u32 c = maths2::aabb_vs_plane(aabb.min, aabb.max, plane.point, plane.normal);
    
    //debug output
    ImGui::Text("Classification %s", classifications[c]);
    
    dbg::add_aabb(aabb.min, aabb.max, classification_colours[c]);
    
    dbg::add_plane(plane.point, plane.normal);
}

void test_sphere_vs_plane(entity_scene* scene, bool initialise)
{
    static debug_sphere sphere;
    static debug_plane plane;
    
    static debug_extents e =
    {
        vec3f(-10.0, -10.0, -10.0),
        vec3f(10.0, 10.0, 10.0)
    };
    
    bool randomise = ImGui::Button("Randomise");
    
    if (initialise || randomise)
    {
        ces::clear_scene(scene);
        
        add_debug_plane(e, scene, plane);
        add_debug_sphere(e, scene, sphere);
    }
    
    u32 c = maths2::sphere_vs_plane(sphere.pos, sphere.radius, plane.point, plane.normal);
    
    //debug output
    ImGui::Text("Classification %s", classifications[c]);
    
    scene->materials[sphere.node].diffuse_rgb_shininess = vec4f( classification_colours[c] );
    
    dbg::add_plane(plane.point, plane.normal);
}

void test_project(entity_scene* scene, bool initialise)
{
    bool randomise = ImGui::Button("Randomise");
    
    if (initialise || randomise)
    {
        ces::clear_scene(scene);
    }
}

void test_point_inside_aabb(entity_scene* scene, bool initialise)
{
    static debug_aabb aabb;
    static debug_point point;
    
    static debug_extents e =
    {
        vec3f(-10.0, -10.0, -10.0),
        vec3f(10.0, 10.0, 10.0)
    };
    
    bool randomise = ImGui::Button("Randomise");
    
    if (initialise || randomise)
    {
        ces::clear_scene(scene);
        
        add_debug_aabb(e, scene, aabb);
        add_debug_point(e, scene, point);
    }
    
    bool inside = maths2::point_inside_aabb(aabb.min, aabb.max, point.point);
    
    vec4f col = inside ? vec4f::red() : vec4f::green();
    
    dbg::add_aabb(aabb.min, aabb.max, col);
    dbg::add_point(point.point, 0.3f, col);
}

void test_point_line(entity_scene* scene, bool initialise)
{
    static debug_line line;
    static debug_point point;
    
    static debug_extents e =
    {
        vec3f(-10.0, -10.0, -10.0),
        vec3f(10.0, 10.0, 10.0)
    };
    
    bool randomise = ImGui::Button("Randomise");
    
    if (initialise || randomise)
    {
        ces::clear_scene(scene);
        
        add_debug_line(e, scene, line);
        add_debug_point(e, scene, point);
    }
    
    vec3f cp = maths2::closest_point_on_line(line.l1, line.l2, point.point);
    
    f32 distance = maths2::point_segment_distance(point.point, line.l1, line.l2);
    ImGui::Text("Distance %f", distance);
    
    dbg::add_line(line.l1, line.l2, vec4f::green());
    
    dbg::add_line(point.point, cp);
    
    dbg::add_point(point.point, 0.3f);
    
    dbg::add_point(cp, 0.3f, vec4f::red());
}

void test_point_triangle(entity_scene* scene, bool initialise)
{
    static debug_triangle tri;
    static debug_point point;
    
    static debug_extents e =
    {
        vec3f(-10.0, 0.0, -10.0),
        vec3f(10.0, 0.0, 10.0)
    };
    
    bool randomise = ImGui::Button("Randomise");
    
    if (initialise || randomise)
    {
        ces::clear_scene(scene);
        
        add_debug_triangle(e, scene, tri);
        add_debug_point(e, scene, point);
    }
    
    bool inside = maths2::point_inside_triangle(tri.t0, tri.t1, tri.t2, point.point);
    
    f32 distance = maths2::point_triangle_distance(point.point, tri.t0, tri.t1, tri.t2);
    ImGui::Text("Distance %f", distance);
    
    vec4f col = inside ? vec4f::red() : vec4f::green();
    
    dbg::add_point(point.point, 0.3f, col);
    dbg::add_triangle(tri.t0, tri.t1, tri.t2, col);
}

const c8* test_names[]
{
	"Point Plane Distance",
	"Ray Plane Intersect",
    "AABB Plane Classification",
    "Sphere Plane Classification",
    "Project",
    "Point Inside AABB",
    "Closest Point on Line / Point Segment Distance",
    "Point Inside Triangle / Point Triangle Distance"
};

typedef void(*maths_test_function)(entity_scene*, bool);

maths_test_function test_functions[] =
{
	test_point_plane_distance,
	test_ray_plane_intersect,
    test_aabb_vs_plane,
    test_sphere_vs_plane,
    test_project,
    test_point_inside_aabb,
    test_point_line,
    test_point_triangle
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

	pmfx::init("data/configs/basic_renderer.json");
	
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
