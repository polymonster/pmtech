// ecs_editor.cpp
// Copyright 2014 - 2019 Alex Dixon.
// License: https://github.com/polymonster/pmtech/blob/master/license.md

#include "ecs/ecs_editor.h"
#include "ecs/ecs_resources.h"
#include "ecs/ecs_utilities.h"

#include "camera.h"
#include "debug_render.h"
#include "dev_ui.h"
#include "physics/physics.h"
#include "pmfx.h"
#include "volume_generator.h"

#include "data_struct.h"
#include "hash.h"
#include "input.h"
#include "os.h"
#include "pen_string.h"
#include "renderer_shared.h"
#include "str_utilities.h"
#include "timer.h"

using namespace pen;
using namespace put;
using namespace ecs;

namespace
{
    const hash_id ID_PICKING_BUFFER = PEN_HASH("picking");

    const hash_id ID_PRIMITIVE[] = {PEN_HASH("quad"),   PEN_HASH("cube"),    PEN_HASH("cylinder"),
                                    PEN_HASH("sphere"), PEN_HASH("capsule"), PEN_HASH("cone")};

    const c8* k_camera_mode_names[] = {"Modelling", "Fly"};

    namespace e_editor_actions
    {
        enum editor_actions_t
        {
            macro_begin = -1,
            macro_end = -2,
            undo = 0,
            redo,
            COUNT
        };
    }
    typedef e_editor_actions::editor_actions_t editor_actions;

    namespace e_picking_state
    {
        enum picking_state_t
        {
            ready,
            single,
            multi,
            complete,
            grabbed
        };
    }

    namespace e_select_flags
    {
        enum select_flags_t
        {
            none,
            widget_selected = 1 << 1
        };
    }

    namespace e_camera_mode
    {
        enum camera_mode_t
        {
            modelling,
            fly
        };
    }
    typedef e_camera_mode::camera_mode_t camera_mode;

    struct transform_undo
    {
        cmp_transform state;
        u32           node_index;
    };

    struct picking_info
    {
        u32  result;
        a_u8 ready;
        u32  x, y;
    };

    struct model_view_controller
    {
        put::camera          main_camera;
        bool                 invalidated = false;
        put::camera_settings settings;
        camera_mode          mode = e_camera_mode::modelling;
        f32                  grid_cell_size;
        f32                  grid_size;
        Str                  current_working_scene = "";
    };

    struct node_state
    {
        void** components = nullptr;
    };

    struct editor_action
    {
        s32        node_index;
        node_state action_state[e_editor_actions::COUNT];
        f32        timer = 0.0f;
    };
    typedef pen::stack<editor_action> action_stack;

    // to move into scene editor context
    u32                   s_editor_lock_flags = 0;
    u32                   s_select_flags = 0;
    picking_info          s_picking_info;
    model_view_controller s_model_view_controller;
    transform_mode        s_transform_mode = e_transform_mode::none;
    action_stack          s_undo_stack;
    action_stack          s_redo_stack;
    editor_action*        s_editor_nodes = nullptr;
    bool                  s_editor_enabled = true;
    bool                  s_editor_enable_camera = true;
} // namespace

namespace put
{
    namespace ecs
    {
        void editor_set_transform_mode(transform_mode mode)
        {
            s_transform_mode = mode;
        }
        
        bool can_edit(u32 flags)
        {
            if(dev_ui::want_capture() & flags)
                return false;
                
            if(s_editor_lock_flags & flags)
                return false;
                
            return true;
        }

        bool shortcut_key(u32 key)
        {
            if (!can_edit(dev_ui::e_io_capture::keyboard))
                return false;

            if (!can_edit(dev_ui::e_io_capture::text))
                return false;

            return pen::input_key(key);
        }

        void update_editor_camera(camera* cam)
        {
            if (s_model_view_controller.invalidated)
            {
                cam->fov = s_model_view_controller.main_camera.fov;
                cam->near_plane = s_model_view_controller.main_camera.near_plane;
                cam->far_plane = s_model_view_controller.main_camera.far_plane;
                camera_update_projection_matrix(cam);
                s_model_view_controller.invalidated = false;
            }

            bool has_focus = can_edit(dev_ui::e_io_capture::mouse | dev_ui::e_io_capture::keyboard);

            switch (s_model_view_controller.mode)
            {
                case e_camera_mode::modelling:
                    put::camera_update_modelling(cam, has_focus, s_model_view_controller.settings);
                    s_model_view_controller.main_camera = *cam;
                    break;
                case e_camera_mode::fly:
                    put::camera_update_fly(cam, has_focus, s_model_view_controller.settings);
                    s_model_view_controller.main_camera = *cam;
                    break;
            }
        }

        void undo(ecs_scene* scene);
        void redo(ecs_scene* scene);
        void update_undo_stack(ecs_scene* scene, f32 dt);

        void view_ui(ecs_scene* scene, bool* opened)
        {
            static const c8* dd_names[] = {
                "Hide Scene", "Hide Debug", "Selected Node",     "Grid",           "Matrices",        "Bones", "AABB",
                "Lights",     "Physics",    "Selected Children", "Debug Geometry", "Camera / Frustum"};
            static_assert(sizeof(dd_names) / sizeof(dd_names[0]) == e_scene_view_flags::COUNT, "mismatched");

            if (ImGui::Begin("View", opened, ImGuiWindowFlags_AlwaysAutoResize))
            {
                for (s32 i = 0; i < e_scene_view_flags::COUNT; ++i)
                    ImGui::CheckboxFlags(dd_names[i], &scene->view_flags, 1 << i);

                if (ImGui::CollapsingHeader("Grid Options"))
                {
                    if (ImGui::InputFloat("Cell Size", &s_model_view_controller.grid_cell_size))
                        dev_ui::set_program_preference("grid_cell_size", s_model_view_controller.grid_cell_size);

                    if (ImGui::InputFloat("Grid Size", &s_model_view_controller.grid_size))
                        dev_ui::set_program_preference("grid_size", s_model_view_controller.grid_size);
                }

                ImGui::End();
            }
        }

        void default_scene(ecs_scene* scene)
        {
            // default view flags
            scene->view_flags |= e_scene_view_flags::defaults;

            // add light
            u32 light = get_new_entity(scene);
            scene->names[light] = "front_light";
            scene->id_name[light] = PEN_HASH("front_light");
            scene->lights[light].colour = vec3f::one();
            scene->lights[light].direction = vec3f::one();
            scene->lights[light].type = e_light_type::dir;
            scene->transforms[light].translation = vec3f::zero();
            scene->transforms[light].rotation = quat();
            scene->transforms[light].scale = vec3f::one();
            scene->entities[light] |= e_cmp::light;
            scene->entities[light] |= e_cmp::transform;
            instantiate_model_cbuffer(scene, light);

            sb_clear(scene->selection_list);
        }

        void editor_init(ecs_scene* scene, camera* cam)
        {
            create_geometry_primitives();

            bool auto_load_last_scene = dev_ui::get_program_preference("load_last_scene").as_bool();
            Str  last_loaded_scene = dev_ui::get_program_preference_filename("last_loaded_scene");
            if (auto_load_last_scene && last_loaded_scene.length() > 0)
            {
                if (last_loaded_scene.length() > 0)
                    load_scene(last_loaded_scene.c_str(), scene);

                s_model_view_controller.current_working_scene = last_loaded_scene;

                auto_load_last_scene = false;
            }
            else
            {
                default_scene(scene);
            }

            s32 wx = put::dev_ui::get_program_preference("window_x").as_s32(-1);
            s32 wy = put::dev_ui::get_program_preference("window_y").as_s32(-1);
            s32 ww = put::dev_ui::get_program_preference("window_width").as_s32(-1);
            s32 wh = put::dev_ui::get_program_preference("window_height").as_s32(-1);
            if (wx != -1)
            {
                pen::window_set_frame({(u32)wx, (u32)wy, (u32)ww, (u32)wh});
            }

            // grid
            s_model_view_controller.grid_cell_size = dev_ui::get_program_preference("grid_cell_size").as_f32(1.0f);
            s_model_view_controller.grid_size = dev_ui::get_program_preference("grid_size").as_f32(100.0f);

            // camera
            s_model_view_controller.main_camera.fov = dev_ui::get_program_preference("camera_fov").as_f32(60.0f);
            s_model_view_controller.main_camera.near_plane = dev_ui::get_program_preference("camera_near").as_f32(0.1f);
            s_model_view_controller.main_camera.far_plane = dev_ui::get_program_preference("camera_far").as_f32(1000.0f);
            s_model_view_controller.settings.invert_x = dev_ui::get_program_preference("camera_invert_x").as_bool();
            s_model_view_controller.settings.invert_y = dev_ui::get_program_preference("camera_invert_y").as_bool();
            s_model_view_controller.settings.zoom_speed = dev_ui::get_program_preference("camera_zoom_speed").as_f32(1.0f);
            s_model_view_controller.invalidated = true;

            ecs_controller controller;
            controller.name = "editor_controller";
            controller.id_name = PEN_HASH(controller.name);
            controller.context = nullptr; // becomes editor
            controller.funcs.update_func = &editor_update;
            controller.camera = cam;

            ecs::register_ecs_controller(scene, controller);

            // svr
            put::scene_view_renderer svr_editor;
            svr_editor.name = "ecs_render_editor";
            svr_editor.id_name = PEN_HASH(svr_editor.name.c_str());
            svr_editor.render_function = &ecs::render_scene_editor;

            pmfx::register_scene_view_renderer(svr_editor);

            // volume generator
            put::vgt::init(scene);
        }

        void editor_shutdown()
        {
            // lol
        }

        void instance_selection(ecs_scene* scene)
        {
            s32 selection_size = sb_count(scene->selection_list);

            if (selection_size <= 1)
                return;

            s32 master = scene->selection_list[0];

            if (scene->entities[master] & e_cmp::master_instance)
                return;

            scene->entities[master] |= e_cmp::master_instance;

            scene->master_instances[master].num_instances = selection_size;
            scene->master_instances[master].instance_stride = sizeof(cmp_draw_call);

            pen::buffer_creation_params bcp;
            bcp.usage_flags = PEN_USAGE_DYNAMIC;
            bcp.bind_flags = PEN_BIND_VERTEX_BUFFER;
            bcp.buffer_size = sizeof(cmp_draw_call) * scene->master_instances[master].num_instances;
            bcp.data = nullptr;
            bcp.cpu_access_flags = PEN_CPU_ACCESS_WRITE;

            scene->master_instances[master].instance_buffer = pen::renderer_create_buffer(bcp);

            // todo - must ensure list is contiguous.
            dev_console_log("[instance] master instance: %i with %i sub instances", master, selection_size);
        }

        void parent_selection(ecs_scene* scene)
        {
            s32 selection_size = sb_count(scene->selection_list);

            if (selection_size <= 1)
                return;

            s32 parent = scene->selection_list[0];

            // check contiguity
            bool valid = true;
            s32  last_index = -1;
            for (s32 i = 1; i < selection_size; ++i)
            {
                if (scene->selection_list[i] < parent)
                {
                    valid = false;
                    break;
                }

                last_index = std::max<s32>(scene->selection_list[i], last_index);
            }

            if (last_index > parent + selection_size)
                valid = false;

            u32 sel_count = sb_count(scene->selection_list);
            if (valid)
            {
                // list is already contiguous
                dev_console_log("[parent] selection is contiguous %i to %i size %i", parent, last_index, selection_size);

                for (int i = 0; i < sel_count; ++i)
                    if (scene->parents[i] == scene->selection_list[i])
                        set_entity_parent(scene, parent, i);
            }
            else
            {
                // move nodes into a contiguous list with the parent first most
                dev_console_log("[parent] selection is not contiguous size %i", parent, last_index, selection_size);

                s32 start, end;
                get_new_entities_append(scene, selection_size, start, end);

                s32 nn = start;
                for (int i = 0; i < sel_count; ++i)
                    clone_entity(scene, scene->selection_list[i], nn++, start, e_clone_mode::move, vec3f::zero(), "");
            }

            scene->flags |= e_scene_flags::invalidate_scene_tree;
        }

        void clear_selection(ecs_scene* scene)
        {
            u32 sel_count = sb_count(scene->selection_list);

            for (u32 i = 0; i < sel_count; ++i)
            {
                u32 si = scene->selection_list[i];
                scene->state_flags[si] &= ~e_state::selected;
            }

            sb_clear(scene->selection_list);
            scene->flags |= e_scene_flags::invalidate_scene_tree;
        }

        void add_selection(ecs_scene* scene, u32 index, u32 select_mode)
        {
            if (pen::input_is_key_down(PK_CONTROL))
                select_mode = e_select_mode::remove;
            else if (pen::input_is_key_down(PK_SHIFT))
                select_mode = e_select_mode::add;

            bool valid = index < scene->num_entities;

            u32 sel_count = sb_count(scene->selection_list);

            if (select_mode == e_select_mode::normal)
            {
                clear_selection(scene);

                if (valid)
                    sb_push(scene->selection_list, index);
            }
            else if (valid)
            {
                s32 existing = -1;
                for (s32 i = 0; i < sel_count; ++i)
                    if (scene->selection_list[i] == index)
                        existing = i;

                u32* new_list = nullptr;
                if (existing != -1 && select_mode == e_select_mode::remove)
                {
                    for (u32 i = 0; i < sel_count; ++i)
                        if (i != existing)
                            sb_push(new_list, scene->selection_list[i]);

                    sb_free(scene->selection_list);
                    scene->selection_list = new_list;
                }

                if (existing == -1 && select_mode == e_select_mode::add)
                    sb_push(scene->selection_list, index);
            }

            if (!valid)
                return;

            if (select_mode == e_select_mode::remove)
                scene->state_flags[index] &= ~e_state::selected;
            else
                scene->state_flags[index] |= e_state::selected;
        }

