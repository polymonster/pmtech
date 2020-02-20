// ecs_scene.cpp
// Copyright 2014 - 2019 Alex Dixon.
// License: https://github.com/polymonster/pmtech/blob/master/license.md

#include <fstream>
#include <functional>

#include "console.h"
#include "data_struct.h"
#include "debug_render.h"
#include "dev_ui.h"
#include "file_system.h"
#include "hash.h"
#include "pmfx.h"
#include "str/Str.h"
#include "str_utilities.h"
#include "timer.h"
#include "os.h"

#include "ecs/ecs_resources.h"
#include "ecs/ecs_scene.h"
#include "ecs/ecs_utilities.h"

using namespace put;

namespace put
{
    namespace ecs
    {
        static std::vector<ecs_scene_instance> s_scenes;

        void register_ecs_extentsions(ecs_scene* scene, const ecs_extension& ext)
        {
            sb_push(scene->extensions, ext);
            scene->num_components += ext.num_components;

            resize_scene_buffers(scene);
        }

        void unregister_ecs_extensions(ecs_scene* scene)
        {
            // todo must make func
            u32 num_ext = sb_count(scene->extensions);
            for (u32 e = 0; e < num_ext; ++e)
                scene->extensions[e].shutdown(scene->extensions[e]);

            sb_free(scene->extensions);
        }

        void register_ecs_controller(ecs_scene* scene, const ecs_controller& controller)
        {
            sb_push(scene->controllers, controller);
        }

        void initialise_free_list(ecs_scene* scene)
        {
            scene->free_list_head = nullptr;

            for (s32 i = scene->soa_size - 1; i >= 0; --i)
            {
                scene->free_list[i].node = i;

                if (!(scene->entities[i] & e_cmp::allocated))
                {
                    free_node_list* l = &scene->free_list[i];
                    l->next = scene->free_list_head;

                    if (l->next)
                        l->next->prev = l;

                    scene->free_list_head = l;
                }
            }

            if (!scene->free_list_head)
                PEN_ASSERT(0);
        }

        void resize_scene_buffers(ecs_scene* scene, s32 size)
        {
            u32 new_size = scene->soa_size + size;

            for (u32 i = 0; i < scene->num_components; ++i)
            {
                generic_cmp_array& cmp = scene->get_component_array(i);
                u32                alloc_size = cmp.size * new_size;

                if (cmp.data)
                {
                    // realloc
                    cmp.data = pen::memory_realloc(cmp.data, alloc_size);

                    // zero new mem
                    u32 prev_size = scene->soa_size * cmp.size;
                    u8* new_offset = (u8*)cmp.data + prev_size;
                    u32 zero_size = alloc_size - prev_size;
                    pen::memory_zero(new_offset, zero_size);

                    continue;
                }

                // alloc and zero
                cmp.data = pen::memory_alloc(alloc_size);
                pen::memory_zero(cmp.data, alloc_size);
            }

            scene->soa_size = new_size;
            initialise_free_list(scene);
        }

        void free_scene_buffers(ecs_scene* scene, bool cmp_mem_only = 0)
        {
            // Remove entites for sub systems (physics, rendering, etc)
            if (!cmp_mem_only)
            {
                for (s32 i = 0; i < scene->num_entities; ++i)
                    delete_entity_first_pass(scene, i);

                for (s32 i = 0; i < scene->num_entities; ++i)
                    delete_entity_second_pass(scene, i);
            }

            // Free component array memory
            for (u32 i = 0; i < scene->num_components; ++i)
            {
                generic_cmp_array& cmp = scene->get_component_array(i);
                pen::memory_free(cmp.data);
                cmp.data = nullptr;
            }

            scene->soa_size = 0;
            scene->num_entities = 0;
        }

        void zero_entity_components(ecs_scene* scene, u32 node_index)
        {
            for (u32 i = 0; i < scene->num_components; ++i)
            {
                generic_cmp_array& cmp = scene->get_component_array(i);
                u8*                offset = (u8*)cmp.data + node_index * cmp.size;
                pen::memory_zero(offset, cmp.size);
            }

            // Annoyingly nodeindex == parent is used to determine if a node is not a child
            scene->parents[node_index] = node_index;
        }

        void delete_entity(ecs_scene* scene, u32 node_index)
        {
            // free allocated stuff
            if (is_valid(scene->physics_handles[node_index]))
                physics::release_entity(scene->physics_handles[node_index]);

            if (is_valid(scene->cbuffer[node_index]))
                pen::renderer_release_buffer(scene->cbuffer[node_index]);

            // zero
            zero_entity_components(scene, node_index);
        }

        void delete_entity_first_pass(ecs_scene* scene, u32 node_index)
        {
            // constraints must be freed or removed before we delete rigidbodies using them
            if (is_valid(scene->physics_handles[node_index]) && (scene->entities[node_index] & e_cmp::constraint))
                physics::release_entity(scene->physics_handles[node_index]);

            if (is_valid(scene->cbuffer[node_index]))
                pen::renderer_release_buffer(scene->cbuffer[node_index]);

            if (scene->entities[node_index] & e_cmp::pre_skinned)
            {
                if (scene->pre_skin[node_index].vertex_buffer)
                    pen::renderer_release_buffer(scene->pre_skin[node_index].vertex_buffer);

                if (scene->pre_skin[node_index].position_buffer)
                    pen::renderer_release_buffer(scene->pre_skin[node_index].position_buffer);
            }

            if (scene->master_instances[node_index].instance_buffer)
                pen::renderer_release_buffer(scene->master_instances[node_index].instance_buffer);
        }

        void delete_entity_second_pass(ecs_scene* scene, u32 node_index)
        {
            // all constraints must be removed by this point.
            if (scene->physics_handles[node_index] && (scene->entities[node_index] & e_cmp::physics))
                physics::release_entity(scene->physics_handles[node_index]);

            zero_entity_components(scene, node_index);
        }

        void clear_scene(ecs_scene* scene)
        {
            free_scene_buffers(scene);
            resize_scene_buffers(scene);
        }

        // a component wise memcpy of all components and extension components
        void entity_cpy(ecs_scene* scene, u32 dst, u32 src)
        {
            // will copy extensions and base
            for (u32 i = 0; i < scene->num_components; ++i)
            {
                generic_cmp_array& cmp = scene->get_component_array(i);
                memcpy(cmp[dst], cmp[src], cmp.size);
            }
        }

        void swap_entities(ecs_scene* scene, u32 a, s32 b)
        {
            u32 temp = get_new_entity(scene);
            entity_cpy(scene, temp, a);
            entity_cpy(scene, a, b);
            entity_cpy(scene, b, temp);

            // swap parents
            for (u32 i = 0; i < scene->num_entities; ++i)
            {
                if (scene->parents[i] == a)
                    scene->parents[i] = b;
                else if (scene->parents[i] == b)
                    scene->parents[i] = a;
            }

            zero_entity_components(scene, temp);
        }

        u32 clone_entity(ecs_scene* scene, u32 src, s32 dst, s32 parent, clone_mode mode, vec3f offset, const c8* suffix)
        {
            if (dst == -1)
            {
                dst = get_new_entity(scene);
            }
            else
            {
                if (dst >= scene->num_entities)
                    scene->num_entities = dst + 1;
            }

            ecs_scene* p_sn = scene;

            // copy components
            for (u32 i = 0; i < scene->num_components; ++i)
            {
                generic_cmp_array& cmp = p_sn->get_component_array(i);
                memcpy(cmp[dst], cmp[src], cmp.size);
            }

            // assign
            Str blank;
            memcpy(&p_sn->names[dst], &blank, sizeof(Str));
            memcpy(&p_sn->material_names[dst], &blank, sizeof(Str));
            memcpy(&p_sn->material_names[dst], &blank, sizeof(Str));

            p_sn->names[dst] = p_sn->names[src].c_str();
            p_sn->names[dst].append(suffix);

            p_sn->geometry_names[dst] = p_sn->geometry_names[src].c_str();
            p_sn->material_names[dst] = p_sn->material_names[src].c_str();

            // fixup
            u32 parent_offset = p_sn->parents[src] - src;
            if (parent == -1)
            {
                p_sn->parents[dst] = dst - parent_offset;
            }
            else
            {
                p_sn->parents[dst] = parent;
            }

            vec3f translation = p_sn->local_matrices[dst].get_translation();
            p_sn->local_matrices[dst].set_translation(translation + offset);

            if (mode == e_clone_mode::instantiate)
            {
                // todo, clone / instantiate constraint

                if (p_sn->physics_handles[src])
                    instantiate_rigid_body(scene, dst);

                if (p_sn->entities[dst] & e_cmp::geometry)
                    instantiate_model_cbuffer(scene, dst);

                if (p_sn->entities[dst] & e_cmp::material)
                {
                    p_sn->materials[dst].material_cbuffer = PEN_INVALID_HANDLE;
                    instantiate_material_cbuffer(scene, dst, p_sn->materials[dst].material_cbuffer_size);
                }
            }
            else if (mode == e_clone_mode::move)
            {
                zero_entity_components(scene, src);
            }

            return dst;
        }

