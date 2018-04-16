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
    "rigid_body_primitives"		//window title / process name
};

namespace physics
{
    extern PEN_TRV physics_thread_main( void* params );
}

void create_physics_objects( ces::entity_scene* scene )
{
    clear_scene( scene );

    material_resource* default_material = get_material_resource( PEN_HASH( "default_material" ) );

    geometry_resource* box = get_geometry_resource(PEN_HASH("cube"));
    geometry_resource* cylinder = get_geometry_resource( PEN_HASH( "cylinder" ) );
    geometry_resource* capsule = get_geometry_resource( PEN_HASH( "capsule" ) );
    geometry_resource* sphere = get_geometry_resource( PEN_HASH( "sphere" ) );
    geometry_resource* cone = get_geometry_resource( PEN_HASH( "cone" ) );

    //add light
    u32 light = get_new_node( scene );
    scene->names[light] = "front_light";
    scene->id_name[light] = PEN_HASH( "front_light" );
    scene->lights[light].colour = vec3f::one();
    scene->lights[light].direction = vec3f::one();
    scene->lights[light].type = LIGHT_TYPE_DIR;
    scene->transforms[light].translation = vec3f::zero();
    scene->transforms[light].rotation = quat();
    scene->transforms[light].scale = vec3f::one();
    scene->entities[light] |= CMP_LIGHT;
    scene->entities[light] |= CMP_TRANSFORM;

    //ground
    u32 ground = get_new_node( scene );
    scene->names[ground] = "ground";
    scene->transforms[ground].translation = vec3f::zero();
    scene->transforms[ground].rotation = quat();
    scene->transforms[ground].scale = vec3f( 50.0f, 1.0f, 50.0f );
    scene->entities[ground] |= CMP_TRANSFORM;
    scene->parents[ground] = ground;
    instantiate_geometry( box, scene, ground );
    instantiate_material( default_material, scene, ground );
    instantiate_model_cbuffer( scene, ground );

    scene->physics_data[ground].rigid_body.shape = physics::BOX;
    scene->physics_data[ground].rigid_body.mass = 0.0f;
    instantiate_rigid_body( scene, ground );

    vec3f start_positions[] =
    {
        vec3f( -20.0f, 10.0f, -20.0f ),
        vec3f( 20.0f, 10.0f,  20.0f ),
        vec3f( -20.0f, 10.0f,  20.0f ),
        vec3f( 20.0f, 10.0f,  -20.0f ),
        vec3f( 0.0f, 10.0f, 0.0f ),
    };

    const c8* primitive_names[] =
    {
        "box",
        "cylinder",
        "capsule",
        "cone",
        "sphere"
    };

    u32 primitive_types[] =
    {
        physics::BOX,
        physics::CYLINDER,
        physics::CAPSULE,
        physics::CONE,
        physics::SPHERE
    };

    geometry_resource* primitive_resources[] =
    {
        box,
        cylinder,
        capsule,
        cone,
        sphere
    };

    s32 num_prims = 5;

    for (s32 p = 0; p < num_prims; ++p)
    {
        //add stack of cubes
        vec3f start_pos = start_positions[p];
        vec3f cur_pos = start_pos;
        for (s32 i = 0; i < 4; ++i)
        {
            cur_pos.y = start_pos.y;

            for (s32 j = 0; j < 4; ++j)
            {
                cur_pos.x = start_pos.x;

                for (s32 k = 0; k < 4; ++k)
                {
                    u32 new_prim = get_new_node( scene );
                    scene->names[new_prim] = primitive_names[p];
                    scene->names[new_prim].appendf( "%i", new_prim );
                    scene->transforms[new_prim].rotation = quat();
                    scene->transforms[new_prim].scale = vec3f::one();
                    scene->transforms[new_prim].translation = cur_pos;
                    scene->entities[new_prim] |= CMP_TRANSFORM;
                    scene->parents[new_prim] = new_prim;
                    instantiate_geometry( primitive_resources[p], scene, new_prim );
                    instantiate_material( default_material, scene, new_prim );
                    instantiate_model_cbuffer( scene, new_prim );

                    scene->physics_data[new_prim].rigid_body.shape = primitive_types[p];
                    scene->physics_data[new_prim].rigid_body.mass = 1.0f;
                    instantiate_rigid_body( scene, new_prim );

                    cur_pos.x += 2.5f;
                }

                cur_pos.y += 2.5f;
            }

            cur_pos.z += 2.5f;
        }
    }
}

PEN_TRV pen::user_entry( void* params )
{
    //unpack the params passed to the thread and signal to the engine it ok to proceed
    pen::job_thread_params* job_params = (pen::job_thread_params*)params;
    pen::job* p_thread_info = job_params->job_info;
    pen::thread_semaphore_signal(p_thread_info->p_sem_continue, 1);
    
    pen::thread_create_job( physics::physics_thread_main, 1024*10, nullptr, pen::THREAD_START_DETACHED );
    
	put::dev_ui::init();
	put::dbg::init();
    
	//create main camera and controller
	put::camera main_camera;
	put::camera_create_perspective( &main_camera, 60.0f, (f32)pen_window.width / (f32)pen_window.height, 0.1f, 1000.0f );
    
    put::scene_controller cc;
    cc.camera = &main_camera;
    cc.update_function = &ces::update_model_viewer_camera;
    cc.name = "model_viewer_camera";
    cc.id_name = PEN_HASH(cc.name.c_str());
    
    //create the main scene and controller
    put::ces::entity_scene* main_scene = put::ces::create_scene("main_scene");
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
    
    pmfx::register_scene_view_renderer(svr_main);
    pmfx::register_scene_view_renderer(svr_editor);

    pmfx::register_scene_controller(sc);
    pmfx::register_scene_controller(cc);
    
    pmfx::init("data/configs/basic_renderer.json");

    create_physics_objects( main_scene );
    
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
        if( pen::thread_semaphore_try_wait( p_thread_info->p_sem_exit ) )
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
    pen::thread_semaphore_signal( p_thread_info->p_sem_terminated, 1);
    
    return PEN_THREAD_OK;
}