        void delete_selection(ecs_scene* scene)
        {
            u32 sel_num = sb_count(scene->selection_list);
            for (u32 s = 0; s < sel_num; ++s)
            {
                u32 i = scene->selection_list[s];

                std::vector<s32> node_index_list;
                build_heirarchy_node_list(scene, i, node_index_list);

                for (auto& c : node_index_list)
                    if (c > -1)
                        delete_entity(scene, c);
            }
            sb_clear(scene->selection_list);

            initialise_free_list(scene);

            scene->flags |= e_scene_flags::invalidate_scene_tree;
        }

        void enumerate_selection_ui(const ecs_scene* scene, bool* opened)
        {
            if (ImGui::Begin("Selection List", opened))
            {
                ImGui::Text("Picking Result: %u", s_picking_info.result);

                u32 sel_count = sb_count(scene->selection_list);
                for (s32 i = 0; i < sel_count; ++i)
                {
                    s32 ii = scene->selection_list[i];

                    ImGui::Text("%s", scene->names[ii].c_str());
                }

                ImGui::End();
            }
        }

        void picking_read_back(void* p_data, u32 row_pitch, u32 depth_pitch, u32 block_size)
        {
            s_picking_info.result = *((u32*)(((u8*)p_data) + s_picking_info.y * row_pitch + s_picking_info.x * block_size));
            s_picking_info.ready = 1;
        }

        void picking_update(ecs_scene* scene, const camera* cam)
        {
            static u32 picking_state = e_picking_state::ready;
            static u32 picking_result = (-1);

            if (!can_edit(dev_ui::e_io_capture::mouse))
                return;

            if (picking_state == e_picking_state::single)
            {
                if (s_picking_info.ready)
                {
                    picking_state = e_picking_state::ready;
                    picking_result = s_picking_info.result;

                    add_selection(scene, picking_result);

                    s_picking_info.ready = false;
                }

                return;
            }

            s32 w, h;
            pen::window_get_size(w, h);
            pen::mouse_state ms = pen::input_get_mouse_state();
            f32              corrected_y = h - ms.y;

            static s32   drag_timer = 0;
            static vec2f drag_start;
            static vec3f frustum_points[2][4];

            // frustum select
            vec3f n[6];
            vec3f p[6];

            vec3f plane_vectors[] = {
                frustum_points[0][0], frustum_points[1][0], frustum_points[0][2], // left
                frustum_points[0][0], frustum_points[0][1], frustum_points[1][0], // top

                frustum_points[0][1], frustum_points[0][3], frustum_points[1][1], // right
                frustum_points[0][2], frustum_points[1][2], frustum_points[0][3], // bottom

                frustum_points[0][0], frustum_points[0][2], frustum_points[0][1], // near
                frustum_points[1][0], frustum_points[1][1], frustum_points[1][2]  // far
            };

            for (s32 i = 0; i < 6; ++i)
            {
                s32   offset = i * 3;
                vec3f v1 = normalised(plane_vectors[offset + 1] - plane_vectors[offset + 0]);
                vec3f v2 = normalised(plane_vectors[offset + 2] - plane_vectors[offset + 0]);

                n[i] = cross(v1, v2);
                p[i] = plane_vectors[offset];
            }

            if (!ms.buttons[PEN_MOUSE_L])
            {
                if (picking_state == e_picking_state::multi)
                {
                    u32 pm = e_select_mode::normal;
                    if (!pen::input_is_key_down(PK_CONTROL) && !pen::input_is_key_down(PK_SHIFT))
                    {
                        // unflag current selected
                        u32 sls = sb_count(scene->selection_list);
                        for (u32 i = 0; i < sls; ++i)
                            scene->state_flags[scene->selection_list[i]] &= ~e_state::selected;

                        sb_clear(scene->selection_list);

                        pm = e_select_mode::add;
                    }

                    for (s32 node = 0; node < scene->num_entities; ++node)
                    {
                        if (!(scene->entities[node] & e_cmp::allocated))
                            continue;

                        if (!(scene->entities[node] & e_cmp::geometry))
                            continue;

                        bool selected = true;
                        for (s32 i = 0; i < 6; ++i)
                        {
                            vec3f& min = scene->bounding_volumes[node].transformed_min_extents;
                            vec3f& max = scene->bounding_volumes[node].transformed_max_extents;

                            u32 c = maths::aabb_vs_plane(min, max, p[i], n[i]);
                            if (c == maths::INFRONT)
                            {
                                selected = false;
                                break;
                            }
                        }

                        if (selected)
                        {
                            add_selection(scene, node, e_select_mode::add_multi);
                        }
                    }

                    sb_clear(scene->selection_list);
                    stb__sbgrow(scene->selection_list, scene->num_entities);

                    s32 pos = 0;
                    for (s32 node = 0; node < scene->num_entities; ++node)
                    {
                        if (scene->state_flags[node] & e_state::selected)
                            scene->selection_list[pos++] = node;
                    }

                    stb__sbm(scene->selection_list) = scene->num_entities;
                    stb__sbn(scene->selection_list) = pos;

                    u32 sls = sb_count(scene->selection_list);
                    for (u32 i = 0; i < sls; ++i)
                    {
                        dev_console_log("selected index %i", scene->selection_list[i]);
                    }

                    picking_state = e_picking_state::ready;
                }

                drag_timer = 0;
                drag_start = vec2f(ms.x, corrected_y);
            }

            if (ms.buttons[PEN_MOUSE_L] && pen::mouse_coords_valid(ms.x, ms.y))
            {
                vec2f cur_mouse = vec2f(ms.x, corrected_y);

                vec2f c1 = vec2f(cur_mouse.x, drag_start.y);
                vec2f c2 = vec2f(drag_start.x, cur_mouse.y);

                // frustum selection
                vec2f source_points[] = {drag_start, c1, c2, cur_mouse};

                // sort source points
                vec2f min = vec2f::flt_max();
                vec2f max = -vec2f::flt_max();
                for (s32 i = 0; i < 4; ++i)
                {
                    min = min_union(source_points[i], min);
                    max = max_union(source_points[i], max);
                }

                source_points[0] = vec2f(min.x, max.y);
                source_points[1] = vec2f(max.x, max.y);
                source_points[2] = vec2f(min.x, min.y);
                source_points[3] = vec2f(max.x, min.y);

                put::dbg::add_line_2f(source_points[0], source_points[1]);
                put::dbg::add_line_2f(source_points[0], source_points[2]);
                put::dbg::add_line_2f(source_points[2], source_points[3]);
                put::dbg::add_line_2f(source_points[3], source_points[1]);

                if (mag(max - min) < 6.0)
                {
                    picking_state = e_picking_state::single;

                    const pmfx::render_target* rt = pmfx::get_render_target(ID_PICKING_BUFFER);

                    if (!rt)
                    {
                        picking_state = e_picking_state::ready;
                        return;
                    }

                    f32 w, h;
                    pmfx::get_render_target_dimensions(rt, w, h);

                    u32 pitch = (u32)w * 4;
                    u32 data_size = (u32)h * pitch;

                    pen::resource_read_back_params rrbp = {rt->handle, rt->format,        pitch, data_size, 4,
                                                           data_size,  &picking_read_back};

                    pen::renderer_read_back_resource(rrbp);

                    s_picking_info.ready = 0;
                    s_picking_info.x = ms.x;
                    s_picking_info.y = ms.y;
                }
                else
                {
                    // do not perfrom frustum picking if min and max are not square
                    bool invalid_quad = min.x == max.x || min.y == max.y;

                    if (!invalid_quad)
                    {
                        picking_state = e_picking_state::multi;

                        // todo this should really be passed in, incase we want non window sized viewports
                        vec2i vpi;
                        pen::window_get_size(vpi.x, vpi.y);

                        for (s32 i = 0; i < 4; ++i)
                        {
                            mat4 view_proj = cam->proj * cam->view;
                            frustum_points[0][i] = maths::unproject_sc(vec3f(source_points[i], 0.0f), view_proj, vpi);

                            frustum_points[1][i] = maths::unproject_sc(vec3f(source_points[i], 1.0f), view_proj, vpi);
                        }
                    }
                }
            }

            // set child selected
            for (u32 n = 0; n < scene->num_entities; ++n)
            {
                u32 p = scene->parents[n];
                if (p == n)
                    continue;

                if (scene->state_flags[p] & e_state::selected || scene->state_flags[p] & e_state::child_selected)
                    scene->state_flags[n] |= e_state::child_selected;
            }
        }

        // undoable / redoable actions
        void free_node_state_mem(ecs_scene* scene, node_state& ns)
        {
            if (!ns.components)
                return;

            u32 num = scene->num_components;
            for (u32 i = 0; i < num; ++i)
                pen::memory_free(ns.components[i]);

            pen::memory_free(ns.components);
            ns.components = nullptr;
        }

        void store_node_state(ecs_scene* scene, u32 node_index, editor_actions action)
        {
            static const f32 undo_push_timer = 33.0f;
            node_state&      ns = s_editor_nodes[node_index].action_state[action];
            u32              num = scene->num_components;

            if (action == e_editor_actions::undo)
                if (ns.components)
                    return;

            if (action == e_editor_actions::redo)
            {
                node_state& us = s_editor_nodes[node_index].action_state[e_editor_actions::undo];

                u32 diff = 0;
                for (u32 i = 0; i < num; ++i)
                {
                    generic_cmp_array& cmp = scene->get_component_array(i);
                    diff += memcmp(cmp[node_index], us.components[i], cmp.size);
                }

                // no change bail out
                if (diff == 0)
                    return;

                // check if we are actively editing
                if (ns.components)
                {
                    u32 edit_diff = 0;
                    for (u32 i = 0; i < num; ++i)
                    {
                        generic_cmp_array& cmp = scene->get_component_array(i);
                        edit_diff += memcmp(cmp[node_index], ns.components[i], cmp.size);
                    }

                    // no change since last frame
                    if (edit_diff == 0)
                        return;
                }
            }

            s_editor_nodes[node_index].timer = undo_push_timer;

            if (!ns.components)
            {
                ns.components = (void**)pen::memory_alloc(num * sizeof(generic_cmp_array));
                pen::memory_zero(ns.components, num * sizeof(generic_cmp_array));
            }

            for (u32 i = 0; i < num; ++i)
            {
                generic_cmp_array& cmp = scene->get_component_array(i);

                if (!ns.components[i])
                    ns.components[i] = pen::memory_alloc(cmp.size);

                void* data = cmp[node_index];

                memcpy(ns.components[i], data, cmp.size);
            }
        }

        void restore_node_state(ecs_scene* scene, node_state& ns, u32 node_index)
        {
            u32 num = scene->num_components;
            for (u32 i = 0; i < num; ++i)
            {
                generic_cmp_array& cmp = scene->get_component_array(i);

                // specialisations
                // remove physics
                if (cmp[node_index] == &scene->physics_handles[node_index])
                {
                    u32 h_cur = scene->physics_handles[node_index];
                    u32 h_prev = *(u32*)ns.components[i];

                    if (h_prev == 0 && h_cur)
                    {
                        // release previous physics handle
                        physics::release_entity(h_cur);
                    }
                }

                if (cmp[node_index] == &scene->physics_handles[node_index])
                {
                    u32 h_cur = scene->physics_handles[node_index];
                    u32 h_prev = *(u32*)ns.components[i];

                    if (h_prev == 0 && h_cur)
                    {
                        // release previous physics handle
                        physics::release_entity(h_cur);
                    }
                }

                memcpy(cmp[node_index], ns.components[i], cmp.size);
            }

            node_state& us = s_editor_nodes[node_index].action_state[e_editor_actions::undo];
            node_state& rs = s_editor_nodes[node_index].action_state[e_editor_actions::redo];

            free_node_state_mem(scene, us);
            free_node_state_mem(scene, rs);
        }

        void restore_from_stack(ecs_scene* scene, action_stack& stack, action_stack& reverse, editor_actions action)
        {
            if (stack.size() <= 0)
                return;

            int macro_count = 0;
            for (;;)
            {
                editor_action ua = stack.pop();
                reverse.push(ua);

                if (ua.node_index < 0)
                {
                    macro_count++;
                    if (macro_count == 2)
                        break;

                    continue;
                }

                if (scene->state_flags[ua.node_index] & e_state::selected)
                    sb_clear(scene->selection_list);

                restore_node_state(scene, ua.action_state[action], ua.node_index);
            }
        }

        void undo(ecs_scene* scene)
        {
            restore_from_stack(scene, s_undo_stack, s_redo_stack, e_editor_actions::undo);
        }

        void redo(ecs_scene* scene)
        {
            restore_from_stack(scene, s_redo_stack, s_undo_stack, e_editor_actions::redo);
        }

        void update_undo_stack(ecs_scene* scene, f32 dt)
        {
            // resize buffers
            if (!s_editor_nodes || sb_count(s_editor_nodes) < scene->soa_size)
            {
                stb__sbgrow(s_editor_nodes, scene->soa_size);
                stb__sbm(s_editor_nodes) = scene->num_entities;
                stb__sbn(s_editor_nodes) = scene->soa_size;
            }

            static editor_action macro_begin;
            static editor_action macro_end;

            macro_begin.node_index = e_editor_actions::macro_begin;
            macro_end.node_index = e_editor_actions::macro_end;

            bool first_item = true;

            for (u32 i = 0; i < scene->soa_size; ++i)
            {
                if (s_editor_nodes[i].action_state[e_editor_actions::redo].components)
                {
                    if (s_editor_nodes[i].timer <= 0.0f)
                    {
                        if (first_item)
                        {
                            s_redo_stack.clear();
                            s_undo_stack.push(macro_end);
                            first_item = false;
                        }

                        s_editor_nodes[i].node_index = i;
                        s_undo_stack.push(s_editor_nodes[i]);
                        s_editor_nodes[i].action_state[e_editor_actions::undo].components = nullptr;
                        s_editor_nodes[i].action_state[e_editor_actions::redo].components = nullptr;
                    }

                    s_editor_nodes[i].timer -= dt * 0.1f;
                }
            }

            if (!first_item)
                s_undo_stack.push(macro_begin);

            // undo / redo
            static bool debounce_undo = false;
            if (pen::input_undo_pressed())
            {
                if (!debounce_undo)
                {
                    debounce_undo = true;
                    undo(scene);
                }
            }
            else if (debounce_undo)
            {
                debounce_undo = false;
            }

            static bool debounce_redo = false;
            if (pen::input_redo_pressed())
            {
                if (!debounce_redo)
                {
                    debounce_redo = true;
                    redo(scene);
                }
            }
            else if (debounce_redo)
            {
                debounce_redo = false;
            }
        }

