// ecs_resources.cpp
// Copyright 2014 - 2019 Alex Dixon.
// License: https://github.com/polymonster/pmtech/blob/master/license.md

#include "debug_render.h"
#include "dev_ui.h"
#include "file_system.h"
#include "hash.h"
#include "pen_string.h"
#include "str_utilities.h"

#include "ecs/ecs_resources.h"
#include "ecs/ecs_utilities.h"

#include "console.h"
#include "data_struct.h"

namespace put
{
    enum pmm_transform_types
    {
        PMM_TRANSLATE = 0,
        PMM_ROTATE = 1,
        PMM_MATRIX = 2,
        PMM_IDENTITY
    };

    // id hashes
    const hash_id ID_CONTROL_RIG = PEN_HASH("controlrig");
    const hash_id ID_JOINT = PEN_HASH("joint");
    const hash_id ID_TRAJECTORY = PEN_HASH("trajectoryshjnt");

    namespace ecs
    {
        static std::vector<geometry_resource*> s_geometry_resources;
        static std::vector<material_resource*> s_material_resources;

        void add_material_resource(material_resource* mr)
        {
            s_material_resources.push_back(mr);
        }

        void add_geometry_resource(geometry_resource* gr)
        {
            s_geometry_resources.push_back(gr);
        }

        geometry_resource* get_geometry_resource(hash_id hash)
        {
            for (auto* g : s_geometry_resources)
            {
                if (hash == g->hash)
                {
                    return g;
                }
            }

            return nullptr;
        }

        void instantiate_constraint(ecs_scene* scene, u32 node_index)
        {
            physics::constraint_params& cp = scene->physics_data[node_index].constraint;

            // hinge
            s32 rb = cp.rb_indices[0];
            cp.pivot = scene->transforms[node_index].translation - scene->physics_data[rb].rigid_body.position;

            scene->physics_handles[node_index] = physics::add_constraint(cp);
            scene->physics_data[node_index].type = PHYSICS_TYPE_CONSTRAINT;

            scene->entities[node_index] |= CMP_CONSTRAINT;
        }

        void instantiate_rigid_body(ecs_scene* scene, u32 node_index)
        {
            u32 s = node_index;

            physics::rigid_body_params& rb = scene->physics_data[s].rigid_body;
            cmp_transform&              pt = scene->physics_offset[s];

            vec3f min = scene->bounding_volumes[s].min_extents;
            vec3f max = scene->bounding_volumes[s].max_extents;
            vec3f scale = scene->transforms[s].scale;

            rb.position = scene->transforms[s].translation + pt.translation;
            rb.rotation = scene->transforms[s].rotation;

            if (!(rb.create_flags & physics::CF_DIMENSIONS))
            {
                rb.dimensions = (max - min) * scale * 0.5f;

                // capsule height is extents height + radius * 2 (for the capsule top and bottom)
                if (rb.shape == physics::CAPSULE)
                    rb.dimensions.y -= rb.dimensions.x / 2.0f;

                // cone height is 1. (-0.5 to 0.5) but radius is 1.0;
                if (rb.shape == physics::CONE)
                    rb.dimensions.y *= 2.0f;
            }

            // fill the matrix array with the first matrix because of thread sync
            mat4 mrot;
            rb.rotation.get_matrix(mrot);
            mat4 start_transform = mrot * mat::create_translation(rb.position);

            // mask 0 and group 0 are invalid
            if (rb.mask == 0)
                rb.mask = 0xffffffff;

            if (rb.group == 0)
                rb.group = 1;

            rb.shape_up_axis = physics::UP_Y;
            rb.start_matrix = start_transform;

            scene->physics_handles[s] = physics::add_rb(rb);
            scene->physics_data[node_index].type = PHYSICS_TYPE_RIGID_BODY;

            scene->entities[s] |= CMP_PHYSICS;
        }

        void destroy_physics(ecs_scene* scene, s32 node_index)
        {
            if (!(scene->entities[node_index] & CMP_PHYSICS))
                return;

            scene->entities[node_index] &= ~CMP_PHYSICS;

            physics::release_entity(scene->physics_handles[node_index]);
            scene->physics_handles[node_index] = PEN_INVALID_HANDLE;
        }

        void instantiate_geometry(geometry_resource* gr, ecs_scene* scene, s32 node_index)
        {
            cmp_geometry* instance = &scene->geometries[node_index];

            instance->position_buffer = gr->position_buffer;
            instance->vertex_buffer = gr->vertex_buffer;
            instance->index_buffer = gr->index_buffer;
            instance->num_indices = gr->num_indices;
            instance->num_vertices = gr->num_vertices;
            instance->index_type = gr->index_type;
            instance->vertex_size = gr->vertex_size;
            instance->p_skin = gr->p_skin;

            cmp_bounding_volume* bv = &scene->bounding_volumes[node_index];

            bv->min_extents = gr->min_extents;
            bv->max_extents = gr->max_extents;
            bv->radius = mag(bv->max_extents - bv->min_extents) * 0.5f;

            scene->geometry_names[node_index] = gr->geometry_name;
            scene->id_geometry[node_index] = gr->hash;
            scene->entities[node_index] |= CMP_GEOMETRY;

            if (gr->p_skin)
                scene->entities[node_index] |= CMP_SKINNED;

            instance->vertex_shader_class = ID_VERTEX_CLASS_BASIC;

            if (scene->entities[node_index] & CMP_SKINNED)
                instance->vertex_shader_class = ID_VERTEX_CLASS_SKINNED;
        }

        void destroy_geometry(ecs_scene* scene, u32 node_index)
        {
            if (!(scene->entities[node_index] & CMP_GEOMETRY))
                return;

            scene->entities[node_index] &= ~CMP_GEOMETRY;
            scene->entities[node_index] &= ~CMP_MATERIAL;

            // zero cmp geom
            pen::memory_zero(&scene->geometries[node_index], sizeof(cmp_geometry));

            // release cbuffer
            pen::renderer_release_buffer(scene->cbuffer[node_index]);
            scene->cbuffer[node_index] = PEN_INVALID_HANDLE;
            scene->geometry_names[node_index] = "";

            // release matrial cbuffer
            if (is_valid(scene->materials[node_index].material_cbuffer))
                pen::renderer_release_buffer(scene->materials[node_index].material_cbuffer);

            scene->materials[node_index].material_cbuffer = PEN_INVALID_HANDLE;
        }

