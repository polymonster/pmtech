#include <fstream>
#include <functional>

#include "data_struct.h"
#include "debug_render.h"
#include "dev_ui.h"
#include "file_system.h"
#include "hash.h"
#include "pmfx.h"
#include "str/Str.h"
#include "str_utilities.h"
#include "console.h"
#include "timer.h"

#include "ces/ces_resources.h"
#include "ces/ces_scene.h"
#include "ces/ces_utilities.h"

using namespace put;

extern pen::user_info pen_user_info;

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
                    l->next = scene->free_list_head;

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
                generic_cmp_array& cmp = scene->get_component_array(i);
                u32                alloc_size = cmp.size * new_size;

                if (cmp.data)
                {
                    // realloc
                    cmp.data = pen::memory_realloc(cmp.data, alloc_size);

                    // zero new mem
                    u32 prev_size = scene->nodes_size * cmp.size;
                    u8* new_offset = (u8*)cmp.data + prev_size;
                    u32 zero_size = alloc_size - prev_size;
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
            scene->num_nodes = 0;
        }

        void zero_entity_components(entity_scene* scene, u32 node_index)
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

        void delete_entity(entity_scene* scene, u32 node_index)
        {
            // free allocated stuff
            if (is_valid(scene->physics_handles[node_index]))
                physics::release_entity(scene->physics_handles[node_index]);

            if (is_valid(scene->cbuffer[node_index]))
                pen::renderer_release_buffer(scene->cbuffer[node_index]);

            // zero
            zero_entity_components(scene, node_index);
        }

        void delete_entity_first_pass(entity_scene* scene, u32 node_index)
        {
            // constraints must be freed or removed before we delete rigidbodies using them
            if (is_valid(scene->physics_handles[node_index]) && (scene->entities[node_index] & CMP_CONSTRAINT))
                physics::release_entity(scene->physics_handles[node_index]);

            if (is_valid(scene->cbuffer[node_index]))
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

            if (flags == CLONE_INSTANTIATE)
            {
                // todo, clone / instantiate constraint

                if (p_sn->physics_handles[src])
                    instantiate_rigid_body(scene, dst);

                if (p_sn->entities[dst] & CMP_GEOMETRY)
                    instantiate_model_cbuffer(scene, dst);

                if (p_sn->entities[dst] & CMP_MATERIAL)
                {
                    p_sn->materials[dst].material_cbuffer = PEN_INVALID_HANDLE;
                    instantiate_material_cbuffer(scene, dst, p_sn->materials[dst].material_cbuffer_size);
                }
            }
            else if (flags == CLONE_MOVE)
            {
                p_sn->cbuffer[dst] = p_sn->cbuffer[src];
                p_sn->physics_handles[dst] = p_sn->physics_handles[src];

                zero_entity_components(scene, src);
            }

            return dst;
        }

        entity_scene* create_scene(const c8* name)
        {
            entity_scene_instance new_instance;
            new_instance.name = name;
            new_instance.scene = new entity_scene();

            k_scenes.push_back(new_instance);

            resize_scene_buffers(new_instance.scene, 64);

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

            // area lights
            bcp.usage_flags = PEN_USAGE_DYNAMIC;
            bcp.bind_flags = PEN_BIND_CONSTANT_BUFFER;
            bcp.cpu_access_flags = PEN_CPU_ACCESS_WRITE;
            bcp.buffer_size = sizeof(area_box_light_buffer);
            bcp.data = nullptr;

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

        void render_light_volumes(const scene_view& view)
        {
            entity_scene* scene = view.scene;

            if (scene->view_flags & SV_HIDE)
                return;

            pen::renderer_set_constant_buffer(view.cb_view, 0, pen::CBUFFER_BIND_PS | pen::CBUFFER_BIND_VS);

            static hash_id id_volume[] = {PEN_HASH("full_screen_quad"), PEN_HASH("sphere"), PEN_HASH("cone")};

            static hash_id id_technique[] = {PEN_HASH("directional_light"), PEN_HASH("point_light"), PEN_HASH("spot_light")};

            static u32 shader = pmfx::load_shader("deferred_render");

            geometry_resource* volume[PEN_ARRAY_SIZE(id_volume)];
            for (u32 i = 0; i < PEN_ARRAY_SIZE(id_volume); ++i)
                volume[i] = get_geometry_resource(id_volume[i]);

            static hash_id id_cull_front = PEN_HASH("front_face_cull");
            u32            cull_front = pmfx::get_render_state(id_cull_front, pmfx::RS_SAMPLER);

            static hash_id id_disable_depth = PEN_HASH("disabled");
            u32            depth_disabled = pmfx::get_render_state(id_disable_depth, pmfx::RS_DEPTH_STENCIL);

            for (u32 n = 0; n < scene->num_nodes; ++n)
            {
                if (!(scene->entities[n] & CMP_LIGHT))
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
                    case LIGHT_TYPE_DIR:
                        ld.pos_radius = vec4f(scene->lights[n].direction * 10000.0f, 0.0f);
                        ld.dir_cutoff = vec4f(scene->lights[n].direction, 0.0f);
                        ld.colour = vec4f(scene->lights[n].colour, 0.0f);
                        break;
                    case LIGHT_TYPE_POINT:
                        ld.pos_radius = vec4f(pos, scene->lights[n].radius);
                        ld.dir_cutoff = vec4f(scene->lights[n].direction, 0.0f);
                        ld.colour = vec4f(scene->lights[n].colour, 0.0f);

                        if (maths::point_inside_sphere(pos, scene->lights[n].radius, view.camera->pos))
                            inside_volume = true;

                        break;
                    case LIGHT_TYPE_SPOT:
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
            entity_scene* scene = view.scene;

            if (scene->view_flags & SV_HIDE)
                return;

            pen::renderer_set_constant_buffer(view.cb_view, 0, pen::CBUFFER_BIND_PS | pen::CBUFFER_BIND_VS);

            // fwd lights
            if (view.render_flags & RENDER_FORWARD_LIT)
                pen::renderer_set_constant_buffer(scene->forward_light_buffer, 3, pen::CBUFFER_BIND_PS);

            // sdf shadows
            for (u32 n = 0; n < scene->num_nodes; ++n)
            {
                pen::renderer_set_constant_buffer(scene->sdf_shadow_buffer, 5, pen::CBUFFER_BIND_PS);

                if (scene->entities[n] & CMP_SDF_SHADOW)
                {
                    cmp_shadow& shadow = scene->shadows[n];

                    if (is_valid(shadow.texture_handle))
                        pen::renderer_set_texture(shadow.texture_handle, shadow.sampler_state, SDF_SHADOW_UNIT,
                                                  pen::TEXTURE_BIND_PS);
                }
            }

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

                // update skin
                if (scene->entities[n] & CMP_SKINNED && !(scene->entities[n] & CMP_SUB_GEOMETRY))
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

                    s32 joints_offset = scene->anim_controller[n].joints_offset;
                    for (s32 i = 0; i < p_geom->p_skin->num_joints; ++i)
                    {
                        bb[i] = scene->world_matrices[joints_offset + i] * p_geom->p_skin->joint_bind_matrices[i];
                    }

                    pen::renderer_update_buffer(p_geom->p_skin->bone_cbuffer, bb, sizeof(bb));
                    pen::renderer_set_constant_buffer(p_geom->p_skin->bone_cbuffer, 2, pen::CBUFFER_BIND_VS);
                }

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
                        if (scene->entities[n] & CMP_MASTER_INSTANCE)
                        {
                            u32 num_instances = scene->master_instances[n].num_instances;
                            n += num_instances;
                        }
                        PEN_ASSERT(0);
                        continue;
                    }
                }

                // set material cbs
                u32 mcb = scene->materials[n].material_cbuffer;
                if (is_valid(mcb))
                {
                    pen::renderer_set_constant_buffer(mcb, 7, pen::CBUFFER_BIND_PS | pen::CBUFFER_BIND_VS);
                }

                pen::renderer_set_constant_buffer(scene->cbuffer[n], 1, pen::CBUFFER_BIND_PS | pen::CBUFFER_BIND_VS);

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
                    cmp_samplers& samplers = scene->samplers[n];
                    for (u32 s = 0; s < MAX_TECHNIQUE_SAMPLER_BINDINGS; ++s)
                    {
                        if (!samplers.sb[s].handle)
                            continue;

                        pen::renderer_set_texture(samplers.sb[s].handle, samplers.sb[s].sampler_state,
                                                  samplers.sb[s].sampler_unit, pen::TEXTURE_BIND_PS);
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
            //u32 timer = pen::timer_create("anim_v2");
            //pen::timer_start(timer);
            
            for (u32 n = 0; n < scene->num_nodes; ++n)
            {
                if (!(scene->entities[n] & CMP_ANIM_CONTROLLER))
                    continue;
                
                cmp_anim_controller_v2 controller = scene->anim_controller_v2[n];
                
                u32 num_anims = sb_count(controller.anim_instances);
                for (u32 ai = 0; ai < num_anims; ++ai)
                {
                    anim_instance& instance = controller.anim_instances[ai];

                    soa_anim& soa = instance.soa;
                    u32       num_channels = soa.num_channels;
                    f32       anim_t = instance.time;
                    
                    // roll on time
                    instance.time += dt * 0.001f;
                    if(instance.time > instance.length)
                        instance.time = 0.0f;
                    
                    u32 num_joints = sb_count(instance.joints);
                    
                    // reset rotations
                    for (u32 j = 0; j < num_joints; ++j)
                        instance.targets[j].q = quat(0.0f, 0.0f, 0.0f);

                    for (s32 c = 0; c < num_channels; ++c)
                    {
                        anim_sampler& sampler = instance.samplers[c];
                        anim_channel& channel = soa.channels[c];
                        
                        if(sampler.joint == PEN_INVALID_HANDLE)
                            continue;
                        
                        // find the frame we are on..
                        for (; sampler.pos < channel.num_frames; ++sampler.pos)
                            if (anim_t < soa.info[sampler.pos][c].time)
                                break;
                        
                        if(sampler.pos >= channel.num_frames || anim_t == 0.0f )
                            sampler.pos = 0;
                        
                        u32 next = (sampler.pos + 1) % channel.num_frames;
                        
                        // get anim data
                        anim_info& info1 = soa.info[sampler.pos][c];
                        anim_info& info2 = soa.info[next][c];
                        
                        f32* d1 = &soa.data[sampler.pos][info1.offset];
                        f32* d2 = &soa.data[next][info2.offset];
                        
                        f32 it = min(max((anim_t - info1.time) / (info2.time - info1.time), 0.0f), 1.0f);
                        
                        for (u32 e = 0; e < channel.element_count; ++e)
                        {
                            u32 eo = channel.element_offset[e];
                            
                            // slerp quats
                            if(eo == A_OUT_QUAT)
                            {
                                quat q1;
                                quat q2;
                                
                                memcpy(&q1.v[0], &d1[e], 16);
                                memcpy(&q2.v[0], &d2[e], 16);
                                
                                quat ql = slerp(q1, q2, it);
                                
                                instance.targets[sampler.joint].q = ql * instance.targets[sampler.joint].q;
                                instance.targets[sampler.joint].flags |= channel.flags;
                                
                                e+=3;
                            }
                            else
                            {
                                // lerp translation / scale
                                if(sampler.joint > 0)
                                {
                                    f32 lf = (1 - it) * d1[e] + it * d2[e];
                                    instance.targets[sampler.joint].t[eo] = lf;
                                }
                                else
                                {
                                    instance.targets[sampler.joint].t[eo] = d1[e];
                                }
                            }
                        }
                    }
                    
                    // bake anim target into a cmp transform for joint
                    u32 tj = PEN_INVALID_HANDLE;
                    for (u32 j = 0; j < num_joints; ++j)
                    {
                        u32 jnode = controller.joint_indices[j];
                        
                        if(scene->entities[jnode] & CMP_ANIM_TRAJECTORY)
                        {
                            tj = j;
                            continue;
                        }
                        
                        f32* f = &instance.targets[j].t[0];
                        
                        instance.joints[j].translation = vec3f(f[A_OUT_TX], f[A_OUT_TY], f[A_OUT_TZ]);
                        instance.joints[j].scale = vec3f(f[A_OUT_SX], f[A_OUT_SY], f[A_OUT_SZ]);
                        
                        if(instance.targets[j].flags & anim_flags::BAKED_QUATERNION)
                            instance.joints[j].rotation = instance.targets[j].q;
                        else
                            instance.joints[j].rotation = scene->initial_transform[jnode].rotation * instance.targets[j].q;
                    }
                    
                    // root motion.. todo rotation
                    if(tj != PEN_INVALID_HANDLE)
                    {
                        f32* f = &instance.targets[tj].t[0];
                        vec3f tt = vec3f(f[0], f[1], f[2]);
                        
                        if(instance.samplers[0].pos > 0)
                        {
                            instance.root_delta = tt - instance.root_translation;
                            instance.root_translation = tt;
                        }
                        else
                        {
                            instance.root_delta = vec3f::zero();
                            instance.root_translation = tt;
                        }
                    }
                }
                
                // for active controller.anim_instances, make trans, quat, scale
                //      blend tree
                if(num_anims > 0)
                {
                    anim_instance& a = controller.anim_instances[controller.blend.anim_a];
                    anim_instance& b = controller.anim_instances[controller.blend.anim_b];
                    f32 t = controller.blend.ratio;
                    
                    u32 num_joints = sb_count(a.joints);
                    for (u32 j = 0; j < num_joints; ++j)
                    {
                        u32 jnode = controller.joint_indices[j];
                        
                        cmp_transform& tc = scene->transforms[jnode];
                        cmp_transform& ta = a.joints[j];
                        cmp_transform& tb = b.joints[j];
                        
                        if(scene->entities[jnode] & CMP_ANIM_TRAJECTORY)
                        {
                            vec3f lerp_delta = lerp(a.root_delta, b.root_delta, t);
                            
                            mat4 rot_mat;
                            quat q = scene->initial_transform[jnode].rotation;
                            q.get_matrix(rot_mat);
                            
                            vec3f transform_translation = rot_mat.transform_vector(lerp_delta);
                            
                            // apply root motion to the root controller, so we bring along the meshes
                            scene->transforms[n].rotation = q;
                            scene->transforms[n].translation += transform_translation;
                            scene->entities[n] |= CMP_TRANSFORM;
                            
                            continue;
                        }
                        
                        tc.translation = lerp(ta.translation, tb.translation, t);
                        tc.rotation = slerp(ta.rotation, tb.rotation, t);
                        tc.scale = lerp(ta.scale, tb.scale, t);
                        
                        scene->entities[jnode] |= CMP_TRANSFORM;
                    }
                }
            }
            
            //f32 ms = pen::timer_elapsed_ms(timer);
            //PEN_LOG("anim_v2 : %f", ms);
        }

        void update_animations_baked(entity_scene* scene, f32 dt)
        {
            for (u32 n = 0; n < scene->num_nodes; ++n)
            {
                if (!(scene->entities[n] & CMP_ANIM_CONTROLLER))
                    continue;
                
                auto& controller = scene->anim_controller[n];
                if (!is_valid(controller.current_animation))
                    continue;
                
                auto* anim = get_animation_resource(controller.current_animation);
                if (!anim)
                    continue;
                
                bool apply_trajectory = false;
                mat4 trajectory = mat4::create_identity();

                if (controller.play_flags == cmp_anim_controller::PLAY)
                    controller.current_time += dt * 0.001f;
                
                s32       joints_offset = scene->anim_controller[n].joints_offset;
                cmp_skin* skin = nullptr;
                if (scene->geometries[n].p_skin)
                    skin = scene->geometries[n].p_skin;
                
                if (controller.current_frame > 0)
                    continue;
                
                for (s32 c = 0; c < anim->num_channels; ++c)
                {
                    s32 num_frames = anim->channels[c].num_frames;
                    
                    if (num_frames <= 0)
                        continue;
                    
                    s32 t = 0;
                    for (t = 0; t < num_frames; ++t)
                        if (controller.current_time < anim->channels[c].times[t])
                            break;
                    
                    bool new_frame = false;
                    if(anim->channels[c].processed_frame != t)
                    {
                        new_frame = true;
                        anim->channels[c].processed_frame = t;
                    }
                    
                    // loop
                    if (t >= num_frames)
                        t = 0;
                    
                    // anim channel to scene node
                    s32 sni = joints_offset + c;
                    
                    if (anim->remap_channels)
                        sni = anim->channels[c].target_node_index;
                    
                    // invalid
                    if (sni < 0)
                        continue;
                    
                    //
                    if (scene->entities[sni] & CMP_ANIM_TRAJECTORY)
                    {
                        if (anim->channels[c].matrices)
                        {
                            trajectory = anim->channels[c].matrices[num_frames - 1];
                        }
                    }
                    
                    if (anim->channels[c].matrices)
                    {
                        // apply baked tansform anim
                        mat4& mat = anim->channels[c].matrices[t];
                        scene->local_matrices[sni] = mat;
                    }
                    
                    if (controller.current_time > anim->length)
                    {
                        apply_trajectory = true;
                        controller.current_time = (controller.current_time) - (anim->length);
                    }
                }
                
                if (apply_trajectory && controller.apply_root_motion)
                {
                    scene->local_matrices[n] *= trajectory;
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
            }

            static u32 timer = pen::timer_create("update_scene");
            pen::timer_start(timer);
            
            // scene node transform
            for (u32 n = 0; n < scene->num_nodes; ++n)
            {
                // force physics entity to sync and ignore controlled transform
                if (scene->state_flags[n] & SF_SYNC_PHYSICS_TRANSFORM)
                {
                    scene->state_flags[n] &= ~SF_SYNC_PHYSICS_TRANSFORM;
                    scene->entities[n] &= ~CMP_TRANSFORM;
                }

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
                        cmp_transform& pt = scene->physics_offset[n];
                        physics::set_transform(scene->physics_handles[n], t.translation + pt.translation, t.rotation );
                        
                        physics::set_v3(scene->physics_handles[n], vec3f::zero(), physics::CMD_SET_ANGULAR_VELOCITY);
                        physics::set_v3(scene->physics_handles[n], vec3f::zero(), physics::CMD_SET_LINEAR_VELOCITY);
                    }

                    // local matrix will be baked
                    scene->entities[n] &= ~CMP_TRANSFORM;
                }
                else if (scene->entities[n] & CMP_PHYSICS)
                {
                    cmp_transform& t = scene->transforms[n];
                    cmp_transform& pt = scene->physics_offset[n];

                    mat4 physics_mat = physics::get_rb_matrix(scene->physics_handles[n]);
                    mat4 scale_mat = mat::create_scale(t.scale);

                    t.translation = physics_mat.get_translation();
                    t.rotation.from_matrix(physics_mat);
                    
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
                trad = mag(tmax - tmin) * 0.5f;

                if (!(scene->entities[n] & CMP_GEOMETRY))
                    continue;

                // also set scene extents
                scene->renderable_extents.min = vec3f::vmin(tmin, scene->renderable_extents.min);
                scene->renderable_extents.max = vec3f::vmax(tmax, scene->renderable_extents.max);
            }

            // reverse iterate over scene and expand parents extents by children
            for (s32 n = scene->num_nodes - 1; n > 0; --n)
            {
                if (!(scene->entities[n] & CMP_ALLOCATED))
                    continue;

                u32 p = scene->parents[n];
                if (p == n)
                    continue;

                vec3f& parent_tmin = scene->bounding_volumes[p].transformed_min_extents;
                vec3f& parent_tmax = scene->bounding_volumes[p].transformed_max_extents;

                vec3f& tmin = scene->bounding_volumes[n].transformed_min_extents;
                vec3f& tmax = scene->bounding_volumes[n].transformed_max_extents;

                if (scene->entities[p] & CMP_ANIM_CONTROLLER)
                {
                    //vec3f pad = (parent_tmax - parent_tmin) * 0.5f;
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
            
            // update draw call data
            for (s32 n = 0; n < scene->num_nodes; ++n)
            {
                if (scene->entities[n] & CMP_MATERIAL)
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

                if (scene->entities[n] & CMP_SUB_INSTANCE)
                    continue;

                // skinned meshes have the world matrix baked into the bones
                if (scene->entities[n] & CMP_SKINNED || scene->entities[n] & CMP_PRE_SKINNED)
                    scene->draw_call_data[n].world_matrix = mat4::create_identity();

                mat4 invt = scene->world_matrices[n];

                invt = invt.transposed();
                invt = mat::inverse4x4(invt);

                scene->draw_call_data[n].world_matrix_inv_transpose = invt;

                // todo mark dirty?
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
            s32                         pos = 0;
            s32                         num_lights = 0;

            memset(&light_buffer, 0x0, sizeof(forward_light_buffer));

            // directional lights
            s32 num_directions_lights = 0;
            for (s32 n = 0; n < scene->num_nodes; ++n)
            {
                if (!(scene->entities[n] & CMP_LIGHT))
                    continue;

                cmp_light& l = scene->lights[n];
                if (l.type != LIGHT_TYPE_DIR)
                    continue;

                // update bv and transform
                scene->bounding_volumes[n].min_extents = -vec3f(FLT_MAX);
                scene->bounding_volumes[n].max_extents = vec3f(FLT_MAX);

                if (num_lights >= MAX_FORWARD_LIGHTS)
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
            for (s32 n = 0; n < scene->num_nodes; ++n)
            {
                if (!(scene->entities[n] & CMP_LIGHT))
                    continue;

                cmp_light& l = scene->lights[n];
                if (l.type != LIGHT_TYPE_POINT)
                    continue;

                // update bv and transform
                scene->bounding_volumes[n].min_extents = -vec3f::one();
                scene->bounding_volumes[n].max_extents = vec3f::one();

                f32 rad = std::max<f32>(l.radius, 1.0f) * 2.0f;
                scene->transforms[n].scale = vec3f(rad, rad, rad);
                scene->entities[n] |= CMP_TRANSFORM;

                if (num_lights >= MAX_FORWARD_LIGHTS)
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
            for (s32 n = 0; n < scene->num_nodes; ++n)
            {
                if (num_lights >= MAX_FORWARD_LIGHTS)
                    break;

                if (!(scene->entities[n] & CMP_LIGHT))
                    continue;

                cmp_light& l = scene->lights[n];

                if (l.type != LIGHT_TYPE_SPOT)
                    continue;

                // update bv and transform
                scene->bounding_volumes[n].min_extents = -vec3f::one();
                scene->bounding_volumes[n].max_extents = vec3f(1.0f, 0.0f, 1.0f);

                f32 angle = acos(1.0f - l.cos_cutoff);
                f32 lo = tan(angle);
                f32 range = l.radius;

                scene->transforms[n].scale = vec3f(lo * range, range, lo * range);
                scene->entities[n] |= CMP_TRANSFORM;

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

            // Distance field shadows
            for (s32 n = 0; n < scene->num_nodes; ++n)
            {
                if (!(scene->entities[n] & CMP_SDF_SHADOW))
                    continue;

                static distance_field_shadow_buffer sdf_buffer;

                sdf_buffer.shadows.world_matrix = scene->world_matrices[n];
                sdf_buffer.shadows.world_matrix_inverse = mat::inverse4x4(scene->world_matrices[n]);

                pen::renderer_update_buffer(scene->sdf_shadow_buffer, &sdf_buffer, sizeof(sdf_buffer));

                pen::renderer_set_constant_buffer(scene->sdf_shadow_buffer, 5, pen::CBUFFER_BIND_PS);
            }

            // Area box lights
            for (s32 n = 0; n < scene->num_nodes; ++n)
            {
                if (!(scene->entities[n] & CMP_LIGHT))
                    continue;

                if (scene->lights[n].type != LIGHT_TYPE_AREA_BOX)
                    continue;

                static area_box_light_buffer abl_buffer;

                abl_buffer.area_lights.world_matrix = scene->world_matrices[n];
                abl_buffer.area_lights.world_matrix_inverse = mat::inverse4x4(scene->world_matrices[n]);

                pen::renderer_update_buffer(scene->area_box_light_buffer, &abl_buffer, sizeof(abl_buffer));

                pen::renderer_set_constant_buffer(scene->area_box_light_buffer, 6, pen::CBUFFER_BIND_PS);
            }

            // Shadow maps
            for (s32 n = 0; n < scene->num_nodes; ++n)
            {
                if (!(scene->entities[n] & CMP_LIGHT))
                    continue;
            }

            // Update pre skinned vertex buffers
            static hash_id id_pre_skin_technique = PEN_HASH("pre_skin");
            static u32     shader = pmfx::load_shader("forward_render");

            if (pmfx::set_technique_perm(shader, id_pre_skin_technique))
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
                        bcp.usage_flags = PEN_USAGE_DYNAMIC;
                        bcp.bind_flags = PEN_BIND_CONSTANT_BUFFER;
                        bcp.cpu_access_flags = PEN_CPU_ACCESS_WRITE;
                        bcp.buffer_size = sizeof(mat4) * 85;
                        bcp.data = nullptr;

                        geom.p_skin->bone_cbuffer = pen::renderer_create_buffer(bcp);
                    }

                    static mat4 bb[85];
                    s32         joints_offset = scene->anim_controller[n].joints_offset;
                    for (s32 i = 0; i < geom.p_skin->num_joints; ++i)
                        bb[i] = scene->world_matrices[joints_offset + i] * geom.p_skin->joint_bind_matrices[i];

                    pen::renderer_update_buffer(geom.p_skin->bone_cbuffer, bb, sizeof(bb));
                    pen::renderer_set_constant_buffer(geom.p_skin->bone_cbuffer, 2, pen::CBUFFER_BIND_VS);

                    // bind stream out targets
                    cmp_pre_skin& pre_skin = scene->pre_skin[n];
                    pen::renderer_set_stream_out_target(geom.vertex_buffer);

                    pen::renderer_set_vertex_buffer(pre_skin.vertex_buffer, 0, pre_skin.vertex_size, 0);

                    // render point list
                    pen::renderer_draw(pre_skin.num_verts, 0, PEN_PT_POINTLIST);

                    pen::renderer_set_stream_out_target(0);
                }
            }

            // update physics running 1 frame behind to allow the sets to take effect
            physics::physics_consume_command_buffer();

            // anims work better here for now to allow user response for collision at the start of the next frame
            update_animations(scene, dt);

            //f32 cost = pen::timer_elapsed_ms(timer);
        }

        struct scene_header
        {
            s32 header_size = sizeof(*this);
            s32 version = entity_scene::k_version;
            u32 num_nodes = 0;
            s32 num_components = 0;
            s32 num_lookup_strings = 0;
            s32 reserved_1[27] = {0};
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

        void save_scene(const c8* filename, entity_scene* scene)
        {
            Str project_dir = dev_ui::get_program_preference_filename("project_dir", pen_user_info.working_directory);

            std::ofstream ofs(filename, std::ofstream::binary);

            sb_free(s_lookup_strings);
            s_lookup_strings = nullptr;

            // write basic components
            for (u32 i = 0; i < scene->num_components; ++i)
            {
                generic_cmp_array& cmp = scene->get_component_array(i);
                ofs.write((const c8*)cmp.data, cmp.size * scene->num_nodes);
            }

            // specialisations ------------------------------------------------------------------------------

            // names
            for (s32 n = 0; n < scene->num_nodes; ++n)
            {
                write_lookup_string(scene->names[n].c_str(), ofs);
                write_lookup_string(scene->geometry_names[n].c_str(), ofs);
                write_lookup_string(scene->material_names[n].c_str(), ofs);
            }

            // geometry
            for (s32 n = 0; n < scene->num_nodes; ++n)
            {
                if (!(scene->entities[n] & CMP_GEOMETRY))
                    continue;

                geometry_resource* gr = get_geometry_resource(scene->id_geometry[n]);

                ofs.write((const c8*)&gr->submesh_index, sizeof(u32));

                write_lookup_string(gr->filename.c_str(), ofs, project_dir.c_str());
                write_lookup_string(gr->geometry_name.c_str(), ofs, project_dir.c_str());
            }

            // animations
            for (s32 n = 0; n < scene->num_nodes; ++n)
            {
                s32 size = 0;

                if (scene->anim_controller[n].handles)
                    size = sb_count(scene->anim_controller[n].handles);

                ofs.write((const c8*)&size, sizeof(s32));

                for (s32 i = 0; i < size; ++i)
                {
                    auto* anim = get_animation_resource(scene->anim_controller[n].handles[i]);
                    write_lookup_string(anim->name.c_str(), ofs, project_dir.c_str());
                }
            }

            // material
            for (s32 n = 0; n < scene->num_nodes; ++n)
            {
                if (!(scene->entities[n] & CMP_MATERIAL))
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
            for (s32 n = 0; n < scene->num_nodes; ++n)
            {
                if (!(scene->entities[n] & CMP_SDF_SHADOW))
                    continue;

                cmp_shadow& shadow = scene->shadows[n];

                write_lookup_string(put::get_texture_filename(shadow.texture_handle).c_str(), ofs, project_dir.c_str());
            }

            // sampler bindings
            for (s32 n = 0; n < scene->num_nodes; ++n)
            {
                if (!(scene->entities[n] & CMP_SAMPLERS))
                    continue;

                cmp_samplers& samplers = scene->samplers[n];

                for (u32 i = 0; i < MAX_TECHNIQUE_SAMPLER_BINDINGS; ++i)
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
            sh.num_nodes = scene->num_nodes;
            sh.view_flags = scene->view_flags;
            sh.selected_index = scene->selected_index;
            sh.num_components = scene->num_components;
            sh.num_lookup_strings = sb_count(s_lookup_strings);
            ofs.write((const c8*)&sh, sizeof(scene_header));

            // component sizes
            for (u32 c = 0; c < sh.num_components; ++c)
            {
                ofs.write((const c8*)&scene->get_component_array(c).size, sizeof(u32));
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

        void load_scene(const c8* filename, entity_scene* scene, bool merge)
        {
            scene->flags |= INVALIDATE_SCENE_TREE;
            bool error = false;
            Str  project_dir = dev_ui::get_program_preference_filename("project_dir", pen_user_info.working_directory);

            std::ifstream ifs(filename, std::ofstream::binary);

            // header
            scene_header sh;
            ifs.read((c8*)&sh, sizeof(scene_header));

            if (!merge)
            {
                scene->version = sh.version;
                scene->filename = filename;
            }

            // unpack header
            s32 num_nodes = sh.num_nodes;

            scene->selected_index = sh.selected_index;
            s32 scene_view_flags = sh.view_flags;

            u32 zero_offset = 0;
            s32 new_num_nodes = num_nodes;

            if (merge)
            {
                zero_offset = scene->num_nodes;
                new_num_nodes = scene->num_nodes + num_nodes;
            }
            else
            {
                clear_scene(scene);
            }

            if (new_num_nodes > scene->nodes_size)
                resize_scene_buffers(scene, num_nodes);

            scene->num_nodes = new_num_nodes;

            // read component sizes
            u32* component_sizes = nullptr;
            for (u32 i = 0; i < sh.num_components; ++i)
            {
                u32 size;
                ifs.read((c8*)&size, sizeof(u32));
                sb_push(component_sizes, size);
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
                if (_cam)
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
                generic_cmp_array& cmp = scene->get_component_array(i);
                if (cmp.size == component_sizes[i])
                {
                    // read whole array
                    ifs.read((c8*)cmp.data, cmp.size * num_nodes);
                }
                else
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
                if (scene->entities[n] & CMP_GEOMETRY)
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

            // instantiate physics
            for (s32 n = zero_offset; n < zero_offset + num_nodes; ++n)
                if (scene->entities[n] & CMP_PHYSICS)
                    instantiate_rigid_body(scene, n);

            for (s32 n = zero_offset; n < zero_offset + num_nodes; ++n)
                if (scene->entities[n] & CMP_CONSTRAINT)
                    instantiate_constraint(scene, n);

            // animations
            for (s32 n = zero_offset; n < zero_offset + num_nodes; ++n)
            {
                s32 size;
                ifs.read((c8*)&size, sizeof(s32));
                scene->anim_controller[n].handles = nullptr;

                for (s32 i = 0; i < size; ++i)
                {
                    Str anim_name = project_dir;
                    anim_name.append(read_lookup_string(ifs).c_str());

                    anim_handle h = load_pma(anim_name.c_str());

                    if (!is_valid(h))
                    {
                        dev_ui::log_level(dev_ui::CONSOLE_ERROR, "[error] animation - cannot find pma file: %s",
                                          anim_name.c_str());
                        error = true;
                    }
                    
                    bind_animation_to_rig(scene, h, n);
                }

                if (scene->anim_controller[n].current_animation > sb_count(scene->anim_controller[n].handles))
                    scene->anim_controller[n].current_animation = PEN_INVALID_HANDLE;
            }

            // materials
            for (s32 n = zero_offset; n < zero_offset + num_nodes; ++n)
            {
                if (!(scene->entities[n] & CMP_MATERIAL))
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
                if (!(scene->entities[n] & CMP_SDF_SHADOW))
                    continue;

                Str sdf_shadow_volume_file = read_lookup_string(ifs);
                sdf_shadow_volume_file = pen::str_replace_string(sdf_shadow_volume_file, ".dds", ".pmv");

                dev_console_log("[scene load] %s", sdf_shadow_volume_file.c_str());
                instantiate_sdf_shadow(sdf_shadow_volume_file.c_str(), scene, n);
            }

            // sampler binding textures
            for (s32 n = zero_offset; n < zero_offset + num_nodes; ++n)
            {
                if (!(scene->entities[n] & CMP_SAMPLERS))
                    continue;

                cmp_samplers& samplers = scene->samplers[n];

                for (u32 i = 0; i < MAX_TECHNIQUE_SAMPLER_BINDINGS; ++i)
                {
                    Str texture_name = read_lookup_string(ifs);

                    if (!texture_name.empty())
                    {
                        samplers.sb[i].handle = put::load_texture(texture_name.c_str());
                        samplers.sb[i].sampler_state = pmfx::get_render_state(PEN_HASH("wrap_linear"), pmfx::RS_SAMPLER);
                    }

                    Str sampler_state_name = read_lookup_string(ifs);

                    if (!sampler_state_name.empty())
                    {
                        samplers.sb[i].sampler_state = pmfx::get_render_state(PEN_HASH(sampler_state_name), pmfx::RS_SAMPLER);
                    }
                }
            }

            bake_material_handles();

            // light geom
            for (s32 n = zero_offset; n < zero_offset + num_nodes; ++n)
            {
                if (!(scene->entities[n] & CMP_LIGHT))
                    continue;

                instantiate_model_cbuffer(scene, n);
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