        ecs_scene* create_scene(const c8* name)
        {
            ecs_scene_instance new_instance;
            new_instance.name = name;
            new_instance.scene = new ecs_scene();

            s_scenes.push_back(new_instance);

            resize_scene_buffers(new_instance.scene, 8192);

            // create buffers
            pen::buffer_creation_params bcp;

            // forward lights
            bcp.usage_flags = PEN_USAGE_DYNAMIC;
            bcp.bind_flags = PEN_BIND_CONSTANT_BUFFER;
            bcp.cpu_access_flags = PEN_CPU_ACCESS_WRITE;
            bcp.buffer_size = sizeof(forward_light_buffer);
            bcp.data = nullptr;

            new_instance.scene->forward_light_buffer = pen::renderer_create_buffer(bcp);

            // sdf shadows
            bcp.usage_flags = PEN_USAGE_DYNAMIC;
            bcp.bind_flags = PEN_BIND_CONSTANT_BUFFER;
            bcp.cpu_access_flags = PEN_CPU_ACCESS_WRITE;
            bcp.buffer_size = sizeof(distance_field_shadow_buffer);
            bcp.data = nullptr;

            new_instance.scene->sdf_shadow_buffer = pen::renderer_create_buffer(bcp);

            // shadow maps
            bcp.usage_flags = PEN_USAGE_DYNAMIC;
            bcp.bind_flags = PEN_BIND_CONSTANT_BUFFER;
            bcp.cpu_access_flags = PEN_CPU_ACCESS_WRITE;
            bcp.buffer_size = sizeof(mat4) * e_scene_limits::max_shadow_maps;
            bcp.data = nullptr;

            new_instance.scene->shadow_map_buffer = pen::renderer_create_buffer(bcp);

            // area lights
            bcp.usage_flags = PEN_USAGE_DYNAMIC;
            bcp.bind_flags = PEN_BIND_CONSTANT_BUFFER;
            bcp.cpu_access_flags = PEN_CPU_ACCESS_WRITE;
            bcp.buffer_size = sizeof(area_light_buffer);
            bcp.data = nullptr;

            new_instance.scene->area_light_buffer = pen::renderer_create_buffer(bcp);

            return new_instance.scene;
        }

        void destroy_scene(ecs_scene* scene)
        {
            free_scene_buffers(scene);

            // todo release resource refs
            // geom
            // anim
        }

        void render_area_light_textures(const scene_view& view)
        {
            ecs_scene* scene = view.scene;

            u32 count = 0;
            u32 area_light = -1;
            for (u32 i = 0; i < scene->num_entities; ++i)
            {
                if (!(scene->entities[i] & e_cmp::light))
                    continue;

                if (!(scene->lights[i].type == e_light_type::area_ex))
                    continue;

                cmp_area_light& al = scene->area_light[i];
                if (!is_valid(al.shader))
                    continue;

                if (count == view.array_index)
                {
                    area_light = i;
                    break;
                }

                ++count;
            }

            if (!is_valid(area_light))
                return;

            cmp_area_light& al = scene->area_light[area_light];

            pen::renderer_set_constant_buffer(scene->cbuffer[area_light], 1, pen::CBUFFER_BIND_PS);

            if (is_valid(al.shader))
            {
                if (is_valid(al.texture_handle))
                    pen::renderer_set_texture(al.texture_handle, al.sampler_state, 0, pen::TEXTURE_BIND_PS);

                scene_view sub = view;
                sub.pmfx_shader = al.shader;
                sub.technique = al.technique;

                pmfx::fullscreen_quad(sub);
            }
        }

        void render_shadow_views(const scene_view& view)
        {
            ecs_scene* scene = view.scene;

            static u32 cb_view = PEN_INVALID_HANDLE;
            if (!is_valid(cb_view))
            {
                pen::buffer_creation_params bcp;
                bcp.usage_flags = PEN_USAGE_DYNAMIC;
                bcp.bind_flags = PEN_BIND_CONSTANT_BUFFER;
                bcp.cpu_access_flags = PEN_CPU_ACCESS_WRITE;
                bcp.buffer_size = sizeof(camera_cbuffer);
                bcp.data = nullptr;

                cb_view = pen::renderer_create_buffer(bcp);
            }

            static mat4 shadow_matrices[e_scene_limits::max_shadow_maps];
            u32         shadow_index = 0;
            for (u32 n = 0; n < scene->num_entities; ++n)
            {
                if (!(scene->entities[n] & e_cmp::light))
                    continue;

                if (!scene->lights[n].shadow_map || scene->lights[n].type == e_light_type::point)
                    continue;

                if (shadow_index++ != view.array_index)
                    continue;

                // create a shadow camera
                camera cam;
                if(scene->lights[n].type == e_light_type::dir)
                {
                    vec3f  light_dir = normalised(-scene->lights[n].direction);
                    camera_update_shadow_frustum(&cam, light_dir, scene->renderable_extents.min - vec3f(0.1f),
                                                scene->renderable_extents.max + vec3f(0.1f));
                }
                else
                {
                    // spot
                    camera_create_perspective(&cam, 100.0f, 1.0f, 0.01f, 500.0f);
                    
                    cam.view.set_row(0, vec4f(normalised(scene->world_matrices[n].get_column(2).xyz), 0.0f));
                    cam.view.set_row(1, vec4f(normalised(scene->world_matrices[n].get_column(0).xyz), 0.0f));
                    cam.view.set_row(2, vec4f(normalised(scene->world_matrices[n].get_column(1).xyz), 0.0f));
                    cam.view.set_row(3, vec4f(0.0f, 0.0f, 0.0f, 1.0f));

                    mat4 translate = mat::create_translation(-scene->world_matrices[n].get_translation());

                    cam.view = cam.view * translate;
                }

                // update view and camera
                scene_view vv = view;
                vv.camera = &cam;
                mat4 shadow_vp = cam.proj * cam.view;
                pen::renderer_update_buffer(cb_view, &shadow_vp, sizeof(mat4));
                shadow_matrices[shadow_index - 1] = shadow_vp;
                vv.cb_view = cb_view;
                
                render_scene_view(vv);
            }

            // update cbuffer
            if (is_valid(scene->shadow_map_buffer))
            {
                pen::renderer_update_buffer(scene->shadow_map_buffer, &shadow_matrices[0], sizeof(mat4) * e_scene_limits::max_shadow_maps);
            }
        }
        
        void single_light_from_entity(light_data& ld, const ecs_scene* scene, u32 n)
        {
            cmp_draw_call dc;
            dc.world_matrix = scene->world_matrices[n];
            vec3f pos = scene->world_matrices[n].get_translation();
            switch (scene->lights[n].type)
            {
                case e_light_type::dir:
                    ld.pos_radius = vec4f(scene->lights[n].direction * 10000.0f, 0.0f);
                    ld.dir_cutoff = vec4f(scene->lights[n].direction, 0.0f);
                    ld.colour = vec4f(scene->lights[n].colour, 0.0f);
                    break;
                case e_light_type::point:
                    ld.pos_radius = vec4f(pos, scene->lights[n].radius);
                    ld.dir_cutoff = vec4f(scene->lights[n].direction, 0.0f);
                    ld.colour = vec4f(scene->lights[n].colour, 0.0f);
                    break;
                case e_light_type::spot:
                    ld.pos_radius = vec4f(pos, scene->lights[n].radius);
                    ld.dir_cutoff = vec4f(-dc.world_matrix.get_column(1).xyz, scene->lights[n].cos_cutoff);
                    ld.colour = vec4f(scene->lights[n].colour, 0.0f);
                    ld.data = vec4f(scene->lights[n].spot_falloff, 0.0f, 0.0f, 0.0f);
                    break;
                default:
                    break;
            }
        }
        
        void render_omni_shadow_views(const scene_view& view)
        {
            ecs_scene* scene = view.scene;
                        
            static camera cam_omni_shadow;
            static u32 cb_light = -1;
            if(!is_valid(cb_light))
            {
                cam_omni_shadow.pos = vec3f(0.0f, 0.0f, 0.0f);
                
                pen::buffer_creation_params bcp;
                bcp.usage_flags = PEN_USAGE_DYNAMIC;
                bcp.bind_flags = PEN_BIND_CONSTANT_BUFFER;
                bcp.cpu_access_flags = PEN_CPU_ACCESS_WRITE;
                bcp.buffer_size = sizeof(light_data);
                bcp.data = nullptr;

                cb_light = pen::renderer_create_buffer(bcp);
            }
            
            u32 target_omni_light_index = view.array_index / 6;
            u32 array_face = view.array_index % 6;
            u32 omni_light_index = 0;
            for (u32 n = 0; n < scene->num_entities; ++n)
            {
                if (!(scene->entities[n] & e_cmp::light))
                    continue;

                if (!scene->lights[n].shadow_map || scene->lights[n].type != e_light_type::point)
                    continue;
                    
                if(omni_light_index++ != target_omni_light_index)
                    continue;
                    
                cam_omni_shadow.pos = scene->transforms[n].translation;
                put::camera_create_cubemap(&cam_omni_shadow, 0.1f, scene->lights[n].radius * 2.0);
                put::camera_set_cubemap_face(&cam_omni_shadow, array_face);
                put::camera_update_shader_constants(&cam_omni_shadow);
                
                light_data ld;
                single_light_from_entity(ld, scene, n);
                pen::renderer_update_buffer(cb_light, &ld, sizeof(light_data));
                pen::renderer_set_constant_buffer(cb_light, 10, pen::CBUFFER_BIND_PS);
                
                scene_view vv = view;
                vv.camera = &cam_omni_shadow;
                vv.cb_view = cam_omni_shadow.cbuffer;
                
                render_scene_view(vv);
            }
        }
        