        void instantiate_material_cbuffer(ecs_scene* scene, s32 node_index, s32 size)
        {
            if (is_valid(scene->materials[node_index].material_cbuffer))
            {
                if (size == scene->materials[node_index].material_cbuffer_size)
                    return;

                pen::renderer_release_buffer(scene->materials[node_index].material_cbuffer);
                scene->materials[node_index].material_cbuffer = PEN_INVALID_HANDLE;
            }

            if (size == 0)
                return;

            scene->materials[node_index].material_cbuffer_size = size;

            pen::buffer_creation_params bcp;
            bcp.usage_flags = PEN_USAGE_DYNAMIC;
            bcp.bind_flags = PEN_BIND_CONSTANT_BUFFER;
            bcp.cpu_access_flags = PEN_CPU_ACCESS_WRITE;
            bcp.buffer_size = scene->materials[node_index].material_cbuffer_size;
            bcp.data = nullptr;

            scene->materials[node_index].material_cbuffer = pen::renderer_create_buffer(bcp);
        }

        void instantiate_model_cbuffer(ecs_scene* scene, s32 node_index)
        {
            pen::buffer_creation_params bcp;
            bcp.usage_flags = PEN_USAGE_DYNAMIC;
            bcp.bind_flags = PEN_BIND_CONSTANT_BUFFER;
            bcp.cpu_access_flags = PEN_CPU_ACCESS_WRITE;
            bcp.buffer_size = sizeof(cmp_draw_call);
            bcp.data = nullptr;

            scene->cbuffer[node_index] = pen::renderer_create_buffer(bcp);
        }

        void instantiate_model_pre_skin(ecs_scene* scene, s32 node_index)
        {
            cmp_geometry& geom = scene->geometries[node_index];
            cmp_pre_skin& pre_skin = scene->pre_skin[node_index];

            u32 num_verts = geom.num_vertices;

            // stream out / transform feedback vertex buffer
            pen::buffer_creation_params bcp;
            bcp.usage_flags = PEN_USAGE_DEFAULT;
            bcp.bind_flags = PEN_STREAM_OUT_VERTEX_BUFFER;
            bcp.cpu_access_flags = 0;
            bcp.buffer_size = sizeof(vertex_model) * num_verts;
            bcp.data = nullptr;

            u32 vb = pen::renderer_create_buffer(bcp);
            u32 pb = 0; // todo - position only buffer is currently not used

            // swap the bufers around

            // pre_skin has skinned vertex format containing weights and indices
            pre_skin.vertex_buffer = geom.vertex_buffer;
            pre_skin.position_buffer = geom.position_buffer;
            pre_skin.vertex_size = geom.vertex_size;
            pre_skin.num_verts = geom.num_vertices;

            // geometry has the stream out target and non-skinned vertex format
            geom.vertex_buffer = vb;
            geom.position_buffer = pb;
            geom.vertex_size = sizeof(vertex_model);

            // set pre-skinned and unset skinned
            scene->entities[node_index] |= CMP_PRE_SKINNED;
            scene->entities[node_index] &= ~CMP_SKINNED;

            geom.vertex_shader_class = ID_VERTEX_CLASS_BASIC;
        }

        void instantiate_anim_controller(ecs_scene* scene, s32 node_index)
        {
            cmp_geometry* geom = &scene->geometries[node_index];

            if (geom->p_skin)
            {
                cmp_anim_controller& controller = scene->anim_controller[node_index];

                std::vector<s32> joint_indices;
                build_heirarchy_node_list(scene, node_index, joint_indices);

                for (s32 jj = 0; jj < joint_indices.size(); ++jj)
                {
                    s32 jnode = joint_indices[jj];

                    if (jnode > -1 && scene->entities[jnode] & CMP_BONE)
                    {
                        controller.joints_offset = jnode;
                        break;
                    }

                    // parent stray nodes to the top level anim / geom node
                    if (jnode > -1)
                        if (scene->parents[jnode] == jnode)
                            scene->parents[jnode] = node_index;
                }

                controller.current_time = 0.0f;
                controller.current_frame = 0;

                scene->entities[node_index] |= CMP_ANIM_CONTROLLER;
            }
        }

        void instantiate_anim_controller_v2(ecs_scene* scene, s32 node_index)
        {
            cmp_geometry* geom = &scene->geometries[node_index];

            if (geom->p_skin)
            {
                cmp_anim_controller_v2& controller = scene->anim_controller_v2[node_index];

                std::vector<s32> joint_indices;
                build_heirarchy_node_list(scene, node_index, joint_indices);

                for (s32 jj = 0; jj < joint_indices.size(); ++jj)
                {
                    s32 jnode = joint_indices[jj];

                    if (jnode > -1 && scene->entities[jnode] & CMP_BONE)
                    {
                        sb_push(controller.joint_indices, jnode);
                    }
                }

                scene->entities[node_index] |= CMP_ANIM_CONTROLLER;
            }
        }

        void instantiate_sdf_shadow(const c8* pmv_filename, ecs_scene* scene, u32 node_index)
        {
            pen::json pmv = pen::json::load_from_file(pmv_filename);

            Str volume_texture_filename = pmv["filename"].as_str();
            u32 volume_texture = put::load_texture(volume_texture_filename.c_str());

            vec3f scale = vec3f(pmv["scale_x"].as_f32(), pmv["scale_y"].as_f32(), pmv["scale_z"].as_f32());

            hash_id id_type = pmv["volume_type"].as_hash_id();

            static hash_id id_sdf = PEN_HASH("signed_distance_field");
            static hash_id id_cl = PEN_HASH("clamp_linear");
            if (id_type != id_sdf)
            {
                dev_console_log_level(dev_ui::CONSOLE_ERROR, "[shadow] %s is not a signed distance field texture",
                                      volume_texture_filename.c_str());
                return;
            }

            scene->transforms[node_index].scale = scale;
            scene->shadows[node_index].texture_handle = volume_texture;
            scene->shadows[node_index].sampler_state = pmfx::get_render_state(id_cl, pmfx::RS_SAMPLER);
        }

        void instantiate_light(ecs_scene* scene, u32 node_index)
        {
            if (is_valid(scene->cbuffer[node_index]) && scene->cbuffer[node_index] != 0)
                return;

            // cbuffer for draw call, light volume for editor / deferred etc
            scene->entities[node_index] |= CMP_LIGHT;
            instantiate_model_cbuffer(scene, node_index);

            scene->bounding_volumes[node_index].min_extents = -vec3f::one();
            scene->bounding_volumes[node_index].max_extents = vec3f::one();

            f32 rad = std::max<f32>(scene->lights[node_index].radius, 1.0f);
            scene->transforms[node_index].scale = vec3f(rad, rad, rad);
            scene->entities[node_index] |= CMP_TRANSFORM;
        }

