#ifndef ces_editor_h__
#define ces_editor_h__

#include "ces/ces_scene.h"

namespace put
{
    namespace ces
    {
        void editor_init( entity_scene* scene );
        
        void scene_browser_ui( entity_scene* scene, bool* open );
        void enumerate_resources( bool* open );
        
        void update_model_viewer_camera( put::camera_controller* cc );
        void update_model_viewer_scene( put::scene_controller* sc );
        
        void render_scene_editor( const scene_view& view );
    }
}

#endif
