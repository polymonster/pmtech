#include <fstream>
#include <functional>

#include "debug_render.h"
#include "dev_ui.h"
#include "file_system.h"
#include "hash.h"
#include "pmfx.h"
#include "str/Str.h"
#include "str_utilities.h"

#include "ces/ces_resources.h"
#include "ces/ces_scene.h"
#include "ces/ces_utilities.h"

using namespace put;

namespace put
{
    namespace ces
    {
        std::vector<entity_scene_instance> k_scenes;

        void initialise_free_list(entity_scene* scene)
        {
            scene->free_list_head = nullptr;

            for (s32 i = scene->nodes_size - 1; i >= 0; --i)
            {
                scene->free_list[i].node = i;

                if (!(scene->entities[i] & CMP_ALLOCATED))
                {
                    free_node_list* l = &scene->free_list[i];
                    l->next           = scene->free_list_head;

                    if (l->next)
                        l->next->prev = l;

                    scene->free_list_head = l;
                }
            }
        }

        void resize_scene_buffers(entity_scene* scene, s32 size)
        {
            u32 new_size = scene->nodes_size + size;

            for (u32 i = 0; i < scene->num_components; ++i)
            {
                generic_cmp_array& cmp        = scene->get_component_array(i);
                u32                alloc_size = cmp.size * new_size;

                if (cmp.data)
                {
                    // realloc
                    cmp.data = pen::memory_realloc(cmp.data, alloc_size);

                    // zero new mem
                    u32 prev_size  = scene->nodes_size * cmp.size;
                    u8* new_offset = (u8*)cmp.data + prev_size;
                    u32 zero_size  = alloc_size - prev_size;
                    pen::memory_zero(new_offset, zero_size);

                    continue;
                }

                // alloc and zero
                cmp.data = pen::memory_alloc(alloc_size);
                pen::memory_zero(cmp.data, alloc_size);
            }

            initialise_free_list(scene);

            scene->nodes_size = new_size;
        }

        void free_scene_buffers(entity_scene* scene)
        {
            // Remove entites for sub systems (physics, rendering, etc)
            for (s32 i = 0; i < scene->num_nodes; ++i)
                delete_entity_first_pass(scene, i);

            for (s32 i = 0; i < scene->num_nodes; ++i)
                delete_entity_second_pass(scene, i);

            // Free component array memory
            for (u32 i = 0; i < scene->num_components; ++i)
            {
                generic_cmp_array& cmp = scene->get_component_array(i);
                pen::memory_free(cmp.data);
                cmp.data = nullptr;
            }

            scene->nodes_size = 0;
            scene->num_nodes  = 0;
        }

        void zero_entity_components(entity_scene* scene, u32 node_index)
        {
            for (u32 i = 0; i < scene->num_components; ++i)
            {
                generic_cmp_array& cmp    = scene->get_component_array(i);
                u8*                offset = (u8*)cmp.data + node_index * cmp.size;
                pen::memory_zero(offset, cmp.size);
            }
        }

        void delete_entity(entity_scene* scene, u32 node_index)
        {
            // free allocated stuff
            if (scene->physics_handles[node_index])
                physics::release_entity(scene->physics_handles[node_index]);

            if (scene->cbuffer[node_index])
                pen::renderer_release_buffer(scene->cbuffer[node_index]);

            // zero
            zero_entity_components(scene, node_index);
        }

        void delete_entity_first_pass(entity_scene* scene, u32 node_index)
        {
            // constraints must be freed or removed before we delete rigidbodies using them
            if (scene->physics_handles[node_index] && (scene->entities[node_index] & CMP_CONSTRAINT))
                physics::release_entity(scene->physics_handles[node_index]);

            if (scene->cbuffer[node_index])
                pen::renderer_release_buffer(scene->cbuffer[node_index]);

            if (scene->entities[node_index] & CMP_PRE_SKINNED)
            {
                if (scene->pre_skin[node_index].vertex_buffer)
                    pen::renderer_release_buffer(scene->pre_skin[node_index].vertex_buffer);

                if (scene->pre_skin[node_index].position_buffer)
                    pen::renderer_release_buffer(scene->pre_skin[node_index].position_buffer);
            }

            if (scene->master_instances[node_index].instance_buffer)
                pen::renderer_release_buffer(scene->master_instances[node_index].instance_buffer);
        }

        void delete_entity_second_pass(entity_scene* scene, u32 node_index)
        {
            // all constraints must be removed by this point.
            if (scene->physics_handles[node_index] && (scene->entities[node_index] & CMP_PHYSICS))
                physics::release_entity(scene->physics_handles[node_index]);

            zero_entity_components(scene, node_index);
        }

        void clear_scene(entity_scene* scene)
        {
            free_scene_buffers(scene);
            resize_scene_buffers(scene);
        }

