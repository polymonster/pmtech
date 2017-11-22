#ifndef _render_controller_h
#define _render_controller_h

#include "pen.h"
#include "renderer.h"
#include "component_entity.h"
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
    
    namespace render_controller
    {
        void init( const c8* filename );
        
        void update( );
        void render( );
        
        void register_scene( const scene_controller& scene );
        void register_camera( const camera_controller& cam );
        
        void show_dev_ui();
    }
}

#endif