        void settings_ui(bool* opened)
        {
            static bool set_project_dir = false;

            static bool load_last_scene = dev_ui::get_program_preference("load_last_scene").as_bool();
            static Str  project_dir_str = dev_ui::get_program_preference_filename("project_dir");

            static bool dynamic_timestep = dev_ui::get_program_preference("dynamic_timestep").as_bool(true);
            static f32  fixed_timestep = dev_ui::get_program_preference("fixed_timestep").as_f32(1.0f / 60.0f);

            if (ImGui::Begin("Settings", opened))
            {
                Str setting_str;
                if (ImGui::Checkbox("Invert Camera Y", &s_model_view_controller.settings.invert_y))
                {
                    dev_ui::set_program_preference("invert_camera_y", s_model_view_controller.settings.invert_y);
                }

                if (ImGui::Checkbox("Invert Camera X", &s_model_view_controller.settings.invert_x))
                {
                    dev_ui::set_program_preference("invert_camera_x", s_model_view_controller.settings.invert_x);
                }

                if (ImGui::SliderFloat("Camera Zoom Speed", &s_model_view_controller.settings.zoom_speed, 0.0f, 10.0f))
                {
                    dev_ui::set_program_preference("camera_zoom_speed", s_model_view_controller.settings.zoom_speed);
                }

                if (ImGui::Checkbox("Auto Load Last Scene", &load_last_scene))
                {
                    dev_ui::set_program_preference("load_last_scene", load_last_scene);
                }

                if (ImGui::Checkbox("Dynamic Timestep", &dynamic_timestep))
                {
                    dev_ui::set_program_preference("dynamic_timestep", dynamic_timestep);
                }

                if (!dynamic_timestep)
                {
                    if (ImGui::InputFloat("Fixed Timestep", &fixed_timestep))
                    {
                        dev_ui::set_program_preference("fixed_timestep", fixed_timestep);
                    }
                }

                if (ImGui::Button("Set Project Dir"))
                {
                    set_project_dir = true;
                }

                ImGui::SameLine();
                ImGui::Text("%s", project_dir_str.c_str());

                ImGui::End();
            }

            if (set_project_dir)
            {
                const c8* set_proj = put::dev_ui::file_browser(set_project_dir, dev_ui::e_file_browser_flags::open, 1, "**.");

                if (set_proj)
                {
                    project_dir_str = set_proj;
                    dev_ui::set_program_preference_filename("project_dir", project_dir_str);
                }
            }
        }

        void context_menu_ui(ecs_scene* scene)
        {
            if(!can_edit(dev_ui::e_io_capture::mouse))
                return;
                
            static ImGuiID cm_id = ImGui::GetID("right click context menu");

            if (pen::input_mouse(1))
            {
                ImGui::OpenPopupEx(cm_id, false);
            }

            if (ImGui::BeginPopupEx(cm_id, ImGuiWindowFlags_ShowBorders | ImGuiWindowFlags_AlwaysAutoResize))
            {
                if (sb_count(scene->selection_list) == 1)
                {
                    if (ImGui::MenuItem("Save Selection Hierarchy"))
                    {
                        save_sub_scene(scene, scene->selection_list[0]);
                    }
                }

                if (ImGui::MenuItem("Hide All"))
                {
                    for (u32 i = 0; i < scene->num_entities; ++i)
                    {
                        scene->state_flags[i] |= e_state::hidden;
                    }
                }

                if (ImGui::MenuItem("Unhide All"))
                {
                    for (u32 i = 0; i < scene->num_entities; ++i)
                    {
                        scene->state_flags[i] &= ~e_state::hidden;
                    }
                }

                if (sb_count(scene->selection_list) > 0)
                {
                    if (ImGui::MenuItem("Hide Selected"))
                    {
                        for (u32 i = 0; i < scene->num_entities; ++i)
                        {
                            if (scene->state_flags[i] & e_state::selected)
                                scene->state_flags[i] |= e_state::hidden;
                        }
                    }

                    if (ImGui::MenuItem("Hide Un-Selected"))
                    {
                        for (u32 i = 0; i < scene->num_entities; ++i)
                        {
                            if (!(scene->state_flags[i] & e_state::selected))
                                scene->state_flags[i] |= e_state::hidden;
                        }
                    }
                }

                ImGui::EndPopup();
            }
        }

        void editor_update(ecs_controller& ecsc, ecs_scene* scene, f32 dt)
        {
            if (!ecsc.camera)
                return;

            if (s_editor_enabled)
                update_editor_scene(ecsc, scene, dt);

            if (s_editor_enable_camera)
                update_editor_camera(ecsc.camera);
        }

        void editor_enable(bool enable)
        {
            s_editor_enabled = enable;
        }

        void editor_enable_camera(bool enable)
        {
            s_editor_enable_camera = enable;
        }
        
        void editor_lock_edits(u32 flags)
        {
            s_editor_lock_flags = flags;
        }

        void update_editor_scene(ecs_controller& ecsc, ecs_scene* scene, f32 dt)
        {
            static bool open_scene_browser = false;
            static bool open_merge = false;
            static bool open_open = false;
            static bool open_save = false;
            static bool open_save_as = false;
            static bool open_camera_menu = false;
            static bool open_resource_menu = false;
            static bool dev_open = false;
            static bool selection_list = false;
            static bool view_menu = false;
            static bool settings_open = false;

            // right click context menu
            context_menu_ui(scene);

            ImGui::BeginMainMenuBar();

            if (ImGui::BeginMenu(ICON_FA_LEMON_O))
            {
                if (ImGui::MenuItem("New"))
                {
                    clear_scene(scene);
                    default_scene(scene);
                }

                if (ImGui::MenuItem("Open", NULL, &open_open))
                {
                    open_merge = false;
                }
                if (ImGui::MenuItem("Import", NULL, &open_open))
                {
                    open_merge = true;
                }

                if (ImGui::MenuItem("Save", NULL, &open_save))
                {
                    open_save_as = false;
                }

                if (ImGui::MenuItem("Save As", nullptr, &open_save))
                {
                    open_save_as = true;
                }

                bool co = dev_ui::is_console_open();
                ImGui::MenuItem("Console", nullptr, &co);
                dev_ui::show_console(co);

                ImGui::MenuItem("Settings", nullptr, &settings_open);
                ImGui::MenuItem("Dev", nullptr, &dev_open);

                ImGui::EndMenu();
            }

            // play pause
            if (scene->flags & e_scene_flags::pause_update)
            {
                if (ImGui::Button(ICON_FA_PLAY))
                    scene->flags &= (~scene->flags);
            }
            else
            {
                if (ImGui::Button(ICON_FA_PAUSE))
                    scene->flags |= e_scene_flags::pause_update;
            }

            static bool debounce_pause = false;
            if (shortcut_key(PK_SPACE))
            {
                if (!debounce_pause)
                {
                    if (!(scene->flags & e_scene_flags::pause_update))
                        scene->flags |= e_scene_flags::pause_update;
                    else
                        scene->flags &= ~e_scene_flags::pause_update;

                    debounce_pause = true;
                }
            }
            else
            {
                debounce_pause = false;
            }

            if (shortcut_key(PK_O))
            {
                // reset physics positions
                for (s32 i = 0; i < scene->num_entities; ++i)
                {
                    if (scene->entities[i] & e_cmp::physics)
                    {
                        if (scene->physics_data[i].type != e_physics_type::rigid_body)
                            continue;

                        vec3f t = scene->physics_data[i].rigid_body.position;
                        quat  q = scene->physics_data[i].rigid_body.rotation;

                        physics::set_transform(scene->physics_handles[i], t, q);

                        scene->transforms[i].translation = t;
                        scene->transforms[i].rotation = q;

                        scene->entities[i] |= e_cmp::transform;

                        // reset velocity
                        physics::set_v3(scene->physics_handles[i], vec3f::zero(), physics::e_cmd::set_linear_velocity);
                        physics::set_v3(scene->physics_handles[i], vec3f::zero(), physics::e_cmd::set_angular_velocity);
                    }
                }
            }

            if (ImGui::Button(ICON_FA_FLOPPY_O))
            {
                open_save = true;
                open_save_as = false;
            }
            dev_ui::set_tooltip("Save");

            if (ImGui::Button(ICON_FA_FOLDER_OPEN))
            {
                open_open = true;
            }
            dev_ui::set_tooltip("Open");

            if (ImGui::Button(ICON_FA_SEARCH))
            {
                open_scene_browser = true;
            }
            dev_ui::set_tooltip("Scene Browser");

            if (ImGui::Button(ICON_FA_EYE))
            {
                view_menu = true;
            }
            dev_ui::set_tooltip("View Settings");

            if (ImGui::Button(ICON_FA_VIDEO_CAMERA))
            {
                open_camera_menu = true;
            }
            dev_ui::set_tooltip("Camera Settings");

            if (ImGui::Button(ICON_FA_CUBES))
            {
                open_resource_menu = true;
            }
            dev_ui::set_tooltip("Resource Browser");

            ImGui::Separator();

            if (ImGui::Button(ICON_FA_UNDO))
            {
                undo(scene);
            }
            dev_ui::set_tooltip("Undo");

            if (ImGui::Button(ICON_FA_REPEAT))
            {
                redo(scene);
            }
            dev_ui::set_tooltip("Redo");

            if (ImGui::Button(ICON_FA_FILES_O))
            {
                clone_selection_hierarchical(scene, &scene->selection_list, "_cloned");
            }
            dev_ui::set_tooltip("Duplicate");

            ImGui::Separator();

            if (ImGui::Button(ICON_FA_LIST))
            {
                selection_list = true;
            }
            dev_ui::set_tooltip("Selection List");

            // clang-format off
            static const c8* transform_icons[] = {
                ICON_FA_MOUSE_POINTER,
                ICON_FA_ARROWS,
                ICON_FA_REFRESH,
                ICON_FA_EXPAND,
                ICON_FA_HAND_POINTER_O
            };

            static const c8* transform_tooltip[] = {
                "Select (Q)",
                "Translate (W)",
                "Rotate (E)",
                "Scale (R)",
                "Grab Physics (T)"
            };
            
            static u32 widget_shortcut_key[] = {
                PK_Q,
                PK_W,
                PK_E,
                PK_R,
                PK_T
            };
            // clang-format on

            static s32 num_transform_icons = PEN_ARRAY_SIZE(transform_icons);

            static_assert(PEN_ARRAY_SIZE(transform_tooltip) == PEN_ARRAY_SIZE(transform_icons), "mistmatched elements");
            static_assert(PEN_ARRAY_SIZE(widget_shortcut_key) == PEN_ARRAY_SIZE(transform_tooltip), "mismatched elements");

            for (s32 i = 0; i < num_transform_icons; ++i)
            {
                u32 mode = e_transform_mode::select + i;
                if (shortcut_key(widget_shortcut_key[i]))
                    s_transform_mode = (transform_mode)mode;

                if (put::dev_ui::state_button(transform_icons[i], s_transform_mode == mode))
                {
                    if (s_transform_mode == mode)
                        s_transform_mode = e_transform_mode::none;
                    else
                        s_transform_mode = (transform_mode)mode;
                }
                put::dev_ui::set_tooltip(transform_tooltip[i]);
            }

            if (ImGui::Button(ICON_FA_LINK) || shortcut_key(PK_P))
            {
                parent_selection(scene);
            }
            put::dev_ui::set_tooltip("Parent (P)");

            if (shortcut_key(PK_I))
            {
                instance_selection(scene);
            }

            ImGui::Separator();

            ImGui::EndMainMenuBar();

            if (open_open)
            {
                const c8* import =
                    put::dev_ui::file_browser(open_open, dev_ui::e_file_browser_flags::open, 3, "**.pmm", "**.pms", "**.pmv");

                if (import)
                {
                    u32 len = pen::string_length(import);

                    char pm = import[len - 1];
                    switch (pm)
                    {
                        case 'm':
                            put::ecs::load_pmm(import, scene);
                            break;
                        case 's':
                        {
                            put::ecs::load_scene(import, scene, open_merge);

                            if (!open_merge)
                                s_model_view_controller.current_working_scene = import;

                            Str fn = import;
                            dev_ui::set_program_preference_filename("last_loaded_scene", import);
                        }
                        break;
                        case 'v':
                            put::ecs::load_pmv(import, scene);
                            break;
                        default:
                            break;
                    }
                }
            }

            if (open_scene_browser)
            {
                ecs::scene_browser_ui(scene, &open_scene_browser);
            }

            if (open_camera_menu)
            {
                if (ImGui::Begin("Camera", &open_camera_menu))
                {
                    ImGui::Combo("Camera Mode", (s32*)&s_model_view_controller.mode, (const c8**)&k_camera_mode_names, 2);

                    if (ImGui::SliderFloat("FOV", &s_model_view_controller.main_camera.fov, 10, 180))
                        dev_ui::set_program_preference("camera_fov", s_model_view_controller.main_camera.fov);

                    if (ImGui::InputFloat("Near", &s_model_view_controller.main_camera.near_plane))
                        dev_ui::set_program_preference("camera_near", s_model_view_controller.main_camera.near_plane);

                    if (ImGui::InputFloat("Far", &s_model_view_controller.main_camera.far_plane))
                        dev_ui::set_program_preference("camera_far", s_model_view_controller.main_camera.far_plane);

                    ImGui::InputFloat("Zoom", &s_model_view_controller.main_camera.zoom);
                    ImGui::InputFloat2("Rotation", (f32*)&s_model_view_controller.main_camera.rot[0]);
                    ImGui::InputFloat3("Focus", (f32*)&s_model_view_controller.main_camera.focus);

                    s_model_view_controller.invalidated = true;

                    ImGui::End();
                }
            }

            if (open_resource_menu)
            {
                put::ecs::enumerate_resources(&open_resource_menu);
            }

            if (open_save)
            {
                const c8* save_file = nullptr;
                if (open_save_as || s_model_view_controller.current_working_scene.length() == 0)
                {
                    save_file = put::dev_ui::file_browser(open_save, dev_ui::e_file_browser_flags::save, 1, "**.pms");
                }
                else
                {
                    save_file = s_model_view_controller.current_working_scene.c_str();
                }

                if (save_file)
                {
                    put::ecs::save_scene(save_file, scene);
                    s_model_view_controller.current_working_scene = save_file;
                    open_save = false;

                    dev_ui::set_program_preference_filename("last_loaded_scene", save_file);
                }
            }

            if (dev_open)
            {
                if (ImGui::Begin("Dev", &dev_open))
                {
                    if (ImGui::CollapsingHeader("Icons"))
                    {
                        debug_show_icons();
                    }

                    ImGui::End();
                }
            }

            if (settings_open)
                settings_ui(&settings_open);

            // disable selection when we are doing something else
            static bool disable_picking = false;
            if (pen::input_key(PK_MENU) || pen::input_key(PK_COMMAND) || (s_select_flags & e_select_flags::widget_selected) ||
                (s_transform_mode == e_transform_mode::physics))
            {
                disable_picking = true;
            }
            else
            {
                if (!pen::input_mouse(PEN_MOUSE_L))
                    disable_picking = false;
            }

            // selection / picking
            if (!disable_picking)
            {
                picking_update(scene, ecsc.camera);
            }

            if (selection_list)
            {
                enumerate_selection_ui(scene, &selection_list);
            }

            // duplicate
            static bool debounce_duplicate = false;
            if (pen::input_key(PK_CONTROL) && shortcut_key(PK_D))
            {
                debounce_duplicate = true;
            }
            else if (debounce_duplicate)
            {
                clone_selection_hierarchical(scene, &scene->selection_list, "_cloned");
                debounce_duplicate = false;
            }

            // delete
            if (shortcut_key(PK_DELETE) || shortcut_key(PK_BACK))
                delete_selection(scene);

            if (view_menu)
                view_ui(scene, &view_menu);

            update_undo_stack(scene, dt * 1000.0);
        }

