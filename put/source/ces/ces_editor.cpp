#include "ces/ces_editor.h"
#include "ces/ces_resources.h"
#include "ces/ces_utilities.h"

#include "camera.h"
#include "debug_render.h"
#include "dev_ui.h"
#include "hash.h"
#include "input.h"
#include "pen_string.h"
#include "physics.h"
#include "pmfx.h"
#include "str_utilities.h"
#include "timer.h"

#include "data_struct.h"

namespace put
{
    namespace dev_ui
    {
        extern bool k_console_open;
    }

    namespace ces
    {
        static hash_id k_primitives[] = {
            PEN_HASH("cube"), PEN_HASH("cylinder"), PEN_HASH("sphere"), PEN_HASH("capsule"), PEN_HASH("cone"),
        };

        struct transform_undo
        {
            cmp_transform state;
            u32           node_index;
        };

        static hash_id ID_PICKING_BUFFER = PEN_HASH("picking");

        struct picking_info
        {
            u32  result;
            a_u8 ready;
            u32  x, y;
        };
        static picking_info k_picking_info;
        u32*                s_selection_list = nullptr;

        enum e_picking_state : u32
        {
            PICKING_READY    = 0,
            PICKING_SINGLE   = 1,
            PICKING_MULTI    = 2,
            PICKING_COMPLETE = 3,
            PICKING_GRABBED  = 4
        };

        enum e_select_flags : u32
        {
            NONE            = 0,
            WIDGET_SELECTED = 1,
        };
        static u32 k_select_flags = 0;

        enum e_camera_mode : s32
        {
            CAMERA_MODELLING = 0,
            CAMERA_FLY       = 1
        };

        const c8* camera_mode_names[] = {"Modelling", "Fly"};

        struct model_view_controller
        {
            put::camera   main_camera;
            bool          invalidated = false;
            bool          invert_y    = false;
            e_camera_mode camera_mode = CAMERA_MODELLING;
            f32           grid_cell_size;
            f32           grid_size;
            Str           current_working_scene = "";
        };
        model_view_controller k_model_view_controller;

        enum transform_mode : u32
        {
            TRANSFORM_NONE      = 0,
            TRANSFORM_SELECT    = 1,
            TRANSFORM_TRANSLATE = 2,
            TRANSFORM_ROTATE    = 3,
            TRANSFORM_SCALE     = 4,
            TRANSFORM_PHYSICS   = 5
        };
        static transform_mode k_transform_mode = TRANSFORM_NONE;

        bool shortcut_key(u32 key)
        {
            u32 flags = dev_ui::want_capture();

            if (flags & dev_ui::KEYBOARD)
                return false;

            if (flags & dev_ui::TEXT)
                return false;

            return pen::input_key(key);
        }

        void update_model_viewer_camera(put::scene_controller* sc)
        {
            if (k_model_view_controller.invalidated)
            {
                sc->camera->fov        = k_model_view_controller.main_camera.fov;
                sc->camera->near_plane = k_model_view_controller.main_camera.near_plane;
                sc->camera->far_plane  = k_model_view_controller.main_camera.far_plane;
                camera_update_projection_matrix(sc->camera);
                k_model_view_controller.invalidated = false;
            }

            bool has_focus = dev_ui::want_capture() == dev_ui::NO_INPUT;

            switch (k_model_view_controller.camera_mode)
            {
                case CAMERA_MODELLING:
                    put::camera_update_modelling(sc->camera, has_focus, k_model_view_controller.invert_y);
                    break;
                case CAMERA_FLY:
                    put::camera_update_fly(sc->camera, has_focus, k_model_view_controller.invert_y);
                    break;
            }
        }

        enum e_debug_draw_flags
        {
            DD_HIDE              = SV_HIDE,
            DD_NODE              = 1 << (SV_BITS_END + 1),
            DD_GRID              = 1 << (SV_BITS_END + 2),
            DD_MATRIX            = 1 << (SV_BITS_END + 3),
            DD_BONES             = 1 << (SV_BITS_END + 4),
            DD_AABB              = 1 << (SV_BITS_END + 5),
            DD_LIGHTS            = 1 << (SV_BITS_END + 6),
            DD_PHYSICS           = 1 << (SV_BITS_END + 7),
            DD_SELECTED_CHILDREN = 1 << (SV_BITS_END + 8),

            DD_NUM_FLAGS = 9
        };

        const c8* dd_names[]{"Hide Scene", "Selected Node", "Grid",    "Matrices",         "Bones",
                             "AABB",       "Lights",        "Physics", "Selected Children"};
        static_assert(sizeof(dd_names) / sizeof(dd_names[0]) == DD_NUM_FLAGS, "mismatched");
        static bool* k_dd_bools = nullptr;

        void undo(entity_scene* scene);
        void redo(entity_scene* scene);
        void update_undo_stack(entity_scene* scene, f32 dt);

        void update_view_flags_ui(entity_scene* scene)
        {
            if (!k_dd_bools)
            {
                k_dd_bools = new bool[DD_NUM_FLAGS];
                pen::memory_set(k_dd_bools, 0x0, sizeof(bool) * DD_NUM_FLAGS);

                // set defaults
                static u32 defaults[] = {DD_NODE, DD_GRID, DD_LIGHTS};
                for (s32 i = 1; i < sizeof(defaults) / sizeof(defaults[0]); ++i)
                    k_dd_bools[i] = true;
            }

            for (s32 i = 0; i < DD_NUM_FLAGS; ++i)
            {
                u32 mask = 1 << i;

                if (k_dd_bools[i])
                    scene->view_flags |= mask;
                else
                    scene->view_flags &= ~(mask);
            }
        }

        void update_view_flags(entity_scene* scene, bool error)
        {
            if (error)
                scene->view_flags |= (DD_MATRIX | DD_BONES);

            for (s32 i = 0; i < DD_NUM_FLAGS; ++i)
            {
                k_dd_bools[i] = false;

                if (scene->view_flags & (1 << i))
                    k_dd_bools[i] = true;
            }
        }

        void view_ui(entity_scene* scene, bool* opened)
        {
            if (ImGui::Begin("View", opened, ImGuiWindowFlags_AlwaysAutoResize))
            {
                for (s32 i = 0; i < DD_NUM_FLAGS; ++i)
                {
                    ImGui::Checkbox(dd_names[i], &k_dd_bools[i]);
                }

                if (ImGui::CollapsingHeader("Grid Options"))
                {
                    if (ImGui::InputFloat("Cell Size", &k_model_view_controller.grid_cell_size))
                        dev_ui::set_program_preference("grid_cell_size", k_model_view_controller.grid_cell_size);

                    if (ImGui::InputFloat("Grid Size", &k_model_view_controller.grid_size))
                        dev_ui::set_program_preference("grid_size", k_model_view_controller.grid_size);
                }

                ImGui::End();
            }

            update_view_flags_ui(scene);
        }

        void default_scene(entity_scene* scene)
        {
            // add default view flags
            scene->view_flags = 0;
            delete[] k_dd_bools;
            k_dd_bools = nullptr;
            update_view_flags_ui(scene);

            // add light
            u32 light                            = get_new_node(scene);
            scene->names[light]                  = "front_light";
            scene->id_name[light]                = PEN_HASH("front_light");
            scene->lights[light].colour          = vec3f::one();
            scene->lights[light].direction       = vec3f::one();
            scene->lights[light].type            = LIGHT_TYPE_DIR;
            scene->transforms[light].translation = vec3f::zero();
            scene->transforms[light].rotation    = quat();
            scene->transforms[light].scale       = vec3f::one();
            scene->entities[light] |= CMP_LIGHT;
            scene->entities[light] |= CMP_TRANSFORM;

            sb_clear(s_selection_list);
        }

        void editor_init(entity_scene* scene)
        {
            update_view_flags_ui(scene);

            create_geometry_primitives();

            bool auto_load_last_scene = dev_ui::get_program_preference("load_last_scene").as_bool();
            Str  last_loaded_scene    = dev_ui::get_program_preference_filename("last_loaded_scene");
            if (auto_load_last_scene && last_loaded_scene.length() > 0)
            {
                if (last_loaded_scene.length() > 0)
                    load_scene(last_loaded_scene.c_str(), scene);

                k_model_view_controller.current_working_scene = last_loaded_scene;

                auto_load_last_scene = false;
            }
            else
            {
                default_scene(scene);
            }

            // grid
            k_model_view_controller.grid_cell_size = dev_ui::get_program_preference("grid_cell_size").as_f32(1.0f);
            k_model_view_controller.grid_size      = dev_ui::get_program_preference("grid_size").as_f32(100.0f);

            // camera
            k_model_view_controller.main_camera.fov        = dev_ui::get_program_preference("camera_fov").as_f32(60.0f);
            k_model_view_controller.main_camera.near_plane = dev_ui::get_program_preference("camera_near").as_f32(0.1f);
            k_model_view_controller.main_camera.far_plane  = dev_ui::get_program_preference("camera_far").as_f32(1000.0f);
            k_model_view_controller.invert_y               = dev_ui::get_program_preference("camera_invert_y").as_bool();
            k_model_view_controller.invalidated            = true;
        }

