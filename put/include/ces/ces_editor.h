#ifndef ces_editor_h__
#define ces_editor_h__

#include "ces/ces_scene.h"

namespace put
{
    namespace ces
    {
        enum e_select_mode : u32
        {
            SELECT_NORMAL    = 0,
            SELECT_ADD       = 1,
            SELECT_REMOVE    = 2,
            SELECT_ADD_MULTI = 3
        };

        void editor_init(entity_scene* scene);
        void editor_shutdown();

        void scene_browser_ui(entity_scene* scene, bool* open);
        void enumerate_resources(bool* open);
        void add_selection(entity_scene* scene, u32 index, u32 select_mode = 0);

        void update_model_viewer_camera(put::scene_controller* sc);
        void update_model_viewer_scene(put::scene_controller* sc);

        void apply_transform_to_selection(entity_scene* scene, const vec3f move_axis);

        void render_scene_editor(const scene_view& view);
    } // namespace ces
} // namespace put

#endif