        void load_geometry_resource(const c8* filename, const c8* geometry_name, const c8* data)
        {
            // generate hash
            pen::hash_murmur hm;
            hm.begin(0);
            hm.add(filename, pen::string_length(filename));
            hm.add(geometry_name, pen::string_length(geometry_name));
            hash_id file_hash = hm.end();

            // check for existing
            for (s32 g = 0; g < s_geometry_resources.size(); ++g)
            {
                if (file_hash == s_geometry_resources[g]->file_hash)
                {
                    return;
                }
            }

            u32* p_reader = (u32*)data;
            u32  version = *p_reader++;
            u32  num_meshes = *p_reader++;

            if (version < 1)
                return;

            std::vector<Str> mat_names;
            for (u32 submesh = 0; submesh < num_meshes; ++submesh)
            {
                mat_names.push_back(read_parsable_string((const u32**)&p_reader));
            }

            for (u32 submesh = 0; submesh < num_meshes; ++submesh)
            {
                hm.begin(0);
                hm.add(filename, pen::string_length(filename));
                hm.add(geometry_name, pen::string_length(geometry_name));
                hm.add(submesh);
                hash_id sub_hash = hm.end();

                geometry_resource* p_geometry = new geometry_resource;

                p_geometry->p_skin = nullptr;
                p_geometry->file_hash = file_hash;
                p_geometry->hash = sub_hash;
                p_geometry->geometry_name = geometry_name;
                p_geometry->filename = filename;
                p_geometry->material_name = mat_names[submesh];
                p_geometry->material_id_name = PEN_HASH(mat_names[submesh].c_str());
                p_geometry->submesh_index = submesh;

                memcpy(&p_geometry->min_extents, p_reader, sizeof(vec3f));
                p_reader += 3;

                memcpy(&p_geometry->max_extents, p_reader, sizeof(vec3f));
                p_reader += 3;

                // vb and ib
                u32 num_pos_floats = *p_reader++;
                u32 num_floats = *p_reader++;
                u32 num_indices = *p_reader++;
                u32 num_collision_floats = *p_reader++;
                u32 skinned = *p_reader++;

                u32 index_size = num_indices < 65535 ? 2 : 4;

                u32 vertex_size = sizeof(vertex_model);

                if (skinned)
                {
                    vertex_size = sizeof(vertex_model_skinned);

                    p_geometry->p_skin = (cmp_skin*)pen::memory_alloc(sizeof(cmp_skin));

                    memcpy(&p_geometry->p_skin->bind_shape_matrix, p_reader, sizeof(mat4));
                    p_reader += 16;

                    // Conversion from max to lhs - this needs to go into the build pipeline
                    // max_swap.axis_swap(vec3f(1.0f, 0.0f, 0.0f), vec3f(0.0f, 0.0f, -1.0f), vec3f(0.0f, 1.0f, 0.0f));
                    // final_bind = max_swap * p_geometry->p_skin->bind_shape_matrix * max_swap_inv;
                    // joint_bind_matrices[joint] = max_swap * joint_bind_matrices[joint] * max_swap_inv;

                    mat4 final_bind = p_geometry->p_skin->bind_shape_matrix;

                    p_geometry->p_skin->bind_shape_matrix = final_bind;

                    u32 num_ijb_floats = *p_reader++;
                    memcpy(&p_geometry->p_skin->joint_bind_matrices[0], p_reader, sizeof(f32) * num_ijb_floats);
                    p_reader += num_ijb_floats;

                    p_geometry->p_skin->num_joints = num_ijb_floats / 16;

                    for (u32 joint = 0; joint < p_geometry->p_skin->num_joints; ++joint)
                    {
                        p_geometry->p_skin->joint_bind_matrices[joint] = p_geometry->p_skin->joint_bind_matrices[joint];
                    }

                    p_geometry->p_skin->bone_cbuffer = PEN_INVALID_HANDLE;
                }

                p_geometry->vertex_size = vertex_size;

                // all vertex data is written out as 4 byte ints
                u32 num_verts = num_floats / (vertex_size / sizeof(u32));
                u32 num_pos_verts = num_pos_floats / (sizeof(vertex_position) / sizeof(u32));

                p_geometry->num_vertices = num_verts;

                pen::buffer_creation_params bcp;
                bcp.usage_flags = PEN_USAGE_DEFAULT;
                bcp.bind_flags = PEN_BIND_VERTEX_BUFFER;
                bcp.cpu_access_flags = 0;
                bcp.buffer_size = sizeof(vertex_position) * num_pos_verts;
                bcp.data = (void*)p_reader;

                // keep a cpu copy of position data
                p_geometry->cpu_position_buffer = pen::memory_alloc(bcp.buffer_size);
                memcpy(p_geometry->cpu_position_buffer, bcp.data, bcp.buffer_size);

                if (p_geometry->min_extents.x == -1.0f)
                {
                    for (u32 v = 0; v < num_pos_verts; ++v)
                    {
                        vertex_position vp = ((vertex_position*)p_geometry->cpu_position_buffer)[v];
                        dev_console_log_level(dev_ui::CONSOLE_MESSAGE, "Pos: %f, %f, %f", vp.x, vp.y, vp.z);
                    }
                }

                p_geometry->position_buffer = pen::renderer_create_buffer(bcp);

                p_reader += bcp.buffer_size / sizeof(f32);

                bcp.buffer_size = vertex_size * num_verts;
                bcp.data = (void*)p_reader;
                
                p_geometry->cpu_vertex_buffer = pen::memory_alloc(bcp.buffer_size);
                memcpy(p_geometry->cpu_vertex_buffer, bcp.data, bcp.buffer_size);
                
                p_geometry->vertex_buffer = pen::renderer_create_buffer(bcp);

                p_reader += bcp.buffer_size / sizeof(u32);

                bcp.usage_flags = PEN_USAGE_DEFAULT;
                bcp.bind_flags = PEN_BIND_INDEX_BUFFER;
                bcp.cpu_access_flags = 0;
                bcp.buffer_size = index_size * num_indices;
                bcp.data = (void*)p_reader;

                p_geometry->num_indices = num_indices;
                p_geometry->index_type = index_size == 2 ? PEN_FORMAT_R16_UINT : PEN_FORMAT_R32_UINT;
                p_geometry->index_buffer = pen::renderer_create_buffer(bcp);

                // keep a cpu copy of index data
                p_geometry->cpu_index_buffer = pen::memory_alloc(bcp.buffer_size);
                memcpy(p_geometry->cpu_index_buffer, bcp.data, bcp.buffer_size);

                p_reader = (u32*)((c8*)p_reader + bcp.buffer_size);

                p_reader += num_collision_floats;

                s_geometry_resources.push_back(p_geometry);
            }
        }