        void editor_shutdown()
        {
            delete[] k_dd_bools;
        }

        void instance_selection(entity_scene* scene)
        {
            s32 selection_size = sb_count(s_selection_list);

            if (selection_size <= 1)
                return;

            s32 master = s_selection_list[0];

            if (scene->entities[master] & CMP_MASTER_INSTANCE)
                return;

            scene->entities[master] |= CMP_MASTER_INSTANCE;

            scene->master_instances[master].num_instances   = selection_size;
            scene->master_instances[master].instance_stride = sizeof(cmp_draw_call);

            pen::buffer_creation_params bcp;
            bcp.usage_flags      = PEN_USAGE_DYNAMIC;
            bcp.bind_flags       = PEN_BIND_VERTEX_BUFFER;
            bcp.buffer_size      = sizeof(cmp_draw_call) * scene->master_instances[master].num_instances;
            bcp.data             = nullptr;
            bcp.cpu_access_flags = PEN_CPU_ACCESS_WRITE;

            scene->master_instances[master].instance_buffer = pen::renderer_create_buffer(bcp);

            // todo - must ensure list is contiguous.
            dev_console_log("[instance] master instance: %i with %i sub instances", master, selection_size);
        }

        void parent_selection(entity_scene* scene)
        {
            s32 selection_size = sb_count(s_selection_list);

            if (selection_size <= 1)
                return;

            s32 parent = s_selection_list[0];

            // check contiguity
            bool valid      = true;
            s32  last_index = -1;
            for (s32 i = 1; i < selection_size; ++i)
            {
                if (s_selection_list[i] < parent)
                {
                    valid = false;
                    break;
                }

                last_index = std::max<s32>(s_selection_list[i], last_index);
            }

            if (last_index > parent + selection_size)
                valid = false;

            u32 sel_count = sb_count(s_selection_list);
            if (valid)
            {
                // list is already contiguous
                dev_console_log("[parent] selection is contiguous %i to %i size %i", parent, last_index, selection_size);

                for (int i = 0; i < sel_count; ++i)
                    if (scene->parents[i] == s_selection_list[i])
                        set_node_parent(scene, parent, i);
            }
            else
            {
                // move nodes into a contiguous list with the parent first most
                dev_console_log("[parent] selection is not contiguous size %i", parent, last_index, selection_size);

                s32 start, end;
                get_new_nodes_append(scene, selection_size, start, end);

                s32 nn = start;
                for (int i = 0; i < sel_count; ++i)
                    clone_node(scene, s_selection_list[i], nn++, start, CLONE_MOVE, vec3f::zero(), "");
            }

            scene->flags |= INVALIDATE_SCENE_TREE;
        }
        
        void clear_selection(entity_scene* scene)
        {
            u32 sel_count = sb_count(s_selection_list);
            
            for (u32 i = 0; i < sel_count; ++i)
            {
                u32 si = s_selection_list[i];
                scene->state_flags[si] &= ~SF_SELECTED;
            }
            
            sb_clear(s_selection_list);
            scene->flags |= INVALIDATE_SCENE_TREE;
        }

        void add_selection(entity_scene* scene, u32 index, u32 select_mode)
        {
            if (pen::input_is_key_down(PK_CONTROL))
                select_mode = SELECT_REMOVE;
            else if (pen::input_is_key_down(PK_SHIFT))
                select_mode = SELECT_ADD;

            bool valid = index < scene->num_nodes;

            u32 sel_count = sb_count(s_selection_list);

            if (select_mode == SELECT_NORMAL)
            {
                clear_selection(scene);

                if (valid)
                    sb_push(s_selection_list, index);
            }
            else if (valid)
            {
                s32 existing = -1;
                for (s32 i = 0; i < sel_count; ++i)
                    if (s_selection_list[i] == index)
                        existing = i;

                u32* new_list = nullptr;
                if (existing != -1 && select_mode == SELECT_REMOVE)
                {
                    for (u32 i = 0; i < sel_count; ++i)
                        if (i != existing)
                            sb_push(new_list, s_selection_list[i]);

                    sb_free(s_selection_list);
                    s_selection_list = new_list;
                }

                if (existing == -1 && select_mode == SELECT_ADD)
                    sb_push(s_selection_list, index);
            }

            if (!valid)
                return;

            if (select_mode == SELECT_REMOVE)
                scene->state_flags[index] &= ~SF_SELECTED;
            else
                scene->state_flags[index] |= SF_SELECTED;
        }
        
        void delete_selection(entity_scene* scene)
        {
            u32 sel_num = sb_count(s_selection_list);
            for (u32 s = 0; s < sel_num; ++s)
            {
                u32 i = s_selection_list[s];
                
                std::vector<s32> node_index_list;
                build_heirarchy_node_list(scene, i, node_index_list);
                
                for (auto& c : node_index_list)
                    if (c > -1)
                        delete_entity(scene, c);
            }
            sb_clear(s_selection_list);
            
            initialise_free_list(scene);
            
            scene->flags |= put::ces::INVALIDATE_SCENE_TREE;
        }

        void enumerate_selection_ui(const entity_scene* scene, bool* opened)
        {
            if (ImGui::Begin("Selection List", opened))
            {
                ImGui::Text("Picking Result: %u", k_picking_info.result);

                u32 sel_count = sb_count(s_selection_list);
                for (s32 i = 0; i < sel_count; ++i)
                {
                    s32 ii = s_selection_list[i];

                    ImGui::Text("%s", scene->names[ii].c_str());
                }

                ImGui::End();
            }
        }

        void picking_read_back(void* p_data, u32 row_pitch, u32 depth_pitch, u32 block_size)
        {
            k_picking_info.result = *((u32*)(((u8*)p_data) + k_picking_info.y * row_pitch + k_picking_info.x * block_size));

            k_picking_info.ready = 1;
        }

        void picking_update(entity_scene* scene, const camera* cam)
        {
            static u32 picking_state  = PICKING_READY;
            static u32 picking_result = (-1);

            if (dev_ui::want_capture() & dev_ui::MOUSE)
                return;

            if (picking_state == PICKING_SINGLE)
            {
                if (k_picking_info.ready)
                {
                    picking_state  = PICKING_READY;
                    picking_result = k_picking_info.result;

                    add_selection(scene, picking_result);

                    k_picking_info.ready = false;
                }

                return;
            }

            pen::mouse_state ms          = pen::input_get_mouse_state();
            f32              corrected_y = pen_window.height - ms.y;

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
                vec3f v1     = normalised(plane_vectors[offset + 1] - plane_vectors[offset + 0]);
                vec3f v2     = normalised(plane_vectors[offset + 2] - plane_vectors[offset + 0]);

                n[i] = cross(v1, v2);
                p[i] = plane_vectors[offset];
            }

            if (!ms.buttons[PEN_MOUSE_L])
            {
                if (picking_state == PICKING_MULTI)
                {
                    u32 pm = SELECT_NORMAL;
                    if (!pen::input_is_key_down(PK_CONTROL) && !pen::input_is_key_down(PK_SHIFT))
                    {
                        // unflag current selected
                        u32 sls = sb_count(s_selection_list);
                        for (u32 i = 0; i < sls; ++i)
                            scene->state_flags[s_selection_list[i]] &= ~SF_SELECTED;

                        sb_clear(s_selection_list);

                        pm = SELECT_ADD;
                    }

                    for (s32 node = 0; node < scene->num_nodes; ++node)
                    {
                        if (!(scene->entities[node] & CMP_ALLOCATED))
                            continue;

                        if (!(scene->entities[node] & CMP_GEOMETRY))
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
                            add_selection(scene, node, SELECT_ADD_MULTI);
                        }
                    }

                    sb_clear(s_selection_list);
                    stb__sbgrow(s_selection_list, scene->num_nodes);

                    s32 pos = 0;
                    for (s32 node = 0; node < scene->num_nodes; ++node)
                    {
                        if (scene->state_flags[node] &= SF_SELECTED)
                            s_selection_list[pos++] = node;
                    }

                    stb__sbm(s_selection_list) = scene->num_nodes;
                    stb__sbn(s_selection_list) = pos;

                    u32 sls = sb_count(s_selection_list);
                    for (u32 i = 0; i < sls; ++i)
                    {
                        dev_console_log("selected index %i", s_selection_list[i]);
                    }

                    picking_state = PICKING_READY;
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
                    min = vec2f::vmin(source_points[i], min);
                    max = vec2f::vmax(source_points[i], max);
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
                    picking_state = PICKING_SINGLE;

                    const pmfx::render_target* rt = pmfx::get_render_target(ID_PICKING_BUFFER);

                    if (!rt)
                    {
                        picking_state = PICKING_READY;
                        return;
                    }

                    f32 w, h;
                    pmfx::get_render_target_dimensions(rt, w, h);

                    u32 pitch     = (u32)w * 4;
                    u32 data_size = (u32)h * pitch;

                    pen::resource_read_back_params rrbp = {rt->handle, rt->format,        pitch, data_size, 4,
                                                           data_size,  &picking_read_back};

                    pen::renderer_read_back_resource(rrbp);

                    k_picking_info.ready = 0;
                    k_picking_info.x     = ms.x;
                    k_picking_info.y     = ms.y;
                }
                else
                {
                    // do not perfrom frustum picking if min and max are not square
                    bool invalid_quad = min.x == max.x || min.y == max.y;

                    if (!invalid_quad)
                    {
                        picking_state = PICKING_MULTI;

                        // todo this should really be passed in, incase we want non window sized viewports
                        vec2i vpi = vec2i(pen_window.width, pen_window.height);

                        for (s32 i = 0; i < 4; ++i)
                        {
                            mat4 view_proj       = cam->proj * cam->view;
                            frustum_points[0][i] = maths::unproject_sc(vec3f(source_points[i], 0.0f), view_proj, vpi);

                            frustum_points[1][i] = maths::unproject_sc(vec3f(source_points[i], 1.0f), view_proj, vpi);
                        }
                    }
                }
            }