        void render_light_volumes(const scene_view& view)
        {
            ecs_scene* scene = view.scene;

            if (scene->view_flags & e_scene_view_flags::hide)
                return;

            pen::renderer_set_constant_buffer(view.cb_view, 0, pen::CBUFFER_BIND_PS | pen::CBUFFER_BIND_VS);

            static hash_id id_volume[] = {PEN_HASH("full_screen_quad"), PEN_HASH("sphere"), PEN_HASH("cone")};

            static hash_id id_technique[] = {PEN_HASH("directional_light"), PEN_HASH("point_light"), PEN_HASH("spot_light")};

            static u32 shader = pmfx::load_shader("deferred_render");

            geometry_resource* volume[PEN_ARRAY_SIZE(id_volume)];
            for (u32 i = 0; i < PEN_ARRAY_SIZE(id_volume); ++i)
                volume[i] = get_geometry_resource(id_volume[i]);

            static hash_id id_cull_front = PEN_HASH("front_face_cull");
            u32            cull_front = pmfx::get_render_state(id_cull_front, pmfx::e_render_state::sampler);

            static hash_id id_disable_depth = PEN_HASH("disabled");
            u32            depth_disabled = pmfx::get_render_state(id_disable_depth, pmfx::e_render_state::depth_stencil);

            for (u32 n = 0; n < scene->num_entities; ++n)
            {
                if (!(scene->entities[n] & e_cmp::light))
                    continue;

                u32                t = scene->lights[n].type;
                geometry_resource* vol = volume[t];

                pmfx::set_technique_perm(shader, id_technique[t], view.permutation);

                cmp_draw_call dc;
                dc.world_matrix = scene->world_matrices[n];

                vec3f pos = dc.world_matrix.get_translation();

                bool inside_volume = false;

                light_data ld = {};

                switch (t)
                {
                    case e_light_type::dir:
                        ld.pos_radius = vec4f(scene->lights[n].direction * 10000.0f, 0.0f);
                        ld.dir_cutoff = vec4f(scene->lights[n].direction, 0.0f);
                        ld.colour = vec4f(scene->lights[n].colour, 0.0f);
                        break;
                    case e_light_type::point:
                        ld.pos_radius = vec4f(pos, scene->lights[n].radius);
                        ld.dir_cutoff = vec4f(scene->lights[n].direction, 0.0f);
                        ld.colour = vec4f(scene->lights[n].colour, 0.0f);

                        if (maths::point_inside_sphere(pos, scene->lights[n].radius, view.camera->pos))
                            inside_volume = true;

                        break;
                    case e_light_type::spot:
                        ld.pos_radius = vec4f(pos, scene->lights[n].radius);
                        ld.dir_cutoff = vec4f(-dc.world_matrix.get_column(1).xyz, scene->lights[n].cos_cutoff);
                        ld.colour = vec4f(scene->lights[n].colour, 0.0f);
                        ld.data = vec4f(scene->lights[n].spot_falloff, 0.0f, 0.0f, 0.0f);

                        if (maths::point_inside_cone(view.camera->pos, pos, ld.dir_cutoff.xyz, scene->transforms[n].scale.y,
                                                     scene->transforms[n].scale.x))
                        {
                            inside_volume = true;
                        }

                        break;
                    default:
                        continue;
                }

                // pack light data into world_matrix_inv_transpose
                memcpy(&dc.world_matrix_inv_transpose, &ld, sizeof(mat4));

                // flip cull mode if we are inside the light volume
                if (inside_volume)
                {
                    pen::renderer_set_rasterizer_state(cull_front);
                    pen::renderer_set_depth_stencil_state(depth_disabled);
                }

                pen::renderer_update_buffer(scene->cbuffer[n], &dc, sizeof(cmp_draw_call));
                pen::renderer_set_constant_buffer(scene->cbuffer[n], 1, pen::CBUFFER_BIND_PS | pen::CBUFFER_BIND_VS);
                pen::renderer_set_vertex_buffer(vol->vertex_buffer, 0, vol->vertex_size, 0);
                pen::renderer_set_index_buffer(vol->index_buffer, vol->index_type, 0);
                pen::renderer_draw_indexed(vol->num_indices, 0, 0, PEN_PT_TRIANGLELIST);

                if (inside_volume)
                {
                    pen::renderer_set_rasterizer_state(view.raster_state);
                    pen::renderer_set_depth_stencil_state(view.depth_stencil_state);
                }
            }
        }

        void render_scene_view(const scene_view& view)
        {
            ecs_scene* scene = view.scene;

            if (scene->view_flags & e_scene_view_flags::hide)
                return;

            s32 draw_count = 0;
            s32 cull_count = 0;

            for (u32 n = 0; n < scene->num_entities; ++n)
            {
                if (!(scene->entities[n] & e_cmp::geometry && scene->entities[n] & e_cmp::material))
                    continue;

                if (scene->entities[n] & e_cmp::sub_instance)
                    continue;

                if (scene->state_flags[n] & e_state::hidden)
                    continue;

                // frustum cull
                bool inside = true;
                for (s32 i = 0; i < 6; ++i)
                {
                    frustum& camera_frustum = view.camera->camera_frustum;

                    vec3f& min = scene->bounding_volumes[n].transformed_min_extents;
                    vec3f& max = scene->bounding_volumes[n].transformed_max_extents;

                    vec3f pos = min + (max - min) * 0.5f;
                    f32   radius = scene->bounding_volumes[n].radius;

                    f32 d = maths::point_plane_distance(pos, camera_frustum.p[i], camera_frustum.n[i]);

                    if (d > radius)
                    {
                        inside = false;
                        break;
                    }
                }
                
                if (!inside)
                {
                    cull_count++;
                    continue;
                }

                draw_count++;

                cmp_geometry* p_geom = &scene->geometries[n];
                cmp_material* p_mat = &scene->materials[n];
                u32           permutation = scene->material_permutation[n];

                // set shader / technique
                if (!is_valid(view.pmfx_shader))
                {
                    // material shader / technique
                    pmfx::set_technique(p_mat->shader, p_mat->technique_index);
                }
                else
                {
                    bool set = pmfx::set_technique_perm(view.pmfx_shader, view.technique, permutation);
                    if (!set)
                    {
                        if (scene->entities[n] & e_cmp::master_instance)
                        {
                            u32 num_instances = scene->master_instances[n].num_instances;
                            n += num_instances;
                        }
                        PEN_ASSERT(0);
                        continue;
                    }
                }

                // update skin
                if (scene->entities[n] & e_cmp::skinned && !(scene->entities[n] & e_cmp::sub_geometry))
                {
                    if (p_geom->p_skin->bone_cbuffer == PEN_INVALID_HANDLE)
                    {
                        pen::buffer_creation_params bcp;
                        bcp.usage_flags = PEN_USAGE_DYNAMIC;
                        bcp.bind_flags = PEN_BIND_CONSTANT_BUFFER;
                        bcp.cpu_access_flags = PEN_CPU_ACCESS_WRITE;
                        bcp.buffer_size = sizeof(mat4) * 85;
                        bcp.data = nullptr;

                        p_geom->p_skin->bone_cbuffer = pen::renderer_create_buffer(bcp);
                    }

                    static mat4 bb[85];
                    s32         joints_offset = scene->anim_controller_v2[n].joints_offset;
                    for (s32 i = 0; i < p_geom->p_skin->num_joints; ++i)
                        bb[i] = scene->world_matrices[joints_offset + i] * p_geom->p_skin->joint_bind_matrices[i];

                    pen::renderer_update_buffer(p_geom->p_skin->bone_cbuffer, bb, sizeof(bb));
                    pen::renderer_set_constant_buffer(p_geom->p_skin->bone_cbuffer, 2, pen::CBUFFER_BIND_VS);
                }

                // set material cbs
                u32 mcb = scene->materials[n].material_cbuffer;
                if (is_valid(mcb))
                {
                    pen::renderer_set_constant_buffer(mcb, 7, pen::CBUFFER_BIND_PS | pen::CBUFFER_BIND_VS);
                }

                pen::renderer_set_constant_buffer(scene->cbuffer[n], 1, pen::CBUFFER_BIND_PS | pen::CBUFFER_BIND_VS);

                // set ib / vb
                if (scene->entities[n] & e_cmp::master_instance)
                {
                    u32 vbs[2] = {p_geom->vertex_buffer, scene->master_instances[n].instance_buffer};
                    u32 strides[2] = {p_geom->vertex_size, scene->master_instances[n].instance_stride};
                    u32 offsets[2] = {0};

                    pen::renderer_set_vertex_buffers(vbs, 2, 0, strides, offsets);
                }
                else
                {
                    pen::renderer_set_vertex_buffer(p_geom->vertex_buffer, 0, p_geom->vertex_size, 0);
                }

                pen::renderer_set_index_buffer(p_geom->index_buffer, p_geom->index_type, 0);

                // set textures
                if (p_mat)
                {
                    cmp_samplers& samplers = scene->samplers[n];
                    for (u32 s = 0; s < e_pmfx_constants::max_technique_sampler_bindings; ++s)
                    {
                        if (!samplers.sb[s].handle)
                            continue;

                        pen::renderer_set_texture(samplers.sb[s].handle, samplers.sb[s].sampler_state,
                                                  samplers.sb[s].sampler_unit, pen::TEXTURE_BIND_PS);
                    }
                }

                // view
                pen::renderer_set_constant_buffer(view.cb_view, 0, pen::CBUFFER_BIND_PS | pen::CBUFFER_BIND_VS);

                // fwd lights
                if (view.render_flags & pmfx::e_scene_render_flags::forward_lit)
                {
                    pen::renderer_set_constant_buffer(scene->forward_light_buffer, 3, pen::CBUFFER_BIND_PS);
                    pen::renderer_set_constant_buffer(scene->shadow_map_buffer, 4, pen::CBUFFER_BIND_PS);
                    pen::renderer_set_constant_buffer(scene->area_light_buffer, 6, pen::CBUFFER_BIND_PS);

                    // ltc lookups
                    static u32 ltc_mat = put::load_texture("data/textures/ltc/ltc_mat.dds");
                    static u32 ltc_mag = put::load_texture("data/textures/ltc/ltc_amp.dds");

					static hash_id id_clamp_linear = PEN_HASH("clamp_linear");
                    u32 clamp_linear = pmfx::get_render_state(id_clamp_linear, pmfx::e_render_state::sampler);

                    pen::renderer_set_texture(ltc_mat, clamp_linear, 13, pen::TEXTURE_BIND_PS);
                    pen::renderer_set_texture(ltc_mag, clamp_linear, 12, pen::TEXTURE_BIND_PS);
                }
                
                // sdf shadows
                pen::renderer_set_constant_buffer(scene->sdf_shadow_buffer, 5, pen::CBUFFER_BIND_PS);
                for (u32 n = 0; n < scene->num_entities; ++n)
                {
                    if (!(scene->entities[n] & e_cmp::sdf_shadow))
                        continue;
                    
                    cmp_shadow& shadow = scene->shadows[n];

                    if (is_valid(shadow.texture_handle))
                        pen::renderer_set_texture(shadow.texture_handle, shadow.sampler_state, e_global_textures::sdf_shadow,
                                                  pen::TEXTURE_BIND_PS);
                                                  
                    // info for sdf
                    pen::renderer_set_constant_buffer(scene->sdf_shadow_buffer, 5, pen::CBUFFER_BIND_PS);
                }

                // draw

                // instances
                if (scene->entities[n] & e_cmp::master_instance)
                {
                    u32 num_instances = scene->master_instances[n].num_instances;
                    pen::renderer_draw_indexed_instanced(num_instances, 0, scene->geometries[n].num_indices, 0, 0,
                                                         PEN_PT_TRIANGLELIST);
                    n += num_instances;
                    continue;
                }

                // single
                pen::renderer_draw_indexed(scene->geometries[n].num_indices, 0, 0, PEN_PT_TRIANGLELIST);
            }
        }