        material_resource* get_material_resource(hash_id hash)
        {
            for (auto* m : s_material_resources)
                if (m->hash == hash)
                    return m;

            return nullptr;
        }

        void instantiate_material(material_resource* mr, ecs_scene* scene, u32 node_index)
        {
            scene->id_material[node_index] = mr->hash;
            scene->material_names[node_index] = mr->material_name;

            scene->entities[node_index] |= CMP_MATERIAL;

            // set defaults
            if (mr->id_shader == 0)
            {
                static hash_id id_default_shader = PEN_HASH("forward_render");
                static hash_id id_default_technique = PEN_HASH("forward_lit");

                mr->shader_name = "forward_render";
                mr->id_shader = id_default_shader;
                mr->id_technique = id_default_technique;
            }

            static hash_id id_default_sampler_state = PEN_HASH("wrap_linear");

            for (u32 i = 0; i < SN_NUM_TEXTURES; ++i)
            {
                if (!mr->id_sampler_state[i])
                    mr->id_sampler_state[i] = id_default_sampler_state;
            }

            scene->material_resources[node_index] = *mr;

            bake_material_handles(scene, node_index);
        }

        void permutation_flags_from_vertex_class(u32& permutation, hash_id vertex_class)
        {
            u32 clear_vertex = ~(PERMUTATION_SKINNED | PERMUTATION_INSTANCED);
            permutation &= clear_vertex;

            if (vertex_class == ID_VERTEX_CLASS_SKINNED)
                permutation |= PERMUTATION_SKINNED;

            if (vertex_class == ID_VERTEX_CLASS_INSTANCED)
                permutation |= PERMUTATION_INSTANCED;
        }

        void bake_material_handles(ecs_scene* scene, u32 node_index)
        {
            material_resource* resource = &scene->material_resources[node_index];
            cmp_material*      material = &scene->materials[node_index];
            cmp_samplers&      samplers = scene->samplers[node_index];
            u32&               permutation = scene->material_permutation[node_index];
            cmp_geometry*      geometry = &scene->geometries[node_index];

            if (!resource)
                return;

            // shader
            material->shader = pmfx::load_shader(resource->shader_name.c_str());
            if (!is_valid(material->shader))
                return;

            // permutation form geom
            permutation_flags_from_vertex_class(permutation, geometry->vertex_shader_class);

            // technique / permutation
            material->technique_index = pmfx::get_technique_index_perm(material->shader, resource->id_technique, permutation);

            PEN_ASSERT(is_valid(material->technique_index));

            // material / technique constant buffers
            s32 cbuffer_size = pmfx::get_technique_cbuffer_size(material->shader, material->technique_index);

            if (!(scene->state_flags[node_index] & SF_MATERIAL_INITIALISED))
            {
                pmfx::initialise_constant_defaults(material->shader, material->technique_index,
                                                   scene->material_data[node_index].data);

                scene->state_flags[node_index] |= SF_MATERIAL_INITIALISED;
            }

            instantiate_material_cbuffer(scene, node_index, cbuffer_size);

            // material samplers
            if (!(scene->state_flags[node_index] & SF_SAMPLERS_INITIALISED))
            {
                pmfx::initialise_sampler_defaults(material->shader, material->technique_index, samplers);

                // set material texture from source data
                for (u32 t = 0; t < SN_NUM_TEXTURES; ++t)
                {
                    if (resource->texture_handles[t] != 0 && is_valid(resource->texture_handles[t]))
                    {
                        for (u32 s = 0; s < MAX_TECHNIQUE_SAMPLER_BINDINGS; ++s)
                        {
                            if (samplers.sb[s].sampler_unit == t)
                            {
                                samplers.sb[s].id_texture = PEN_HASH(put::get_texture_filename(resource->texture_handles[t]));
                                samplers.sb[s].handle = resource->texture_handles[t];
                                break;
                            }
                        }
                    }
                }

                scene->entities[node_index] |= CMP_SAMPLERS;
                scene->state_flags[node_index] |= SF_SAMPLERS_INITIALISED;
            }

            // bake ss handles
            for (u32 s = 0; s < MAX_TECHNIQUE_SAMPLER_BINDINGS; ++s)
                if (samplers.sb[s].id_sampler_state != 0)
                    samplers.sb[s].sampler_state = pmfx::get_render_state(samplers.sb[s].id_sampler_state, pmfx::RS_SAMPLER);
        }

        void bake_material_handles()
        {
            ecs_scene_list* scenes = get_scenes();
            for (auto& si : *scenes)
            {
                ecs_scene* scene = si.scene;

                for (u32 n = 0; n < scene->nodes_size; ++n)
                {
                    if (scene->entities[n] & CMP_MATERIAL)
                        bake_material_handles(scene, n);
                }
            }
        }

        void load_material_resource(const c8* filename, const c8* material_name, const c8* data)
        {
            pen::hash_murmur hm;
            hm.begin();
            hm.add(filename, pen::string_length(filename));
            hm.add(material_name, pen::string_length(material_name));
            hash_id hash = hm.end();

            for (s32 m = 0; m < s_material_resources.size(); ++m)
                if (s_material_resources[m]->hash == hash)
                    return;

            const u32* p_reader = (u32*)data;

            u32 version = *p_reader++;

            if (version < 1)
                return;

            material_resource* p_mat = new material_resource;

            p_mat->material_name = material_name;
            p_mat->hash = hash;

            // diffuse
            memcpy(&p_mat->data[0], p_reader, sizeof(vec4f));
            p_reader += 4;

            // specular
            memcpy(&p_mat->data[4], p_reader, sizeof(vec4f));
            p_reader += 4;

            // shininess
            memcpy(&p_mat->data[3], p_reader, sizeof(f32));
            p_reader++;

            // reflectivity
            memcpy(&p_mat->data[7], p_reader, sizeof(f32));
            p_reader++;

            u32 num_maps = *p_reader++;

            // clear all maps to invalid
            static const u32 default_maps[] = {put::load_texture("data/textures/defaults/albedo.dds"),
                                               put::load_texture("data/textures/defaults/normal.dds"),
                                               put::load_texture("data/textures/defaults/spec.dds"),
                                               put::load_texture("data/textures/defaults/spec.dds"),
                                               put::load_texture("data/textures/defaults/black.dds"),
                                               put::load_texture("data/textures/defaults/black.dds")};
            static_assert(SN_NUM_TEXTURES == PEN_ARRAY_SIZE(default_maps), "mismatched defaults size");

            for (u32 map = 0; map < SN_NUM_TEXTURES; ++map)
                p_mat->texture_handles[map] = default_maps[map];

            for (u32 map = 0; map < num_maps; ++map)
            {
                u32 map_type = *p_reader++;
                Str texture_name = read_parsable_string(&p_reader);
                p_mat->texture_handles[map_type] = put::load_texture(texture_name.c_str());
            }

            s_material_resources.push_back(p_mat);

            return;
        }