            // set child selected
            for (u32 n = 0; n < scene->num_nodes; ++n)
            {
                u32 p = scene->parents[n];
                if (p == n)
                    continue;

                if (scene->state_flags[p] & SF_SELECTED || scene->state_flags[p] & SF_CHILD_SELECTED)
                    scene->state_flags[n] |= SF_CHILD_SELECTED;
            }
        }

        // undoable / redoable actions
        enum e_editor_actions
        {
            MACRO_BEGIN = -1,
            MACRO_END   = -2,
            UNDO        = 0,
            REDO,
            NUM_ACTIONS
        };

        struct node_state
        {
            void** components = nullptr;
        };

        struct editor_action
        {
            s32        node_index;
            node_state action_state[NUM_ACTIONS];
            f32        timer = 0.0f;
        };

        typedef pen_stack<editor_action> action_stack;

        static action_stack   k_undo_stack;
        static action_stack   k_redo_stack;
        static editor_action* k_editor_nodes = nullptr;

        void free_node_state_mem(entity_scene* scene, node_state& ns)
        {
            if (!ns.components)
                return;

            u32 num = scene->num_components;
            for (u32 i = 0; i < num; ++i)
                pen::memory_free(ns.components[i]);

            pen::memory_free(ns.components);
            ns.components = nullptr;
        }

        void store_node_state(entity_scene* scene, u32 node_index, e_editor_actions action)
        {
            static const f32 undo_push_timer = 33.0f;
            node_state&      ns              = k_editor_nodes[node_index].action_state[action];
            u32              num             = scene->num_components;

            if (action == UNDO)
                if (ns.components)
                    return;

            if (action == REDO)
            {
                node_state& us = k_editor_nodes[node_index].action_state[UNDO];

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

            k_editor_nodes[node_index].timer = undo_push_timer;

            if (!ns.components)
            {
                dev_console_log("%s", "allocing state mem");
                ns.components = (void**)pen::memory_alloc(num * sizeof(generic_cmp_array));
                pen::memory_zero(ns.components, num * sizeof(generic_cmp_array));
            }

            for (u32 i = 0; i < num; ++i)
            {
                generic_cmp_array& cmp = scene->get_component_array(i);

                if (!ns.components[i])
                    ns.components[i] = pen::memory_alloc(cmp.size);

                void* data = cmp[node_index];

                pen::memory_cpy(ns.components[i], data, cmp.size);
            }
        }

        void restore_node_state(entity_scene* scene, node_state& ns, u32 node_index)
        {
            u32 num = scene->num_components;
            for (u32 i = 0; i < num; ++i)
            {
                generic_cmp_array& cmp = scene->get_component_array(i);

                pen::memory_cpy(cmp[node_index], ns.components[i], cmp.size);
            }

            node_state& us = k_editor_nodes[node_index].action_state[UNDO];
            node_state& rs = k_editor_nodes[node_index].action_state[REDO];
            free_node_state_mem(scene, us);
            free_node_state_mem(scene, rs);
        }

        void restore_from_stack(entity_scene* scene, action_stack& stack, action_stack& reverse, e_editor_actions action)
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

                if (scene->state_flags[ua.node_index] & SF_SELECTED)
                    sb_clear(s_selection_list);

                restore_node_state(scene, ua.action_state[action], ua.node_index);
            }
        }

        void undo(entity_scene* scene)
        {
            restore_from_stack(scene, k_undo_stack, k_redo_stack, UNDO);
        }

        void redo(entity_scene* scene)
        {
            restore_from_stack(scene, k_redo_stack, k_undo_stack, REDO);
        }

        void update_undo_stack(entity_scene* scene, f32 dt)
        {
            // resize buffers
            if (!k_editor_nodes || sb_count(k_editor_nodes) < scene->nodes_size)
            {
                stb__sbgrow(k_editor_nodes, scene->nodes_size);
                stb__sbm(k_editor_nodes) = scene->num_nodes;
                stb__sbn(k_editor_nodes) = scene->nodes_size;
            }

            static editor_action macro_begin;
            static editor_action macro_end;

            macro_begin.node_index = MACRO_BEGIN;
            macro_end.node_index   = MACRO_END;

            bool first_item = true;

            for (u32 i = 0; i < scene->nodes_size; ++i)
            {
                if (k_editor_nodes[i].action_state[REDO].components)
                {
                    if (k_editor_nodes[i].timer <= 0.0f)
                    {
                        if (first_item)
                        {
                            k_redo_stack.clear();
                            k_undo_stack.push(macro_end);
                            first_item = false;
                        }

                        k_editor_nodes[i].node_index = i;
                        k_undo_stack.push(k_editor_nodes[i]);
                        k_editor_nodes[i].action_state[UNDO].components = nullptr;
                        k_editor_nodes[i].action_state[REDO].components = nullptr;
                    }

                    k_editor_nodes[i].timer -= dt * 0.1f;
                }
            }

            if (!first_item)
                k_undo_stack.push(macro_begin);

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

            if (ImGui::Begin("Settings", opened))
            {
                Str setting_str;
                if (ImGui::Checkbox("Invert Camera Y", &k_model_view_controller.invert_y))
                {
                    dev_ui::set_program_preference("invert_camera_y", k_model_view_controller.invert_y);
                }

                if (ImGui::Checkbox("Auto Load Last Scene", &load_last_scene))
                {
                    dev_ui::set_program_preference("load_last_scene", load_last_scene);
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
                const c8* set_proj = put::dev_ui::file_browser(set_project_dir, dev_ui::FB_OPEN, 1, "**.");

                if (set_proj)
                {
                    project_dir_str = set_proj;
                    dev_ui::set_program_preference_filename("project_dir", project_dir_str);
                }
            }
        }

        void update_model_viewer_scene(put::scene_controller* sc)
        {
            static bool open_scene_browser = false;
            static bool open_merge         = false;
            static bool open_open          = false;
            static bool open_save          = false;
            static bool open_save_as       = false;
            static bool open_camera_menu   = false;
            static bool open_resource_menu = false;
            static bool dev_open           = false;
            static bool selection_list     = false;
            static bool view_menu          = false;
            static bool settings_open      = false;

            // auto save
            bool auto_save = dev_ui::get_program_preference("auto_save").as_bool();
            if (auto_save)
            {
            }

            ImGui::BeginMainMenuBar();

            if (ImGui::BeginMenu(ICON_FA_LEMON_O))
            {
                if (ImGui::MenuItem("New"))
                {
                    clear_scene(sc->scene);
                    default_scene(sc->scene);
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

                if (ImGui::MenuItem("Save As", NULL, &open_save))
                {
                    open_save_as = true;
                }

                ImGui::MenuItem("Console", NULL, &put::dev_ui::k_console_open);
                ImGui::MenuItem("Settings", NULL, &settings_open);
                ImGui::MenuItem("Dev", NULL, &dev_open);

                ImGui::EndMenu();
            }

            // play pause
            if (sc->scene->flags & PAUSE_UPDATE)
            {
                if (ImGui::Button(ICON_FA_PLAY))
                    sc->scene->flags &= (~sc->scene->flags);
            }
            else
            {
                if (ImGui::Button(ICON_FA_PAUSE))
                    sc->scene->flags |= PAUSE_UPDATE;
            }

            static bool debounce_pause = false;
            if (shortcut_key(PK_SPACE))
            {
                if (!debounce_pause)
                {
                    if (!(sc->scene->flags & PAUSE_UPDATE))
                        sc->scene->flags |= PAUSE_UPDATE;
                    else
                        sc->scene->flags &= ~PAUSE_UPDATE;

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
                for (s32 i = 0; i < sc->scene->num_nodes; ++i)
                {
                    if (sc->scene->entities[i] & CMP_PHYSICS)
                    {
                        vec3f t = sc->scene->physics_data[i].rigid_body.position;
                        quat  q = sc->scene->physics_data[i].rigid_body.rotation;

                        physics::set_transform(sc->scene->physics_handles[i], t, q);

                        // reset velocity
                        physics::set_v3(sc->scene->physics_handles[i], vec3f::zero(), physics::CMD_SET_LINEAR_VELOCITY);
                        physics::set_v3(sc->scene->physics_handles[i], vec3f::zero(), physics::CMD_SET_ANGULAR_VELOCITY);
                    }
                }
            }

            if (ImGui::Button(ICON_FA_FLOPPY_O))
            {
                open_save    = true;
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
                undo(sc->scene);
            }
            dev_ui::set_tooltip("Undo");

            if (ImGui::Button(ICON_FA_REPEAT))
            {
                redo(sc->scene);
            }
            dev_ui::set_tooltip("Redo");

            if (ImGui::Button(ICON_FA_FILES_O))
            {
                clone_selection_hierarchical(sc->scene, &s_selection_list, "_cloned");
            }
            dev_ui::set_tooltip("Duplicate");

            ImGui::Separator();

            if (ImGui::Button(ICON_FA_LIST))
            {
                selection_list = true;
            }
            dev_ui::set_tooltip("Selection List");

            static const c8* transform_icons[]   = {ICON_FA_MOUSE_POINTER, ICON_FA_ARROWS, ICON_FA_REFRESH, ICON_FA_EXPAND,
                                                  ICON_FA_HAND_POINTER_O};
            static s32       num_transform_icons = PEN_ARRAY_SIZE(transform_icons);

            static const c8* transform_tooltip[] = {"Select (Q)", "Translate (W)", "Rotate (E)", "Scale (R)",
                                                    "Grab Physics (T)"};
            static_assert(PEN_ARRAY_SIZE(transform_tooltip) == PEN_ARRAY_SIZE(transform_icons), "mistmatched elements");

            static u32 widget_shortcut_key[] = {PK_Q, PK_W, PK_E, PK_R, PK_T};
            static_assert(PEN_ARRAY_SIZE(widget_shortcut_key) == PEN_ARRAY_SIZE(transform_tooltip), "mismatched elements");

            for (s32 i = 0; i < num_transform_icons; ++i)
            {
                u32 mode = TRANSFORM_SELECT + i;
                if (shortcut_key(widget_shortcut_key[i]))
                    k_transform_mode = (transform_mode)mode;

                if (put::dev_ui::state_button(transform_icons[i], k_transform_mode == mode))
                {
                    if (k_transform_mode == mode)
                        k_transform_mode = TRANSFORM_NONE;
                    else
                        k_transform_mode = (transform_mode)mode;
                }
                put::dev_ui::set_tooltip(transform_tooltip[i]);
            }

            if (ImGui::Button(ICON_FA_LINK) || shortcut_key(PK_P))
            {
                parent_selection(sc->scene);
            }
            put::dev_ui::set_tooltip("Parent (P)");

            if (shortcut_key(PK_I))
            {
                instance_selection(sc->scene);
            }

            ImGui::Separator();

            ImGui::EndMainMenuBar();

            if (open_open)
            {
                const c8* import = put::dev_ui::file_browser(open_open, dev_ui::FB_OPEN, 3, "**.pmm", "**.pms", "**.pmv");

                if (import)
                {
                    u32 len = pen::string_length(import);
                    
                    char pm = import[len - 1];
                    switch(pm)
                    {
                        case 'm':
                            put::ces::load_pmm(import, sc->scene);
                            break;
                        case 's':
                        {
                            put::ces::load_scene(import, sc->scene, open_merge);
                            
                            if (!open_merge)
                                k_model_view_controller.current_working_scene = import;
                            
                            Str fn = import;
                            dev_ui::set_program_preference_filename("last_loaded_scene", import);
                        }
                            break;
                        case 'v':
                            put::ces::load_pmv(import, sc->scene);
                            break;
                        default:
                            break;
                    }
                }
            }

            if (open_scene_browser)
            {
                ces::scene_browser_ui(sc->scene, &open_scene_browser);
            }

            if (open_camera_menu)
            {
                if (ImGui::Begin("Camera", &open_camera_menu))
                {
                    ImGui::Combo("Camera Mode", (s32*)&k_model_view_controller.camera_mode, (const c8**)&camera_mode_names,
                                 2);

                    if (ImGui::SliderFloat("FOV", &k_model_view_controller.main_camera.fov, 10, 180))
                        dev_ui::set_program_preference("camera_fov", k_model_view_controller.main_camera.fov);

                    if (ImGui::InputFloat("Near", &k_model_view_controller.main_camera.near_plane))
                        dev_ui::set_program_preference("camera_near", k_model_view_controller.main_camera.near_plane);

                    if (ImGui::InputFloat("Far", &k_model_view_controller.main_camera.far_plane))
                        dev_ui::set_program_preference("camera_far", k_model_view_controller.main_camera.far_plane);

                    k_model_view_controller.invalidated = true;

                    ImGui::End();
                }
            }

            if (open_resource_menu)
            {
                put::ces::enumerate_resources(&open_resource_menu);
            }

            if (open_save)
            {
                const c8* save_file = nullptr;
                if (open_save_as || k_model_view_controller.current_working_scene.length() == 0)
                {
                    save_file = put::dev_ui::file_browser(open_save, dev_ui::FB_SAVE, 1, "**.pms");
                }
                else
                {
                    save_file = k_model_view_controller.current_working_scene.c_str();
                }

                if (save_file)
                {
                    put::ces::save_scene(save_file, sc->scene);
                    k_model_view_controller.current_working_scene = save_file;
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
            if (pen::input_key(PK_MENU) || pen::input_key(PK_COMMAND) || (k_select_flags & WIDGET_SELECTED) ||
                (k_transform_mode == TRANSFORM_PHYSICS))
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
                picking_update(sc->scene, sc->camera);
            }

            if (selection_list)
            {
                enumerate_selection_ui(sc->scene, &selection_list);
            }

            // duplicate
            static bool debounce_duplicate = false;
            if (pen::input_key(PK_CONTROL) && shortcut_key(PK_D))
            {
                debounce_duplicate = true;
            }
            else if (debounce_duplicate)
            {
                clone_selection_hierarchical(sc->scene, &s_selection_list, "_cloned");
                debounce_duplicate = false;
            }

            // delete
            if (shortcut_key(PK_DELETE) || shortcut_key(PK_BACK))
                delete_selection(sc->scene);

            if (view_menu)
                view_ui(sc->scene, &view_menu);

            // todo move this to main?
            static u32 timer_index = -1;
            if (timer_index == -1)
            {
                timer_index = pen::timer_create("scene_update_timer");
                pen::timer_start(timer_index);
            }
            f32 dt_ms = pen::timer_elapsed_ms(timer_index);

            pen::timer_start(timer_index);

            put::ces::update_scene(sc->scene, dt_ms);

            update_undo_stack(sc->scene, dt_ms);
        }

        struct physics_preview
        {
            bool        active = false;
            cmp_physics params;

            physics_preview(){};
            ~physics_preview(){};
        };
        static physics_preview k_physics_preview;

        void scene_constraint_ui(entity_scene* scene)
        {
            physics::constraint_params& preview_constraint = k_physics_preview.params.constraint;
            s32                         constraint_type    = preview_constraint.type - 1;

            k_physics_preview.params.type = PHYSICS_TYPE_CONSTRAINT;

            ImGui::Combo("Constraint##Physics", (s32*)&constraint_type, "Six DOF\0Hinge\0Point to Point\0", 7);

            preview_constraint.type = constraint_type + 1;
            if (preview_constraint.type == physics::CONSTRAINT_HINGE)
            {
                ImGui::InputFloat3("Axis", (f32*)&preview_constraint.axis);
                ImGui::InputFloat("Lower Limit Rotation", (f32*)&preview_constraint.lower_limit_rotation);
                ImGui::InputFloat("Upper Limit Rotation", (f32*)&preview_constraint.upper_limit_rotation);
            }
            else if (preview_constraint.type == physics::CONSTRAINT_DOF6)
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
            for (s32 i = 0; i < scene->num_nodes; ++i)
            {
                if (scene->entities[i] & CMP_PHYSICS)
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

            bool valid_type = preview_constraint.type <= physics::CONSTRAINT_P2P && preview_constraint.type > 0;
            if (rb_index > -1 && valid_type)
            {
                preview_constraint.rb_indices[0] = index_lookup[rb_index];

                if (ImGui::Button("Add"))
                {
                    u32 sel_num = sb_count(s_selection_list);
                    for (u32 s = 0; s < sel_num; ++s)
                    {
                        u32 i = s_selection_list[s];

                        if (scene->entities[i] & CMP_CONSTRAINT)
                            continue;

                        scene->physics_data[i].constraint = preview_constraint;

                        instantiate_constraint(scene, i);
                    }
                }
            }

            if (sb_count(s_selection_list) == 1)
                scene->physics_data[s_selection_list[0]].constraint = preview_constraint;
        }

        void scene_rigid_body_ui(entity_scene* scene)
        {
            u32 collision_shape = k_physics_preview.params.rigid_body.shape - 1;

            ImGui::InputFloat("Mass", &k_physics_preview.params.rigid_body.mass);
            ImGui::Combo("Shape##Physics", (s32*)&collision_shape,
                         "Box\0Cylinder\0Sphere\0Capsule\0Cone\0Hull\0Mesh\0Compound\0", 7);

            k_physics_preview.params.rigid_body.shape = collision_shape + 1;

            Str button_text = "Set Start Transform";
            u32 sel_num     = sb_count(s_selection_list);
            for (u32 s = 0; s < sel_num; ++s)
            {
                u32 i = s_selection_list[s];

                if (!(scene->entities[i] & CMP_PHYSICS))
                {
                    button_text = "Add";
                    break;
                }
            }

            if (ImGui::Button(button_text.c_str()))
            {
                for (u32 s = 0; s < sel_num; ++s)
                {
                    u32 i                             = s_selection_list[s];
                    scene->physics_data[i].rigid_body = k_physics_preview.params.rigid_body;

                    scene->physics_data[i].rigid_body.position = scene->transforms[i].translation;
                    scene->physics_data[i].rigid_body.rotation = scene->transforms[i].rotation;

                    if (!(scene->entities[i] & CMP_PHYSICS))
                    {
                        instantiate_rigid_body(scene, i);
                    }

                    k_physics_preview.params.rigid_body = scene->physics_data[i].rigid_body;
                }
            }
        }

        void scene_physics_ui(entity_scene* scene)
        {
            static s32 physics_type  = 0;
            k_physics_preview.active = false;

            u32 num_selected = sb_count(s_selection_list);

            if (num_selected == 0)
                return;

            if (num_selected == 1)
            {
                k_physics_preview.params = scene->physics_data[s_selection_list[0]];
                physics_type             = k_physics_preview.params.type;
            }

            if (ImGui::CollapsingHeader("Physics"))
            {
                k_physics_preview.active = true;

                ImGui::Combo("Type##Physics", &physics_type, "Rigid Body\0Constraint\0");

                k_physics_preview.params.type = physics_type;

                if (physics_type == PHYSICS_TYPE_RIGID_BODY)
                {
                    // rb
                    scene_rigid_body_ui(scene);
                }
                else if (physics_type == PHYSICS_TYPE_CONSTRAINT)
                {
                    // constraint
                    scene_constraint_ui(scene);
                }
            }

            if (sb_count(s_selection_list) == 1)
                scene->physics_data[s_selection_list[0]] = k_physics_preview.params;
        }

        bool scene_geometry_ui(entity_scene* scene)
        {
            bool iv = false;

            if (sb_count(s_selection_list) != 1)
                return iv;

            u32 selected_index = s_selection_list[0];

            // geom
            if (ImGui::CollapsingHeader("Geometry"))
            {
                if (scene->geometry_names[selected_index].c_str())
                {
                    ImGui::Text("Geometry Name: %s", scene->geometry_names[selected_index].c_str());
                    return iv;
                }

                static s32 primitive_type = -1;
                ImGui::Combo("Shape##Primitive", (s32*)&primitive_type, "Box\0Cylinder\0Sphere\0Capsule\0Cone\0", 5);

                if (ImGui::Button("Add Primitive") && primitive_type > -1)
                {
                    iv = true;

                    geometry_resource* gr = get_geometry_resource(k_primitives[primitive_type]);

                    instantiate_geometry(gr, scene, selected_index);

                    material_resource* mr = get_material_resource(PEN_HASH("default_material"));

                    instantiate_material(mr, scene, selected_index);

                    scene->geometry_names[selected_index] = gr->geometry_name;

                    instantiate_model_cbuffer(scene, selected_index);

                    scene->entities[selected_index] |= CMP_TRANSFORM;
                }
            }

            return iv;
        }

        bool scene_transform_ui(entity_scene* scene)
        {
            if (sb_count(s_selection_list) != 1)
                return false;

            u32 selected_index = s_selection_list[0];

            if (ImGui::CollapsingHeader("Transform"))
            {
                bool           perform_transform = false;
                cmp_transform& t                 = scene->transforms[selected_index];
                perform_transform |= ImGui::InputFloat3("Translation", (float*)&t.translation);

                vec3f euler = t.rotation.to_euler();
                euler       = euler * (f32)M_PI_OVER_180;

                if (ImGui::InputFloat3("Rotation", (float*)&euler))
                {
                    euler = euler * (f32)M_180_OVER_PI;
                    t.rotation.euler_angles(euler.z, euler.y, euler.x);
                    perform_transform = true;
                }

                perform_transform |= ImGui::InputFloat3("Scale", (float*)&t.scale);

                if (perform_transform)
                {
                    scene->entities[selected_index] |= CMP_TRANSFORM;
                }

                apply_transform_to_selection(scene, vec3f::zero());
            }

            // transforms undos are handled by apply_transform_to_selection
            return false;
        }

        bool scene_material_ui(entity_scene* scene)
        {
            bool iv = false;

            u32 num_selected = sb_count(s_selection_list);
            if (num_selected <= 0)
                return false;

            u32 selected_index = s_selection_list[0];

            // master mat
            cmp_material& mm = scene->materials[selected_index];

            // multi parameters
            s32 shader = mm.pmfx_shader;
            s32 technique = mm.technique;

            // set parameters if all are shared, if not set to invalid
            for (u32 i = 1; i < num_selected; ++i)
            {
                cmp_material& m2 = scene->materials[s_selection_list[i]];

                if (shader != m2.pmfx_shader)
                    shader = PEN_INVALID_HANDLE;

                if (technique != m2.technique)
                    technique = PEN_INVALID_HANDLE;
            }

            // material
            if (ImGui::CollapsingHeader("Material"))
            {
                cmp_material_data mat = scene->material_data[selected_index];

                ImGui::Text("%s", scene->material_names[selected_index].c_str());

                if (scene->entities[s_selection_list[0]] & CMP_MATERIAL)
                {
                    u32 count = 0;
                    for (u32 t = 0; t < put::ces::SN_NUM_TEXTURES; ++t)
                    {
                        if (scene->materials[selected_index].texture_handles[t] > 0)
                        {
                            if (count++ > 0)
                                ImGui::SameLine();

                            ImGui::Image(&scene->materials[selected_index].texture_handles[t], ImVec2(64, 64));
                        }
                    }

                    auto& mm = scene->materials[selected_index];
                    bool cm = false;

                    u32        num_shaders;
                    const c8** shader_list = pmfx::get_shader_list(num_shaders);
                    cm |= ImGui::Combo("Shader", (s32*)&shader, shader_list, num_shaders);

                    u32        num_techniques;
                    const c8** technique_list = pmfx::get_technique_list(mm.pmfx_shader, num_techniques);
                    cm |= ImGui::Combo("Technique", (s32*)&technique, technique_list, num_techniques);

                    // apply shader changes
                    if (cm)
                    {
                        for (u32 i = 0; i < num_selected; ++i)
                        {
                            scene->materials[s_selection_list[i]].pmfx_shader = shader;
                            scene->materials[s_selection_list[i]].technique = technique;
                        }
                    }

                    pmfx::technique_constant* tc = pmfx::get_technique_constants(shader, technique);

                    if (tc)
                    {
                        u32 num_constants = sb_count(tc);
                        for (u32 i = 0; i < num_constants; ++i)
                        {
                            float* f = &mat.data[tc[i].cb_offset];

                            switch (tc[i].widget)
                            {
                            case pmfx::CW_INPUT:
                                ImGui::InputFloatN(tc[i].name.c_str(), f, tc[i].num_elements, 3, 0);
                                break;
                            case pmfx::CW_SLIDER:
                                ImGui::SliderFloatN(tc[i].name.c_str(), f, tc[i].num_elements, tc[i].min, tc[i].max, "%.3f", 1.0f);
                                break;
                            case pmfx::CW_COLOUR:
                                ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(f[0], f[1], f[2], 1.0f));
                                if (ImGui::CollapsingHeader(tc[i].name.c_str()))
                                {
                                    if (tc[i].num_elements == 3)
                                    {
                                        ImGui::ColorPicker3(tc[i].name.c_str(), f);
                                    }
                                    else
                                    {
                                        ImGui::ColorPicker4(tc[i].name.c_str(), f);
                                    }
                                }
                                ImGui::PopStyleColor();
                                break;
                            }
                        }

                        // apply technique constant changes
                        cmp_material_data pre_edit = scene->material_data[selected_index];

                        for (u32 i = 0; i < num_selected; ++i)
                        {
                            u32 si = s_selection_list[i];

                            for (u32 c = 0; c < num_constants; ++c)
                            {
                                u32 cb_offset = tc[c].cb_offset;
                                u32 tc_size = sizeof(f32) * tc[c].num_elements;

                                f32* f1 = &mat.data[cb_offset];
                                f32* f2 = &pre_edit.data[cb_offset];

                                if (memcmp(f1, f2, tc_size) == 0)
                                    continue;

                                f32* f3 = &scene->material_data[si].data[cb_offset];
                                memcpy(f3, f1, tc_size);
                            }
                        }
                    }
                }
            }

            return iv;
        }

        void scene_anim_ui(entity_scene* scene)
        {
            if (sb_count(s_selection_list) != 1)
                return;

            u32 selected_index = s_selection_list[0];

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
                        s32   h    = scene->anim_controller[selected_index].handles[ih];
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
                    }

                    if (open_anim_import)
                    {
                        const c8* anim_import = put::dev_ui::file_browser(open_anim_import, dev_ui::FB_OPEN, 1, "**.pma");

                        if (anim_import)
                        {
                            anim_handle ah   = load_pma(anim_import);
                            auto*       anim = get_animation_resource(ah);

                            if (is_valid(ah))
                            {
                                // validate that the anim can fit the rig
                                std::vector<s32> joint_indices;
                                build_heirarchy_node_list(scene, selected_index, joint_indices);

                                s32  channel_index = 0;
                                s32  joints_offset = -1; // scene tree has a -1 node
                                bool compatible    = true;
                                for (s32 jj = 0; jj < joint_indices.size(); ++jj)
                                {
                                    s32 jnode = joint_indices[jj];

                                    if (scene->entities[jnode] & CMP_BONE && jnode > -1)
                                    {
                                        if (anim->channels[channel_index].target != scene->id_name[jnode])
                                        {
                                            dev_console_log_level(dev_ui::CONSOLE_ERROR, "%s",
                                                                  "[error] animation - does not fit rig");

                                            compatible = false;
                                            break;
                                        }

                                        channel_index++;
                                    }
                                    else
                                    {
                                        joints_offset++;
                                    }
                                }

                                if (compatible)
                                {
                                    scene->anim_controller[selected_index].joints_offset = joints_offset;
                                    scene->entities[selected_index] |= CMP_ANIM_CONTROLLER;

                                    bool exists = false;

                                    s32 size = sb_count(controller.handles);
                                    for (s32 h = 0; h < size; ++h)
                                        if (h == ah)
                                            exists = true;

                                    if (!exists)
                                        sb_push(scene->anim_controller[selected_index].handles, ah);
                                }
                            }
                        }
                    }

                    ImGui::Separator();
                }
            }
        }

        void scene_light_ui(entity_scene* scene)
        {
            if (sb_count(s_selection_list) != 1)
                return;

            u32 selected_index = s_selection_list[0];

            static bool colour_picker_open = false;

            if (ImGui::CollapsingHeader("Light"))
            {
                if (scene->entities[selected_index] & CMP_LIGHT)
                {
                    cmp_light& snl = scene->lights[selected_index];

                    ImGui::Combo("Type", (s32*)&scene->lights[selected_index].type, "Directional\0Point\0Spot\0Area Box (wip)\0",
                                 4);

                    switch (scene->lights[selected_index].type)
                    {
                        case LIGHT_TYPE_DIR:
                            ImGui::SliderAngle("Azimuth", &snl.azimuth);
                            ImGui::SliderAngle("Altitude", &snl.altitude);
                            break;

                        case LIGHT_TYPE_POINT:
                            ImGui::SliderFloat("Radius##slider", &snl.radius, 0.0f, 100.0f);
                            ImGui::InputFloat("Radius##input", &snl.radius);
                            break;

                        case LIGHT_TYPE_SPOT:
                            ImGui::SliderAngle("Cos Cutoff", &snl.cos_cutoff, -60.0f, -22.0f);
                            ImGui::InputFloat("Falloff", &snl.spot_falloff, 0.01);
                            break;

                        case LIGHT_TYPE_AREA_BOX:
                            break;
                    }

                    snl.direction = maths::azimuth_altitude_to_xyz(snl.azimuth, snl.altitude);

                    vec3f& col = scene->lights[selected_index].colour;
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(col.x, col.y, col.z, 1.0f));

                    if (ImGui::Button("Colour"))
                        colour_picker_open = true;

                    ImGui::PopStyleColor();
                }
                else
                {
                    if (ImGui::Button("Add Light"))
                    {
                        s32 s = selected_index;

                        // todo move to resources
                        scene->entities[s] |= CMP_LIGHT;

                        scene->bounding_volumes[s].min_extents = -vec3f::one();
                        scene->bounding_volumes[s].max_extents = vec3f::one();
                    }
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

        void scene_browser_ui(entity_scene* scene, bool* open)
        {
            if (ImGui::Begin("Scene Browser", open))
            {
                if (ImGui::Button(ICON_FA_PLUS))
                {
                    u32 ni = ces::get_next_node(scene);
                    store_node_state(scene, ni, UNDO);

                    u32 nn = ces::get_new_node(scene);

                    scene->entities[nn] |= CMP_ALLOCATED;

                    scene->names[nn] = "node_";
                    scene->names[nn].appendf("%u", nn);

                    scene->parents[nn] = nn;

                    scene->transforms[nn].translation = vec3f::zero();
                    scene->transforms[nn].rotation.euler_angles(0.0f, 0.0f, 0.0f);
                    scene->transforms[nn].scale = vec3f::one();

                    scene->entities[nn] |= CMP_TRANSFORM;

                    add_selection(scene, nn);

                    store_node_state(scene, ni, REDO);
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

                static scene_num_dump dumps[] = {{CMP_ALLOCATED, "Allocated", 0},
                                                 {CMP_GEOMETRY, "Geometries", 0},
                                                 {CMP_BONE, "Bones", 0},
                                                 {CMP_ANIM_CONTROLLER, "Anim Controllers", 0}};

                if (ImGui::CollapsingHeader("Scene Info"))
                {
                    ImGui::Text("Total Scene Nodes: %i", scene->num_nodes);
                    ImGui::Text("Selected: %i", (s32)sb_count(s_selection_list));

                    for (s32 i = 0; i < PEN_ARRAY_SIZE(dumps); ++i)
                        dumps[i].count = 0;

                    for (s32 i = 0; i < scene->num_nodes; ++i)
                        for (s32 j = 0; j < PEN_ARRAY_SIZE(dumps); ++j)
                            if (scene->entities[i] & dumps[j].component)
                                dumps[j].count++;

                    for (s32 i = 0; i < PEN_ARRAY_SIZE(dumps); ++i)
                        ImGui::Text("%s: %i", dumps[i].display_name, dumps[i].count);
                }

                ImGui::BeginChild("Entities", ImVec2(0, 300), true);

                s32 selected_index = -1;
                if (sb_count(s_selection_list) == 1)
                    selected_index = s_selection_list[0];

                if (list_view)
                {
                    for (u32 i = 0; i < scene->num_nodes; ++i)
                    {
                        if (!(scene->entities[i] & CMP_ALLOCATED))
                            continue;

                        bool selected = scene->state_flags[i] & SF_SELECTED;

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
                    if (scene->flags & INVALIDATE_SCENE_TREE)
                    {
                        tree = scene_tree();
                        build_scene_tree(scene, -1, tree);

                        scene->flags &= ~INVALIDATE_SCENE_TREE;
                    }

                    s32 pre_selected = selected_index;
                    scene_tree_enumerate(scene, tree);

                    if (pre_selected != selected_index)
                        add_selection(scene, selected_index);
                }

                ImGui::EndChild();

                // node header
                if (selected_index != -1)
                {
                    static c8 buf[64];
                    u32       end_pos = std::min<u32>(scene->names[selected_index].length(), 64);
                    pen::memory_cpy(buf, scene->names[selected_index].c_str(), end_pos);
                    buf[end_pos] = '\0';

                    if (ImGui::InputText("", buf, 64))
                    {
                        scene->names[selected_index]   = buf;
                        scene->id_name[selected_index] = PEN_HASH(buf);
                    }

                    ImGui::SameLine();
                    ImGui::Text("ces node index %i", selected_index);

                    s32 parent_index = scene->parents[selected_index];
                    if (parent_index != selected_index)
                        ImGui::Text("Parent: %s", scene->names[parent_index].c_str());
                }

                // Transform undo's are handled by the transform selection function
                scene_transform_ui(scene);

                // Undoable actions
                if (sb_count(s_selection_list) == 1)
                {
                    store_node_state(scene, s_selection_list[0], UNDO);
                }

                scene_physics_ui(scene);

                scene_geometry_ui(scene);

                scene_anim_ui(scene);

                scene_light_ui(scene);

                scene_material_ui(scene);

                if (sb_count(s_selection_list) == 1)
                {
                    store_node_state(scene, s_selection_list[0], REDO);
                }

                ImGui::End();
            }
        }

        void apply_transform_to_selection(entity_scene* scene, const vec3f move_axis)
        {
            if (move_axis == vec3f::zero())
                return;

            u32 sel_num = sb_count(s_selection_list);
            for (u32 s = 0; s < sel_num; ++s)
            {
                u32 i = s_selection_list[s];

                // only move if parent isnt selected
                s32 parent = scene->parents[i];
                if (parent != i)
                {
                    bool found = false;
                    for (u32 t = 0; t < sel_num; ++t)
                        if (s_selection_list[t] == parent)
                        {
                            found = true;
                            break;
                        }

                    if (found)
                        continue;
                }

                store_node_state(scene, i, UNDO);

                cmp_transform& t = scene->transforms[i];
                if (k_transform_mode == TRANSFORM_TRANSLATE)
                    t.translation += move_axis;
                if (k_transform_mode == TRANSFORM_SCALE)
                    t.scale += move_axis * 0.1f;
                if (k_transform_mode == TRANSFORM_ROTATE)
                {
                    quat q;
                    q.euler_angles(move_axis.z, move_axis.y, move_axis.x);

                    t.rotation = q * t.rotation;
                }

                if (!(scene->entities[i] & CMP_TRANSFORM))
                {
                    // save history
                }

                scene->entities[i] |= CMP_TRANSFORM;

                store_node_state(scene, i, REDO);
            }
        }

        struct physics_pick
        {
            vec3f pos;
            a_u8  state          = {0};
            bool  grabbed        = false;
            s32   constraint     = -1;
            s32   physics_handle = -1;
        };
        physics_pick k_physics_pick_info;

        void physics_pick_callback(const physics::ray_cast_result& result)
        {
            k_physics_pick_info.state          = PICKING_COMPLETE;
            k_physics_pick_info.pos            = result.point;
            k_physics_pick_info.grabbed        = false;
            k_physics_pick_info.physics_handle = result.physics_handle;
        }

        void transform_widget(const scene_view& view)
        {
            if(pen::input_key(PK_MENU) || pen::input_key(PK_COMMAND))
                return;
            
            k_select_flags &= ~(WIDGET_SELECTED);

            entity_scene* scene = view.scene;
            vec2i         vpi   = vec2i(view.viewport->width, view.viewport->height);

            static vec3f widget_points[4];
            static vec3f pre_click_axis_pos[3];
            static u32   selected_axis    = 0;
            static f32   selection_radius = 5.0f;

            const pen::mouse_state& ms      = pen::input_get_mouse_state();
            vec3f                   mousev3 = vec3f(ms.x, view.viewport->height - ms.y, 0.0f);

            mat4  view_proj = view.camera->proj * view.camera->view;
            vec3f r0        = maths::unproject_sc(vec3f(mousev3.x, mousev3.y, 0.0f), view_proj, vpi);
            vec3f r1        = maths::unproject_sc(vec3f(mousev3.x, mousev3.y, 1.0f), view_proj, vpi);
            vec3f vr        = normalised(r1 - r0);

            if (k_transform_mode == TRANSFORM_PHYSICS)
            {
                if (!k_physics_pick_info.grabbed && k_physics_pick_info.constraint == -1)
                {
                    if (k_physics_pick_info.state == PICKING_READY)
                    {
                        if (ms.buttons[PEN_MOUSE_L])
                        {
                            k_physics_pick_info.state = PICKING_SINGLE;

                            physics::ray_cast_params rcp;
                            rcp.start     = r0;
                            rcp.end       = r1;
                            rcp.timestamp = pen::get_time_ms();
                            rcp.callback  = physics_pick_callback;

                            physics::cast_ray(rcp);
                        }
                    }
                    else if (k_physics_pick_info.state == PICKING_COMPLETE)
                    {
                        if (k_physics_pick_info.physics_handle != -1)
                        {
                            physics::constraint_params cp;
                            cp.pivot         = k_physics_pick_info.pos;
                            cp.type          = physics::CONSTRAINT_P2P;
                            cp.rb_indices[0] = k_physics_pick_info.physics_handle;

                            k_physics_pick_info.constraint = physics::add_constraint(cp);
                            k_physics_pick_info.state      = PICKING_GRABBED;
                        }
                        else
                        {
                            k_physics_pick_info.state = PICKING_READY;
                        }
                    }
                }
                else if (k_physics_pick_info.constraint > 0)
                {
                    if (ms.buttons[PEN_MOUSE_L])
                    {
                        vec3f new_pos =
                            maths::ray_plane_intersect(r0, vr, k_physics_pick_info.pos, view.camera->view.get_fwd());

                        physics::set_v3(k_physics_pick_info.constraint, new_pos, physics::CMD_SET_P2P_CONSTRAINT_POS);
                    }
                    else
                    {
                        physics::release_entity(k_physics_pick_info.constraint);
                        k_physics_pick_info.constraint = -1;
                        k_physics_pick_info.state      = PICKING_READY;
                    }
                }
            }

            if (sb_count(s_selection_list) == 0)
                return;

            vec3f pos = vec3f::zero();
            vec3f min = vec3f::flt_max();
            vec3f max = -vec3f::flt_max();

            u32 sel_num = sb_count(s_selection_list);
            for (u32 i = 0; i < sel_num; ++i)
            {
                u32 s = s_selection_list[i];

                if (scene->entities[s] & CMP_LIGHT)
                {
                    pos += scene->transforms[s].translation;
                    continue;
                }

                vec3f& _min = scene->bounding_volumes[s].transformed_min_extents;
                vec3f& _max = scene->bounding_volumes[s].transformed_max_extents;

                min = vec3f::vmin(min, _min);
                max = vec3f::vmax(max, _max);

                pos += _min + (_max - _min) * 0.5f;
            }

            f32 extents_mag = mag(max - min);

            pos /= (f32)sb_count(s_selection_list);

            mat4 widget;
            widget.set_vectors(vec3f::unit_x(), vec3f::unit_y(), vec3f::unit_z(), pos);

            if (shortcut_key(PK_F))
            {
                view.camera->focus = pos;
                view.camera->zoom  = extents_mag;
            }

            // distance for consistent-ish size
            mat4 res = view.camera->proj * view.camera->view;

            f32   w          = 1.0;
            vec3f screen_pos = res.transform_vector(pos, w);
            f32   d          = fabs(screen_pos.z) * 0.1f;

            if (screen_pos.z < -0.0)
                return;

            if (k_transform_mode == TRANSFORM_ROTATE)
            {
                float rd = d * 0.75;

                vec3f plane_normals[] = {vec3f(1.0f, 0.0f, 0.0f), vec3f(0.0f, 1.0f, 0.0f), vec3f(0.0f, 0.0f, 1.0f)};

                vec3f       _cp         = vec3f::zero();
                static bool selected[3] = {0};
                for (s32 i = 0; i < 3; ++i)
                {
                    vec3f cp = maths::ray_plane_intersect(r0, vr, pos, plane_normals[i]);

                    if (!ms.buttons[PEN_MOUSE_L])
                    {
                        selected[i] = false;
                        f32 dd      = mag(cp - pos);
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
                for (s32 i = 0; i < 3; ++i)
                {
                    if (!ms.buttons[PEN_MOUSE_L])
                    {
                        attach_point = _cp;
                        continue;
                    }

                    if (selected[i])
                    {
                        k_select_flags |= WIDGET_SELECTED;

                        vec3f prev_line = normalised(attach_point - pos);
                        vec3f cur_line  = normalised(_cp - pos);

                        dbg::add_line(pos, attach_point, vec4f::cyan());
                        dbg::add_line(pos, _cp, vec4f::magenta());

                        vec3f x   = cross(prev_line, cur_line);
                        f32   amt = dot(x, plane_normals[i]);

                        apply_transform_to_selection(view.scene, plane_normals[i] * amt);

                        attach_point = _cp;
                        break;
                    }
                }

                return;
            }

            if (k_transform_mode == TRANSFORM_TRANSLATE || k_transform_mode == TRANSFORM_SCALE)
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

                    pp[i]   = maths::project_to_sc(widget_points[i], view_proj, vpi);
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

                    ppj[j_index]   = maths::project_to_sc(pos + unit_axis[i + 1] * d * 0.3f, view_proj, vpi);
                    ppj[j_index].z = 0.0f;

                    ppj[j_index + 1]   = maths::project_to_sc(pos + unit_axis[next_index] * d * 0.3f, view_proj, vpi);
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
                        u32 i_next  = i + 2;
                        u32 ii      = i + 1;

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
                        k_select_flags |= WIDGET_SELECTED;
                        col = vec4f::one();
                    }

                    put::dbg::add_line_2f(pp[0].xy, pp[i].xy, col);

                    if (k_transform_mode == TRANSFORM_TRANSLATE)
                    {
                        vec2f v  = normalised(pp[i].xy - pp[0].xy);
                        vec2f px = perp(v) * 5.0f;

                        vec2f base = pp[i].xy - v * 5.0f;

                        put::dbg::add_line_2f(pp[i].xy, base + px, col);
                        put::dbg::add_line_2f(pp[i].xy, base - px, col);
                    }
                    else if (k_transform_mode == TRANSFORM_SCALE)
                    {
                        put::dbg::add_quad_2f(pp[i].xy, vec2f(3.0f, 3.0f), col);
                    }
                }

                // draw joins
                for (s32 i = 0; i < 3; ++i)
                {
                    u32 j_index = i * 2;
                    u32 i_next  = i + 2;
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

                    vec3f plane_normal = cross(translation_axis[i], view.camera->view.get_up());

                    if (i == 1)
                        plane_normal = cross(translation_axis[i], view.camera->view.get_right());

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

        void render_constraint(const entity_scene* scene, u32 index, const physics::constraint_params& con)
        {
            vec3f pos  = scene->transforms[index].translation;
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
                case physics::CONSTRAINT_HINGE:
                    put::dbg::add_circle(axis, pos, 0.25f, vec4f::green());
                    put::dbg::add_line(pos - axis, pos + axis, vec4f::green());
                    put::dbg::add_circle_segment(axis, pos, 1.0f, min_rot, max_rot, vec4f::white());
                    break;

                case physics::CONSTRAINT_P2P:
                    put::dbg::add_point(pos, 0.5f, vec4f::magenta());
                    put::dbg::add_line(link_pos, pos, vec4f::magenta());
                    break;

                case physics::CONSTRAINT_DOF6:
                    put::dbg::add_circle_segment(vec3f::unit_x(), pos, 0.25f, min_rot_v3.x, max_rot_v3.x, vec4f::white());
                    put::dbg::add_circle_segment(vec3f::unit_y(), pos, 0.25f, min_rot_v3.y, max_rot_v3.y, vec4f::white());
                    put::dbg::add_circle_segment(vec3f::unit_z(), pos, 0.25f, min_rot_v3.z, max_rot_v3.z, vec4f::white());
                    put::dbg::add_aabb(pos + min_pos_v3, pos + max_pos_v3, vec4f::white());
                    break;
            }
        }

        void render_physics_debug(const scene_view& view)
        {
            entity_scene* scene = view.scene;

            pen::renderer_set_constant_buffer(view.cb_view, 0, PEN_SHADER_TYPE_VS);
            pen::renderer_set_constant_buffer(view.cb_view, 0, PEN_SHADER_TYPE_PS);

            for (u32 n = 0; n < scene->num_nodes; ++n)
            {
                if (!(scene->state_flags[n] & SF_SELECTED) && !(scene->view_flags & DD_PHYSICS))
                    continue;

                bool preview_rb = k_physics_preview.active && k_physics_preview.params.type == PHYSICS_TYPE_RIGID_BODY;

                if ((scene->entities[n] & CMP_PHYSICS) || preview_rb)
                {
                    u32 prim = scene->physics_data[n].rigid_body.shape;

                    if (prim == 0)
                        continue;

                    geometry_resource* gr = get_geometry_resource(k_primitives[prim - 1]);

                    if (!pmfx::set_technique(view.pmfx_shader, view.technique, 0))
                        continue;

                    // draw
                    pen::renderer_set_constant_buffer(scene->cbuffer[n], 1, PEN_SHADER_TYPE_VS);
                    pen::renderer_set_constant_buffer(scene->cbuffer[n], 1, PEN_SHADER_TYPE_PS);
                    pen::renderer_set_vertex_buffer(gr->vertex_buffer, 0, gr->vertex_size, 0);
                    pen::renderer_set_index_buffer(gr->index_buffer, gr->index_type, 0);
                    pen::renderer_draw_indexed(gr->num_indices, 0, 0, PEN_PT_TRIANGLELIST);
                }

                bool preview_con = k_physics_preview.active && k_physics_preview.params.type == PHYSICS_TYPE_CONSTRAINT;

                if (preview_con)
                    render_constraint(scene, n, k_physics_preview.params.constraint);

                if ((scene->entities[n] & CMP_CONSTRAINT))
                    render_constraint(scene, n, scene->physics_data[n].constraint);
            }
        }

        void render_scene_editor(const scene_view& view)
        {
            vec2i vpi = vec2i(view.viewport->width, view.viewport->height);

            entity_scene* scene = view.scene;

            dbg::add_frustum(view.camera->camera_frustum.corners[0], view.camera->camera_frustum.corners[1]);

            render_physics_debug(view);

            mat4 view_proj = view.camera->proj * view.camera->view;

            if (scene->view_flags & DD_LIGHTS)
            {
                for (u32 n = 0; n < scene->num_nodes; ++n)
                {
                    if (scene->entities[n] & CMP_LIGHT)
                    {
                        vec3f p = scene->world_matrices[n].get_translation();

                        p = maths::project_to_sc(p, view_proj, vpi);

                        if (p.z > 0.0f)
                            put::dbg::add_quad_2f(p.xy, vec2f(5.0f, 5.0f), vec4f(scene->lights[n].colour, 1.0f));
                    }
                }
            }

            if (scene->view_flags & DD_MATRIX)
            {
                for (u32 n = 0; n < scene->num_nodes; ++n)
                {
                    put::dbg::add_coord_space(scene->world_matrices[n], 0.5f);
                }
            }

            if (scene->view_flags & DD_AABB)
            {
                for (u32 n = 0; n < scene->num_nodes; ++n)
                {
                    dbg::add_aabb(scene->bounding_volumes[n].transformed_min_extents,
                                  scene->bounding_volumes[n].transformed_max_extents);
                }
            }

            if (scene->view_flags & DD_NODE)
            {
                u32 sel_num = sb_count(s_selection_list);
                for (u32 i = 0; i < sel_num; ++i)
                {
                    u32 s = s_selection_list[i];

                    if (scene->entities[s] & CMP_LIGHT)
                    {
                        cmp_light& snl = scene->lights[s];

                        if (snl.type == LIGHT_TYPE_DIR)
                        {
                            dbg::add_line(vec3f::zero(), snl.direction * 10000.0f, vec4f(snl.colour, 1.0f));
                        }

                        if (snl.type == LIGHT_TYPE_SPOT)
                        {
                            vec3f dir = scene->world_matrices[s].get_fwd();
                            vec3f pos = scene->world_matrices[s].get_translation();

                            dbg::add_line(pos, pos + dir * 100.0f, vec4f(snl.colour, 1.0f));
                        }

                        if (snl.type == LIGHT_TYPE_AREA_BOX)
                        {
                            dbg::add_obb(scene->world_matrices[s], vec4f(snl.colour, 1.0f));
                        }
                    }
                    else
                    {
                        dbg::add_aabb(scene->bounding_volumes[s].transformed_min_extents,
                                      scene->bounding_volumes[s].transformed_max_extents);
                    }
                }
            }

            if (scene->view_flags & DD_SELECTED_CHILDREN)
            {
                for (u32 n = 0; n < scene->num_nodes; ++n)
                {
                    vec4f col      = vec4f::white();
                    bool  selected = false;
                    if (scene->state_flags[n] & SF_SELECTED)
                        selected = true;

                    u32 p = scene->parents[n];
                    if (p != n)
                    {
                        if (scene->state_flags[p] & SF_SELECTED || scene->state_flags[p] & SF_CHILD_SELECTED)
                        {
                            scene->state_flags[n] |= SF_CHILD_SELECTED;
                            selected = true;
                            col      = vec4f(0.75f, 0.75f, 0.75f, 1.0f);
                        }
                        else
                        {
                            scene->state_flags[n] &= ~SF_CHILD_SELECTED;
                        }
                    }

                    if (selected)
                        dbg::add_aabb(scene->bounding_volumes[n].transformed_min_extents,
                                      scene->bounding_volumes[n].transformed_max_extents, col);
                }
            }

            if (scene->view_flags & DD_BONES)
            {
                for (u32 n = 0; n < scene->num_nodes; ++n)
                {
                    if (!(scene->entities[n] & CMP_BONE))
                        continue;

                    if (scene->entities[n] & CMP_ANIM_TRAJECTORY)
                    {
                        vec3f p = scene->world_matrices[n].get_translation();

                        put::dbg::add_aabb(p - vec3f(0.1f, 0.1f, 0.1f), p + vec3f(0.1f, 0.1f, 0.1f), vec4f::green());
                    }

                    u32 p = scene->parents[n];
                    if (p != n)
                    {
                        if (!(scene->entities[p] & CMP_BONE) || (scene->entities[p] & CMP_ANIM_TRAJECTORY))
                            continue;

                        vec3f p1 = scene->world_matrices[n].get_translation();
                        vec3f p2 = scene->world_matrices[p].get_translation();

                        put::dbg::add_line(p1, p2, vec4f::one());
                    }
                }
            }

            if (scene->view_flags & DD_GRID)
            {
                f32 divisions = k_model_view_controller.grid_size / k_model_view_controller.grid_cell_size;
                put::dbg::add_grid(vec3f::zero(), vec3f(k_model_view_controller.grid_size), vec3f(divisions));
            }

            put::dbg::render_3d(view.cb_view);

            // no depth test
            u32 depth_disabled = pmfx::get_render_state_by_name(PEN_HASH("disabled_depth_stencil_state"));
            u32 fill           = pmfx::get_render_state_by_name(PEN_HASH("default_raster_state"));

            pen::renderer_set_depth_stencil_state(depth_disabled);
            pen::renderer_set_rasterizer_state(fill);

            transform_widget(view);

            put::dbg::render_3d(view.cb_view);

            put::dbg::render_2d(view.cb_2d_view);

            // reset depth state
            pen::renderer_set_depth_stencil_state(view.depth_stencil_state);
        }
    } // namespace ces
} // namespace put