        u32 clone_node(entity_scene* scene, u32 src, s32 dst, s32 parent, u32 flags, vec3f offset, const c8* suffix)
        {
            if (dst == -1)
            {
                dst = get_new_node(scene);
            }
            else
            {
                if (dst >= scene->num_nodes)
                    scene->num_nodes = dst + 1;
            }

            entity_scene* p_sn = scene;

            u32 parent_offset = p_sn->parents[src] - src;
            if (parent == -1)
            {
                p_sn->parents[dst] = dst - parent_offset;
            }
            else
            {
                p_sn->parents[dst] = parent;
            }

            pen::memory_cpy(&p_sn->entities[dst], &p_sn->entities[src], sizeof(a_u64));

            // ids
            p_sn->id_name[dst]     = p_sn->id_name[src];
            p_sn->id_geometry[dst] = p_sn->id_geometry[src];
            p_sn->id_material[dst] = p_sn->id_material[src];
            p_sn->id_resource[dst] = p_sn->id_resource[src];

            // componenets
            p_sn->transforms[dst]       = p_sn->transforms[src];
            p_sn->local_matrices[dst]   = p_sn->local_matrices[src];
            p_sn->world_matrices[dst]   = p_sn->world_matrices[src];
            p_sn->offset_matrices[dst]  = p_sn->offset_matrices[src];
            p_sn->physics_matrices[dst] = p_sn->physics_matrices[src];
            p_sn->bounding_volumes[dst] = p_sn->bounding_volumes[src];
            p_sn->lights[dst]           = p_sn->lights[src];
            p_sn->physics_data[dst]     = p_sn->physics_data[src];
            p_sn->geometries[dst]       = p_sn->geometries[src];
            p_sn->materials[dst]        = p_sn->materials[src];
            p_sn->anim_controller[dst]  = p_sn->anim_controller[src];
            p_sn->physics_data[dst].rigid_body.position += offset;
            p_sn->draw_call_data[dst] = p_sn->draw_call_data[src];

            vec3f right       = p_sn->local_matrices[dst].get_right();
            vec3f up          = p_sn->local_matrices[dst].get_up();
            vec3f fwd         = p_sn->local_matrices[dst].get_fwd();
            vec3f translation = p_sn->local_matrices[dst].get_translation();

            p_sn->local_matrices[dst].set_vectors(right, up, fwd, translation + offset);

            p_sn->names[dst]          = Str();
            p_sn->geometry_names[dst] = Str();
            p_sn->material_names[dst] = Str();

            p_sn->names[dst] = p_sn->names[src].c_str();
            p_sn->names[dst].append(suffix);
            p_sn->geometry_names[dst] = p_sn->geometry_names[src].c_str();
            p_sn->material_names[dst] = p_sn->material_names[src].c_str();

            if (flags == CLONE_INSTANTIATE)
            {
                // todo, clone / instantiate constraint
                if (p_sn->physics_handles[src])
                    instantiate_rigid_body(scene, dst);

                if (p_sn->entities[dst] & CMP_GEOMETRY)
                    instantiate_model_cbuffer(scene, dst);
            }
            else if (flags == CLONE_MOVE)
            {
                p_sn->cbuffer[dst]         = p_sn->cbuffer[src];
                p_sn->physics_handles[dst] = p_sn->physics_handles[src];

                p_sn->entities[dst] |= CMP_TRANSFORM;
                zero_entity_components(scene, src);
            }

            return dst;
        }

        entity_scene* create_scene(const c8* name)
        {
            entity_scene_instance new_instance;
            new_instance.name  = name;
            new_instance.scene = new entity_scene();

            k_scenes.push_back(new_instance);

            resize_scene_buffers(new_instance.scene, 64);

            // create buffers
            pen::buffer_creation_params bcp;

            // forward lights
            bcp.usage_flags      = PEN_USAGE_DYNAMIC;
            bcp.bind_flags       = PEN_BIND_CONSTANT_BUFFER;
            bcp.cpu_access_flags = PEN_CPU_ACCESS_WRITE;
            bcp.buffer_size      = sizeof(forward_light_buffer);
            bcp.data             = nullptr;

            new_instance.scene->forward_light_buffer = pen::renderer_create_buffer(bcp);

            // sdf shadows
            bcp.usage_flags      = PEN_USAGE_DYNAMIC;
            bcp.bind_flags       = PEN_BIND_CONSTANT_BUFFER;
            bcp.cpu_access_flags = PEN_CPU_ACCESS_WRITE;
            bcp.buffer_size      = sizeof(distance_field_shadow_buffer);
            bcp.data             = nullptr;

            new_instance.scene->sdf_shadow_buffer = pen::renderer_create_buffer(bcp);

            // area lights
            bcp.usage_flags      = PEN_USAGE_DYNAMIC;
            bcp.bind_flags       = PEN_BIND_CONSTANT_BUFFER;
            bcp.cpu_access_flags = PEN_CPU_ACCESS_WRITE;
            bcp.buffer_size      = sizeof(area_box_light_buffer);
            bcp.data             = nullptr;

            new_instance.scene->area_box_light_buffer = pen::renderer_create_buffer(bcp);

            return new_instance.scene;
        }