        struct physics_preview
        {
            bool          active = false;
            cmp_physics   params;
            cmp_transform offset;

            physics_preview(){};
            ~physics_preview(){};
        };
        static physics_preview s_physics_preview;

        void scene_constraint_ui(ecs_scene* scene)
        {
            physics::constraint_params& preview_constraint = s_physics_preview.params.constraint;
            s32                         constraint_type = preview_constraint.type - 1;

            s_physics_preview.params.type = e_physics_type::constraint;

            ImGui::Combo("Constraint##Physics", (s32*)&constraint_type, "Six DOF\0Hinge\0Point to Point\0", 7);

            preview_constraint.type = constraint_type + 1;
            if (preview_constraint.type == physics::e_constraint::hinge)
            {
                ImGui::InputFloat3("Axis", (f32*)&preview_constraint.axis);
                ImGui::InputFloat("Lower Limit Rotation", (f32*)&preview_constraint.lower_limit_rotation);
                ImGui::InputFloat("Upper Limit Rotation", (f32*)&preview_constraint.upper_limit_rotation);
            }
            else if (preview_constraint.type == physics::e_constraint::dof6)
            {
                ImGui::InputFloat3("Lower Limit Rotation", (f32*)&preview_constraint.lower_limit_rotation);
                ImGui::InputFloat3("Upper Limit Rotation", (f32*)&preview_constraint.upper_limit_rotation);

                ImGui::InputFloat3("Lower Limit Translation", (f32*)&preview_constraint.lower_limit_translation);
                ImGui::InputFloat3("Upper Limit Translation", (f32*)&preview_constraint.upper_limit_translation);

                ImGui::InputFloat("Linear Damping", (f32*)&preview_constraint.linear_damping);
                ImGui::InputFloat("Angular Damping", (f32*)&preview_constraint.angular_damping);
            }

            std::vector<u32>       index_lookup;
            std::vector<const c8*> rb_names;
            for (s32 i = 0; i < scene->num_entities; ++i)
            {
                if (scene->entities[i] & e_cmp::physics)
                {
                    rb_names.push_back(scene->names[i].c_str());
                    index_lookup.push_back(scene->physics_handles[i]);
                }
            }

            static s32 rb_index = -1;
            if (rb_names.size() > 0)
                ImGui::Combo("Rigid Body##Constraint", &rb_index, (const c8* const*)&rb_names[0], rb_names.size());
            else
                ImGui::Text("No rigid bodies to constrain!");

            bool valid_type = preview_constraint.type <= physics::e_constraint::p2p && preview_constraint.type > 0;
            if (rb_index > -1 && valid_type)
            {
                preview_constraint.rb_indices[0] = index_lookup[rb_index];

                if (ImGui::Button("Add"))
                {
                    u32 sel_num = sb_count(scene->selection_list);
                    for (u32 s = 0; s < sel_num; ++s)
                    {
                        u32 i = scene->selection_list[s];

                        if (scene->entities[i] & e_cmp::constraint)
                            continue;

                        scene->physics_data[i].constraint = preview_constraint;

                        instantiate_constraint(scene, i);
                    }
                }
            }

            if (sb_count(scene->selection_list) == 1)
                scene->physics_data[scene->selection_list[0]].constraint = preview_constraint;
        }

        void scene_rigid_body_ui(ecs_scene* scene)
        {
            // shape, mass, group etc
            u32 collision_shape = s_physics_preview.params.rigid_body.shape - 1;

            ImGui::InputFloat("Mass", &s_physics_preview.params.rigid_body.mass);
            ImGui::Combo("Shape##Physics", (s32*)&collision_shape,
                         "Box\0Cylinder\0Sphere\0Capsule\0Cone\0Hull\0Mesh\0Compound\0", 7);

            s_physics_preview.params.rigid_body.shape = physics::shape_type(collision_shape + 1);

            ImGui::InputInt("Group", (s32*)&s_physics_preview.params.rigid_body.group);
            ImGui::InputInt("Mask", (s32*)&s_physics_preview.params.rigid_body.mask);

            // transform info / use geom
            bool cfg = s_physics_preview.params.rigid_body.create_flags == 0;
            ImGui::Checkbox("Create Dimensions Geometry", &cfg);

            if (!cfg)
            {
                s_physics_preview.params.rigid_body.create_flags |= physics::e_create_flags::set_all_transform;
                ImGui::InputFloat3("Dimensions", &s_physics_preview.params.rigid_body.dimensions[0]);
            }
            else
            {
                s_physics_preview.params.rigid_body.create_flags = 0;
            }

            ImGui::InputFloat3("Offset", &s_physics_preview.offset.translation[0]);

            // check if an instance exists or is new
            Str button_text = "Set Start Transform";
            u32 sel_num = sb_count(scene->selection_list);
            for (u32 s = 0; s < sel_num; ++s)
            {
                u32 i = scene->selection_list[s];

                if (!(scene->entities[i] & e_cmp::physics))
                {
                    button_text = "Add";
                    break;
                }
            }

            // create
            if (ImGui::Button(button_text.c_str()))
            {
                for (u32 s = 0; s < sel_num; ++s)
                {
                    u32 i = scene->selection_list[s];
                    scene->physics_data[i].rigid_body = s_physics_preview.params.rigid_body;
                    scene->physics_offset[i].translation = s_physics_preview.offset.translation;

                    if (!(scene->entities[i] & e_cmp::physics))
                        instantiate_rigid_body(scene, i);

                    s_physics_preview.params.rigid_body = scene->physics_data[i].rigid_body;
                }
            }
        }

        void scene_physics_ui(ecs_scene* scene)
        {
            static s32 physics_type = 0;
            s_physics_preview.active = false;

            u32 num_selected = sb_count(scene->selection_list);

            if (num_selected == 0)
                return;

            if (num_selected == 1)
            {
                s_physics_preview.params = scene->physics_data[scene->selection_list[0]];
                physics_type = s_physics_preview.params.type;
            }

            if (ImGui::CollapsingHeader("Physics"))
            {
                s_physics_preview.active = true;

                // Delete selection if all have physics
                bool del_button = true;
                u32  sel_num = sb_count(scene->selection_list);
                for (u32 s = 0; s < sel_num; ++s)
                {
                    u32 i = scene->selection_list[s];

                    if (!(scene->entities[i] & e_cmp::physics))
                    {
                        del_button = false;
                        break;
                    }
                }

                if (del_button)
                {
                    ImGui::PushID("physics");

                    if (ImGui::Button(ICON_FA_TRASH))
                    {
                        for (u32 s = 0; s < sel_num; ++s)
                        {
                            u32 si = scene->selection_list[s];

                            destroy_physics(scene, si);
                        }
                    }

                    ImGui::PopID();
                }

                ImGui::Combo("Type##Physics", &physics_type, "Rigid Body\0Constraint\0");

                s_physics_preview.params.type = physics_type;

                if (physics_type == e_physics_type::rigid_body)
                {
                    // rb
                    scene_rigid_body_ui(scene);
                }
                else if (physics_type == e_physics_type::constraint)
                {
                    // constraint
                    scene_constraint_ui(scene);
                }
            }

            if (sb_count(scene->selection_list) == 1)
                scene->physics_data[scene->selection_list[0]] = s_physics_preview.params;
        }

        bool scene_geometry_ui(ecs_scene* scene)
        {
            bool iv = false;

            if (sb_count(scene->selection_list) != 1)
                return iv;

            u32 selected_index = scene->selection_list[0];

            // geom
            if (ImGui::CollapsingHeader("Geometry"))
            {
                if (scene->entities[selected_index] & e_cmp::geometry)
                {
                    ImGui::PushID("geom");

                    if (ImGui::Button(ICON_FA_TRASH))
                        destroy_geometry(scene, selected_index);
                    ImGui::SameLine();

                    ImGui::Text("Geometry Name: %s", scene->geometry_names[selected_index].c_str());

                    put::dev_ui::set_tooltip("Delete Geometry");
                    ImGui::PopID();

                    return iv;
                }

                static s32 primitive_type = -1;
                ImGui::Combo("Shape##Primitive", (s32*)&primitive_type, "Quad\0Box\0Cylinder\0Sphere\0Capsule\0Cone\0",
                             PEN_ARRAY_SIZE(ID_PRIMITIVE));

                if (ImGui::Button("Add Primitive") && primitive_type > -1)
                {
                    iv = true;

                    geometry_resource* gr = get_geometry_resource(ID_PRIMITIVE[primitive_type]);

                    instantiate_geometry(gr, scene, selected_index);

                    material_resource* mr = get_material_resource(PEN_HASH("default_material"));

                    instantiate_material(mr, scene, selected_index);

                    scene->geometry_names[selected_index] = gr->geometry_name;

                    instantiate_model_cbuffer(scene, selected_index);

                    scene->entities[selected_index] |= e_cmp::transform;
                }
            }

            return iv;
        }

        void scene_options_ui(ecs_scene* scene)
        {
            if (sb_count(scene->selection_list) <= 0)
                return;

            bool visible = true;

            u32 num_selected = sb_count(scene->selection_list);

            for (u32 i = 0; i < num_selected; ++i)
            {
                u32 s = scene->selection_list[i];
                if (scene->state_flags[s] & e_state::hidden)
                    visible = false;
            }

            if (ImGui::Checkbox("Visible", &visible))
            {
                for (u32 i = 0; i < num_selected; ++i)
                {
                    u32 s = scene->selection_list[i];
                    if (!visible)
                    {
                        scene->state_flags[s] |= e_state::hidden;
                    }
                    else
                    {
                        scene->state_flags[s] &= ~e_state::hidden;
                    }
                }
            }
        }

        void scene_components_ui(ecs_scene* scene)
        {
            if (sb_count(scene->selection_list) != 1)
                return;

            u32 selected_index = scene->selection_list[0];

            // clang-format off
            const c8* component_flag_names[] = {
                "Allocated",
                "Geometry",
                "Physics",
                "Physics Multi Body",
                "Material",
                "Hand",
                "Skinned",
                "Bone",
                "Dynamic",
                "Animation Controller",
                "Animation Trajectory",
                "Light",
                "Transform",
                "Constraint",
                "Sub Instance",
                "Master Instance",
                "Pre Skinned",
                "Sub Geometry",
                "Signed Distance Field Shadow",
                "Volume",
                "Samplers"
            };
            // clang-format on

            if (ImGui::CollapsingHeader("Components"))
            {
                Str components = "";
                for (u32 i = 0; i < PEN_ARRAY_SIZE(component_flag_names); ++i)
                {
                    if (scene->entities[selected_index] & (u64)((u64)1 << (u64)i))
                    {
                        if (i > 0)
                            components.append(" | ");

                        components.append(component_flag_names[i]);
                    }
                }

                ImGui::TextWrapped("%s", components.c_str());
            }
        }