        void update_animations(ecs_scene* scene, f32 dt)
        {
            //dt = 16.66;
            //pen::timer* timer = pen::timer_create("anim_v2");
            //pen::timer_start(timer);

            for (u32 n = 0; n < scene->num_entities; ++n)
            {
                if (!(scene->entities[n] & e_cmp::anim_controller))
                    continue;

                cmp_anim_controller_v2 controller = scene->anim_controller_v2[n];

                u32 num_anims = sb_count(controller.anim_instances);
                for (u32 ai = 0; ai < num_anims; ++ai)
                {
                    anim_instance& instance = controller.anim_instances[ai];

                    if (instance.flags & e_anim_flags::paused)
                        continue;

                    soa_anim& soa = instance.soa;
                    u32       num_channels = soa.num_channels;
                    f32       anim_t = instance.time;

                    bool looped = false;

                    // roll on time
                    instance.time += dt;
                    if (instance.time >= instance.length)
                    {
                        instance.time = 0.0f;
                        looped = true;
                    }

                    if (instance.flags & e_anim_flags::looped)
                    {
                        instance.flags &= ~e_anim_flags::looped;
                        looped = true;
                    }

                    u32 num_joints = sb_count(instance.joints);

                    // reset rotations
                    for (u32 j = 0; j < num_joints; ++j)
                        instance.targets[j].q = quat(0.0f, 0.0f, 0.0f);

                    for (s32 c = 0; c < num_channels; ++c)
                    {
                        anim_sampler& sampler = instance.samplers[c];
                        anim_channel& channel = soa.channels[c];

                        if (sampler.joint == PEN_INVALID_HANDLE)
                            continue;

                        // find the frame we are on..
                        for (; sampler.pos < channel.num_frames; sampler.pos++)
                            if (anim_t <= soa.info[sampler.pos][c].time)
                            {
                                sampler.pos -= 1;
                                break;
                            }

                        //reset flag
                        sampler.flags &= ~e_anim_flags::looped;

                        if (sampler.pos >= channel.num_frames || looped)
                        {
                            sampler.pos = 0;
                            sampler.flags = e_anim_flags::looped;
                        }

                        u32 next = (sampler.pos + 1) % channel.num_frames;

                        // get anim data
                        anim_info& info1 = soa.info[sampler.pos][c];
                        anim_info& info2 = soa.info[next][c];

                        f32* d1 = &soa.data[sampler.pos][info1.offset];
                        f32* d2 = &soa.data[next][info2.offset];

                        f32 a = (anim_t - info1.time);
                        f32 b = (info2.time - info1.time);

                        f32 it = min(max(a / b, 0.0f), 1.0f);

                        sampler.prev_t = sampler.cur_t;
                        sampler.cur_t = it;

                        for (u32 e = 0; e < channel.element_count; ++e)
                        {
                            u32 eo = channel.element_offset[e];

                            // slerp quats
                            if (eo == e_anim_output::quaternion)
                            {
                                quat q1;
                                quat q2;

                                memcpy(&q1.v[0], &d1[e], 16);
                                memcpy(&q2.v[0], &d2[e], 16);

                                quat ql = slerp(q1, q2, it);

                                instance.targets[sampler.joint].q = ql * instance.targets[sampler.joint].q;
                                instance.targets[sampler.joint].flags |= channel.flags;
                                e += 3;
                            }
                            else
                            {
                                // lerp translation / scale
                                f32 lf = (1 - it) * d1[e] + it * d2[e];
                                instance.targets[sampler.joint].t[eo] = lf;
                            }
                        }
                    }

                    // bake anim target into a cmp transform for joint
                    u32 tj = PEN_INVALID_HANDLE;
                    for (u32 j = 0; j < num_joints; ++j)
                    {
                        u32 jnode = controller.joint_indices[j];

                        if (scene->entities[jnode] & e_cmp::anim_trajectory)
                        {
                            tj = j;
                            continue;
                        }

                        f32* f = &instance.targets[j].t[0];

                        instance.joints[j].translation = vec3f(f[e_anim_output::translate_x],
                            f[e_anim_output::translate_y], f[e_anim_output::translate_z]);
                            
                        instance.joints[j].scale = vec3f(f[e_anim_output::scale_x],
                            f[e_anim_output::scale_y], f[e_anim_output::scale_z]);

                        if (instance.targets[j].flags & e_anim_flags::baked_quaternion)
                            instance.joints[j].rotation = instance.targets[j].q;
                        else
                            instance.joints[j].rotation = scene->initial_transform[jnode].rotation * instance.targets[j].q;
                    }

                    // root motion.. todo rotation
                    if (tj != PEN_INVALID_HANDLE)
                    {
                        f32*  f = &instance.targets[tj].t[0];
                        vec3f tt = vec3f(f[0], f[1], f[2]);

                        if (instance.samplers[0].flags & e_anim_flags::looped)
                        {
                            // inherit prev root motion
                            instance.root_translation = tt;
                        }
                        else
                        {
                            instance.root_delta = tt - instance.root_translation;
                            instance.root_translation = tt;
                        }
                    }
                }

                // for active controller.anim_instances, make trans, quat, scale
                //      blend tree
                if (num_anims > 0)
                {
                    anim_instance& a = controller.anim_instances[controller.blend.anim_a];
                    anim_instance& b = controller.anim_instances[controller.blend.anim_b];
                    f32            t = controller.blend.ratio;

                    u32 num_joints = sb_count(a.joints);
                    for (u32 j = 0; j < num_joints; ++j)
                    {
                        u32 jnode = controller.joint_indices[j];

                        cmp_transform& tc = scene->transforms[jnode];
                        cmp_transform& ta = a.joints[j];
                        cmp_transform& tb = b.joints[j];

                        if (scene->entities[jnode] & e_cmp::anim_trajectory)
                        {
                            vec3f lerp_delta = lerp(a.root_delta, b.root_delta, t);

                            mat4 rot_mat;
                            quat q = scene->initial_transform[jnode].rotation;
                            q.get_matrix(rot_mat);

                            vec3f transform_translation = rot_mat.transform_vector(lerp_delta);

                            // apply root motion to the root controller, so we bring along the meshes
                            scene->transforms[n].rotation = q;
                            scene->transforms[n].translation += transform_translation;
                            scene->entities[n] |= e_cmp::transform;

                            continue;
                        }

                        tc.translation = lerp(ta.translation, tb.translation, t);
                        tc.rotation = slerp2(ta.rotation, tb.rotation, t);
                        tc.scale = lerp(ta.scale, tb.scale, t);

                        scene->entities[jnode] |= e_cmp::transform;
                    }
                }
            }

            //f32 ms = pen::timer_elapsed_ms(timer);
            //PEN_LOG("anim_v2 : %f", ms);
        }

        void update()
        {
            static pen::timer*  dt_timer = pen::timer_create();
            f32                 dt = pen::timer_elapsed_ms(dt_timer) * 0.001f;
            pen::timer_start(dt_timer);

            static f32 fft = 1.0f / 60.0f;
            bool       bdt = dev_ui::get_program_preference("dynamic_timestep").as_bool(true);
            f32        ft = dev_ui::get_program_preference("fixed_timestep").as_f32(fft);

            if (!bdt)
            {
                dt = ft;
            }

            for (auto& si : s_scenes)
            {
                update_scene(si.scene, dt);
            }
        }

        std::vector<ecs_scene_instance>* get_scenes()
        {
            return &s_scenes;
        }