        void destroy_scene(entity_scene* scene)
        {
            free_scene_buffers(scene);

            // todo release resource refs
            // geom
            // anim
        }

        void render_scene_view(const scene_view& view)
        {
            entity_scene* scene = view.scene;

            if (scene->view_flags & SV_HIDE)
                return;

            pen::renderer_set_constant_buffer(view.cb_view, 0, PEN_SHADER_TYPE_VS);
            pen::renderer_set_constant_buffer(view.cb_view, 0, PEN_SHADER_TYPE_PS);

            // forward lights
            if (view.render_flags & RENDER_FORWARD_LIT)
                pen::renderer_set_constant_buffer(scene->forward_light_buffer, 3, PEN_SHADER_TYPE_PS);

            s32 draw_count = 0;
            s32 cull_count = 0;

            for (u32 n = 0; n < scene->num_nodes; ++n)
            {
                if (!(scene->entities[n] & CMP_GEOMETRY && scene->entities[n] & CMP_MATERIAL))
                    continue;

                if (scene->entities[n] & CMP_SUB_INSTANCE)
                    continue;

                if (scene->state_flags[n] & SF_HIDDEN)
                    continue;

                // frustum cull
                bool inside = true;
                for (s32 i = 0; i < 6; ++i)
                {
                    frustum& camera_frustum = view.camera->camera_frustum;

                    vec3f& min = scene->bounding_volumes[n].transformed_min_extents;
                    vec3f& max = scene->bounding_volumes[n].transformed_max_extents;

                    vec3f pos    = min + (max - min) * 0.5f;
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
                cmp_material* p_mat  = &scene->materials[n];

                if (scene->entities[n] & CMP_SKINNED && !(scene->entities[n] & CMP_SUB_GEOMETRY))
                {
                    if (p_geom->p_skin->bone_cbuffer == PEN_INVALID_HANDLE)
                    {
                        pen::buffer_creation_params bcp;
                        bcp.usage_flags      = PEN_USAGE_DYNAMIC;
                        bcp.bind_flags       = PEN_BIND_CONSTANT_BUFFER;
                        bcp.cpu_access_flags = PEN_CPU_ACCESS_WRITE;
                        bcp.buffer_size      = sizeof(mat4) * 85;
                        bcp.data             = nullptr;

                        p_geom->p_skin->bone_cbuffer = pen::renderer_create_buffer(bcp);
                    }

                    static mat4 bb[85];

                    s32 joints_offset = scene->anim_controller[n].joints_offset;
                    for (s32 i = 0; i < p_geom->p_skin->num_joints; ++i)
                    {
                        bb[i] = scene->world_matrices[n + joints_offset + i] * p_geom->p_skin->joint_bind_matrices[i];
                    }

                    pen::renderer_update_buffer(p_geom->p_skin->bone_cbuffer, bb, sizeof(bb));
                    pen::renderer_set_constant_buffer(p_geom->p_skin->bone_cbuffer, 2, PEN_SHADER_TYPE_VS);
                }

                // set shader / technique
                u32 shader = view.pmfx_shader;

                if (!is_valid(shader))
                {
                    shader        = p_mat->pmfx_shader;
                    u32 technique = p_mat->technique;

                    pmfx::set_technique(shader, technique);
                }
                else
                {
                    hash_id technique = view.technique;
                    if (!pmfx::set_technique(shader, technique, p_geom->vertex_shader_class))
                    {
                        if (scene->entities[n] & CMP_MASTER_INSTANCE)
                        {
                            u32 num_instances = scene->master_instances[n].num_instances;
                            n += num_instances;
                        }
                        continue;
                    }
                }

                // set cbs
                pen::renderer_set_constant_buffer(scene->cbuffer[n], 1, PEN_SHADER_TYPE_VS);
                pen::renderer_set_constant_buffer(scene->cbuffer[n], 1, PEN_SHADER_TYPE_PS);

                // set ib / vb
                if (scene->entities[n] & CMP_MASTER_INSTANCE)
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
                    for (u32 t = 0; t < put::ces::SN_EMISSIVE_MAP; ++t)
                    {
                        if (is_valid(p_mat->texture_handles[t]))
                        {
                            pen::renderer_set_texture(p_mat->texture_handles[t], p_mat->sampler_states[t], t,
                                                      PEN_SHADER_TYPE_PS);
                        }
                    }
                }

                // stride over instances
                if (scene->entities[n] & CMP_MASTER_INSTANCE)
                {
                    u32 num_instances = scene->master_instances[n].num_instances;
                    pen::renderer_draw_indexed_instanced(num_instances, 0, scene->geometries[n].num_indices, 0, 0,
                                                         PEN_PT_TRIANGLELIST);
                    n += num_instances;
                    continue;
                }

                // draw
                pen::renderer_draw_indexed(scene->geometries[n].num_indices, 0, 0, PEN_PT_TRIANGLELIST);
            }
        }