        bool scene_hierarchy_ui(ecs_scene* scene)
        {
            if (sb_count(scene->selection_list) != 1)
                return false;

            u32 si = scene->selection_list[0];

            if (ImGui::CollapsingHeader("Hierarchy"))
            {
                s32 p = scene->parents[si];
                ImGui::InputInt("Parent", &p);

                if (p != scene->parents[si])
                    set_entity_parent(scene, p, si);
            }

            return true;
        }

        bool scene_transform_ui(ecs_scene* scene)
        {
            if (sb_count(scene->selection_list) != 1)
                return false;

            u32 selected_index = scene->selection_list[0];

            if (ImGui::CollapsingHeader("Transform"))
            {
                bool           perform_transform = false;
                cmp_transform& t = scene->transforms[selected_index];
                perform_transform |= ImGui::InputFloat3("Translation", (float*)&t.translation);

                vec3f euler = t.rotation.to_euler();
                for (u32 i = 0; i < 3; ++i)
                    euler[i] = maths::rad_to_deg(euler[i]);

                if (ImGui::InputFloat3("Rotation", (float*)&euler))
                {
                    vec3f euler_rad;
                    for (u32 i = 0; i < 3; ++i)
                        euler_rad[i] = maths::deg_to_rad(euler[i]);

                    t.rotation.euler_angles(euler_rad.z, euler_rad.y, euler_rad.x);

                    perform_transform = true;
                }

                perform_transform |= ImGui::InputFloat3("Scale", (float*)&t.scale);

                if (perform_transform)
                {
                    scene->entities[selected_index] |= e_cmp::transform;
                }

                apply_transform_to_selection(scene, vec3f::zero());
            }

            // transforms undos are handled by apply_transform_to_selection
            return false;
        }

        bool scene_bounds_ui(ecs_scene* scene)
        {
            if (sb_count(scene->selection_list) != 1)
                return false;

            u32 si = scene->selection_list[0];

            if (ImGui::CollapsingHeader("Bounds"))
            {
                ImGui::InputFloat3("Min", &scene->bounding_volumes[si].min_extents[0]);
                ImGui::InputFloat3("Max", &scene->bounding_volumes[si].max_extents[0]);
                ImGui::InputFloat3("Transformed Min", &scene->bounding_volumes[si].transformed_min_extents[0]);
                ImGui::InputFloat3("Transformed Min", &scene->bounding_volumes[si].transformed_max_extents[0]);
                ImGui::InputFloat("Radius", &scene->bounding_volumes[si].radius);
            }

            return true;
        }

        bool scene_material_ui(ecs_scene* scene)
        {
            bool iv = false;

            u32 num_selected = sb_count(scene->selection_list);
            if (num_selected <= 0)
                return false;

            u32 selected_index = scene->selection_list[0];
            if (!(scene->entities[selected_index] & e_cmp::material))
                return false;

            if (!ImGui::CollapsingHeader("Material"))
                return false;

            // master mat
            cmp_material&      mm = scene->materials[selected_index];
            material_resource& mr = scene->material_resources[selected_index];
            cmp_material_data  mat = scene->material_data[selected_index];
            cmp_samplers       samp = scene->samplers[selected_index];
            u32                perm = scene->material_permutation[selected_index];

            // multi parameters
            s32 shader = mm.shader;
            s32 technique_list_index = pmfx::get_technique_list_index(shader, mr.id_technique);

            // set parameters if all are shared, if not set to invalid
            for (u32 i = 1; i < num_selected; ++i)
            {
                cmp_material&      m2 = scene->materials[scene->selection_list[i]];
                material_resource& mr2 = scene->material_resources[scene->selection_list[i]];

                if (shader != m2.shader)
                {
                    // mismatched shader and techniques selected
                    shader = PEN_INVALID_HANDLE;
                    technique_list_index = PEN_INVALID_HANDLE;
                }
                else
                {
                    u32 m2_tli = pmfx::get_technique_list_index(shader, mr2.id_technique);

                    // mismatched techniques
                    if (technique_list_index != m2_tli)
                        technique_list_index = PEN_INVALID_HANDLE;
                }
            }

            ImGui::Text("%s", scene->material_names[selected_index].c_str());
            ImGui::Separator();

            bool cs = false;
            bool ct = false;

            u32        num_shaders;
            const c8** shader_list = pmfx::get_shader_list(num_shaders);
            cs |= ImGui::Combo("Shader", (s32*)&shader, shader_list, num_shaders);

            u32        num_techniques;
            const c8** technique_list = pmfx::get_technique_list(mm.shader, num_techniques);
            ct |= ImGui::Combo("Technique", (s32*)&technique_list_index, technique_list, num_techniques);

            bool rebake = false;

            // apply shader changes
            if (cs)
            {
                // changing shader will leave us with a bunk technique
                // choose technique 0

                hash_id id_shader = PEN_HASH(shader_list[shader]);

                u32 new_shader = pmfx::load_shader(shader_list[shader]);

                hash_id id_technique = pmfx::get_technique_id(new_shader, 0);

                for (u32 i = 0; i < num_selected; ++i)
                {
                    u32 si = scene->selection_list[i];

                    scene->material_resources[si].id_technique = id_technique;
                    scene->material_resources[si].id_shader = id_shader;
                    scene->material_resources[si].shader_name = shader_list[shader];
                }

                rebake = true;
            }

            // apply technique changes
            if (ct)
            {
                // changing technique - set id technique in mat resource
                hash_id id_technique = PEN_HASH(technique_list[technique_list_index]);

                for (u32 i = 0; i < num_selected; ++i)
                {
                    u32 si = scene->selection_list[i];

                    scene->material_resources[si].id_technique = id_technique;
                }

                rebake = true;
            }

            // display technique ui if valid
            if (technique_list_index != PEN_INVALID_HANDLE && !rebake)
            {
                rebake |= pmfx::show_technique_ui(shader, mm.technique_index, &mat.data[0], samp, &perm);

                pmfx::technique_constant* tc = pmfx::get_technique_constants(shader, mm.technique_index);
                pmfx::technique_sampler*  ts = pmfx::get_technique_samplers(shader, mm.technique_index);

                cmp_material_data pre_edit_tc = scene->material_data[selected_index];
                u32               pre_edit_perm = scene->material_permutation[selected_index];
                cmp_samplers      pre_edit_samp = scene->samplers[selected_index];

                // set material edits on multiple edited entities
                for (u32 i = 0; i < num_selected; ++i)
                {
                    u32 si = scene->selection_list[i];

                    // technique constants
                    if (tc)
                    {
                        u32 num_constants = sb_count(tc);
                        for (u32 c = 0; c < num_constants; ++c)
                        {
                            u32 cb_offset = tc[c].cb_offset;
                            u32 tc_size = sizeof(f32) * tc[c].num_elements;

                            f32* f1 = &mat.data[cb_offset];
                            f32* f2 = &pre_edit_tc.data[cb_offset];

                            if (memcmp(f1, f2, tc_size) == 0)
                                continue;

                            f32* f3 = &scene->material_data[si].data[cb_offset];
                            memcpy(f3, f1, tc_size);
                        }
                    }

                    // samplers
                    if (ts)
                    {
                        u32 num_samplers = sb_count(ts);
                        for (u32 s = 0; s < num_samplers; ++s)
                        {
                            if (memcmp(&samp.sb[s], &pre_edit_samp.sb[s], sizeof(sampler_binding)) == 0)
                                continue;

                            memcpy(&scene->samplers[si].sb[s], &samp.sb[s], sizeof(sampler_binding));
                        }
                    }

                    // permutation
                    if (perm != pre_edit_perm)
                    {
                        scene->material_permutation[si] = perm;
                    }
                }
            }

            // rebake all selected handles
            if (rebake)
            {
                for (u32 i = 0; i < num_selected; ++i)
                {
                    u32 si = scene->selection_list[i];
                    ecs::bake_material_handles(scene, si);
                }
            }

            return iv;
        }
        
        void scene_anim_ui(ecs_scene* scene)
        {
            if (sb_count(scene->selection_list) != 1)
                return;

            u32 selected_index = scene->selection_list[0];
            
            if (scene->geometries[selected_index].p_skin)
            {
                if (ImGui::CollapsingHeader("Animations"))
                {
                    auto& controller = scene->anim_controller_v2[selected_index];
                    ImGui::InputInt("Joint Offset", (s32*)&controller.joints_offset);

                    u32 num_anims = sb_count(controller.anim_instances);
                    for(u32 i = 0; i < num_anims; ++i)
                    {
                        auto res = get_animation_resource(controller.anim_instance_handles[i]);
                        ImGui::Text("%s", res->name.c_str());
                    }
                }
            }
        }

        void scene_anim_ui_v1(ecs_scene* scene)
        {
#if 0
            if (sb_count(scene->selection_list) != 1)
                return;

            u32 selected_index = scene->selection_list[0];

            if (scene->geometries[selected_index].p_skin)
            {
                static bool open_anim_import = false;

                if (ImGui::CollapsingHeader("Animations"))
                {
                    auto& controller = scene->anim_controller[selected_index];

                    ImGui::Checkbox("Apply Root Motion", &scene->anim_controller[selected_index].apply_root_motion);

                    if (ImGui::Button("Add Animation"))
                        open_anim_import = true;

                    if (ImGui::Button("Reset Root Motion"))
                    {
                        scene->local_matrices[selected_index].create_identity();
                    }

                    s32 num_anims = sb_count(scene->anim_controller[selected_index].handles);
                    for (s32 ih = 0; ih < num_anims; ++ih)
                    {
                        s32   h = scene->anim_controller[selected_index].handles[ih];
                        auto* anim = get_animation_resource(h);

                        bool selected = false;
                        ImGui::Selectable(anim->name.c_str(), &selected);

                        if (selected)
                            controller.current_animation = h;
                    }

                    if (is_valid(controller.current_animation))
                    {
                        if (ImGui::InputInt("Frame", &controller.current_frame))
                            controller.play_flags = cmp_anim_controller::STOPPED;

                        ImGui::SameLine();

                        if (controller.play_flags == cmp_anim_controller::STOPPED)
                        {
                            if (ImGui::Button(ICON_FA_PLAY))
                                controller.play_flags = cmp_anim_controller::PLAY;
                        }
                        else
                        {
                            if (ImGui::Button(ICON_FA_STOP))
                                controller.play_flags = cmp_anim_controller::STOPPED;
                        }

                        ImGui::InputFloat("Time", &controller.current_time, 0.01f);
                    }

                    if (open_anim_import)
                    {
                        const c8* anim_import = put::dev_ui::file_browser(open_anim_import, dev_ui::e_file_browser_flags::open, 1, "**.pma");

                        if (anim_import)
                        {
                            anim_handle ah = load_pma(anim_import);

                            if (is_valid(ah))
                            {
                                bind_animation_to_rig(scene, ah, selected_index);
                                bind_animation_to_rig(scene, ah, selected_index);
                            }
                        }
                    }

                    ImGui::Separator();
                }
            }
#endif
        }

        void scene_light_ui(ecs_scene* scene)
        {
            if (sb_count(scene->selection_list) != 1)
                return;

            u32 selected_index = scene->selection_list[0];

            static bool colour_picker_open = false;

            if (ImGui::CollapsingHeader("Light"))
            {
                cmp_light& snl = scene->lights[selected_index];

                if (scene->entities[selected_index] & e_cmp::light)
                {
                    bool changed = ImGui::Combo("Type", (s32*)&scene->lights[selected_index].type,
                                                "Directional\0Point\0Spot\0Area\0Area Ex\0", 4);

                    if (snl.azimuth == 0.0f && snl.altitude == 0.0f)
                        maths::xyz_to_azimuth_altitude(snl.direction, snl.azimuth, snl.altitude);

                    bool edited = changed;

                    u32 flags = snl.flags;
                    ImGui::CheckboxFlags("Shadow Map", &flags, 1);
                    snl.flags = flags;

                    switch (scene->lights[selected_index].type)
                    {
                        case e_light_type::dir:
                            ImGui::SliderAngle("Azimuth", &snl.azimuth);
                            ImGui::SliderAngle("Altitude", &snl.altitude);
                            break;

                        case e_light_type::point:
                            edited |= ImGui::SliderFloat("Radius##slider", &snl.radius, 0.0f, 100.0f);
                            edited |= ImGui::InputFloat("Radius##input", &snl.radius);
                            break;

                        case e_light_type::spot:
                            edited |= ImGui::SliderFloat("Cos Cutoff", &snl.cos_cutoff, 0.0f, 1.0f);
                            edited |= ImGui::SliderFloat("Range", &snl.radius, 0.0f, 100.0f);
                            edited |= ImGui::InputFloat("Falloff", &snl.spot_falloff, 0.01f);

                            // prevent negative or zero fall off
                            snl.spot_falloff = max(snl.spot_falloff, 0.001f);
                            break;

                        case e_light_type::area:
                            if (changed)
                                instantiate_area_light(scene, selected_index);
                            break;
                        case e_light_type::area_ex:
                        {
                            if (changed)
                            {
                                area_light_resource alr;
                                alr.shader_name = "trace";
                                alr.technique_name = "box";
                                instantiate_area_light_ex(scene, selected_index, alr);
                            }

                            area_light_resource& alr = scene->area_light_resources[selected_index];

                            u32 shader = 0;
                            u32 technique_list_index = 0;

                            u32        num_shaders;
                            const c8** shader_list = pmfx::get_shader_list(num_shaders);

                            // find shader in list
                            hash_id id_shader = PEN_HASH(alr.shader_name);
                            for (shader = 0; shader < num_shaders; ++shader)
                                if (PEN_HASH(shader_list[shader]) == id_shader)
                                    break;

                            u32        num_techniques;
                            const c8** technique_list = pmfx::get_technique_list(shader, num_techniques);

                            // find technique in list
                            hash_id id_technique = PEN_HASH(alr.technique_name);
                            for (technique_list_index = 0; technique_list_index < num_techniques; ++technique_list_index)
                                if (PEN_HASH(technique_list[technique_list_index]) == id_technique)
                                    break;

                            edited |= ImGui::Combo("Shader", (s32*)&shader, shader_list, num_shaders);
                            edited |= ImGui::Combo("Technique", (s32*)&technique_list_index, technique_list, num_techniques);
                        }
                        break;
                    }

                    snl.direction = maths::azimuth_altitude_to_xyz(snl.azimuth, snl.altitude);

                    vec3f& col = scene->lights[selected_index].colour;
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(col.x, col.y, col.z, 1.0f));

                    if (ImGui::Button("Colour"))
                        colour_picker_open = true;

                    ImGui::PopStyleColor();

                    if (changed)
                    {
                        s32 s = selected_index;
                        scene->world_matrices[s] = mat4::create_identity();
                    }
                }
                else
                {
                    if (ImGui::Button("Add Light"))
                        instantiate_light(scene, selected_index);
                }
            }

