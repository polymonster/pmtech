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
    "physics_constraints"		//window title / process name
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

    //add light
    u32 light = get_new_node( scene );
    scene->names[light] = "front_light";
    scene->id_name[light] = PEN_HASH( "front_light" );
    scene->lights[light].colour = vec3f::one();
    scene->lights[light].direction = vec3f::one();
    scene->lights[light].type = LIGHT_TYPE_DIR;
    scene->transforms->translation = vec3f::zero();
    scene->transforms->rotation = quat();
    scene->transforms->scale = vec3f::one();
    scene->entities[light] |= CMP_LIGHT;
    scene->entities[light] |= CMP_TRANSFORM;
    
    //add a hinge in the x-axis
    vec3f hinge_x_pos = vec3f(-30.0f, 0.0f, 0.0f);
    
    u32 hinge_x_body = get_new_node( scene );
    scene->names[hinge_x_body] = "hinge_x_body";
    scene->parents[hinge_x_body] = hinge_x_body;
    scene->transforms[hinge_x_body].translation = hinge_x_pos;
    scene->transforms[hinge_x_body].rotation = quat();
    scene->transforms[hinge_x_body].scale = vec3f( 10.0f, 1.0f, 10.0f );
    scene->entities[hinge_x_body] |= CMP_TRANSFORM;
    instantiate_geometry( box, scene, hinge_x_body );
    instantiate_material( default_material, scene, hinge_x_body );
    instantiate_model_cbuffer( scene, hinge_x_body );
    scene->physics_data[hinge_x_body].rigid_body.shape = physics::BOX;
    scene->physics_data[hinge_x_body].rigid_body.mass = 1.0f;
    instantiate_rigid_body( scene, hinge_x_body );
    
    u32 hinge_x_constraint = get_new_node(scene);
    scene->names[hinge_x_constraint] = "hinge_x_constraint";
    scene->parents[hinge_x_constraint] = hinge_x_constraint;
    scene->transforms[hinge_x_constraint].translation = hinge_x_pos - vec3f( 0.0f, 0.0f, -10.0f);
    scene->transforms[hinge_x_constraint].rotation = quat();
    scene->transforms[hinge_x_constraint].scale = vec3f( 1.0f, 1.0f, 1.0f );
    scene->entities[hinge_x_constraint] |= CMP_TRANSFORM;
    scene->physics_data[hinge_x_constraint].constraint.type = physics::CONSTRAINT_HINGE;
    scene->physics_data[hinge_x_constraint].constraint.axis = vec3f::unit_x();
    scene->physics_data[hinge_x_constraint].constraint.rb_indices[0] = scene->physics_handles[hinge_x_body];
    scene->physics_data[hinge_x_constraint].constraint.lower_limit_rotation.x = -PI;
    scene->physics_data[hinge_x_constraint].constraint.upper_limit_rotation.x = PI;
    instantiate_constraint( scene, hinge_x_constraint );
    
    //add hinge in the y-axis with rotational limits
    vec3f hinge_y_pos = vec3f(-30.0f, 10.0f, -30.0f);
    
    u32 hinge_y_body = get_new_node( scene );
    scene->names[hinge_y_body] = "hinge_x_body";
    scene->parents[hinge_y_body] = hinge_y_body;
    scene->transforms[hinge_y_body].translation = hinge_y_pos;
    scene->transforms[hinge_y_body].rotation = quat();
    scene->transforms[hinge_y_body].scale = vec3f( 10.0f, 10.0f, 1.0f );
    scene->entities[hinge_y_body] |= CMP_TRANSFORM;
    instantiate_geometry( box, scene, hinge_y_body );
    instantiate_material( default_material, scene, hinge_y_body );
    instantiate_model_cbuffer( scene, hinge_y_body );
    scene->physics_data[hinge_y_body].rigid_body.shape = physics::BOX;
    scene->physics_data[hinge_y_body].rigid_body.mass = 1.0f;
    instantiate_rigid_body( scene, hinge_y_body );
    
    u32 hinge_y_constraint = get_new_node(scene);
    scene->names[hinge_y_constraint] = "hinge_y_constraint";
    scene->parents[hinge_y_constraint] = hinge_y_constraint;
    scene->transforms[hinge_y_constraint].translation = hinge_y_pos - vec3f( 10.0f, 0.0f, 0.0f);
    scene->transforms[hinge_y_constraint].rotation = quat();
    scene->transforms[hinge_y_constraint].scale = vec3f( 1.0f, 1.0f, 1.0f );
    scene->entities[hinge_y_constraint] |= CMP_TRANSFORM;
    scene->physics_data[hinge_y_constraint].constraint.type = physics::CONSTRAINT_HINGE;
    scene->physics_data[hinge_y_constraint].constraint.axis = vec3f::unit_y();
    scene->physics_data[hinge_y_constraint].constraint.rb_indices[0] = scene->physics_handles[hinge_y_body];
    scene->physics_data[hinge_y_constraint].constraint.lower_limit_rotation.x = -PI/2;
    scene->physics_data[hinge_y_constraint].constraint.upper_limit_rotation.x = PI/2;
    instantiate_constraint( scene, hinge_y_constraint );
    
    //add box with a point to point constraint
    vec3f p2p_pos = vec3f(0.0f, 5.0f, -10.0f);
    
    u32 p2p_body = get_new_node( scene );
    scene->names[p2p_body] = "p2p_body";
    scene->parents[p2p_body] = p2p_body;
    scene->transforms[p2p_body].translation = p2p_pos;
    scene->transforms[p2p_body].rotation = quat();
    scene->transforms[p2p_body].scale = vec3f( 2.0f, 2.0f, 2.0f );
    scene->entities[p2p_body] |= CMP_TRANSFORM;
    instantiate_geometry( box, scene, p2p_body );
    instantiate_material( default_material, scene, p2p_body );
    instantiate_model_cbuffer( scene, p2p_body );
    scene->physics_data[p2p_body].rigid_body.shape = physics::BOX;
    scene->physics_data[p2p_body].rigid_body.mass = 1.0f;
    instantiate_rigid_body( scene, p2p_body );
    
    u32 p2p_constraint = get_new_node(scene);
    scene->names[p2p_constraint] = "p2p_constraint";
    scene->parents[p2p_constraint] = p2p_constraint;
    scene->transforms[p2p_constraint].translation = p2p_pos + vec3f( 0.0f, 10.0f, 0.0f);
    scene->transforms[p2p_constraint].rotation = quat();
    scene->transforms[p2p_constraint].scale = vec3f( 1.0f, 1.0f, 1.0f );
    scene->entities[p2p_constraint] |= CMP_TRANSFORM;
    scene->physics_data[p2p_constraint].constraint.type = physics::CONSTRAINT_P2P;
    scene->physics_data[p2p_constraint].constraint.rb_indices[0] = scene->physics_handles[p2p_body];
    instantiate_constraint( scene, p2p_constraint );
    
    //add slider constraint (six degrees of freedom in an axis)
    vec3f slider_x_pos = vec3f( 5.0f, 3.0f, 20.0f );
    
    u32 slider_x_body = get_new_node( scene );
    scene->names[slider_x_body] = "slider_x_body";
    scene->parents[slider_x_body] = slider_x_body;
    scene->transforms[slider_x_body].translation = slider_x_pos;
    scene->transforms[slider_x_body].rotation = quat();
    scene->transforms[slider_x_body].scale = vec3f( 2.0f, 2.0f, 2.0f );
    scene->entities[slider_x_body] |= CMP_TRANSFORM;
    instantiate_geometry( box, scene, slider_x_body );
    instantiate_material( default_material, scene, slider_x_body );
    instantiate_model_cbuffer( scene, slider_x_body );
    scene->physics_data[slider_x_body].rigid_body.shape = physics::BOX;
    scene->physics_data[slider_x_body].rigid_body.mass = 1.0f;
    instantiate_rigid_body( scene, slider_x_body );
    
    u32 slider_x_constraint = get_new_node(scene);
    scene->names[slider_x_constraint] = "slider_x_constraint";
    scene->parents[slider_x_constraint] = slider_x_constraint;
    scene->transforms[slider_x_constraint].translation = slider_x_pos;
    scene->transforms[slider_x_constraint].rotation = quat();
    scene->transforms[slider_x_constraint].scale = vec3f( 1.0f, 1.0f, 1.0f );
    scene->entities[slider_x_constraint] |= CMP_TRANSFORM;
    scene->physics_data[slider_x_constraint].constraint.type = physics::CONSTRAINT_DOF6;
    scene->physics_data[slider_x_constraint].constraint.rb_indices[0] = scene->physics_handles[slider_x_body];
    scene->physics_data[slider_x_constraint].constraint.lower_limit_rotation = vec3f::zero();
    scene->physics_data[slider_x_constraint].constraint.upper_limit_rotation = vec3f::zero();
    scene->physics_data[slider_x_constraint].constraint.lower_limit_translation = - vec3f::unit_x() * 10.0f;
    scene->physics_data[slider_x_constraint].constraint.upper_limit_translation = vec3f::unit_x() * 10.0f;
    instantiate_constraint( scene, slider_x_constraint );
    
}

PEN_TRV pen::user_entry( void* params )
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

    pmfx::register_scene(sc);
    pmfx::register_camera(cc);
    
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
    
    return PEN_OK;
}