        void update_animations(entity_scene* scene, f32 dt)
        {
            for (u32 n = 0; n < scene->num_nodes; ++n)
            {
                if (scene->entities[n] & CMP_ANIM_CONTROLLER)
                {
                    auto& controller = scene->anim_controller[n];

                    bool apply_trajectory = false;
                    mat4 trajectory;

                    if (is_valid(controller.current_animation))
                    {
                        auto* anim = get_animation_resource(controller.current_animation);

                        if (!anim)
                            continue;

                        if (controller.play_flags == cmp_anim_controller::PLAY)
                            controller.current_time += dt * 0.1f;

                        s32 joints_offset = scene->anim_controller[n].joints_offset;

                        for (s32 c = 0; c < anim->num_channels; ++c)
                        {
                            s32 num_frames = anim->channels[c].num_frames;

                            if (num_frames <= 0)
                                continue;

                            s32 t = 0;
                            for (t = 0; t < num_frames; ++t)
                                if (controller.current_time < anim->channels[c].times[t])
                                    break;

                            // loop
                            if (t >= num_frames)
                                t = 0;

                            mat4& mat = anim->channels[c].matrices[t];

                            s32 scene_node_index = n + c + joints_offset;

                            scene->local_matrices[scene_node_index] = mat;

                            if (scene->entities[scene_node_index] & CMP_ANIM_TRAJECTORY)
                            {
                                trajectory = anim->channels[c].matrices[num_frames - 1];
                            }

                            if (controller.current_time > anim->length)
                            {
                                apply_trajectory        = true;
                                controller.current_time = (controller.current_time) - (anim->length);
                            }
                        }
                    }

                    if (apply_trajectory && controller.apply_root_motion)
                    {
                        scene->local_matrices[n] *= trajectory;
                    }
                }
            }
        }