        void update_scene(ecs_scene* scene, f32 dt)
        {
            // static anim time to pass into draw calls etc..
            f32 anim_time = pen::get_time_ms() / 1000.0;

            u32 num_controllers = sb_count(scene->controllers);
            u32 num_extensions = sb_count(scene->extensions);

            // pre update controllers
            for (u32 c = 0; c < num_controllers; ++c)
                if (scene->controllers[c].update_func)
                    scene->controllers[c].update_func(scene->controllers[c], scene, dt);

            if (scene->flags & e_scene_flags::pause_update)
            {
                physics::set_paused(1);
            }
            else
            {
                physics::set_paused(0);
                update_animations(scene, dt);
            }

            // extension component update
            for (u32 e = 0; e < num_extensions; ++e)
                if (scene->extensions[e].update_func)
                    scene->extensions[e].update_func(scene->extensions[e], scene, dt);

            static pen::timer* timer = pen::timer_create();
            pen::timer_start(timer);

            // scene node transform
            for (size_t n = 0; n < scene->num_entities; ++n)
            {
                // force physics entity to sync and ignore controlled transform
                if (scene->state_flags[n] & e_state::sync_physics_transform)
                {
                    scene->state_flags[n] &= ~e_state::sync_physics_transform;
                    scene->entities[n] &= ~e_cmp::transform;
                }

                // controlled transform
                if (scene->entities[n] & e_cmp::transform)
                {
                    cmp_transform& t = scene->transforms[n];

                    // generate matrix from transform
                    mat4 rot_mat;
                    t.rotation.get_matrix(rot_mat);

                    mat4 translation_mat = mat::create_translation(t.translation);

                    mat4 scale_mat = mat::create_scale(t.scale);

                    scene->local_matrices[n] = translation_mat * rot_mat * scale_mat;

                    if (scene->entities[n] & e_cmp::physics)
                    {
                        if (scene->physics_data[n].type == e_physics_type::rigid_body)
                        {
                            cmp_transform& pt = scene->physics_offset[n];
                            physics::set_transform(scene->physics_handles[n], t.translation + pt.translation, t.rotation);
                            physics::set_v3(scene->physics_handles[n], vec3f::zero(), physics::e_cmd::set_angular_velocity);
                            physics::set_v3(scene->physics_handles[n], vec3f::zero(), physics::e_cmd::set_linear_velocity);
                        }
                    }

                    // local matrix will be baked
                    scene->entities[n] &= ~e_cmp::transform;
                }
                else if (scene->entities[n] & e_cmp::physics)
                {
                    if (!physics::has_rb_matrix(n))
                        continue;

                    cmp_transform& t = scene->transforms[n];
                    cmp_transform& pt = scene->physics_offset[n];

                    mat4 scale_mat = mat::create_scale(t.scale);

                    vec3f os = t.scale;
                    t = physics::get_rb_transform(scene->physics_handles[n]);
                    t.scale = os;

                    mat4 rot_mat;
                    t.rotation.get_matrix(rot_mat);

                    mat4 translation_mat = mat::create_translation(t.translation - pt.translation);

                    scene->local_matrices[n] = translation_mat * rot_mat * scale_mat;
                }

                // heirarchical scene transform
                u32 parent = scene->parents[n];
                if (parent == n)
                    scene->world_matrices[n] = scene->local_matrices[n];
                else
                    scene->world_matrices[n] = scene->world_matrices[parent] * scene->local_matrices[n];
            }

            // bounding volume transform
            static vec3f corners[] = {vec3f(0.0f, 0.0f, 0.0f),

                                      vec3f(1.0f, 0.0f, 0.0f), vec3f(0.0f, 1.0f, 0.0f), vec3f(0.0f, 0.0f, 1.0f),

                                      vec3f(1.0f, 1.0f, 0.0f), vec3f(0.0f, 1.0f, 1.0f), vec3f(1.0f, 0.0f, 1.0f),

                                      vec3f(1.0f, 1.0f, 1.0f)};

            scene->renderable_extents.min = vec3f::flt_max();
            scene->renderable_extents.max = -vec3f::flt_max();

            // transform extents by transform
            for (size_t n = 0; n < scene->num_entities; ++n)
            {
                vec3f min = scene->bounding_volumes[n].min_extents;
                vec3f max = scene->bounding_volumes[n].max_extents - min;

                vec3f& tmin = scene->bounding_volumes[n].transformed_min_extents;
                vec3f& tmax = scene->bounding_volumes[n].transformed_max_extents;

                if (scene->entities[n] & e_cmp::bone)
                {
                    tmin = tmax = scene->world_matrices[n].get_translation();
                    continue;
                }

                tmax = -vec3f::flt_max();
                tmin = vec3f::flt_max();

                for (s32 c = 0; c < 8; ++c)
                {
                    vec3f p = scene->world_matrices[n].transform_vector(min + max * corners[c]);

                    tmax = vec3f::vmax(tmax, p);
                    tmin = vec3f::vmin(tmin, p);
                }

                f32& trad = scene->bounding_volumes[n].radius;
                trad = mag(tmax - tmin) * 0.5f;

                if (!(scene->entities[n] & e_cmp::geometry))
                    continue;

                // also set scene extents
                scene->renderable_extents.min = vec3f::vmin(tmin, scene->renderable_extents.min);
                scene->renderable_extents.max = vec3f::vmax(tmax, scene->renderable_extents.max);
            }

            // reverse iterate over scene and expand parents extents by children
            for (size_t n = scene->num_entities - 1; n > 0; --n)
            {
                if (!(scene->entities[n] & e_cmp::allocated))
                    continue;

                u32 p = scene->parents[n];
                if (p == n)
                    continue;

                vec3f& parent_tmin = scene->bounding_volumes[p].transformed_min_extents;
                vec3f& parent_tmax = scene->bounding_volumes[p].transformed_max_extents;

                vec3f& tmin = scene->bounding_volumes[n].transformed_min_extents;
                vec3f& tmax = scene->bounding_volumes[n].transformed_max_extents;

                if (scene->entities[p] & e_cmp::anim_controller)
                {
                    vec3f pad = vec3f(0.0f);

                    parent_tmin = vec3f::vmin(parent_tmin, tmin - pad);
                    parent_tmax = vec3f::vmax(parent_tmax, tmax + pad);
                }
                else
                {
                    parent_tmin = vec3f::vmin(parent_tmin, tmin);
                    parent_tmax = vec3f::vmax(parent_tmax, tmax);
                }
            }

            // Forward light buffer
            static forward_light_buffer light_buffer;
            s32                         pos = 0;
            s32                         num_lights = 0;

            memset(&light_buffer, 0x0, sizeof(forward_light_buffer));

            // directional lights
            s32 num_directions_lights = 0;
            for (size_t n = 0; n < scene->num_entities; ++n)
            {
                if (!(scene->entities[n] & e_cmp::light))
                    continue;

                cmp_light& l = scene->lights[n];
                if (l.type != e_light_type::dir)
                    continue;

                // update bv and transform
                scene->bounding_volumes[n].min_extents = -vec3f(FLT_MAX);
                scene->bounding_volumes[n].max_extents = vec3f(FLT_MAX);

                if (num_lights >= e_scene_limits::max_forward_lights)
                    break;

                // current directional light is a point light very far away
                // with no attenuation..
                vec3f light_pos = l.direction * k_dir_light_offset;
                light_buffer.lights[pos].pos_radius = vec4f(light_pos, 0.0);
                light_buffer.lights[pos].colour = vec4f(l.colour, l.shadow_map ? 1.0 : 0.0);

                ++num_directions_lights;
                ++num_lights;
                ++pos;
            }

            // point lights
            s32 num_point_lights = 0;
            for (size_t n = 0; n < scene->num_entities; ++n)
            {
                if (!(scene->entities[n] & e_cmp::light))
                    continue;

                cmp_light& l = scene->lights[n];
                if (l.type != e_light_type::point)
                    continue;

                // update bv and transform
                scene->bounding_volumes[n].min_extents = -vec3f::one();
                scene->bounding_volumes[n].max_extents = vec3f::one();

                f32 rad = std::max<f32>(l.radius, 1.0f) * 2.0f;
                scene->transforms[n].scale = vec3f(rad, rad, rad);
                scene->entities[n] |= e_cmp::transform;

                if (num_lights >= e_scene_limits::max_forward_lights)
                    break;

                cmp_transform& t = scene->transforms[n];

                light_buffer.lights[pos].pos_radius = vec4f(t.translation, l.radius);
                light_buffer.lights[pos].colour = vec4f(l.colour, l.shadow_map ? 1.0 : 0.0);

                ++num_point_lights;
                ++num_lights;
                ++pos;
            }

            // spot lights
            s32 num_spot_lights = 0;
            for (size_t n = 0; n < scene->num_entities; ++n)
            {
                if (num_lights >= e_scene_limits::max_forward_lights)
                    break;

                if (!(scene->entities[n] & e_cmp::light))
                    continue;

                cmp_light& l = scene->lights[n];

                if (l.type != e_light_type::spot)
                    continue;

                // update bv and transform
                scene->bounding_volumes[n].min_extents = -vec3f::one();
                scene->bounding_volumes[n].max_extents = vec3f(1.0f, 0.0f, 1.0f);

                f32 angle = acos(1.0f - l.cos_cutoff);
                f32 lo = tan(angle);
                f32 range = l.radius;

                scene->transforms[n].scale = vec3f(lo * range, range, lo * range);
                scene->entities[n] |= e_cmp::transform;

                cmp_transform& t = scene->transforms[n];

                vec3f dir = normalized(-scene->world_matrices[n].get_column(1).xyz);

                light_buffer.lights[pos].pos_radius = vec4f(t.translation, l.radius);
                light_buffer.lights[pos].dir_cutoff = vec4f(dir, l.cos_cutoff);
                light_buffer.lights[pos].colour = vec4f(l.colour, l.shadow_map ? 1.0 : 0.0);
                light_buffer.lights[pos].data = vec4f(l.spot_falloff, 0.0f, 0.0f, 0.0f);

                ++num_spot_lights;
                ++num_lights;
                ++pos;
            }

            // info for loops
            light_buffer.info = vec4f(num_directions_lights, num_point_lights, num_spot_lights, 0.0f);

            pen::renderer_update_buffer(scene->forward_light_buffer, &light_buffer, sizeof(light_buffer));

            // Area light buffer
            static area_light_buffer al_buffer;

            static vec4f corners_al[] = {vec4f(-1.0, 0.0, -1.0, 1.0), vec4f(1.0, 0.0, -1.0, 1.0), vec4f(1.0, 0.0, 1.0, 1.0),
                                         vec4f(-1.0, 0.0, 1.0, 1.0)};

            u32 num_area_lights = 0;
            u32 num_constant_colour_area_lights = 0;
            u32 num_textured_area_lights = 0;
            // constant colour area light
            for (size_t n = 0; n < scene->num_entities; ++n)
            {
                if (num_lights >= e_scene_limits::max_forward_lights)
                    break;

                if (!(scene->entities[n] & e_cmp::light))
                    continue;

                cmp_light& l = scene->lights[n];
                if (l.type != e_light_type::area)
                    continue;

                mat4& wm = scene->world_matrices[n];
                for (u32 c = 0; c < 4; ++c)
                    al_buffer.lights[num_area_lights].corners[c] = wm.transform_vector(corners_al[c]);

                al_buffer.lights[num_area_lights].colour = vec4f(l.colour, num_textured_area_lights);
                ++num_constant_colour_area_lights;

                ++num_area_lights;
            }
            // textured / shader / animated area light
            for (size_t n = 0; n < scene->num_entities; ++n)
            {
                if (num_lights >= e_scene_limits::max_forward_lights)
                    break;

                if (!(scene->entities[n] & e_cmp::light))
                    continue;

                cmp_light& l = scene->lights[n];
                if (l.type != e_light_type::area_ex)
                    continue;

                mat4& wm = scene->world_matrices[n];
                for (u32 c = 0; c < 4; ++c)
                    al_buffer.lights[num_area_lights].corners[c] = wm.transform_vector(corners_al[c]);

                scene->draw_call_data[n].v1.y = (f32)anim_time; // time
                al_buffer.lights[num_area_lights].colour = vec4f(l.colour, num_textured_area_lights);
                scene->draw_call_data[n].v1.z = (f32)num_textured_area_lights;
                ++num_textured_area_lights;

                ++num_area_lights;
            }

            al_buffer.info.x = num_constant_colour_area_lights;
            al_buffer.info.y = num_textured_area_lights;

            pen::renderer_update_buffer(scene->area_light_buffer, &al_buffer, sizeof(al_buffer));

            const pmfx::render_target* alrt = pmfx::get_render_target(PEN_HASH("area_light_textures"));
            if (!alrt)
                return;

            if (alrt->num_arrays < num_area_lights)
            {
                pmfx::rt_resize_params rrp;
                rrp.width = 640;
                rrp.height = 480;
                rrp.format = nullptr;
                rrp.num_arrays = std::max<u32>(num_area_lights, 1);
                rrp.num_mips = -1;
                rrp.collection = pen::TEXTURE_COLLECTION_ARRAY;
                pmfx::resize_render_target(PEN_HASH("area_light_textures"), rrp);
            }

            // Distance field shadows
            for (size_t n = 0; n < scene->num_entities; ++n)
            {
                if (!(scene->entities[n] & e_cmp::sdf_shadow))
                    continue;

                static distance_field_shadow_buffer sdf_buffer;

                sdf_buffer.shadows.world_matrix = scene->world_matrices[n];
                sdf_buffer.shadows.world_matrix_inverse = mat::inverse4x4(scene->world_matrices[n]);

                pen::renderer_update_buffer(scene->sdf_shadow_buffer, &sdf_buffer, sizeof(sdf_buffer));
            }

            // Shadow maps
            
            // directional
            u32 num_shadow_maps = 0;
            u32 num_omni_shadow_maps = 0;
            for (size_t n = 0; n < scene->num_entities; ++n)
            {
                if (!(scene->entities[n] & e_cmp::light))
                    continue;

                cmp_light& l = scene->lights[n];
                if (!l.shadow_map)
                    continue;

                if(l.type == e_light_type::point)
                    ++num_omni_shadow_maps;
                else
                    ++num_shadow_maps;
            }

            const pmfx::render_target* sm = pmfx::get_render_target(PEN_HASH("shadow_map"));

            if (sm->num_arrays < num_shadow_maps)
            {
                pmfx::rt_resize_params rrp;
                rrp.width = 2048;
                rrp.height = 2048;
                rrp.format = nullptr;
                rrp.num_arrays = num_shadow_maps;
                rrp.num_mips = 1;
                rrp.collection = pen::TEXTURE_COLLECTION_ARRAY;
                pmfx::resize_render_target(PEN_HASH("shadow_map"), rrp);
            }
            
            // omni directional
            const pmfx::render_target* osm = pmfx::get_render_target(PEN_HASH("omni_shadow_map"));
            if(osm)
            {
                if (osm->num_arrays < num_omni_shadow_maps*6)
                {
                    pmfx::rt_resize_params rrp;
                    rrp.width = osm->width;
                    rrp.height = osm->height;
                    rrp.format = nullptr;
                    rrp.num_arrays = num_omni_shadow_maps*6;
                    rrp.num_mips = 1;
                    rrp.collection = pen::TEXTURE_COLLECTION_CUBE_ARRAY;
                    pmfx::resize_render_target(PEN_HASH("omni_shadow_map"), rrp);
                }
            }

            // Update pre skinned vertex buffers
            static hash_id id_pre_skin_technique = PEN_HASH("pre_skin");
            static u32 shader = pmfx::load_shader("forward_render");
            if (pmfx::set_technique_perm(shader, id_pre_skin_technique))
            {
                for (size_t n = 0; n < scene->num_entities; ++n)
                {
                    if (!(scene->entities[n] & e_cmp::pre_skinned))
                        continue;

                    // update bone cbuffer
                    cmp_geometry& geom = scene->geometries[n];
                    if (geom.p_skin->bone_cbuffer == PEN_INVALID_HANDLE)
                    {
                        pen::buffer_creation_params bcp;
                        bcp.usage_flags = PEN_USAGE_DYNAMIC;
                        bcp.bind_flags = PEN_BIND_CONSTANT_BUFFER;
                        bcp.cpu_access_flags = PEN_CPU_ACCESS_WRITE;
                        bcp.buffer_size = sizeof(mat4) * 85;
                        bcp.data = nullptr;

                        geom.p_skin->bone_cbuffer = pen::renderer_create_buffer(bcp);
                    }

                    static mat4 bb[85];
                    s32         joints_offset = scene->anim_controller_v2[n].joints_offset;
                    for (s32 i = 0; i < geom.p_skin->num_joints; ++i)
                        bb[i] = scene->world_matrices[joints_offset + i] * geom.p_skin->joint_bind_matrices[i];

                    pen::renderer_update_buffer(geom.p_skin->bone_cbuffer, bb, sizeof(bb));

                    // bind stream out targets
                    cmp_pre_skin& pre_skin = scene->pre_skin[n];
                    pen::renderer_set_stream_out_target(geom.vertex_buffer);
                    pen::renderer_set_constant_buffer(geom.p_skin->bone_cbuffer, 2, pen::CBUFFER_BIND_VS);
                    pen::renderer_set_vertex_buffer(pre_skin.vertex_buffer, 0, pre_skin.vertex_size, 0);

                    // render point list
                    pen::renderer_draw(pre_skin.num_verts, 0, PEN_PT_POINTLIST);
                    pen::renderer_set_stream_out_target(0);
                }
            }

            // update draw call data
            for (size_t n = 0; n < scene->num_entities; ++n)
            {
                if (scene->entities[n] & e_cmp::material)
                {
                    // per node material cbuffer
                    if (is_valid(scene->materials[n].material_cbuffer))
                        pen::renderer_update_buffer(scene->materials[n].material_cbuffer, &scene->material_data[n].data[0],
                                                    scene->materials[n].material_cbuffer_size);
                }

                scene->draw_call_data[n].world_matrix = scene->world_matrices[n];

                // store node index in v1.x
                scene->draw_call_data[n].v1.x = (f32)n;

                if (is_invalid_or_null(scene->cbuffer[n]))
                    continue;

                if (scene->entities[n] & e_cmp::sub_instance)
                    continue;

                // skinned meshes have the world matrix baked into the bones
                if (scene->entities[n] & e_cmp::skinned || scene->entities[n] & e_cmp::pre_skinned)
                    scene->draw_call_data[n].world_matrix = mat4::create_identity();

                mat4 invt = scene->world_matrices[n];

                invt = invt.transposed();
                invt = mat::inverse4x4(invt);

                scene->draw_call_data[n].world_matrix_inv_transpose = invt;

                // todo mark dirty?
                pen::renderer_update_buffer(scene->cbuffer[n], &scene->draw_call_data[n], sizeof(cmp_draw_call));
            }

            // update instance buffers
            for (size_t n = 0; n < scene->num_entities; ++n)
            {
                if (!(scene->entities[n] & e_cmp::master_instance))
                    continue;

                cmp_master_instance& master = scene->master_instances[n];

                u32 instance_data_size = master.num_instances * master.instance_stride;
                pen::renderer_update_buffer(master.instance_buffer, &scene->draw_call_data[n + 1], instance_data_size);

                // stride over sub instances
                n += scene->master_instances[n].num_instances;
            }

            // update physics running 1 frame behind to allow the sets to take effect
            physics::step();
            physics::physics_consume_command_buffer();

            // controllers post update
            for (u32 c = 0; c < num_controllers; ++c)
                if (scene->controllers[c].post_update_func)
                    scene->controllers[c].post_update_func(scene->controllers[c], scene, dt);
                    
            f32 elapsed = pen::timer_elapsed_ms(timer);
            PEN_UNUSED(elapsed);
            // PEN_LOG("scene update: %f(ms)", elapsed);
        }