        static std::vector<animation_resource> k_animations;

        animation_resource* get_animation_resource(anim_handle h)
        {
            if (h >= k_animations.size())
                return nullptr;

            return &k_animations[h];
        }

        anim_handle load_pma(const c8* filename)
        {
            Str pd = put::dev_ui::get_program_preference_filename("project_dir");

            Str stipped_filename = pen::str_replace_string(filename, pd.c_str(), "");

            hash_id filename_hash = PEN_HASH(stipped_filename.c_str());

            // search for existing
            s32 num_anims = k_animations.size();
            for (s32 i = 0; i < num_anims; ++i)
            {
                if (k_animations[i].id_name == filename_hash)
                {
                    return (anim_handle)i;
                }
            }

            void* anim_file;
            u32   anim_file_size;

            pen_error err = pen::filesystem_read_file_to_buffer(filename, &anim_file, anim_file_size);

            if (err != PEN_ERR_OK || anim_file_size == 0)
            {
                // TODO error dialog
                return PEN_INVALID_HANDLE;
            }

            const u32* p_u32reader = (u32*)anim_file;

            u32 version = *p_u32reader++;

            if (version < 1)
            {
                pen::memory_free(anim_file);
                return PEN_INVALID_HANDLE;
            }

            k_animations.push_back(animation_resource());
            animation_resource& new_animation = k_animations.back();

            new_animation.name = stipped_filename;
            new_animation.id_name = filename_hash;

            u32 num_channels = *p_u32reader++;

            new_animation.num_channels = num_channels;
            new_animation.channels = new animation_channel[num_channels];

            new_animation.length = 0.0f;

            u32 max_frames = 0;

            for (s32 i = 0; i < num_channels; ++i)
            {
                Str bone_name = read_parsable_string(&p_u32reader);
                new_animation.channels[i].target = PEN_HASH(bone_name.c_str());
                new_animation.channels[i].target_name = bone_name;

                u32 num_sources = *p_u32reader++;

                // null arrays
                new_animation.channels[i].matrices = nullptr;
                for (u32 o = 0; o < 3; ++o)
                {
                    new_animation.channels[i].offset[o] = nullptr;
                    new_animation.channels[i].scale[o] = nullptr;
                    new_animation.channels[i].rotation[o] = nullptr;
                }

                s32 num_rots = 0;

                for (s32 j = 0; j < num_sources; ++j)
                {
                    u32 sematic = *p_u32reader++;
                    u32 type = *p_u32reader++;
                    u32 target = *p_u32reader++;

                    // read float buffer
                    u32 num_elements = *p_u32reader++;

                    if (sematic == A_INTERPOLATION)
                    {
                        PEN_ASSERT(type == A_INT);

                        u32* data = new u32[num_elements];
                        memcpy(data, p_u32reader, sizeof(u32) * num_elements);
                        new_animation.channels[i].interpolation = data;
                    }
                    else if (sematic == A_TIME)
                    {
                        PEN_ASSERT(type == A_FLOAT);

                        f32* data = new f32[num_elements];
                        memcpy(data, p_u32reader, sizeof(f32) * num_elements);
                        new_animation.channels[i].num_frames = num_elements;
                        new_animation.channels[i].times = data;
                    }
                    else
                    {
                        u32  num_floats = num_elements;
                        f32* data = new f32[num_floats];
                        memcpy(data, p_u32reader, sizeof(f32) * num_floats);

                        switch (target)
                        {
                            case A_TRANSFORM_TARGET:
                            {
                                new_animation.channels[i].matrices = (mat4*)data;

                                // make a set of channels from matrix
                                u32 num_mats = num_floats / 16;

                                f32*  to[3] = {0};
                                f32*  ts[3] = {0};
                                quat* tq[3] = {0};
                                tq[0] = new quat[num_mats];

                                for (u32 t = 0; t < 3; ++t)
                                {
                                    to[t] = new f32[num_mats];
                                    ts[t] = new f32[num_mats];

                                    new_animation.channels[i].offset[t] = to[t];
                                    new_animation.channels[i].scale[t] = ts[t];
                                    new_animation.channels[i].rotation[t] = tq[t];
                                }

                                for (u32 m = 0; m < num_mats; ++m)
                                {
                                    mat4& mat = new_animation.channels[i].matrices[m];

                                    vec3f trans = mat.get_translation();
                                    quat  rot;
                                    rot.from_matrix(mat);

                                    tq[0][m] = rot;

                                    f32 sx = mag(mat.get_row(0).xyz);
                                    f32 sy = mag(mat.get_row(1).xyz);
                                    f32 sz = mag(mat.get_row(2).xyz);

                                    vec3f scale = vec3f(sx, sy, sz);

                                    for (u32 t = 0; t < 3; ++t)
                                    {
                                        to[t][m] = trans[t];
                                        ts[t][m] = scale[t];
                                    }
                                }
                            }
                            break;

                            case A_TRANSLATE_X_TARGET:
                            case A_TRANSLATE_Y_TARGET:
                            case A_TRANSLATE_Z_TARGET:
                                new_animation.channels[i].offset[target - A_TRANSLATE_X_TARGET] = (f32*)data;
                                break;
                            case A_ROTATE_X_TARGET:
                            case A_ROTATE_Y_TARGET:
                            case A_ROTATE_Z_TARGET:
                            {
                                new_animation.channels[i].rotation[num_rots] = new quat[num_floats];

                                for (u32 q = 0; q < num_floats; ++q)
                                {
                                    vec3f mask[] = {vec3f::unit_x(), vec3f::unit_y(), vec3f::unit_z()};

                                    vec3f vr = vec3f(data[q]) * mask[target - A_ROTATE_X_TARGET];
                                    new_animation.channels[i].rotation[num_rots][q] = quat(vr.z, vr.y, vr.x);
                                }

                                num_rots++;
                            }
                            break;
                            case A_SCALE_X_TARGET:
                            case A_SCALE_Y_TARGET:
                            case A_SCALE_Z_TARGET:
                                new_animation.channels[i].scale[target - A_SCALE_X_TARGET] = (f32*)data;
                                break;
                            case A_TRANSLATE_TARGET:
                                new_animation.channels[i].offset[sematic - A_X] = (f32*)data;
                                break;
                            case A_ROTATE_TARGET:
                            {
                                PEN_ASSERT(0); // code path hasnt been tested

                                u32 num_quats = num_floats / 3;

                                new_animation.channels[i].rotation[num_rots] = new quat[num_quats];

                                for (u32 q = 0; q < num_quats; q++)
                                {
                                    u32   qi = q * 3;
                                    vec3f vr = vec3f(data[qi], data[qi + 1], data[qi + 2]);
                                    new_animation.channels[i].rotation[num_rots][q] = quat(vr.z, vr.y, vr.x);
                                }

                                num_rots++;
                            }
                            break;
                            case A_SCALE_TARGET:
                                new_animation.channels[i].scale[sematic - A_X] = (f32*)data;
                                break;
                            default:
                                // PEN_ASSERT(0); // unhandled targets
                                break;
                        };
                    }

                    p_u32reader += num_elements;
                }

                for (s32 t = 0; t < new_animation.channels[i].num_frames; ++t)
                {
                    f32* times = new_animation.channels[i].times;
                    new_animation.length = fmax(times[t], new_animation.length);
                }

                max_frames = std::max<u32>(new_animation.channels[i].num_frames, max_frames);
            }

            // free file mem
            pen::memory_free(anim_file);

            // bake animations into soa.

            // allocate vertical arrays
            soa_anim& soa = new_animation.soa;
            soa.data = new f32*[max_frames];
            soa.channels = new anim_channel[num_channels];
            soa.info = new anim_info*[max_frames];
            soa.num_channels = num_channels;

            // null ptrs
            memset(soa.data, 0x0, max_frames * sizeof(f32*));
            memset(soa.info, 0x0, max_frames * sizeof(f32*));

            // push channels into horizontal contiguous arrays
            for (s32 c = 0; c < num_channels; ++c)
            {
                animation_channel& channel = new_animation.channels[c];

                // setup sampler
                soa.channels[c].num_frames = channel.num_frames;

                u32 elm = 0;

                // translate
                for (u32 i = 0; i < 3; ++i)
                    if (channel.offset[i])
                        soa.channels[c].element_offset[elm++] = A_OUT_TX + i;

                // scale
                for (u32 i = 0; i < 3; ++i)
                    if (channel.scale[i])
                        soa.channels[c].element_offset[elm++] = A_OUT_SX + i;

                // quaternion
                for (u32 i = 0; i < 3; ++i)
                    if (channel.rotation[i])
                        for (u32 q = 0; q < 4; ++q)
                            soa.channels[c].element_offset[elm++] = A_OUT_QUAT;

                if (channel.matrices)
                {
                    // baked
                    soa.channels[c].flags = anim_flags::BAKED_QUATERNION;
                }

                soa.channels[c].element_count = elm;

                for (u32 t = 0; t < channel.num_frames; ++t)
                {
                    u32 start_offset = sb_count(soa.data[t]);

                    // translate
                    for (u32 i = 0; i < 3; ++i)
                        if (channel.offset[i])
                            sb_push(soa.data[t], channel.offset[i][t]);

                    // scale
                    for (u32 i = 0; i < 3; ++i)
                        if (channel.scale[i])
                            sb_push(soa.data[t], channel.scale[i][t]);

                    // quat
                    for (u32 i = 0; i < 3; ++i)
                        if (channel.rotation[i])
                        {
                            sb_push(soa.data[t], channel.rotation[i][t].x);
                            sb_push(soa.data[t], channel.rotation[i][t].y);
                            sb_push(soa.data[t], channel.rotation[i][t].z);
                            sb_push(soa.data[t], channel.rotation[i][t].w);
                        }

                    u32 end_offset = sb_count(soa.data[t]);

                    soa.channels[c].element_count = end_offset - start_offset;

                    anim_info ai;
                    ai.offset = start_offset;
                    ai.time = channel.times[t];
                    sb_push(soa.info[t], ai);
                }

                // pad sparse info array
                for (u32 t = channel.num_frames; t < max_frames; ++t)
                {
                    anim_info ai;
                    ai.offset = -1;
                    ai.time = 0.0f;
                    sb_push(soa.info[t], ai);
                }
            }

            return (anim_handle)k_animations.size() - 1;
        }