        void update_scene(entity_scene* scene, f32 dt)
        {
            if (scene->flags & PAUSE_UPDATE)
            {
                physics::set_paused(1);
            }
            else
            {
                physics::set_paused(0);
                update_animations(scene, dt);
            }

            // update physics
            physics::physics_consume_command_buffer();

            // scene node transform
            for (u32 n = 0; n < scene->num_nodes; ++n)
            {
                // controlled transform
                if (scene->entities[n] & CMP_TRANSFORM)
                {
                    cmp_transform& t = scene->transforms[n];

                    // generate matrix from transform
                    mat4 rot_mat;
                    t.rotation.get_matrix(rot_mat);

                    mat4 translation_mat = mat::create_translation(t.translation);

                    mat4 scale_mat = mat::create_scale(t.scale);

                    scene->local_matrices[n] = translation_mat * rot_mat * scale_mat;

                    if (scene->entities[n] & CMP_PHYSICS)
                    {
                        physics::set_transform(scene->physics_handles[n], t.translation, t.rotation);
                    }

                    // local matrix will be baked
                    scene->entities[n] &= ~CMP_TRANSFORM;
                }
                else
                {
                    if (scene->entities[n] & CMP_PHYSICS)
                    {
                        scene->local_matrices[n] = physics::get_rb_matrix(scene->physics_handles[n]);

                        scene->local_matrices[n].transposed();

                        scene->local_matrices[n] *= scene->offset_matrices[n];

                        cmp_transform& t = scene->transforms[n];

                        t.translation = scene->local_matrices[n].get_translation();
                        t.rotation.from_matrix(scene->local_matrices[n]);
                    }
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
            for (s32 n = 0; n < scene->num_nodes; ++n)
            {
                vec3f min = scene->bounding_volumes[n].min_extents;
                vec3f max = scene->bounding_volumes[n].max_extents - min;

                vec3f& tmin = scene->bounding_volumes[n].transformed_min_extents;
                vec3f& tmax = scene->bounding_volumes[n].transformed_max_extents;

                if (scene->entities[n] & CMP_BONE)
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
                trad      = mag(tmax - tmin) * 0.5f;

                if (!(scene->entities[n] & CMP_GEOMETRY))
                    continue;

                // also set scene extents
                scene->renderable_extents.min = vec3f::vmin(tmin, scene->renderable_extents.min);
                scene->renderable_extents.max = vec3f::vmax(tmax, scene->renderable_extents.max);
            }

            // reverse iterate over scene and expand parents extents by children
            for (s32 n = scene->num_nodes; n > 0; --n)
            {
                u32 p = scene->parents[n];
                if (p == n)
                    continue;

                vec3f& parent_tmin = scene->bounding_volumes[p].transformed_min_extents;
                vec3f& parent_tmax = scene->bounding_volumes[p].transformed_max_extents;

                vec3f& tmin = scene->bounding_volumes[n].transformed_min_extents;
                vec3f& tmax = scene->bounding_volumes[n].transformed_max_extents;

                if (scene->entities[p] & CMP_ANIM_CONTROLLER)
                {
                    vec3f pad   = (parent_tmax - parent_tmin) * 0.5f;
                    parent_tmin = tmin - pad;
                    parent_tmax = tmax + pad;
                }
                else
                {
                    parent_tmin = vec3f::vmin(parent_tmin, tmin);
                    parent_tmax = vec3f::vmax(parent_tmax, tmax);
                }
            }

            // todo - per draw call data is quite basic, might be a need in the future for more cutsom
            // sets of data for draw calls

            // update draw call data
            for (s32 n = 0; n < scene->num_nodes; ++n)
            {
                if (!(scene->entities[n] & CMP_MATERIAL))
                    continue;

                cmp_material& mat                     = scene->materials[n];
                scene->draw_call_data[n].world_matrix = scene->world_matrices[n];
                scene->draw_call_data[n].v1 = vec4f((f32)n, mat.diffuse_rgb_shininess.w, mat.specular_rgb_reflect.w, 0.0f);
                scene->draw_call_data[n].v2 = vec4f(mat.diffuse_rgb_shininess.xyz, 1.0f);

                if (!scene->cbuffer[n])
                    continue;

                if (scene->entities[n] & CMP_SUB_INSTANCE)
                    continue;

                // skinned meshes have the world matrix baked into the bones
                if (scene->entities[n] & CMP_SKINNED || scene->entities[n] & CMP_PRE_SKINNED)
                    scene->draw_call_data[n].world_matrix = mat4::create_identity();

                mat4 invt = scene->world_matrices[n];
                // invt.set_translation(vec3f(0.0f, 0.0f, 0.0f));
                invt = invt.transposed();
                invt = mat::inverse4x4(invt);

                scene->draw_call_data[n].world_matrix_inv_transpose = invt;

                // per node cbuffer
                pen::renderer_update_buffer(scene->cbuffer[n], &scene->draw_call_data[n], sizeof(cmp_draw_call));
            }

            // update instance buffers
            for (s32 n = 0; n < scene->num_nodes; ++n)
            {
                if (!(scene->entities[n] & CMP_MASTER_INSTANCE))
                    continue;

                cmp_master_instance& master = scene->master_instances[n];

                u32 instance_data_size = master.num_instances * master.instance_stride;
                pen::renderer_update_buffer(master.instance_buffer, &scene->draw_call_data[n + 1], instance_data_size);

                // stride over sub instances
                n += scene->master_instances[n].num_instances;
            }

            // Forward light buffer
            static forward_light_buffer light_buffer;
            s32                         pos        = 0;
            s32                         num_lights = 0;

            // directional lights
            s32 num_directions_lights = 0;
            for (s32 n = 0; n < scene->num_nodes; ++n)
            {
                if (num_lights >= MAX_FORWARD_LIGHTS)
                    break;

                if (!(scene->entities[n] & CMP_LIGHT))
                    continue;

                cmp_light& l = scene->lights[n];

                if (l.type != LIGHT_TYPE_DIR)
                    continue;

                // current directional light is a point light very far away
                // with no attenuation.. todo optimise
                vec3f light_pos                     = l.direction * 100000.0f;
                light_buffer.lights[pos].pos_radius = vec4f(light_pos, 0.0);
                light_buffer.lights[pos].colour     = vec4f(l.colour, l.shadow ? 1.0 : 0.0);

                ++num_directions_lights;
                ++num_lights;
                ++pos;
            }

            // point lights
            s32 num_point_lights = 0;
            for (s32 n = 0; n < scene->num_nodes; ++n)
            {
                if (num_lights >= MAX_FORWARD_LIGHTS)
                    break;

                if (!(scene->entities[n] & CMP_LIGHT))
                    continue;

                cmp_light& l = scene->lights[n];

                if (l.type != LIGHT_TYPE_POINT)
                    continue;

                cmp_transform& t = scene->transforms[n];

                light_buffer.lights[pos].pos_radius = vec4f(t.translation, l.radius);
                light_buffer.lights[pos].colour     = vec4f(l.colour, l.shadow ? 1.0 : 0.0);

                ++num_point_lights;
                ++num_lights;
                ++pos;
            }

            // spot lights
            s32 num_spot_lights = 0;

            // info for loops
            light_buffer.info = vec4f(num_directions_lights, num_point_lights, num_spot_lights, 0.0f);

            pen::renderer_update_buffer(scene->forward_light_buffer, &light_buffer, sizeof(light_buffer));

            // Distance field shadows
            for (s32 n = 0; n < scene->num_nodes; ++n)
            {
                if (!(scene->entities[n] & CMP_SDF_SHADOW))
                    continue;

                static distance_field_shadow_buffer sdf_buffer;

                sdf_buffer.shadows.world_matrix         = scene->world_matrices[n];
                sdf_buffer.shadows.world_matrix_inverse = mat::inverse4x4(scene->world_matrices[n]);

                pen::renderer_update_buffer(scene->sdf_shadow_buffer, &sdf_buffer, sizeof(sdf_buffer));

                pen::renderer_set_constant_buffer(scene->sdf_shadow_buffer, 5, PEN_SHADER_TYPE_PS);
            }

            // Area box lights
            for (s32 n = 0; n < scene->num_nodes; ++n)
            {
                if (!(scene->entities[n] & CMP_LIGHT))
                    continue;

                if (scene->lights[n].type != LIGHT_TYPE_AREA_BOX)
                    continue;

                static area_box_light_buffer abl_buffer;

                abl_buffer.area_lights.world_matrix         = scene->world_matrices[n];
                abl_buffer.area_lights.world_matrix_inverse = mat::inverse4x4(scene->world_matrices[n]);

                pen::renderer_update_buffer(scene->area_box_light_buffer, &abl_buffer, sizeof(abl_buffer));

                pen::renderer_set_constant_buffer(scene->area_box_light_buffer, 6, PEN_SHADER_TYPE_PS);
            }

            // Update pre skinned vertex buffers
            static hash_id             id_pre_skin_technique = PEN_HASH("pre_skin");
            static pmfx::shader_handle ph                    = pmfx::load_shader("forward_render");

            if (pmfx::set_technique(ph, id_pre_skin_technique, 0))
            {
                for (s32 n = 0; n < scene->num_nodes; ++n)
                {
                    if (!(scene->entities[n] & CMP_PRE_SKINNED))
                        continue;

                    // update bone cbuffer
                    cmp_geometry& geom = scene->geometries[n];
                    if (geom.p_skin->bone_cbuffer == PEN_INVALID_HANDLE)
                    {
                        pen::buffer_creation_params bcp;
                        bcp.usage_flags      = PEN_USAGE_DYNAMIC;
                        bcp.bind_flags       = PEN_BIND_CONSTANT_BUFFER;
                        bcp.cpu_access_flags = PEN_CPU_ACCESS_WRITE;
                        bcp.buffer_size      = sizeof(mat4) * 85;
                        bcp.data             = nullptr;

                        geom.p_skin->bone_cbuffer = pen::renderer_create_buffer(bcp);
                    }

                    static mat4 bb[85];
                    s32         joints_offset = scene->anim_controller[n].joints_offset;
                    for (s32 i = 0; i < geom.p_skin->num_joints; ++i)
                        bb[i] = scene->world_matrices[n + joints_offset + i] * geom.p_skin->joint_bind_matrices[i];

                    pen::renderer_update_buffer(geom.p_skin->bone_cbuffer, bb, sizeof(bb));
                    pen::renderer_set_constant_buffer(geom.p_skin->bone_cbuffer, 2, PEN_SHADER_TYPE_VS);

                    // bind stream out targets
                    cmp_pre_skin& pre_skin = scene->pre_skin[n];
                    pen::renderer_set_stream_out_target(geom.vertex_buffer);

                    pen::renderer_set_vertex_buffer(pre_skin.vertex_buffer, 0, pre_skin.vertex_size, 0);

                    // render point list
                    pen::renderer_draw(pre_skin.num_verts, 0, PEN_PT_POINTLIST);

                    pen::renderer_set_stream_out_target(0);
                }
            }
        }

        void save_scene(const c8* filename, entity_scene* scene)
        {
            // todo fix down
            //-take subsets of scenes and make into a new scene, to save unity style prefabs

            std::ofstream ofs(filename, std::ofstream::binary);

            static s32 version = 3;
            ofs.write((const c8*)&version, sizeof(s32));
            ofs.write((const c8*)&scene->num_nodes, sizeof(u32));

            // user prefs
            ofs.write((const c8*)&scene->view_flags, sizeof(u32));
            ofs.write((const c8*)&scene->selected_index, sizeof(s32));

            // names
            for (s32 n = 0; n < scene->num_nodes; ++n)
            {
                write_parsable_string(scene->names[n], ofs);
                write_parsable_string(scene->geometry_names[n], ofs);
                write_parsable_string(scene->material_names[n], ofs);
            }

            // simple parts of the scene are just homogeonous chucks of data
            ofs.write((const c8*)scene->entities.data, sizeof(a_u64) * scene->num_nodes);
            ofs.write((const c8*)scene->parents.data, sizeof(u32) * scene->num_nodes);
            ofs.write((const c8*)scene->transforms.data, sizeof(cmp_transform) * scene->num_nodes);
            ofs.write((const c8*)scene->local_matrices.data, sizeof(mat4) * scene->num_nodes);
            ofs.write((const c8*)scene->world_matrices.data, sizeof(mat4) * scene->num_nodes);
            ofs.write((const c8*)scene->offset_matrices.data, sizeof(mat4) * scene->num_nodes);
            ofs.write((const c8*)scene->physics_matrices.data, sizeof(mat4) * scene->num_nodes);
            ofs.write((const c8*)scene->bounding_volumes.data, sizeof(cmp_bounding_volume) * scene->num_nodes);
            ofs.write((const c8*)scene->lights.data, sizeof(cmp_light) * scene->num_nodes);

            if (version > 1)
                ofs.write((const c8*)scene->physics_data.data, sizeof(cmp_physics) * scene->num_nodes);

            // animations need reloading from files
            for (s32 n = 0; n < scene->num_nodes; ++n)
            {
                s32 size = scene->anim_controller[n].handles.size();

                ofs.write((const c8*)&size, sizeof(s32));

                for (s32 i = 0; i < size; ++i)
                {
                    auto* anim = get_animation_resource(scene->anim_controller[n].handles[i]);

                    write_parsable_string(anim->name, ofs);
                }

                ofs.write((const c8*)&scene->anim_controller[n].joints_offset,
                          sizeof(cmp_anim_controller) - sizeof(std::vector<anim_handle>));
            }

            Str project_dir = dev_ui::get_program_preference_filename("project_dir");

            // geometry
            for (s32 n = 0; n < scene->num_nodes; ++n)
            {
                if (scene->entities[n] & CMP_GEOMETRY)
                {
                    geometry_resource* gr = get_geometry_resource(scene->id_geometry[n]);

                    // has geometry
                    u32 one = 1;
                    ofs.write((const c8*)&one, sizeof(u32));
                    ofs.write((const c8*)&gr->submesh_index, sizeof(u32));

                    Str stripped_filename = gr->filename;
                    stripped_filename     = put::str_replace_string(stripped_filename, project_dir.c_str(), "");

                    write_parsable_string(stripped_filename.c_str(), ofs);
                    write_parsable_string(gr->geometry_name, ofs);
                }
                else
                {
                    u32 zero = 0;
                    ofs.write((const c8*)&zero, sizeof(u32));
                }
            }

            // material
            for (s32 n = 0; n < scene->num_nodes; ++n)
            {
                material_resource* mr = get_material_resource(scene->id_material[n]);

                if (scene->entities[n] & CMP_MATERIAL && mr)
                {
                    // has geometry
                    u32 one = 1;
                    ofs.write((const c8*)&one, sizeof(u32));

                    Str stripped_filename = mr->filename;
                    stripped_filename     = put::str_replace_string(stripped_filename, project_dir.c_str(), "");

                    write_parsable_string(stripped_filename.c_str(), ofs);
                    write_parsable_string(mr->material_name, ofs);
                }
                else
                {
                    u32 zero = 0;
                    ofs.write((const c8*)&zero, sizeof(u32));
                }
            }

            ofs.close();
        }

        void load_scene(const c8* filename, entity_scene* scene, bool merge)
        {
            scene->flags |= INVALIDATE_SCENE_TREE;
            bool error       = false;
            Str  project_dir = dev_ui::get_program_preference_filename("project_dir");

            std::ifstream ifs(filename, std::ofstream::binary);

            s32 version;
            s32 num_nodes = 0;
            ifs.read((c8*)&version, sizeof(s32));
            ifs.read((c8*)&num_nodes, sizeof(u32));

            u32 zero_offset   = 0;
            s32 new_num_nodes = num_nodes;

            if (merge)
            {
                zero_offset   = scene->num_nodes;
                new_num_nodes = scene->num_nodes + num_nodes;
            }
            else
            {
                clear_scene(scene);
            }

            if (new_num_nodes > scene->nodes_size)
                resize_scene_buffers(scene, num_nodes);

            scene->num_nodes = new_num_nodes;

            // user prefs
            u32 scene_view_flags = 0;
            ifs.read((c8*)&scene_view_flags, sizeof(u32));
            ifs.read((c8*)&scene->selected_index, sizeof(s32));

            // names
            for (s32 n = zero_offset; n < zero_offset + num_nodes; ++n)
            {
                scene->names[n]          = read_parsable_string(ifs);
                scene->geometry_names[n] = read_parsable_string(ifs);
                scene->material_names[n] = read_parsable_string(ifs);

                // generate hashes
                scene->id_name[n]     = PEN_HASH(scene->names[n].c_str());
                scene->id_geometry[n] = PEN_HASH(scene->geometry_names[n].c_str());
                scene->id_material[n] = PEN_HASH(scene->material_names[n].c_str());
            }

            // data
            ifs.read((c8*)&scene->entities[zero_offset], sizeof(a_u64) * num_nodes);
            ifs.read((c8*)&scene->parents[zero_offset], sizeof(u32) * num_nodes);
            ifs.read((c8*)&scene->transforms[zero_offset], sizeof(cmp_transform) * num_nodes);
            ifs.read((c8*)&scene->local_matrices[zero_offset], sizeof(mat4) * num_nodes);
            ifs.read((c8*)&scene->world_matrices[zero_offset], sizeof(mat4) * num_nodes);
            ifs.read((c8*)&scene->offset_matrices[zero_offset], sizeof(mat4) * num_nodes);
            ifs.read((c8*)&scene->physics_matrices[zero_offset], sizeof(mat4) * num_nodes);
            ifs.read((c8*)&scene->bounding_volumes[zero_offset], sizeof(cmp_bounding_volume) * num_nodes);
            ifs.read((c8*)&scene->lights[zero_offset], sizeof(cmp_light) * num_nodes);

            if (version > 1)
            {
                // version 2 adds physics
                ifs.read((c8*)&scene->physics_data[zero_offset], sizeof(cmp_physics) * num_nodes);

                for (s32 n = zero_offset; n < zero_offset + num_nodes; ++n)
                    if (scene->entities[n] & CMP_PHYSICS)
                        instantiate_rigid_body(scene, n);

                // version 3 adds constraints
                if (version > 2)
                {
                    for (s32 n = zero_offset; n < zero_offset + num_nodes; ++n)
                        if (scene->entities[n] & CMP_CONSTRAINT)
                            instantiate_constraint(scene, n);
                }
            }

            // fixup parents for scene import / merge
            for (s32 n = zero_offset; n < zero_offset + num_nodes; ++n)
                scene->parents[n] += zero_offset;

            // animations
            for (s32 n = zero_offset; n < zero_offset + num_nodes; ++n)
            {
                s32 size;
                ifs.read((c8*)&size, sizeof(s32));

                for (s32 i = 0; i < size; ++i)
                {
                    Str anim_name = project_dir;
                    anim_name.append(read_parsable_string(ifs).c_str());

                    anim_handle h = load_pma(anim_name.c_str());

                    if (!is_valid(h))
                    {
                        dev_ui::log_level(dev_ui::CONSOLE_ERROR, "[error] animation - cannot find pma file: %s",
                                          anim_name.c_str());
                        error = true;
                    }

                    scene->anim_controller[n].handles.push_back(h);
                }

                ifs.read((c8*)&scene->anim_controller[n].joints_offset,
                         sizeof(cmp_anim_controller) - sizeof(std::vector<anim_handle>));

                if (scene->anim_controller[n].current_animation > scene->anim_controller[n].handles.size())
                    scene->anim_controller[n].current_animation = PEN_INVALID_HANDLE;
            }

            // geometry
            for (s32 n = zero_offset; n < zero_offset + num_nodes; ++n)
            {
                u32 has = 0;
                ifs.read((c8*)&has, sizeof(u32));

                if (scene->entities[n] & CMP_GEOMETRY && has)
                {
                    u32 submesh;
                    ifs.read((c8*)&submesh, sizeof(u32));

                    Str            filename     = project_dir;
                    Str            name         = read_parsable_string(ifs).c_str();
                    hash_id        name_hash    = PEN_HASH(name.c_str());
                    static hash_id primitive_id = PEN_HASH("primitive");

                    filename.append(name.c_str());

                    Str geometry_name = read_parsable_string(ifs);

                    geometry_resource* gr = nullptr;

                    if (name_hash != primitive_id)
                    {
                        load_pmm(filename.c_str(), nullptr, PMM_GEOMETRY);

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
                            instantiate_anim_controller(scene, n);
                    }
                    else
                    {
                        dev_ui::log_level(dev_ui::CONSOLE_ERROR, "[error] geometry - cannot find pmm file: %s",
                                          filename.c_str());
                        scene->entities[n] &= ~CMP_GEOMETRY;
                        error = true;
                    }
                }
            }

            // materials
            for (s32 n = zero_offset; n < zero_offset + num_nodes; ++n)
            {
                u32 has = 0;
                ifs.read((c8*)&has, sizeof(u32));

                if (scene->entities[n] & CMP_MATERIAL && has)
                {
                    Str     filename  = project_dir;
                    Str     name      = read_parsable_string(ifs).c_str();
                    hash_id name_hash = PEN_HASH(name.c_str());
                    filename.append(name.c_str());

                    static hash_id default_id = PEN_HASH("default_material");

                    Str material_name = read_parsable_string(ifs);

                    material_resource* mr;

                    hash_id material_hash;

                    if (name_hash != default_id)
                    {
                        load_pmm(filename.c_str(), nullptr, PMM_MATERIAL);

                        pen::hash_murmur hm;
                        hm.begin();
                        hm.add(filename.c_str(), filename.length());
                        hm.add(material_name.c_str(), material_name.length());
                        material_hash = hm.end();

                        mr = get_material_resource(material_hash);
                    }
                    else
                    {
                        static hash_id default_material_hash = PEN_HASH("default_material");
                        material_hash                        = default_material_hash;

                        mr = get_material_resource(default_material_hash);
                    }

                    if (mr)
                    {
                        scene->material_names[n] = material_name;
                        scene->id_material[n]    = material_hash;

                        instantiate_material(mr, scene, n);
                    }
                    else
                    {
                        dev_ui::log_level(dev_ui::CONSOLE_ERROR, "[error] material - cannot find pmm file: %s",
                                          filename.c_str());
                        scene->entities[n] &= ~CMP_MATERIAL;
                        error = true;
                    }
                }
            }

            if (!merge)
            {
                scene->view_flags = scene_view_flags;
                update_view_flags(scene, error);
            }

            ifs.close();

            initialise_free_list(scene);
        }
    } // namespace ces
} // namespace put