            if (colour_picker_open)
            {
                if (ImGui::Begin("Light Colour", &colour_picker_open, ImGuiWindowFlags_AlwaysAutoResize))
                {
                    ImGui::ColorPicker3("Colour", (f32*)&scene->lights[selected_index].colour);

                    ImGui::End();
                }
            }
        }

        void scene_shadow_ui(ecs_scene* scene)
        {
            if (sb_count(scene->selection_list) != 1)
                return;

            static bool s_file_browser_open = false;

            u32 si = scene->selection_list[0];

            enum e_caster_type
            {
                CAST_NONE,
                CAST_GEOMETRY,
                CAST_SDF
            };

            if (ImGui::CollapsingHeader("Shadow Caster"))
            {
                s32 caster_type = 0;
                if (scene->entities[si] & e_cmp::sdf_shadow)
                {
                    caster_type = 2;
                }
                else
                {
                    if (scene->entities[si] & e_cmp::geometry)
                        caster_type = 1;

                    if (scene->state_flags[si] & e_state::no_shadow)
                        caster_type = 0;
                }

                if (ImGui::Combo("Shadow Caster", &caster_type, "None\0Geometry\0Signed Distance Field\0"))
                {
                    if (caster_type == 0)
                        scene->state_flags[si] |= e_state::no_shadow;

                    if (caster_type > 0)
                        scene->state_flags[si] &= e_state::no_shadow;

                    if (caster_type == 2)
                        scene->entities[si] |= e_cmp::sdf_shadow;
                }

                if (caster_type == CAST_SDF)
                {
                    if (ImGui::Button("..."))
                    {
                        s_file_browser_open = true;
                    }

                    if (s_file_browser_open)
                    {
                        const c8* file =
                            dev_ui::file_browser(s_file_browser_open, dev_ui::e_file_browser_flags::open, 1, "**.pmv");

                        if (file)
                            instantiate_sdf_shadow(file, scene, si);
                    }
                }
            }
        }

        void scene_browser_ui(ecs_scene* scene, bool* open)
        {
            if (ImGui::Begin("Scene Browser", open))
            {
                if (ImGui::Button(ICON_FA_PLUS))
                {
                    u32 ni = ecs::get_next_entity(scene);
                    store_node_state(scene, ni, e_editor_actions::undo);

                    u32 nn = ecs::get_new_entity(scene);

                    scene->entities[nn] |= e_cmp::allocated;

                    scene->names[nn] = "node_";
                    scene->names[nn].appendf("%u", nn);

                    scene->parents[nn] = nn;

                    scene->transforms[nn].translation = vec3f::zero();
                    scene->transforms[nn].rotation.euler_angles(0.0f, 0.0f, 0.0f);
                    scene->transforms[nn].scale = vec3f::one();

                    scene->entities[nn] |= e_cmp::transform;

                    add_selection(scene, nn);

                    store_node_state(scene, ni, e_editor_actions::redo);
                }
                put::dev_ui::set_tooltip("Add New Node");

                ImGui::SameLine();

                if (ImGui::Button(ICON_FA_TRASH))
                    delete_selection(scene);

                put::dev_ui::set_tooltip("Delete Selection");

                ImGui::SameLine();

                static bool list_view = false;
                if (ImGui::Button(ICON_FA_LIST))
                    list_view = true;
                dev_ui::set_tooltip("List View");

                ImGui::SameLine();
                if (ImGui::Button(ICON_FA_USB))
                    list_view = false;
                dev_ui::set_tooltip("Tree View");

                struct scene_num_dump
                {
                    u32       component;
                    const c8* display_name;
                    u32       count;
                };

                static scene_num_dump dumps[] = {{e_cmp::allocated, "Allocated", 0},
                                                 {e_cmp::geometry, "Geometries", 0},
                                                 {e_cmp::bone, "Bones", 0},
                                                 {e_cmp::anim_controller, "Anim Controllers", 0}};

                s32 selected_index = -1;
                u32 num_selected = sb_count(scene->selection_list);
                if (num_selected == 1)
                    selected_index = scene->selection_list[0];

                if (ImGui::CollapsingHeader("Scene Info"))
                {
                    if (!scene->filename.empty())
                        ImGui::Text("Scene File: %s (version %i)", scene->filename.c_str(), scene->version);

                    u32 num_extensions = sb_count(scene->extensions);
                    ImGui::Text("Extensions: %i", num_extensions);
                    ImGui::Indent();
                    for (s32 i = 0; i < num_extensions; ++i)
                    {
                        ImGui::Text("%s", scene->extensions[i].name.c_str());
                    }
                    ImGui::Unindent();

                    ImGui::Text("Total Entities: %lu", scene->num_entities);
                    ImGui::Text("Selected: %i", (s32)sb_count(scene->selection_list));

                    for (s32 i = 0; i < PEN_ARRAY_SIZE(dumps); ++i)
                        dumps[i].count = 0;

                    for (s32 i = 0; i < scene->num_entities; ++i)
                        for (s32 j = 0; j < PEN_ARRAY_SIZE(dumps); ++j)
                            if (scene->entities[i] & dumps[j].component)
                                dumps[j].count++;

                    for (s32 i = 0; i < PEN_ARRAY_SIZE(dumps); ++i)
                        ImGui::Text("%s: %i", dumps[i].display_name, dumps[i].count);
                }

                if (ImGui::CollapsingHeader("Entities"))
                {
                    ImGui::BeginChild("Entities", ImVec2(0, 300), true);

                    if (list_view)
                    {
                        for (u32 i = 0; i < scene->num_entities; ++i)
                        {
                            if (!(scene->entities[i] & e_cmp::allocated))
                                continue;

                            bool selected = scene->state_flags[i] & e_state::selected;

                            ImGui::Unindent(ImGui::GetTreeNodeToLabelSpacing());

                            ImGuiTreeNodeFlags node_flags = selected ? ImGuiTreeNodeFlags_Selected : 0;
                            bool node_open = ImGui::TreeNodeEx((void*)(intptr_t)i, node_flags, scene->names[i].c_str(), i);

                            if (ImGui::IsItemClicked())
                                add_selection(scene, i);

                            if (node_open)
                                ImGui::TreePop();

                            ImGui::Indent(ImGui::GetTreeNodeToLabelSpacing());
                        }
                    }
                    else
                    {
                        static scene_tree tree;
                        if (scene->flags & e_scene_flags::invalidate_scene_tree)
                        {
                            tree = scene_tree();
                            build_scene_tree(scene, -1, tree);

                            scene->flags &= ~e_scene_flags::invalidate_scene_tree;
                        }

                        s32 pre_selected = selected_index;
                        scene_tree_enumerate(scene, tree);

                        if (pre_selected != selected_index)
                            add_selection(scene, selected_index);
                    }

                    ImGui::EndChild();
                }

                ImGui::Separator();

                // selection
                if (selected_index != -1)
                {
                    // single node header
                    static c8 buf[64];
                    if (scene->names[selected_index].empty())
                        scene->names[selected_index] = "unnamed node";

                    u32 end_pos = std::min<u32>(scene->names[selected_index].length(), 64);
                    memcpy(buf, scene->names[selected_index].c_str(), end_pos);
                    buf[end_pos] = '\0';

                    if (ImGui::InputText("", buf, 64))
                    {
                        scene->names[selected_index] = buf;
                        scene->id_name[selected_index] = PEN_HASH(buf);
                    }

                    ImGui::SameLine();
                    ImGui::Text("ces node index %i", selected_index);

                    s32 parent_index = scene->parents[selected_index];
                    if (parent_index != selected_index)
                        ImGui::Text("Parent: %s", scene->names[parent_index].c_str());
                }
                else if (num_selected > 0)
                {
                    ImGui::Text("%i Selected Items", num_selected);
                }

                // Undoable actions
                if (sb_count(scene->selection_list) == 1)
                    store_node_state(scene, scene->selection_list[0], e_editor_actions::undo);

                scene_options_ui(scene);

                scene_components_ui(scene);

                scene_hierarchy_ui(scene);

                scene_transform_ui(scene);

                scene_bounds_ui(scene);

                scene_physics_ui(scene);

                scene_geometry_ui(scene);

                scene_anim_ui(scene);

                scene_material_ui(scene);

                scene_light_ui(scene);

                scene_shadow_ui(scene);

                // for extensions do thier ui
                u32 num_ext = sb_count(scene->extensions);
                for (u32 e = 0; e < num_ext; ++e)
                    scene->extensions[e].funcs.browser_func(scene->extensions[e], scene);

                if (sb_count(scene->selection_list) == 1)
                    store_node_state(scene, scene->selection_list[0], e_editor_actions::redo);

                ImGui::End();
            }
        }

        void apply_transform_to_selection(ecs_scene* scene, const vec3f move_axis)
        {
            if (move_axis == vec3f::zero())
                return;

            u32 sel_num = sb_count(scene->selection_list);
            for (u32 s = 0; s < sel_num; ++s)
            {
                u32 i = scene->selection_list[s];

                // only move if parent isnt selected
                s32 parent = scene->parents[i];
                if (parent != i)
                {
                    bool found = false;
                    for (u32 t = 0; t < sel_num; ++t)
                        if (scene->selection_list[t] == parent)
                        {
                            found = true;
                            break;
                        }

                    if (found)
                        continue;
                }

                store_node_state(scene, i, e_editor_actions::undo);

                cmp_transform& t = scene->transforms[i];

                if (s_transform_mode == e_transform_mode::translate)
                {
                    t.translation += move_axis;
                }
                else if (s_transform_mode == e_transform_mode::scale)
                {
                    t.scale += move_axis * 0.1f;
                }
                else if (s_transform_mode == e_transform_mode::rotate)
                {
                    quat q;
                    q.euler_angles(move_axis.z, move_axis.y, move_axis.x);
                    t.rotation = t.rotation * q;
                }

                scene->entities[i] |= e_cmp::transform;

                store_node_state(scene, i, e_editor_actions::redo);
            }
        }

        struct physics_pick
        {
            vec3f pos;
            a_u8  state = {0};
            bool  grabbed = false;
            s32   constraint = -1;
            s32   physics_handle = -1;
        };
        static physics_pick s_physics_pick_info;

        void physics_pick_callback(const physics::cast_result& result)
        {
            s_physics_pick_info.state = e_picking_state::complete;
            s_physics_pick_info.pos = result.point;
            s_physics_pick_info.grabbed = false;
            s_physics_pick_info.physics_handle = result.physics_handle;
        }

