// ecs_editor.h
// Copyright 2014 - 2019 Alex Dixon.
// License: https://github.com/polymonster/pmtech/blob/master/license.md

#pragma once

#include "ecs/ecs_scene.h"

namespace put
{
    namespace ecs
    {
        namespace e_select_mode
        {
            enum select_mode_t
            {
                normal,
                add,
                remove,
                add_multi
            };
        }
        typedef e_select_mode::select_mode_t select_mode;

        namespace e_transform_mode
        {
            enum transform_mode_t
            {
                none,
                select,
                translate,
                rotate,
                scale,
                physics
            };
        }
        typedef e_transform_mode::transform_mode_t transform_mode;

        void editor_init(ecs_scene* scene, camera* cam = nullptr);
        void editor_shutdown();
        void editor_update(ecs_controller&, ecs_scene* scene, f32 dt);
        void editor_enable(bool enable);
        void editor_enable_camera(bool enable);
        void editor_lock_edits(u32 flags);
        void editor_set_transform_mode(transform_mode mode);

        void scene_browser_ui(ecs_scene* scene, bool* open);
        void enumerate_resources(bool* open);
        void add_selection(ecs_scene* scene, u32 index, u32 select_mode = 0);
        void clear_selection(ecs_scene* scene);

        // todo.. these are being phased out

        void update_editor_camera(put::camera* cam);
        void update_editor_scene(ecs_controller& ecsc, ecs_scene* scene, f32 dt);
        void apply_transform_to_selection(ecs_scene* scene, const vec3f move_axis);
        void render_scene_editor(const scene_view& view);
        Str  strip_project_dir(const Str& filename);

    } // namespace ecs
} // namespace put
