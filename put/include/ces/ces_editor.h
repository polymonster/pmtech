#ifndef ces_editor_h__
#define ces_editor_h__

#include "ces/ces_scene.h"

namespace put
{
    namespace ces
    {
        void editor_init( entity_scene* scene );
		void editor_shutdown();

        void scene_browser_ui( entity_scene* scene, bool* open );
        void enumerate_resources( bool* open );
        void add_selection( entity_scene* scene, u32 index, u32 select_mode = 0 );

        void update_model_viewer_camera( put::camera_controller* cc );
        void update_model_viewer_scene( put::scene_controller* sc );

		void apply_transform_to_selection(entity_scene* scene, const vec3f move_axis);

        void render_scene_editor( const scene_view& view );
    }
}

#endif
