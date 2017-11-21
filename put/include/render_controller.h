#ifndef _render_controller_h
#define _render_controller_h

#include "pen.h"
#include "renderer.h"
#include "component_entity.h"
#include "camera.h"

namespace put
{
    namespace render_controller
    {
        void init( const c8* filename );

        void register_scene( ces::scene_view* sv );
        
        void render( );
        
        void show_dev_ui();
    }
}

#endif