        struct mat_symbol_name
        {
            hash_id symbol;
            Str     name;
        };

        s32 load_pmm(const c8* filename, ecs_scene* scene, u32 load_flags)
        {
            if (scene)
                scene->flags |= INVALIDATE_SCENE_TREE;

            void* model_file;
            u32   model_file_size;

            pen_error err = pen::filesystem_read_file_to_buffer(filename, &model_file, model_file_size);

            if (err != PEN_ERR_OK || model_file_size == 0)
            {
                dev_ui::log_level(dev_ui::CONSOLE_ERROR, "[error] load pmm - failed to find file: %s", filename);
                return PEN_INVALID_HANDLE;
            }

            const u32* p_u32reader = (u32*)model_file;

            u32 num_scene = *p_u32reader++;
            u32 num_geom = *p_u32reader++;
            u32 num_materials = *p_u32reader++;

            std::vector<u32> scene_offsets;
            std::vector<u32> geom_offsets;
            std::vector<u32> material_offsets;

            std::vector<Str> material_names;
            std::vector<Str> geometry_names;

            std::vector<hash_id> id_geometry;

            for (s32 i = 0; i < num_scene; ++i)
                scene_offsets.push_back(*p_u32reader++);

            for (s32 i = 0; i < num_materials; ++i)
            {
                Str name = read_parsable_string(&p_u32reader);
                material_offsets.push_back(*p_u32reader++);
                material_names.push_back(name);
            }

            for (s32 i = 0; i < num_geom; ++i)
            {
                Str name = read_parsable_string(&p_u32reader);
                geom_offsets.push_back(*p_u32reader++);
                id_geometry.push_back(PEN_HASH(name.c_str()));
                geometry_names.push_back(name);
            }

            c8* p_data_start = (c8*)p_u32reader;

            p_u32reader = (u32*)p_data_start + scene_offsets[0];
            u32 version = *p_u32reader++;
            u32 num_import_nodes = *p_u32reader++;

            if (version < 1)
            {
                pen::memory_free(model_file);
                return PEN_INVALID_HANDLE;
            }

            // load resources
            if (load_flags & PMM_MATERIAL)
            {
                for (u32 m = 0; m < num_materials; ++m)
                {
                    u32* p_mat_data = (u32*)(p_data_start + material_offsets[m]);
                    load_material_resource(filename, material_names[m].c_str(), (const c8*)p_mat_data);
                }
            }

            if (load_flags & PMM_GEOMETRY)
            {
                for (u32 g = 0; g < num_geom; ++g)
                {
                    u32* p_geom_data = (u32*)(p_data_start + geom_offsets[g]);
                    load_geometry_resource(filename, geometry_names[g].c_str(), (const c8*)p_geom_data);
                }
            }

            if (!(load_flags & PMM_NODES))
            {
                pen::memory_free(model_file);
                return PEN_INVALID_HANDLE;
            }

            // scene nodes
            s32 nodes_start, nodes_end;
            get_new_nodes_append(scene, num_import_nodes, nodes_start, nodes_end);

            u32 node_zero_offset = nodes_start;
            u32 current_node = node_zero_offset;
            u32 inserted_nodes = 0;

            // load scene nodes
            for (u32 n = 0; n < num_import_nodes; ++n)
            {
                p_u32reader++; // e_node type

                Str node_name = read_parsable_string(&p_u32reader);
                Str geometry_name = read_parsable_string(&p_u32reader);

                scene->id_name[current_node] = PEN_HASH(node_name.c_str());
                scene->id_geometry[current_node] = PEN_HASH(geometry_name.c_str());

                scene->names[current_node] = node_name;
                scene->geometry_names[current_node] = geometry_name;

                scene->entities[current_node] |= CMP_ALLOCATED;

                if (scene->id_geometry[current_node] == ID_JOINT)
                    scene->entities[current_node] |= CMP_BONE;

                if (scene->id_name[current_node] == ID_TRAJECTORY)
                    scene->entities[current_node] |= CMP_ANIM_TRAJECTORY;

                u32 num_meshes = *p_u32reader++;

                std::vector<mat_symbol_name> mesh_material_names;

                // material pre load
                for (u32 mat = 0; mat < num_meshes; ++mat)
                {
                    Str name = read_parsable_string(&p_u32reader);
                    Str symbol = read_parsable_string(&p_u32reader);

                    mesh_material_names.push_back({PEN_HASH(symbol.c_str()), name});
                }

                // transformation load
                u32 parent = *p_u32reader++ + node_zero_offset + inserted_nodes;
                scene->parents[current_node] = parent;
                u32 transforms = *p_u32reader++;

                // parent fix up
                if (scene->id_name[current_node] == ID_CONTROL_RIG)
                    scene->parents[current_node] = node_zero_offset;

                vec3f translation;
                vec4f rotations[3];
                mat4  matrix;
                bool  has_matrix_transform = false;
                u32   num_rotations = 0;

                static f32 zero_rotation_epsilon = 0.000001f;
                for (u32 t = 0; t < transforms; ++t)
                {
                    u32 type = *p_u32reader++;

                    switch (type)
                    {
                        case PMM_TRANSLATE:
                            memcpy(&translation, p_u32reader, 12);
                            p_u32reader += 3;
                            break;
                        case PMM_ROTATE:
                            memcpy(&rotations[num_rotations], p_u32reader, 16);
                            rotations[num_rotations].w = maths::deg_to_rad(rotations[num_rotations].w);
                            if (rotations[num_rotations].w < zero_rotation_epsilon &&
                                rotations[num_rotations].w > zero_rotation_epsilon)
                                rotations[num_rotations].w = 0.0f;
                            num_rotations++;
                            p_u32reader += 4;
                            break;
                        case PMM_MATRIX:
                            has_matrix_transform = true;
                            memcpy(&matrix, p_u32reader, 16 * 4);
                            p_u32reader += 16;
                            break;
                        case PMM_IDENTITY:
                            has_matrix_transform = true;
                            matrix = mat4::create_identity();
                            break;
                        default:
                            // unsupported transform type
                            PEN_ASSERT(0);
                            break;
                    }
                }

                quat final_rotation;
                if (num_rotations == 0)
                {
                    // no rotation
                    final_rotation.euler_angles(0.0f, 0.0f, 0.0f);
                }
                else if (num_rotations == 1)
                {
                    // axis angle
                    final_rotation.axis_angle(rotations[0]);
                }
                else if (num_rotations == 3)
                {
                    // euler angles
                    f32 z_theta = 0;
                    f32 y_theta = 0;
                    f32 x_theta = 0;

                    for (u32 r = 0; r < 3; ++r)
                    {
                        if (rotations[r].z == 1.0f)
                        {
                            z_theta = rotations[r].w;
                        }
                        else if (rotations[r].y == 1.0f)
                        {
                            y_theta = rotations[r].w;
                        }
                        else if (rotations[r].x == 1.0f)
                        {
                            x_theta = rotations[r].w;
                        }

                        final_rotation.euler_angles(z_theta, y_theta, x_theta);
                    }
                }

                if (!has_matrix_transform)
                {
                    // create matrix from transform
                    scene->transforms[current_node].translation = translation;
                    scene->transforms[current_node].rotation = final_rotation;
                    scene->transforms[current_node].scale = vec3f::one();

                    // make a transform matrix for geometry
                    mat4 rot_mat;
                    final_rotation.get_matrix(rot_mat);

                    mat4 translation_mat = mat::create_translation(translation);

                    matrix = translation_mat * rot_mat;
                }
                else
                {
                    // decompose matrix into transform
                    scene->transforms[current_node].translation = matrix.get_translation();
                    scene->transforms[current_node].rotation.from_matrix(matrix);

                    f32 sx = mag(matrix.get_row(0).xyz);
                    f32 sy = mag(matrix.get_row(1).xyz);
                    f32 sz = mag(matrix.get_row(2).xyz);

                    scene->transforms[current_node].scale = vec3f(sx, sy, sz);
                }

                scene->initial_transform[current_node].rotation = scene->transforms[current_node].rotation;
                scene->initial_transform[current_node].translation = scene->transforms[current_node].translation;
                scene->initial_transform[current_node].scale = scene->transforms[current_node].scale;

                scene->local_matrices[current_node] = (matrix);

                // store intial position for physics to hook into later
                scene->physics_data[current_node].rigid_body.position = translation;
                scene->physics_data[current_node].rigid_body.rotation = final_rotation;

                // assign geometry, materials and physics
                u32 dest = current_node;
                if (num_meshes > 0)
                {
                    for (u32 submesh = 0; submesh < num_meshes; ++submesh)
                    {
                        dest = current_node + submesh;

                        Str node_suffix;
                        node_suffix.appendf("_%i", submesh);

                        if (submesh > 0)
                        {
                            inserted_nodes++;
                            clone_node(scene, current_node, dest, current_node, CLONE_INSTANTIATE, vec3f::zero(),
                                       (const c8*)node_suffix.c_str());
                            scene->local_matrices[dest] = mat4::create_identity();

                            // child geometry which will inherit any skinning from its parent
                            scene->entities[dest] |= CMP_SUB_GEOMETRY;
                        }

                        // generate geometry hash
                        pen::hash_murmur hm;
                        hm.begin(0);
                        hm.add(filename, pen::string_length(filename));
                        hm.add(geometry_name.c_str(), geometry_name.length());
                        hm.add(submesh);
                        hash_id geom_hash = hm.end();

                        scene->id_geometry[dest] = geom_hash;

                        geometry_resource* gr = get_geometry_resource(geom_hash);

                        if (gr)
                        {
                            instantiate_geometry(gr, scene, dest);

                            instantiate_model_cbuffer(scene, dest);

                            // find mat name from symbol
                            Str mat_name = "";
                            for (auto& ms : mesh_material_names)
                                if (ms.symbol == gr->material_id_name)
                                    mat_name = ms.name;

                            hm.begin();
                            hm.add(filename, pen::string_length(filename));
                            hm.add(mat_name.c_str(), mat_name.length());
                            hash_id material_hash = hm.end();

                            material_resource* mr = get_material_resource(material_hash);

                            if (mr)
                            {
                                scene->material_names[dest] = mat_name;

                                // due to cloning, clear these flags
                                scene->state_flags[dest] &= ~SF_MATERIAL_INITIALISED;
                                scene->state_flags[dest] &= ~SF_SAMPLERS_INITIALISED;

                                instantiate_material(mr, scene, dest);

                                scene->id_material[dest] = material_hash;
                            }
                            else
                            {
                                static hash_id id_default = PEN_HASH("default_material");

                                mr = get_material_resource(id_default);

                                scene->material_names[dest] = "default_material";

                                instantiate_material(mr, scene, dest);

                                scene->id_material[dest] = id_default;
                            }
                        }
                        else
                        {
                            put::dev_ui::log_level(dev_ui::CONSOLE_ERROR, "[error] geometry - missing file : %s",
                                                   geometry_name.c_str());
                        }
                    }
                }

                current_node = dest + 1;
            }

            // now we have loaded the whole scene fix up any anim controllers
            for (s32 i = node_zero_offset; i < node_zero_offset + num_import_nodes; ++i)
            {
                if (scene->entities[i] & CMP_SUB_GEOMETRY)
                    continue;

                // parent geometry deals with skinning
                if ((scene->entities[i] & CMP_GEOMETRY) && scene->geometries[i].p_skin)
                {
                    instantiate_anim_controller(scene, i);
                    instantiate_anim_controller_v2(scene, i);
                }
            }

            pen::memory_free(model_file);
            return nodes_start;
        }