        void transform_widget(const scene_view& view)
        {
            if (pen::input_key(PK_MENU) || pen::input_key(PK_COMMAND))
                return;

            s_select_flags &= ~(e_select_flags::widget_selected);

            ecs_scene* scene = view.scene;

            viewport vp = _renderer_resolve_viewport_ratio(*view.viewport);
            vec2i    vpi = vec2i(vp.width, vp.height);

            static vec3f widget_points[4];
            static vec3f pre_click_axis_pos[3];
            static u32   selected_axis = 0;
            static f32   selection_radius = 5.0f;

            const pen::mouse_state& ms = pen::input_get_mouse_state();
            vec3f                   mousev3 = vec3f(ms.x, vpi.y - ms.y, 0.0f);

            mat4  view_proj = view.camera->proj * view.camera->view;
            vec3f r0 = maths::unproject_sc(vec3f(mousev3.x, mousev3.y, 0.0f), view_proj, vpi);
            vec3f r1 = maths::unproject_sc(vec3f(mousev3.x, mousev3.y, 1.0f), view_proj, vpi);
            vec3f vr = normalised(r1 - r0);

            if (s_transform_mode == e_transform_mode::physics)
            {
                if (!s_physics_pick_info.grabbed && s_physics_pick_info.constraint == -1)
                {
                    if (s_physics_pick_info.state == e_picking_state::ready)
                    {
                        if (ms.buttons[PEN_MOUSE_L])
                        {
                            s_physics_pick_info.state = e_picking_state::single;

                            physics::ray_cast_params rcp;
                            rcp.start = r0;
                            rcp.end = r1;
                            rcp.mask = 0xffffff;
                            rcp.group = 0xffffff;
                            rcp.timestamp = pen::get_time_ms();
                            rcp.callback = physics_pick_callback;

                            physics::cast_ray(rcp);
                        }
                    }
                    else if (s_physics_pick_info.state == e_picking_state::complete)
                    {
                        if (s_physics_pick_info.physics_handle != -1)
                        {
                            physics::constraint_params cp;
                            cp.pivot = s_physics_pick_info.pos;
                            cp.type = physics::e_constraint::p2p;
                            cp.rb_indices[0] = s_physics_pick_info.physics_handle;

                            s_physics_pick_info.constraint = physics::add_constraint(cp);
                            s_physics_pick_info.state = e_picking_state::grabbed;
                        }
                        else
                        {
                            s_physics_pick_info.state = e_picking_state::ready;
                        }
                    }
                }
                else if (s_physics_pick_info.constraint > 0)
                {
                    if (ms.buttons[PEN_MOUSE_L])
                    {
                        vec3f new_pos =
                            maths::ray_plane_intersect(r0, vr, s_physics_pick_info.pos, view.camera->view.get_row(2).xyz);

                        physics::set_v3(s_physics_pick_info.constraint, new_pos, physics::e_cmd::set_p2p_constraint_pos);
                    }
                    else
                    {
                        physics::release_entity(s_physics_pick_info.constraint);
                        s_physics_pick_info.constraint = -1;
                        s_physics_pick_info.state = e_picking_state::ready;
                    }
                }
            }

            if (sb_count(scene->selection_list) == 0)
                return;

            vec3f pos = vec3f::zero();
            vec3f min = vec3f::flt_max();
            vec3f max = -vec3f::flt_max();

            u32 sel_num = sb_count(scene->selection_list);
            for (u32 i = 0; i < sel_num; ++i)
            {
                u32 s = scene->selection_list[i];

                vec3f& _min = scene->bounding_volumes[s].transformed_min_extents;
                vec3f& _max = scene->bounding_volumes[s].transformed_max_extents;

                min = min_union(min, _min);
                max = max_union(max, _max);

                pos += _min + (_max - _min) * 0.5f;
            }

            f32 extents_mag = mag(max - min);

            pos /= (f32)sb_count(scene->selection_list);

            mat4 widget;
            widget.set_vectors(vec3f::unit_x(), vec3f::unit_y(), vec3f::unit_z(), pos);

            if (shortcut_key(PK_F))
            {
                view.camera->focus = pos;
                view.camera->zoom = extents_mag;
            }

            // distance for consistent-ish size
            mat4 res = view.camera->proj * view.camera->view;

            f32   w = 1.0;
            vec3f screen_pos = res.transform_vector(pos, w);
            f32   d = fabs(screen_pos.z) * 0.1f;

            if (screen_pos.z < -0.0)
                return;

            if (s_transform_mode == e_transform_mode::rotate)
            {
                float rd = d * 0.75;

                vec3f plane_normals[] = {vec3f(1.0f, 0.0f, 0.0f), vec3f(0.0f, 1.0f, 0.0f), vec3f(0.0f, 0.0f, 1.0f)};

                vec3f       _cp = vec3f::zero();
                static bool selected[3] = {0};
                for (s32 i = 0; i < 3; ++i)
                {
                    vec3f cp = maths::ray_plane_intersect(r0, vr, pos, plane_normals[i]);

                    if (!ms.buttons[PEN_MOUSE_L])
                    {
                        selected[i] = false;
                        f32 dd = mag(cp - pos);
                        if (dd < rd + rd * 0.05 && dd > rd - rd * 0.05)
                            selected[i] = true;
                    }

                    vec3f col = plane_normals[i] * 0.7f;
                    if (selected[i])
                    {
                        _cp = cp;
                        col = vec3f::one();
                    }

                    dbg::add_circle(plane_normals[i], pos, rd, vec4f(col, 1.0));
                }

                static vec3f attach_point = vec3f::zero();
                static vec3f drag_point = vec3f::zero();
                for (s32 i = 0; i < 3; ++i)
                {
                    if (!ms.buttons[PEN_MOUSE_L])
                    {
                        attach_point = _cp;
                        drag_point = vec3f::zero();
                        continue;
                    }

                    if (drag_point == vec3f::zero())
                        drag_point = _cp;

                    if (selected[i])
                    {
                        s_select_flags |= e_select_flags::widget_selected;

                        vec3f prev_line = normalised(attach_point - pos);
                        vec3f cur_line = normalised(_cp - pos);
                        vec3f start_line = normalised(drag_point - pos);

                        dbg::add_line(pos, pos + cur_line * rd, vec4f::white());
                        dbg::add_line(pos, pos + start_line * rd, vec4f::white() * vec4f(0.3f, 0.3f, 0.3f, 1.0f));

                        vec3f x = cross(prev_line, cur_line);
                        f32   amt = dot(x, plane_normals[i]);

                        apply_transform_to_selection(view.scene, plane_normals[i] * amt);

                        attach_point = _cp;
                        break;
                    }
                }

                return;
            }

            if (s_transform_mode == e_transform_mode::translate || s_transform_mode == e_transform_mode::scale)
            {
                static vec3f unit_axis[] = {
                    vec3f::zero(),
                    vec3f::unit_x(),
                    vec3f::unit_y(),
                    vec3f::unit_z(),
                };

                mat4 view_proj = view.camera->proj * view.camera->view;

                // work out major axes
                vec3f pp[4];
                for (s32 i = 0; i < 4; ++i)
                {
                    widget_points[i] = pos + unit_axis[i] * d;

                    pp[i] = maths::project_to_sc(widget_points[i], view_proj, vpi);
                    pp[i].z = 0.0f;
                }

                // work out joint axes
                vec3f ppj[6];
                for (s32 i = 0; i < 3; ++i)
                {
                    u32 j_index = i * 2;

                    u32 next_index = i + 2;
                    if (next_index > 3)
                        next_index = 1;

                    ppj[j_index] = maths::project_to_sc(pos + unit_axis[i + 1] * d * 0.3f, view_proj, vpi);
                    ppj[j_index].z = 0.0f;

                    ppj[j_index + 1] = maths::project_to_sc(pos + unit_axis[next_index] * d * 0.3f, view_proj, vpi);
                    ppj[j_index + 1].z = 0.0f;
                }

                if (!ms.buttons[PEN_MOUSE_L])
                {
                    selected_axis = 0;
                    for (s32 i = 1; i < 4; ++i)
                    {
                        vec3f cp = maths::closest_point_on_line(pp[0], pp[i], mousev3);

                        if (dist(cp, mousev3) < selection_radius)
                            selected_axis |= (1 << i);
                    }

                    for (s32 i = 0; i < 3; ++i)
                    {
                        u32 j_index = i * 2;
                        u32 i_next = i + 2;
                        u32 ii = i + 1;

                        if (i_next > 3)
                            i_next = 1;

                        vec3f cp = maths::closest_point_on_line(ppj[j_index], ppj[j_index + 1], mousev3);

                        if (dist(cp, mousev3) < selection_radius)
                            selected_axis |= (1 << ii) | (1 << i_next);
                    }
                }

                // draw axes
                for (s32 i = 1; i < 4; ++i)
                {
                    vec4f col = vec4f(unit_axis[i] * 0.7f, 1.0f);

                    if (selected_axis & (1 << i))
                    {
                        s_select_flags |= e_select_flags::widget_selected;
                        col = vec4f::one();
                    }

                    put::dbg::add_line_2f(pp[0].xy, pp[i].xy, col);

                    if (s_transform_mode == e_transform_mode::translate)
                    {
                        vec2f v = normalised(pp[i].xy - pp[0].xy);
                        vec2f px = perp(v) * 5.0f;

                        vec2f base = pp[i].xy - v * 5.0f;

                        put::dbg::add_line_2f(pp[i].xy, base + px, col);
                        put::dbg::add_line_2f(pp[i].xy, base - px, col);
                    }
                    else if (s_transform_mode == e_transform_mode::scale)
                    {
                        put::dbg::add_quad_2f(pp[i].xy, vec2f(3.0f, 3.0f), col);
                    }
                }

                // draw joins
                for (s32 i = 0; i < 3; ++i)
                {
                    u32 j_index = i * 2;
                    u32 i_next = i + 2;
                    if (i_next > 3)
                        i_next = 1;

                    u32 ii = i + 1;

                    vec4f col = vec4f(0.2f, 0.2f, 0.2f, 1.0f);

                    if ((selected_axis & (1 << ii)) && (selected_axis & (1 << i_next)))
                        col = vec4f::one();

                    put::dbg::add_line_2f(ppj[j_index].xy, ppj[j_index + 1].xy, col);
                }

                // project mouse to planes
                static vec3f translation_axis[] = {
                    vec3f::unit_x(),
                    vec3f::unit_y(),
                    vec3f::unit_z(),
                };

                vec3f axis_pos[3];
                vec3f move_axis = vec3f::zero();

                vec3f restrict_axis = vec3f::zero();
                for (s32 i = 0; i < 3; ++i)
                    if ((selected_axis & 1 << (i + 1)))
                        restrict_axis += translation_axis[i];

                for (s32 i = 0; i < 3; ++i)
                {
                    if (!(selected_axis & 1 << (i + 1)))
                        continue;

                    vec3f plane_normal = cross(translation_axis[i], (vec3f)view.camera->view.get_row(1).xyz);

                    if (i == 1)
                        plane_normal = cross(translation_axis[i], (vec3f)view.camera->view.get_row(0).xyz);

                    axis_pos[i] = maths::ray_plane_intersect(r0, vr, widget_points[0], plane_normal);

                    if (!ms.buttons[PEN_MOUSE_L])
                        pre_click_axis_pos[i] = axis_pos[i];

                    vec3f line = (axis_pos[i] - pre_click_axis_pos[i]);

                    vec3f line_x = line * restrict_axis;

                    move_axis += line_x;

                    // only move in one plane at a time
                    break;
                }

                apply_transform_to_selection(view.scene, move_axis);

                for (s32 i = 0; i < 3; ++i)
                {
                    pre_click_axis_pos[i] = axis_pos[i];
                }
            }
        }

        void render_constraint(const ecs_scene* scene, u32 index, const physics::constraint_params& con)
        {
            vec3f pos = scene->transforms[index].translation;
            vec3f axis = con.axis;

            f32 min_rot = con.lower_limit_rotation.x;
            f32 max_rot = con.upper_limit_rotation.x;

            vec3f min_rot_v3 = con.lower_limit_rotation;
            vec3f max_rot_v3 = con.upper_limit_rotation;

            vec3f min_pos_v3 = con.lower_limit_translation - vec3f(0.0001f);
            vec3f max_pos_v3 = con.upper_limit_translation + vec3f(0.0001f);

            vec3f link_pos = pos;
            if (con.rb_indices[0] > -1)
                link_pos = scene->transforms[con.rb_indices[0]].translation;

            switch (con.type)
            {
                case physics::e_constraint::hinge:
                    put::dbg::add_circle(axis, pos, 0.25f, vec4f::green());
                    put::dbg::add_line(pos - axis, pos + axis, vec4f::green());
                    put::dbg::add_circle_segment(axis, pos, 1.0f, min_rot, max_rot, vec4f::white());
                    break;

                case physics::e_constraint::p2p:
                    put::dbg::add_point(pos, 0.5f, vec4f::magenta());
                    put::dbg::add_line(link_pos, pos, vec4f::magenta());
                    break;

                case physics::e_constraint::dof6:
                    put::dbg::add_circle_segment(vec3f::unit_x(), pos, 0.25f, min_rot_v3.x, max_rot_v3.x, vec4f::white());
                    put::dbg::add_circle_segment(vec3f::unit_y(), pos, 0.25f, min_rot_v3.y, max_rot_v3.y, vec4f::white());
                    put::dbg::add_circle_segment(vec3f::unit_z(), pos, 0.25f, min_rot_v3.z, max_rot_v3.z, vec4f::white());
                    put::dbg::add_aabb(pos + min_pos_v3, pos + max_pos_v3, vec4f::white());
                    break;
            }
        }

        void render_light_debug(const scene_view& view)
        {
            bool selected_only = !(view.scene->view_flags & e_scene_view_flags::lights);

            vec2i vpi = vec2i(view.viewport->width, view.viewport->height);
            mat4  view_proj = view.camera->proj * view.camera->view;

            ecs_scene* scene = view.scene;

            if (scene->view_flags & e_scene_view_flags::hide_debug)
                return;

            pen::renderer_set_constant_buffer(view.cb_view, 0, pen::CBUFFER_BIND_PS | pen::CBUFFER_BIND_VS);

            static const hash_id id_volume[] = {PEN_HASH("full_screen_quad"), PEN_HASH("sphere"), PEN_HASH("cone")};

            static const hash_id id_technique = PEN_HASH("constant_colour");
            static const u32     shader = pmfx::load_shader("pmfx_utility");

            geometry_resource* volume[PEN_ARRAY_SIZE(id_volume)];
            for (u32 i = 0; i < PEN_ARRAY_SIZE(id_volume); ++i)
                volume[i] = get_geometry_resource(id_volume[i]);

            for (u32 n = 0; n < scene->num_entities; ++n)
            {
                if (!(scene->entities[n] & e_cmp::light))
                    continue;

                if (selected_only && !(scene->state_flags[n] & e_state::selected))
                    continue;

                cmp_light& snl = scene->lights[n];

                switch (snl.type)
                {
                    case e_light_type::dir:
                    {
                        // line only
                        dbg::add_line(vec3f::zero(), scene->lights[n].direction * k_dir_light_offset,
                                      vec4f(scene->lights[n].colour, 1.0f));

                        continue;
                    }
                    break;

                    case e_light_type::area:
                    {
                        dbg::add_obb(scene->world_matrices[n], vec4f(snl.colour, 1.0f));
                    }
                    break;

                    case e_light_type::spot:
                    case e_light_type::point:
                    {
                        // quad point at pos
                        vec3f p = scene->world_matrices[n].get_translation();

                        p = maths::project_to_sc(p, view_proj, vpi);

                        if (p.z > 0.0f)
                            put::dbg::add_quad_2f(p.xy, vec2f(5.0f, 5.0f), vec4f(scene->lights[n].colour, 1.0f));

                        // volume geometry
                        geometry_resource* vol = volume[snl.type];
                        pmm_renderable&    r = vol->renderable[e_pmm_renderable::full_vertex_buffer];

                        pmfx::set_technique_perm(shader, id_technique);

                        cmp_draw_call dc;
                        dc.world_matrix = scene->world_matrices[n];
                        dc.world_matrix_inv_transpose = mat4::create_identity();
                        dc.v2 = vec4f(scene->lights[n].colour, 1.0f);

                        pen::renderer_update_buffer(scene->cbuffer[n], &dc, sizeof(cmp_draw_call));
                        pen::renderer_set_constant_buffer(scene->cbuffer[n], 1, pen::CBUFFER_BIND_PS | pen::CBUFFER_BIND_VS);
                        pen::renderer_set_vertex_buffer(r.vertex_buffer, 0, r.vertex_size, 0);
                        pen::renderer_set_index_buffer(r.index_buffer, r.index_type, 0);
                        pen::renderer_draw_indexed(r.num_indices, 0, 0, PEN_PT_TRIANGLELIST);
                    }
                    break;
                }
            }
        }