        struct scene_header
        {
            s32 header_size = sizeof(*this);
            s32 version = ecs_scene::k_version;
            u32 num_nodes = 0;
            s32 num_components = 0;
            s32 num_lookup_strings = 0;
            s32 num_extensions = 0;
            s32 num_base_components = 0;
            s32 reserved_1[25] = {0};
            u32 view_flags = 0;
            s32 selected_index = 0;
            s32 reserved_2[30] = {0};
        };

        struct lookup_string
        {
            Str     name;
            hash_id id;
        };
        static lookup_string* s_lookup_strings = nullptr;

        void write_lookup_string(const char* string, std::ofstream& ofs, const c8* strip_project_dir = nullptr)
        {
            hash_id id = 0;

            Str stripped = string;
            if (strip_project_dir)
            {
                stripped = pen::str_replace_string(stripped, strip_project_dir, "");
                string = stripped.c_str();
            }

            if (!string)
            {
                ofs.write((const c8*)&id, sizeof(hash_id));
                return;
            }

            id = PEN_HASH(string);
            ofs.write((const c8*)&id, sizeof(hash_id));

            u32 num_strings = sb_count(s_lookup_strings);
            for (u32 i = 0; i < num_strings; ++i)
            {
                if (id == s_lookup_strings[i].id)
                {
                    return;
                }
            }

            lookup_string ls = {string, id};
            sb_push(s_lookup_strings, ls);
        }

        Str read_lookup_string(std::ifstream& ifs)
        {
            hash_id id;
            ifs.read((c8*)&id, sizeof(hash_id));

            u32 num_strings = sb_count(s_lookup_strings);
            for (u32 i = 0; i < num_strings; ++i)
            {
                if (s_lookup_strings[i].id == id)
                {
                    return s_lookup_strings[i].name;
                }
            }

            return "";
        }

        hash_id rehash_lookup_string(hash_id id)
        {
            u32 num_strings = sb_count(s_lookup_strings);
            for (u32 i = 0; i < num_strings; ++i)
            {
                if (s_lookup_strings[i].id == id)
                {
                    return PEN_HASH(s_lookup_strings[i].name);
                }
            }

            return 0;
        }

