// ecs_editor.h
// Copyright 2014 - 2019 Alex Dixon.
// License: https://github.com/polymonster/pmtech/blob/master/license.md

#pragma once

#include "ecs/ecs_scene.h"

namespace put
{
    namespace ecs
    {
        enum e_select_mode : u32
        {
            SELECT_NORMAL = 0,
            SELECT_ADD = 1,
            SELECT_REMOVE = 2,
            SELECT_ADD_MULTI = 3
        };

        void editor_init(ecs_scene* scene, camera* cam = nullptr);
        void editor_shutdown();
        void editor_update(ecs_controller&, ecs_scene* scene, f32 dt);

        void scene_browser_ui(ecs_scene* scene, bool* open);
        void enumerate_resources(bool* open);
        void add_selection(ecs_scene* scene, u32 index, u32 select_mode = 0);
        void clear_selection(ecs_scene* scene);

        // todo.. these are being phased out
        void update_model_viewer_camera(put::scene_controller* sc);
        void update_model_viewer_scene(put::scene_controller* sc);

        void apply_transform_to_selection(ecs_scene* scene, const vec3f move_axis);

        void render_scene_editor(const scene_view& view);

        Str strip_project_dir(const Str& filename);

    } // namespace ecs
} // namespace put