        struct volume_instance
        {
            hash_id id;
            hash_id id_technique;
            hash_id id_sampler_state;
            u32     cmp_flags;
        };

        s32 load_pmv(const c8* filename, ecs_scene* scene)
        {
            pen::json pmv = pen::json::load_from_file(filename);

            Str volume_texture_filename = pmv["filename"].as_str();
            u32 volume_texture = put::load_texture(volume_texture_filename.c_str());

            vec3f scale = vec3f(pmv["scale_x"].as_f32(), pmv["scale_y"].as_f32(), pmv["scale_z"].as_f32());

            hash_id id_type = pmv["volume_type"].as_hash_id();

            static volume_instance vi[] = {
                {PEN_HASH("volume_texture"), PEN_HASH("volume_texture"), PEN_HASH("clamp_point_sampler_state"), CMP_VOLUME},

                {PEN_HASH("signed_distance_field"), PEN_HASH("volume_sdf"), PEN_HASH("clamp_linear_sampler_state"),
                 CMP_SDF_SHADOW}};

            int i = 0;
            for (auto& v : vi)
            {
                if (v.id == id_type)
                    break;

                ++i;
            }

            // create material for volume sdf sphere trace
            material_resource* material = new material_resource;
            material->material_name = "volume_sdf_material";
            material->shader_name = "pmfx_utility";
            material->id_shader = PEN_HASH("pmfx_utility");
            material->id_technique = vi[i].id_technique;
            material->id_sampler_state[SN_VOLUME_TEXTURE] = vi[i].id_sampler_state;
            material->texture_handles[SN_VOLUME_TEXTURE] = volume_texture;
            add_material_resource(material);

            geometry_resource* cube = get_geometry_resource(PEN_HASH("cube"));

            vec3f pos = vec3f::zero();

            u32 v = get_new_node(scene);

            scene->names[v] = "volume";
            scene->names[v].appendf("%i", v);
            scene->transforms[v].rotation = quat();
            scene->transforms[v].scale = scale;
            scene->transforms[v].translation = pos;
            scene->entities[v] |= CMP_TRANSFORM;
            scene->parents[v] = v;

            instantiate_geometry(cube, scene, v);
            instantiate_material(material, scene, v);
            instantiate_model_cbuffer(scene, v);

            return v;
        }

        void enumerate_resources(bool* open)
        {
            ImGui::Begin("Resource Browser", open);

            if (ImGui::CollapsingHeader("Geometry"))
            {
                for (auto* g : s_geometry_resources)
                {
                    ImGui::Text("Source: %s", g->filename.c_str());
                    ImGui::Text("Geometry: %s", g->geometry_name.c_str());
                    ImGui::Text("Material: %s", g->material_name.c_str());
                    ImGui::Text("File Hash: %i", g->file_hash);
                    ImGui::Text("Hash: %i", g->hash);
                    ImGui::Text("Vertices: %i", g->num_vertices);
                    ImGui::Text("Indices: %i", g->num_indices);

                    ImGui::Separator();
                }
            }

            if (ImGui::CollapsingHeader("Textures"))
            {
                put::texture_browser_ui();
            }

            ImGui::End();
        }
    } // namespace ecs
} // namespace put