        void render_physics_debug(const scene_view& view)
        {
            ecs_scene* scene = view.scene;

            pen::renderer_set_constant_buffer(view.cb_view, 0, pen::CBUFFER_BIND_PS | pen::CBUFFER_BIND_VS);

            for (u32 n = 0; n < scene->num_entities; ++n)
            {
                if (!(scene->state_flags[n] & e_state::selected) && !(scene->view_flags & e_scene_view_flags::physics))
                    continue;

                bool preview_rb = s_physics_preview.active && s_physics_preview.params.type == e_physics_type::rigid_body;

                if ((scene->entities[n] & e_cmp::physics) || preview_rb)
                {
                    u32 prim = scene->physics_data[n].rigid_body.shape;

                    if (prim == 0)
                        continue;

                    geometry_resource* gr = get_geometry_resource(ID_PRIMITIVE[prim]);
                    pmm_renderable&    r = gr->renderable[e_pmm_renderable::full_vertex_buffer];

                    if (!gr)
                        continue;

                    if (!pmfx::set_technique_perm(view.pmfx_shader, view.id_technique))
                        continue;

                    if (is_invalid_or_null(scene->physics_debug_cbuffer[n]))
                    {
                        pen::buffer_creation_params bcp;
                        bcp.usage_flags = PEN_USAGE_DYNAMIC;
                        bcp.bind_flags = PEN_BIND_CONSTANT_BUFFER;
                        bcp.cpu_access_flags = PEN_CPU_ACCESS_WRITE;
                        bcp.buffer_size = sizeof(cmp_draw_call);
                        bcp.data = nullptr;

                        scene->physics_debug_cbuffer[n] = pen::renderer_create_buffer(bcp);
                    }

                    // update cbuffer
                    cmp_draw_call dc;
                    if (preview_rb && !(scene->entities[n] & e_cmp::physics))
                    {
                        // from preview
                        mat4 scale = mat::create_scale(s_physics_preview.params.rigid_body.dimensions);

                        mat4 translation_mat = mat::create_translation(s_physics_preview.offset.translation);

                        dc.world_matrix = translation_mat * scene->world_matrices[n] * scale;
                    }
                    else
                    {
                        // from physics instance
                        mat4 scale = mat::create_scale(scene->physics_data[n].rigid_body.dimensions);
                        mat4 rbmat = physics::get_rb_matrix(scene->physics_handles[n]);
                        dc.world_matrix = rbmat * scale;
                    }

                    dc.v2 = vec4f::white();

                    pen::renderer_update_buffer(scene->physics_debug_cbuffer[n], &dc, sizeof(cmp_draw_call));

                    // draw
                    pen::renderer_set_constant_buffer(scene->physics_debug_cbuffer[n], 1,
                                                      pen::CBUFFER_BIND_PS | pen::CBUFFER_BIND_VS);

                    pen::renderer_set_vertex_buffer(r.vertex_buffer, 0, r.vertex_size, 0);
                    pen::renderer_set_index_buffer(r.index_buffer, r.index_type, 0);
                    pen::renderer_draw_indexed(r.num_indices, 0, 0, PEN_PT_TRIANGLELIST);
                }

                bool preview_con = s_physics_preview.active && s_physics_preview.params.type == e_physics_type::constraint;

                if (preview_con)
                    render_constraint(scene, n, s_physics_preview.params.constraint);

                if ((scene->entities[n] & e_cmp::constraint))
                    render_constraint(scene, n, scene->physics_data[n].constraint);
            }
        }

        void render_scene_editor(const scene_view& view)
        {
            ecs_scene* scene = view.scene;
            if (scene->view_flags & e_scene_view_flags::hide_debug)
                return;

            render_physics_debug(view);

            if (scene->view_flags & e_scene_view_flags::matrix)
            {
                for (u32 n = 0; n < scene->num_entities; ++n)
                {
                    put::dbg::add_coord_space(scene->world_matrices[n], 2.0f);
                }
            }

            if (scene->view_flags & e_scene_view_flags::aabb)
            {
                for (u32 n = 0; n < scene->num_entities; ++n)
                {
                    dbg::add_aabb(scene->bounding_volumes[n].transformed_min_extents,
                                  scene->bounding_volumes[n].transformed_max_extents);
                }
            }

            // all lights or selected only
            render_light_debug(view);

            // Selected Node
            u32 sel_num = sb_count(scene->selection_list);

            if (scene->view_flags & e_scene_view_flags::node)
            {
                for (u32 i = 0; i < sel_num; ++i)
                {
                    u32 s = scene->selection_list[i];

                    dbg::add_aabb(scene->bounding_volumes[s].transformed_min_extents,
                                  scene->bounding_volumes[s].transformed_max_extents);
                }
            }

            // Detach frustum and camera data to debug
            static bool detach_cam = true;
            if (scene->view_flags & e_scene_view_flags::camera)
            {
                static camera dc;

                if (detach_cam)
                {
                    dc = *view.camera;
                }

                dbg::add_frustum(dc.camera_frustum.corners[0], dc.camera_frustum.corners[1]);

                mat4 iv = mat::inverse3x4(dc.view);

                dbg::add_coord_space(iv, 0.3f);

                dbg::add_point(dc.pos, 0.2f);

                detach_cam = false;
            }
            else
            {
                detach_cam = true;
            }

            // Debug triangles and vertices
            if (scene->view_flags & e_scene_view_flags::geometry && sel_num > 0)
            {
                ImGui::Begin("Debug Geometry");

                static s32 trii = 0;
                ImGui::InputInt("Debug Triangle", &trii);

                static bool show_verts = false;
                ImGui::Checkbox("Show Vertices", &show_verts);

                for (u32 i = 0; i < sel_num; ++i)
                {
                    u32 s = scene->selection_list[i];

                    if (scene->id_geometry[s] != 0)
                    {
                        geometry_resource* gr = get_geometry_resource(scene->id_geometry[s]);
                        pmm_renderable&    r = gr->renderable[e_pmm_renderable::position_only];

                        s32 index_offset = trii * 3;

                        s32 tri_indices[3] = {};

                        if (r.index_type == PEN_FORMAT_R16_UINT)
                        {
                            u16* indices = (u16*)r.cpu_index_buffer;
                            for (u32 i = 0; i < 3; ++i)
                                tri_indices[i] = indices[index_offset + i];
                        }
                        else
                        {
                            u32* indices = (u32*)r.cpu_index_buffer;
                            for (u32 i = 0; i < 3; ++i)
                                tri_indices[i] = indices[index_offset + i];
                        }

                        vec4f* pb = (vec4f*)r.cpu_vertex_buffer;

                        vec3f positions[3] = {};
                        for (u32 i = 0; i < 3; ++i)
                        {
                            memcpy(&positions[i], &pb[tri_indices[i]], sizeof(vec3f));
                        }

                        for (u32 i = 0; i < 3; ++i)
                        {
                            u32 x = (i + 1) % 3;
                            dbg::add_line(positions[i], positions[x], vec4f::cyan());
                        }

                        ImGui::Text("Node %i: %i, %i, %i", s, tri_indices[0], tri_indices[1], tri_indices[2]);

                        if (show_verts)
                        {
                            for (u32 v = 0; v < r.num_vertices; ++v)
                            {
                                dbg::add_point(pb[v].xyz, 0.05f, vec4f::green());
                            }
                        }
                    }
                }

                ImGui::End();
            }

            if (scene->view_flags & e_scene_view_flags::selected_children)
            {
                for (u32 n = 0; n < scene->num_entities; ++n)
                {
                    vec4f col = vec4f::white();
                    bool  selected = false;
                    if (scene->state_flags[n] & e_state::selected)
                        selected = true;

                    u32 p = scene->parents[n];
                    if (p != n)
                    {
                        if (scene->state_flags[p] & e_state::selected || scene->state_flags[p] & e_state::child_selected)
                        {
                            scene->state_flags[n] |= e_state::child_selected;
                            selected = true;
                            col = vec4f(0.75f, 0.75f, 0.75f, 1.0f);
                        }
                        else
                        {
                            scene->state_flags[n] &= ~e_state::child_selected;
                        }
                    }

                    if (selected)
                        dbg::add_aabb(scene->bounding_volumes[n].transformed_min_extents,
                                      scene->bounding_volumes[n].transformed_max_extents, col);
                }
            }

            if (scene->view_flags & e_scene_view_flags::bones)
            {
                for (u32 n = 0; n < scene->num_entities; ++n)
                {
                    if (!(scene->entities[n] & e_cmp::bone))
                        continue;

                    if (scene->entities[n] & e_cmp::anim_trajectory)
                    {
                        vec3f p = scene->world_matrices[n].get_translation();

                        put::dbg::add_aabb(p - vec3f(0.1f, 0.1f, 0.1f), p + vec3f(0.1f, 0.1f, 0.1f), vec4f::green());
                    }

                    u32 p = scene->parents[n];
                    if (p != n)
                    {
                        if (!(scene->entities[p] & e_cmp::bone) || (scene->entities[p] & e_cmp::anim_trajectory))
                            continue;

                        vec3f p1 = scene->world_matrices[n].get_translation();
                        vec3f p2 = scene->world_matrices[p].get_translation();

                        put::dbg::add_line(p1, p2, vec4f::one());
                    }
                }
            }

            if (scene->view_flags & e_scene_view_flags::grid)
            {
                f32 divisions = s_model_view_controller.grid_size / s_model_view_controller.grid_cell_size;
                put::dbg::add_grid(vec3f::zero(), vec3f(s_model_view_controller.grid_size), vec3f(divisions));
            }

            put::dbg::render_3d(view.cb_view);

            // no depth test and default raster state
            static hash_id id_disabled = PEN_HASH("disabled");
            static hash_id id_default = PEN_HASH("default");

            u32 depth_disabled = pmfx::get_render_state(id_disabled, pmfx::e_render_state::depth_stencil);
            u32 fill = pmfx::get_render_state(id_default, pmfx::e_render_state::rasterizer);

            pen::renderer_set_depth_stencil_state(depth_disabled);
            pen::renderer_set_raster_state(fill);

            transform_widget(view);

            put::dbg::render_3d(view.cb_view);

            put::dbg::render_2d(view.cb_2d_view);

            // reset depth state
            pen::renderer_set_depth_stencil_state(view.depth_stencil_state);
        }

        Str strip_project_dir(const Str& filename)
        {
            Str project_dir = dev_ui::get_program_preference_filename("project_dir", os_get_user_info().working_directory);
            Str stripped = pen::str_replace_string(filename, project_dir.c_str(), "");
            return stripped;
        }
    } // namespace ecs
} // namespace put

#if 0 // code to calc tangents
for (long i = 0; i < vertices.size(); i += 3)
{
    if (i + 2 < vertices.size())
    {
        long i1 = i;
        long i2 = i + 1;
        long i3 = i + 2;

        const Vector3f& v1 = vertices.at(i1);
        const Vector3f& v2 = vertices.at(i2);
        const Vector3f& v3 = vertices.at(i3);

        const Vector2f& w1 = tex_coords.at(i1);
        const Vector2f& w2 = tex_coords.at(i2);
        const Vector2f& w3 = tex_coords.at(i3);

        float x1 = v2.x - v1.x;
        float x2 = v3.x - v1.x;
        float y1 = v2.y - v1.y;
        float y2 = v3.y - v1.y;
        float z1 = v2.z - v1.z;
        float z2 = v3.z - v1.z;

        float s1 = w2.x - w1.x;
        float s2 = w3.x - w1.x;
        float t1 = w2.y - w1.y;
        float t2 = w3.y - w1.y;

        float r = 1.0F / (s1 * t2 - s2 * t1);
        Vector3f sdir((t2 * x1 - t1 * x2) * r, (t2 * y1 - t1 * y2) * r,
            (t2 * z1 - t1 * z2) * r);

        Vector3f tdir((s1 * x2 - s2 * x1) * r, (s1 * y2 - s2 * y1) * r,
            (s1 * z2 - s2 * z1) * r);

        tan1.push_back(sdir);
        tan1.push_back(sdir);
        tan1.push_back(sdir);

        tan2.push_back(tdir);
        tan2.push_back(tdir);
        tan2.push_back(tdir);
    }
    else
    {
        missed_faces += 3;
    }
}

for (long i = 0; i < vertices.size() - missed_faces; i++)
{
    Vector3f n = normals.at(i);
    Vector3f t = tan1.at(i);

    //Orthogonalize
    t = psmath::normalise((t - n * psmath::dot(n, t)));

    //Calculate handedness which way? possibly only for opposite face culling
    float handedness = (psmath::dot(psmath::cross(n, t), tan2.at(i)) < 0.0F) ? -1.0F : 1.0F;

    Vector4f final_tan = Vector4f(t, handedness);

    vbo_tangents.push_back(final_tan);
}

#endif