        void save_sub_scene(ecs_scene* scene, u32 root)
        {
            std::vector<s32> nodes;
            build_heirarchy_node_list(scene, root, nodes);

            u32 num = nodes.size();

            ecs_scene sub_scene;

            // create sub scene with same components
            u32 num_ext = sb_count(scene->extensions);
            for (u32 e = 0; e < num_ext; ++e)
                scene->extensions[e].ext_func(&sub_scene);

            resize_scene_buffers(&sub_scene, num);

            for (u32 i = 0; i < num; ++i)
            {
                u32 ii = nodes[i];
                if (ii == PEN_INVALID_HANDLE)
                    continue;

                u32 ni = sub_scene.num_entities;

                for (u32 c = 0; c < scene->num_components; ++c)
                {
                    generic_cmp_array& src = scene->get_component_array(c);
                    generic_cmp_array& dst = sub_scene.get_component_array(c);

                    memcpy(dst[ni], src[ii], src.size);
                }

                sub_scene.parents[ni] -= root;
                sub_scene.num_entities++;
            }

            Str fn = "";
            fn.appendf("../../assets/scene/%s.pms", sub_scene.names[0].c_str());

            save_scene(fn.c_str(), &sub_scene);

            free_scene_buffers(&sub_scene, true);
            unregister_ecs_extensions(&sub_scene);
        }

        void save_scene(const c8* filename, ecs_scene* scene)
        {
            const c8* wd = pen::os_get_user_info().working_directory;
            Str project_dir = dev_ui::get_program_preference_filename("project_dir", wd);

            std::ofstream ofs(filename, std::ofstream::binary);

            sb_free(s_lookup_strings);
            s_lookup_strings = nullptr;

            // write basic components
            for (u32 i = 0; i < scene->num_components; ++i)
            {
                generic_cmp_array& cmp = scene->get_component_array(i);
                ofs.write((const c8*)cmp.data, cmp.size * scene->num_entities);
            }

            // specialisations ------------------------------------------------------------------------------

            // names
            for (s32 n = 0; n < scene->num_entities; ++n)
            {
                write_lookup_string(scene->names[n].c_str(), ofs);
                write_lookup_string(scene->geometry_names[n].c_str(), ofs);
                write_lookup_string(scene->material_names[n].c_str(), ofs);
            }

            // geometry
            for (s32 n = 0; n < scene->num_entities; ++n)
            {
                if (!(scene->entities[n] & e_cmp::geometry))
                    continue;

                geometry_resource* gr = get_geometry_resource(scene->id_geometry[n]);

                ofs.write((const c8*)&gr->submesh_index, sizeof(u32));

                write_lookup_string(gr->filename.c_str(), ofs, project_dir.c_str());
                write_lookup_string(gr->geometry_name.c_str(), ofs, project_dir.c_str());
            }

            // animations
            for (s32 n = 0; n < scene->num_entities; ++n)
            {
                s32 size = 0;

                if (scene->anim_controller_v2[n].anim_instances)
                    size = sb_count(scene->anim_controller_v2[n].anim_instances);

                ofs.write((const c8*)&size, sizeof(s32));

                for (s32 i = 0; i < size; ++i)
                {
                    // todo with anim controller v2
                    // auto* anim = get_animation_resource(scene->anim_controller_v2[n].anim_instances[i].);
                    write_lookup_string("placeholder", ofs, project_dir.c_str());
                }
            }

            // material
            for (s32 n = 0; n < scene->num_entities; ++n)
            {
                if (!(scene->entities[n] & e_cmp::material))
                    continue;

                cmp_material&      mat = scene->materials[n];
                material_resource& mat_res = scene->material_resources[n];

                const char* shader_name = pmfx::get_shader_name(mat.shader);
                const char* technique_name = pmfx::get_technique_name(mat.shader, mat_res.id_technique);

                write_lookup_string(mat_res.material_name.c_str(), ofs);
                write_lookup_string(shader_name, ofs);
                write_lookup_string(technique_name, ofs);
            }

            // shadow
            for (s32 n = 0; n < scene->num_entities; ++n)
            {
                if (!(scene->entities[n] & e_cmp::sdf_shadow))
                    continue;

                cmp_shadow& shadow = scene->shadows[n];

                write_lookup_string(put::get_texture_filename(shadow.texture_handle).c_str(), ofs, project_dir.c_str());
            }

            // sampler bindings
            for (s32 n = 0; n < scene->num_entities; ++n)
            {
                if (!(scene->entities[n] & e_cmp::samplers))
                    continue;

                cmp_samplers& samplers = scene->samplers[n];

                for (u32 i = 0; i < e_pmfx_constants::max_technique_sampler_bindings; ++i)
                {
                    write_lookup_string(put::get_texture_filename(samplers.sb[i].handle).c_str(), ofs, project_dir.c_str());
                    write_lookup_string(pmfx::get_render_state_name(samplers.sb[i].sampler_state).c_str(), ofs,
                                        project_dir.c_str());
                }
            }

            // cameras
            camera** cams = pmfx::get_cameras();
            u32      num_cams = sb_count(cams);
            for (u32 i = 0; i < num_cams; ++i)
            {
                write_lookup_string(cams[i]->name.c_str(), ofs);
            }

            // call extensions specific save
            u32 num_extensions = sb_count(scene->extensions);
            for (u32 i = 0; i < num_extensions; ++i)
                if (scene->extensions[i].save_func)
                    scene->extensions[i].save_func(scene->extensions[i], scene);

            ofs.close();

            std::ifstream infile(filename, std::ifstream::binary);

            // get size of file
            infile.seekg(0, infile.end);
            u32 scene_data_size = infile.tellg();
            infile.seekg(0);

            // allocate memory for file content
            c8* scene_data = new c8[scene_data_size];

            // read content of infile
            infile.read(scene_data, scene_data_size);

            ofs = std::ofstream(filename, std::ofstream::binary);

            // header
            scene_header sh;
            sh.num_nodes = scene->num_entities;
            sh.view_flags = scene->view_flags;
            sh.selected_index = scene->selected_index;
            sh.num_components = scene->num_components;
            sh.num_base_components = scene->num_base_components;
            sh.num_lookup_strings = sb_count(s_lookup_strings);
            sh.num_extensions = sb_count(scene->extensions);
            ofs.write((const c8*)&sh, sizeof(scene_header));

            // component sizes
            for (u32 c = 0; c < sh.num_components; ++c)
            {
                ofs.write((const c8*)&scene->get_component_array(c).size, sizeof(u32));
            }

            // extensions
            for (u32 i = 0; i < sh.num_extensions; ++i)
            {
                u32 co = get_extension_component_offset(scene, i);
                write_lookup_string(scene->extensions[i].name.c_str(), ofs);
                ofs.write((const c8*)&co, sizeof(u32));
                ofs.write((const c8*)&scene->extensions[i].num_components, sizeof(u32));
            }

            // string lookups
            for (u32 l = 0; l < sh.num_lookup_strings; ++l)
            {
                write_parsable_string(s_lookup_strings[l].name.c_str(), ofs);
                ofs.write((const c8*)&s_lookup_strings[l].id, sizeof(hash_id));
            }

            // write camera info
            ofs.write((const c8*)&num_cams, sizeof(u32));
            for (u32 i = 0; i < num_cams; ++i)
            {
                hash_id id_cam = PEN_HASH(cams[i]->name);
                ofs.write((const c8*)&id_cam, sizeof(hash_id));
                ofs.write((const c8*)&cams[i]->pos, sizeof(vec3f));
                ofs.write((const c8*)&cams[i]->focus, sizeof(vec3f));
                ofs.write((const c8*)&cams[i]->rot, sizeof(vec2f));
                ofs.write((const c8*)&cams[i]->fov, sizeof(f32));
                ofs.write((const c8*)&cams[i]->aspect, sizeof(f32));
                ofs.write((const c8*)&cams[i]->near_plane, sizeof(f32));
                ofs.write((const c8*)&cams[i]->far_plane, sizeof(f32));
                ofs.write((const c8*)&cams[i]->zoom, sizeof(f32));
            }

            // write scene data
            ofs.write(scene_data, scene_data_size);
            ofs.close();
        }

