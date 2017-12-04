#ifndef _render_controller_h
#define _render_controller_h

#include "pen.h"
#include "renderer.h"
#include "ces/ces_scene.h"
#include "camera.h"

namespace put
{
    struct camera_controller
    {
        hash_id id_name;
        put::camera* camera;
        
        void(*update_function)(put::camera_controller*) = nullptr;
        
        Str name;
    };
    
    struct scene_controller
    {
        hash_id id_name;
        put::ces::entity_scene* scene;
        
        void(*update_function)(put::scene_controller*) = nullptr;
        
        Str name;
    };
    
    struct render_target
    {
        hash_id id_name;
        Str     name;
        
        s32     width = 0;
        s32     height = 0;
        f32     ratio = 0;
        
        s32     num_mips;
        u32     format;
        u32     handle;
        bool    msaa = false;
    };
    
    
    namespace render_controller
    {
        void init( const c8* filename );
        
        void update( );
        void render( );
        
        void register_scene( const scene_controller& scene );
        void register_camera( const camera_controller& cam );
        
        const render_target*  get_render_target( hash_id h );
        void                  get_render_target_dimensions( const render_target* rt, f32& w, f32& h);
        u32                   get_render_state_by_name( hash_id id_name );
        
        void show_dev_ui();
    }
}

#endif