        void load_scene(const c8* filename, ecs_scene* scene, bool merge)
        {
            scene->flags |= e_scene_flags::invalidate_scene_tree;
            bool error = false;
            const c8* wd = pen::os_get_user_info().working_directory;
            Str  project_dir = dev_ui::get_program_preference_filename("project_dir", wd);

            std::ifstream ifs(pen::os_path_for_resource(filename), std::ofstream::binary);

            // header
            scene_header sh;
            ifs.read((c8*)&sh, sizeof(scene_header));

            if (!merge)
            {
                scene->version = sh.version;
                scene->filename = filename;
            }

            // version 9 adds extensions
            if (sh.version < 9)
                sh.num_base_components = sh.num_components;

            // unpack header
            s32 num_nodes = sh.num_nodes;

            scene->selected_index = sh.selected_index;
            s32 scene_view_flags = sh.view_flags;

            u32 zero_offset = 0;
            s32 new_num_nodes = num_nodes;

            if (merge)
            {
                zero_offset = scene->num_entities;
                new_num_nodes = scene->num_entities + num_nodes;
            }
            else
            {
                clear_scene(scene);
            }

            if (new_num_nodes > scene->soa_size)
                resize_scene_buffers(scene, num_nodes);

            scene->num_entities = new_num_nodes;

            // read component sizes
            u32* component_sizes = nullptr;
            for (u32 i = 0; i < sh.num_components; ++i)
            {
                u32 size;
                ifs.read((c8*)&size, sizeof(u32));
                sb_push(component_sizes, size);
            }

            // extensions
            struct ext_components
            {
                hash_id id;
                u32     start_cmp;
                u32     num_cmp;
            };
            ext_components* exts = nullptr;

            for (u32 i = 0; i < sh.num_extensions; ++i)
            {
                ext_components ext;
                ifs.read((c8*)&ext.id, sizeof(hash_id));
                ifs.read((c8*)&ext.start_cmp, sizeof(u32));
                ifs.read((c8*)&ext.num_cmp, sizeof(u32));

                sb_push(exts, ext);
            }

            // read string lookups
            sb_free(s_lookup_strings);
            s_lookup_strings = nullptr;

            for (u32 n = 0; n < sh.num_lookup_strings; ++n)
            {
                lookup_string ls;
                ls.name = read_parsable_string(ifs);
                ifs.read((c8*)&ls.id, sizeof(hash_id));

                sb_push(s_lookup_strings, ls);
            }

            // rehash extension ids
            for (u32 i = 0; i < sh.num_extensions; ++i)
            {
                exts[i].id = rehash_lookup_string(exts[i].id);
            }

            // read cameras
            u32 num_cams;
            ifs.read((c8*)&num_cams, sizeof(u32));

            for (u32 i = 0; i < num_cams; ++i)
            {
                camera  cam;
                hash_id id_cam;

                ifs.read((c8*)&id_cam, sizeof(hash_id));
                ifs.read((c8*)&cam.pos, sizeof(vec3f));
                ifs.read((c8*)&cam.focus, sizeof(vec3f));
                ifs.read((c8*)&cam.rot, sizeof(vec2f));
                ifs.read((c8*)&cam.fov, sizeof(f32));
                ifs.read((c8*)&cam.aspect, sizeof(f32));
                ifs.read((c8*)&cam.near_plane, sizeof(f32));
                ifs.read((c8*)&cam.far_plane, sizeof(f32));
                ifs.read((c8*)&cam.zoom, sizeof(f32));

                // find camera and set
                camera* _cam = pmfx::get_camera(id_cam);
                if (_cam && !merge)
                {
                    _cam->pos = cam.pos;
                    _cam->focus = cam.focus;
                    _cam->rot = cam.rot;
                    _cam->fov = cam.fov;
                    _cam->aspect = cam.aspect;
                    _cam->near_plane = cam.near_plane;
                    _cam->far_plane = cam.far_plane;
                    _cam->zoom = cam.zoom;
                }
            }

            // read all components
            for (u32 i = 0; i < sh.num_components; ++i)
            {
                u32 ri = i; // remap i.. if we have extensions

                // extensions
                if (i >= sh.num_base_components)
                {
                    ri = -1;

                    //find extension that maps to this component, allow out of order or missing components
                    for (u32 e = 0; e < sh.num_extensions; ++e)
                    {
                        u32 ext_i = i - exts[e].start_cmp;
                        if (i >= exts[e].start_cmp && ext_i < exts[e].num_cmp)
                        {
                            ri = get_extension_component_offset_from_id(scene, exts[e].id) + ext_i;
                            break;
                        }
                    }
                }

                bool read = false;

                if (ri != -1)
                {
                    generic_cmp_array& cmp = scene->get_component_array(ri);

                    if (cmp.size == component_sizes[i])
                    {
                        // read whole array
                        c8* data_offset = (c8*)cmp.data + zero_offset * cmp.size;
                        ifs.read(data_offset, cmp.size * num_nodes);
                        read = true;
                    }
                }

                if (!read)
                {
                    // read the old size
                    u32 array_size = component_sizes[i] * num_nodes;
                    c8* old = (c8*)pen::memory_alloc(array_size);
                    ifs.read(old, array_size);

                    // here any fuxup can be applied old into cmp.data

                    pen::memory_free(old);
                }
            }

            // fixup parents for scene import / merge
            for (s32 n = zero_offset; n < zero_offset + num_nodes; ++n)
                scene->parents[n] += zero_offset;

            // read specialisations
            for (s32 n = zero_offset; n < zero_offset + num_nodes; ++n)
            {
                memset(&scene->names[n], 0x0, sizeof(Str));
                memset(&scene->geometry_names[n], 0x0, sizeof(Str));
                memset(&scene->material_names[n], 0x0, sizeof(Str));

                scene->names[n] = read_lookup_string(ifs);
                scene->geometry_names[n] = read_lookup_string(ifs);
                scene->material_names[n] = read_lookup_string(ifs);
            }

            // geometry
            for (s32 n = zero_offset; n < zero_offset + num_nodes; ++n)
            {
                if (scene->entities[n] & e_cmp::geometry)
                {
                    u32 submesh;
                    ifs.read((c8*)&submesh, sizeof(u32));

                    Str filename = project_dir;
                    Str name = read_lookup_string(ifs).c_str();
                    Str geometry_name = read_lookup_string(ifs);

                    hash_id        name_hash = PEN_HASH(name.c_str());
                    static hash_id primitive_id = PEN_HASH("primitive");

                    filename.append(name.c_str());

                    geometry_resource* gr = nullptr;

                    if (name_hash != primitive_id)
                    {
                        dev_console_log("[scene load] %s", name.c_str());
                        load_pmm(filename.c_str(), nullptr, e_pmm_load_flags::geometry);

                        pen::hash_murmur hm;
                        hm.begin(0);
                        hm.add(filename.c_str(), filename.length());
                        hm.add(geometry_name.c_str(), geometry_name.length());
                        hm.add(submesh);
                        hash_id geom_hash = hm.end();

                        gr = get_geometry_resource(geom_hash);

                        scene->id_geometry[n] = geom_hash;
                    }
                    else
                    {
                        hash_id geom_hash = PEN_HASH(geometry_name.c_str());
                        gr = get_geometry_resource(geom_hash);
                    }

                    if (gr)
                    {
                        instantiate_geometry(gr, scene, n);
                        instantiate_model_cbuffer(scene, n);

                        if (gr->p_skin)
                            instantiate_anim_controller_v2(scene, n);
                    }
                    else
                    {
                        dev_ui::log_level(dev_ui::console_level::error, "[error] geometry - cannot find pmm file: %s",
                                          filename.c_str());

                        scene->entities[n] &= ~e_cmp::geometry;
                        error = true;
                    }
                }
            }

            // instantiate physics
            for (s32 n = zero_offset; n < zero_offset + num_nodes; ++n)
                if (scene->entities[n] & e_cmp::physics)
                    instantiate_rigid_body(scene, n);

            for (s32 n = zero_offset; n < zero_offset + num_nodes; ++n)
                if (scene->entities[n] & e_cmp::constraint)
                    instantiate_constraint(scene, n);

            // animations
            for (s32 n = zero_offset; n < zero_offset + num_nodes; ++n)
            {
                s32 size;
                ifs.read((c8*)&size, sizeof(s32));

                for (s32 i = 0; i < size; ++i)
                {
                    Str anim_name = project_dir;
                    anim_name.append(read_lookup_string(ifs).c_str());

                    anim_handle h = load_pma(anim_name.c_str());

                    if (!is_valid(h))
                    {
                        dev_ui::log_level(dev_ui::console_level::error, "[error] animation - cannot find pma file: %s",
                                          anim_name.c_str());
                        error = true;
                    }

                    bind_animation_to_rig(scene, h, n);
                }
            }

            // materials
            for (s32 n = zero_offset; n < zero_offset + num_nodes; ++n)
            {
                if (!(scene->entities[n] & e_cmp::material))
                    continue;

                cmp_material&      mat = scene->materials[n];
                material_resource& mat_res = scene->material_resources[n];

                // Invalidate stuff we need to recreate
                memset(&mat_res.material_name, 0x0, sizeof(Str));
                memset(&mat_res.shader_name, 0x0, sizeof(Str));
                mat.material_cbuffer = PEN_INVALID_HANDLE;

                Str material_name = read_lookup_string(ifs);
                Str shader = read_lookup_string(ifs);
                Str technique = read_lookup_string(ifs);

                mat_res.material_name = material_name;
                mat_res.id_shader = PEN_HASH(shader.c_str());
                mat_res.id_technique = PEN_HASH(technique.c_str());
                mat_res.shader_name = shader;
            }

            // sdf shadow
            for (s32 n = zero_offset; n < zero_offset + num_nodes; ++n)
            {
                if (!(scene->entities[n] & e_cmp::sdf_shadow))
                    continue;

                Str sdf_shadow_volume_file = read_lookup_string(ifs);
                sdf_shadow_volume_file = pen::str_replace_string(sdf_shadow_volume_file, ".dds", ".pmv");

                dev_console_log("[scene load] %s", sdf_shadow_volume_file.c_str());
                instantiate_sdf_shadow(sdf_shadow_volume_file.c_str(), scene, n);
            }

            // sampler binding textures
            for (s32 n = zero_offset; n < zero_offset + num_nodes; ++n)
            {
                if (!(scene->entities[n] & e_cmp::samplers))
                    continue;

                cmp_samplers& samplers = scene->samplers[n];

                for (u32 i = 0; i < e_pmfx_constants::max_technique_sampler_bindings; ++i)
                {
                    Str texture_name = read_lookup_string(ifs);

                    if (!texture_name.empty())
                    {
                        samplers.sb[i].handle = put::load_texture(texture_name.c_str());
                        samplers.sb[i].sampler_state = pmfx::get_render_state(PEN_HASH("wrap_linear"), pmfx::e_render_state::sampler);
                    }

                    Str sampler_state_name = read_lookup_string(ifs);

                    if (!sampler_state_name.empty())
                    {
                        samplers.sb[i].sampler_state = pmfx::get_render_state(PEN_HASH(sampler_state_name), pmfx::e_render_state::sampler);
                    }
                }
            }

            // read cams strings
            for (u32 i = 0; i < num_cams; ++i)
                read_lookup_string(ifs);

            // read extensions
            for (u32 i = 0; i < sh.num_extensions; ++i)
                if (scene->extensions[i].load_func)
                    scene->extensions[i].load_func(scene->extensions[i], scene);

            bake_material_handles();

            // light geom
            for (s32 n = zero_offset; n < zero_offset + num_nodes; ++n)
            {
                if (!(scene->entities[n] & e_cmp::light))
                    continue;

                instantiate_model_cbuffer(scene, n);
            }

            // invalidate physics debug cbuffer.. will recreate on demand
            for (s32 n = zero_offset; n < zero_offset + num_nodes; ++n)
                scene->physics_debug_cbuffer[n] = PEN_INVALID_HANDLE;

            if (!merge)
            {
                scene->view_flags = scene_view_flags;
                
                // show bones and mats if we have an error, to aid deugging
                if(error)
                    scene->view_flags |= (e_scene_view_flags::matrix | e_scene_view_flags::bones);
            }

            ifs.close();

            initialise_free_list(scene);

            // cleanup
            sb_free(component_sizes);
            sb_free(exts);
        }
    } // namespace ecs
} // namespace put
