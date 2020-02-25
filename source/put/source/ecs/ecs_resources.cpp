// ecs_resources.cpp
// Copyright 2014 - 2019 Alex Dixon.
// License: https://github.com/polymonster/pmtech/blob/master/license.md

#include "ecs/ecs_resources.h"
#include "ecs/ecs_utilities.h"

#include "debug_render.h"
#include "dev_ui.h"

#include "console.h"
#include "data_struct.h"
#include "file_system.h"
#include "hash.h"
#include "pen_string.h"
#include "str_utilities.h"

#include "meshoptimizer.h"

#include <fstream>

using namespace put;
using namespace ecs;

namespace
{
    // id hashes
    const hash_id ID_CONTROL_RIG = PEN_HASH("controlrig");
    const hash_id ID_JOINT = PEN_HASH("joint");
    const hash_id ID_TRAJECTORY = PEN_HASH("trajectoryshjnt");

    // constants
    static const u32 k_matrix_floats = 16;
    static const u32 k_extent_floats = 3;

    namespace e_pmm_transform
    {
        enum pmm_transform_t
        {
            translate = 0,
            rotate,
            matrix,
            identity
        };
    }
    typedef e_pmm_transform::pmm_transform_t pmm_transform;

    struct pmm_contents
    {
        u32              num_scene = 0;
        u32              num_geometry = 0;
        u32              num_materials = 0;
        u8*              data_start = nullptr;
        void*            file_data = nullptr;
        u32              file_size = 0;
        std::vector<u32> scene_offsets;
        std::vector<u32> material_offsets;
        std::vector<Str> material_names;
        std::vector<u32> geometry_offsets;
        std::vector<Str> geometry_names;
    };

    struct pmm_submesh
    {
        // pmm submesh header
        vec3f min_extents;
        vec3f max_extents;
        u32   num_verts;
        u32   index_size;
        u32   num_pos_floats;
        u32   num_vertex_floats;
        u32   num_indices;
        u32   num_collision_floats;
        u32   skinned;
        u32   num_joint_floats;
        mat4  bind_shape_matrix;
        // end of header
        u32    vertex_size;
        void*  joint_data;
        size_t joint_data_size;
        void*  position_data;
        size_t position_data_size;
        void*  vertex_data;
        size_t vertex_data_size;
        void*  index_data;
        size_t index_data_size;
        void*  collision_data;
        size_t collision_data_size;
    };

    struct pmm_geometry
    {
        u32                      version;
        u32                      num_meshes;
        std::vector<Str>         mat_names;
        std::vector<pmm_submesh> submeshes;
    };

    struct volume_instance
    {
        hash_id id;
        hash_id id_technique;
        hash_id id_sampler_state;
        u32     cmp_flags;
    };

    std::vector<geometry_resource*> s_geometry_resources;
    std::vector<material_resource*> s_material_resources;
    std::vector<animation_resource> s_animation_resources;

    bool parse_pmm_contents(const c8* filename, pmm_contents& contents)
    {
        // read in file from disk
        pen_error err = pen::filesystem_read_file_to_buffer(filename, &contents.file_data, contents.file_size);
        if (err != PEN_ERR_OK || contents.file_size == 0)
        {
            dev_ui::log_level(dev_ui::console_level::error, "[error] load pmm - failed to find file: %s", filename);
            return false;
        }

        // start reading file
        const u32* p_u32reader = (u32*)contents.file_data;

        // small header.. containing the number of each sub resource
        contents.num_scene = *p_u32reader++;
        contents.num_materials = *p_u32reader++;
        contents.num_geometry = *p_u32reader++;

        // parse scenes offsets
        for (s32 i = 0; i < contents.num_scene; ++i)
            contents.scene_offsets.push_back(*p_u32reader++);

        // parse material offsets and names
        for (s32 i = 0; i < contents.num_materials; ++i)
        {
            Str name = read_parsable_string(&p_u32reader);
            contents.material_offsets.push_back(*p_u32reader++);
            contents.material_names.push_back(name);
        }

        // parse geometry offsets and names
        for (s32 i = 0; i < contents.num_geometry; ++i)
        {
            Str name = read_parsable_string(&p_u32reader);
            contents.geometry_offsets.push_back(*p_u32reader++);
            contents.geometry_names.push_back(name);
        }

        // start of sub resource data
        contents.data_start = (u8*)p_u32reader;
        return true;
    }

    bool parse_pmm_geometry(pmm_contents& contents, std::vector<pmm_geometry>& geom)
    {
        // load geometry resources
        for (u32 g = 0; g < contents.num_geometry; ++g)
        {
            pmm_geometry og;

            // read small header
            u32* p_reader = (u32*)(contents.data_start + contents.geometry_offsets[g]);

            og.version = *p_reader++;
            og.num_meshes = *p_reader++;

            if (og.version < 1)
                return false;

            // parse material names, for submeshes
            for (u32 submesh = 0; submesh < og.num_meshes; ++submesh)
                og.mat_names.push_back(read_parsable_string((const u32**)&p_reader));

            // parse submeshes
            for (u32 s = 0; s < og.num_meshes; ++s)
            {
                // extents
                pmm_submesh sm = {0};
                memcpy(&sm.min_extents, p_reader, sizeof(vec3f));
                p_reader += k_extent_floats;

                memcpy(&sm.max_extents, p_reader, sizeof(vec3f));
                p_reader += k_extent_floats;

                // parse vertex and index data
                sm.num_verts = *p_reader++;
                sm.index_size = *p_reader++;
                sm.num_pos_floats = *p_reader++;
                sm.num_vertex_floats = *p_reader++;
                sm.num_indices = *p_reader++;
                sm.num_collision_floats = *p_reader++;
                sm.skinned = *p_reader++;
                sm.num_joint_floats = *p_reader++;
                memcpy(&sm.bind_shape_matrix, p_reader, sizeof(mat4));
                p_reader += k_matrix_floats;

                sm.vertex_size = sizeof(vertex_model);
                if (sm.skinned)
                {
                    sm.vertex_size = sizeof(vertex_model_skinned);
                    sm.joint_data_size = sizeof(f32) * sm.num_joint_floats;
                    sm.joint_data = pen::memory_alloc(sm.joint_data_size);
                    memcpy(sm.joint_data, p_reader, sm.joint_data_size);
                    p_reader += sm.num_joint_floats;
                }

                // first is position only buffer
                // .. currently not optimised
                sm.position_data_size = sm.num_pos_floats * sizeof(f32);
                sm.position_data = pen::memory_alloc(sm.position_data_size);
                memcpy(sm.position_data, p_reader, sm.position_data_size);
                p_reader += sm.position_data_size / sizeof(f32);

                // second is model vertex buffer (skinned or unskinned)
                sm.vertex_data_size = sm.vertex_size * sm.num_verts;
                sm.vertex_data = pen::memory_alloc(sm.vertex_data_size);
                memcpy(sm.vertex_data, p_reader, sm.vertex_data_size);
                p_reader += sm.vertex_data_size / sizeof(f32);

                // index data
                sm.index_data_size = sm.num_indices * sm.index_size;
                sm.index_data = pen::memory_alloc(sm.index_data_size);
                memcpy(sm.index_data, p_reader, sm.index_data_size);
                p_reader = (u32*)((c8*)p_reader + sm.index_data_size);

                // collsion float data
                sm.collision_data_size = sm.num_collision_floats * sizeof(f32);
                sm.collision_data = pen::memory_alloc(sm.collision_data_size);
                memcpy(sm.collision_data, p_reader, sm.collision_data_size);
                p_reader += sm.num_collision_floats;

                og.submeshes.push_back(sm);
            }

            geom.push_back(og);
        }

        return true;
    }

    void load_pmm_geometry(const c8* filename, pmm_contents& contents)
    {
        std::vector<pmm_geometry> geom;
        parse_pmm_geometry(contents, geom);

        for (u32 g = 0; g < contents.num_geometry; ++g)
        {
            // generate hash
            pmm_geometry& gg = geom[g];

            const c8*        gname = contents.geometry_names[g].c_str();
            pen::hash_murmur hm;
            hm.begin(0);
            hm.add(filename, pen::string_length(filename));
            hm.add(gname, pen::string_length(gname));
            hash_id geom_hash = hm.end();

            // check for existing
            for (s32 g = 0; g < s_geometry_resources.size(); ++g)
                if (geom_hash == s_geometry_resources[g]->geom_hash)
                    return;

            for (u32 submesh = 0; submesh < geom[g].submeshes.size(); ++submesh)
            {
                pmm_submesh& sm = gg.submeshes[submesh];

                hm.begin(0);
                hm.add(filename, pen::string_length(filename));
                hm.add(gname, pen::string_length(gname));
                hm.add(submesh);
                hash_id sub_hash = hm.end();

                geometry_resource* p_geometry = new geometry_resource;

                // assign info
                p_geometry->p_skin = nullptr;
                p_geometry->file_hash = PEN_HASH(filename);
                p_geometry->geom_hash = geom_hash;
                p_geometry->hash = sub_hash;
                p_geometry->geometry_name = gname;
                p_geometry->filename = filename;
                p_geometry->material_name = gg.mat_names[submesh];
                p_geometry->material_id_name = PEN_HASH(gg.mat_names[submesh].c_str());
                p_geometry->submesh_index = submesh;
                p_geometry->min_extents = sm.min_extents;
                p_geometry->max_extents = sm.max_extents;
                p_geometry->num_vertices = sm.num_verts;
                p_geometry->num_indices = sm.num_indices;
                p_geometry->vertex_size = sm.vertex_size;
                p_geometry->index_type = sm.index_size == 2 ? PEN_FORMAT_R16_UINT : PEN_FORMAT_R32_UINT;

                // assign skinning
                if (sm.skinned)
                {
                    p_geometry->p_skin = (cmp_skin*)pen::memory_alloc(sizeof(cmp_skin));
                    p_geometry->p_skin->bone_cbuffer = PEN_INVALID_HANDLE;
                    p_geometry->p_skin->bind_shape_matrix = sm.bind_shape_matrix;
                    p_geometry->p_skin->num_joints = sm.num_joint_floats / k_matrix_floats;
                    memset(p_geometry->p_skin->joint_bind_matrices, 0x0, sizeof(p_geometry->p_skin->joint_bind_matrices));
                    memcpy(p_geometry->p_skin->joint_bind_matrices, sm.joint_data, sm.joint_data_size);
                }

                // assign cpu data copies
                p_geometry->cpu_position_buffer = sm.position_data;
                p_geometry->cpu_vertex_buffer = sm.vertex_data;
                p_geometry->cpu_index_buffer = sm.index_data;
                // sm.collision_data if u want

                // gpu position buffer
                pen::buffer_creation_params bcp;
                bcp.usage_flags = PEN_USAGE_DEFAULT;
                bcp.bind_flags = PEN_BIND_VERTEX_BUFFER;
                bcp.cpu_access_flags = 0;
                bcp.buffer_size = sm.position_data_size;
                bcp.data = p_geometry->cpu_position_buffer;
                p_geometry->position_buffer = pen::renderer_create_buffer(bcp);

                // gpu vertex buffer
                bcp.usage_flags = PEN_USAGE_DEFAULT;
                bcp.bind_flags = PEN_BIND_VERTEX_BUFFER;
                bcp.cpu_access_flags = 0;
                bcp.buffer_size = sm.vertex_data_size;
                bcp.data = p_geometry->cpu_vertex_buffer;
                p_geometry->vertex_buffer = pen::renderer_create_buffer(bcp);

                // gpu index buffer
                bcp.usage_flags = PEN_USAGE_DEFAULT;
                bcp.bind_flags = PEN_BIND_INDEX_BUFFER;
                bcp.cpu_access_flags = 0;
                bcp.buffer_size = sm.index_data_size;
                bcp.data = p_geometry->cpu_index_buffer;
                p_geometry->index_buffer = pen::renderer_create_buffer(bcp);

                s_geometry_resources.push_back(p_geometry);
            }
        }
    }

    void load_material_resource(const c8* filename, const c8* material_name, const void* data)
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
        static const u32 default_maps[] = {
            put::load_texture("data/textures/defaults/albedo.dds"), put::load_texture("data/textures/defaults/normal.dds"),
            put::load_texture("data/textures/defaults/spec.dds"),   put::load_texture("data/textures/defaults/spec.dds"),
            put::load_texture("data/textures/defaults/black.dds"),  put::load_texture("data/textures/defaults/black.dds")};
        static_assert(e_texture::COUNT == PEN_ARRAY_SIZE(default_maps), "mismatched defaults size");

        for (u32 map = 0; map < e_texture::COUNT; ++map)
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

    s32 load_nodes_resource(const c8* filename, ecs_scene* scene, const void* data)
    {
        const u32* p_u32reader = (const u32*)data;
        u32        version = *p_u32reader++;
        u32        num_import_nodes = *p_u32reader++;
        if (version < 1)
            return PEN_INVALID_HANDLE;

        // scene nodes
        bool has_control_rig = false;
        s32  nodes_start, nodes_end;
        get_new_entities_append(scene, num_import_nodes, nodes_start, nodes_end);

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

            scene->entities[current_node] |= e_cmp::allocated;

            if (scene->id_geometry[current_node] == ID_JOINT)
                scene->entities[current_node] |= e_cmp::bone;

            if (scene->id_name[current_node] == ID_TRAJECTORY)
                scene->entities[current_node] |= e_cmp::anim_trajectory;

            u32 num_meshes = *p_u32reader++;

            struct mat_symbol_name
            {
                hash_id symbol;
                Str     name;
            };
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

            // parent fix up to contain control rig
            if (scene->id_name[current_node] == ID_CONTROL_RIG)
            {
                has_control_rig = true;
                scene->parents[current_node] = node_zero_offset;
            }

            // if we find a trajectory node with no control rig.. we can use that to identify a rig
            if (!has_control_rig)
                if (scene->id_name[current_node] == ID_TRAJECTORY)
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
                    case e_pmm_transform::translate:
                        memcpy(&translation, p_u32reader, 12);
                        p_u32reader += 3;
                        break;
                    case e_pmm_transform::rotate:
                        memcpy(&rotations[num_rotations], p_u32reader, 16);
                        rotations[num_rotations].w = maths::deg_to_rad(rotations[num_rotations].w);
                        if (rotations[num_rotations].w < zero_rotation_epsilon &&
                            rotations[num_rotations].w > zero_rotation_epsilon)
                            rotations[num_rotations].w = 0.0f;
                        num_rotations++;
                        p_u32reader += 4;
                        break;
                    case e_pmm_transform::matrix:
                        has_matrix_transform = true;
                        memcpy(&matrix, p_u32reader, 16 * 4);
                        p_u32reader += 16;
                        break;
                    case e_pmm_transform::identity:
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
                        clone_entity(scene, current_node, dest, current_node, e_clone_mode::instantiate, vec3f::zero(),
                                     (const c8*)node_suffix.c_str());
                        scene->local_matrices[dest] = mat4::create_identity();

                        // child geometry which will inherit any skinning from its parent
                        scene->entities[dest] |= e_cmp::sub_geometry;
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
                            scene->state_flags[dest] &= ~e_state::material_initialised;
                            scene->state_flags[dest] &= ~e_state::samplers_initialised;

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
                        put::dev_ui::log_level(dev_ui::console_level::error, "[error] geometry - missing file : %s",
                                               geometry_name.c_str());
                    }
                }
            }

            current_node = dest + 1;
        }

        // now we have loaded the whole scene fix up any anim controllers
        for (s32 i = node_zero_offset; i < node_zero_offset + num_import_nodes; ++i)
        {
            if (scene->entities[i] & e_cmp::sub_geometry)
                continue;

            // parent geometry deals with skinning
            if ((scene->entities[i] & e_cmp::geometry) && scene->geometries[i].p_skin)
                instantiate_anim_controller_v2(scene, i);
        }

        return nodes_start;
    }
} // namespace

namespace put
{
    namespace ecs
    {
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
                if (hash == g->hash)
                    return g;

            return nullptr;
        }

        geometry_resource* get_geometry_resource_by_index(hash_id id_filename, u32 index)
        {
            for (auto* g : s_geometry_resources)
                if (id_filename == g->file_hash)
                    if (g->submesh_index == index)
                        return g;

            return nullptr;
        }

        animation_resource* get_animation_resource(anim_handle h)
        {
            if (h >= s_animation_resources.size())
                return nullptr;

            return &s_animation_resources[h];
        }

        material_resource* get_material_resource(hash_id hash)
        {
            for (auto* m : s_material_resources)
                if (m->hash == hash)
                    return m;

            return nullptr;
        }

        void instantiate_constraint(ecs_scene* scene, u32 node_index)
        {
            physics::constraint_params& cp = scene->physics_data[node_index].constraint;

            // hinge
            s32 rb = cp.rb_indices[0];
            cp.pivot = scene->transforms[node_index].translation - scene->physics_data[rb].rigid_body.position;

            scene->physics_handles[node_index] = physics::add_constraint(cp);
            scene->physics_data[node_index].type = e_physics_type::constraint;

            scene->entities[node_index] |= e_cmp::constraint;
        }

        void bake_rigid_body_params(ecs_scene* scene, u32 node_index)
        {
            u32 s = node_index;

            physics::rigid_body_params& rb = scene->physics_data[s].rigid_body;
            cmp_transform&              pt = scene->physics_offset[s];

            vec3f min = scene->bounding_volumes[s].min_extents;
            vec3f max = scene->bounding_volumes[s].max_extents;
            vec3f scale = scene->transforms[s].scale;

            rb.position = scene->transforms[s].translation + pt.translation;
            rb.rotation = scene->transforms[s].rotation;

            if (!(rb.create_flags & physics::e_create_flags::dimensions))
            {
                rb.dimensions = (max - min) * scale * 0.5f;

                // capsule height is extents height + radius * 2 (for the capsule top and bottom)
                if (rb.shape == physics::e_shape::capsule)
                    rb.dimensions.y -= rb.dimensions.x / 2.0f;

                // cone height is 1. (-0.5 to 0.5) but radius is 1.0;
                if (rb.shape == physics::e_shape::cone)
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

            rb.shape_up_axis = physics::e_up_axis::y;
            rb.start_matrix = start_transform;
        }

        void instantiate_rigid_body(ecs_scene* scene, u32 node_index)
        {
            u32 s = node_index;

            bake_rigid_body_params(scene, node_index);

            physics::rigid_body_params& rb = scene->physics_data[s].rigid_body;

            if (rb.shape == physics::e_shape::compound)
            {
                physics::compound_rb_params cbpr;
                cbpr.rb = nullptr;
                cbpr.base = rb;
                cbpr.num_shapes = 0;

                u32* child_handles = nullptr;
                scene->physics_handles[s] = physics::add_compound_rb(cbpr, &child_handles);
            }
            else
            {
                scene->physics_handles[s] = physics::add_rb(rb);
            }

            scene->physics_data[node_index].type = e_physics_type::rigid_body;
            scene->entities[s] |= e_cmp::physics;
        }

        using physics::rigid_body_params;
        void instantiate_compound_rigid_body(ecs_scene* scene, u32 parent, u32* children, u32 num_children)
        {
            bake_rigid_body_params(scene, parent);

            rigid_body_params* rbchild = (rigid_body_params*)pen::memory_alloc(sizeof(rigid_body_params) * num_children);

            for (u32 i = 0; i < num_children; ++i)
            {
                u32 ci = children[i];
                bake_rigid_body_params(scene, ci);
                rbchild[i] = scene->physics_data[ci].rigid_body;
            }

            physics::rigid_body_params& rb = scene->physics_data[parent].rigid_body;

            physics::compound_rb_params cbpr;
            cbpr.rb = nullptr;
            cbpr.base = rb;
            cbpr.rb = rbchild;
            cbpr.num_shapes = num_children;

            u32* child_handles = nullptr;
            scene->physics_handles[parent] = physics::add_compound_rb(cbpr, &child_handles);
            scene->physics_data[parent].type = e_physics_type::rigid_body;
            scene->entities[parent] |= e_cmp::physics;

            // fixup children
            PEN_ASSERT(sb_count(child_handles) == num_children);
            for (u32 i = 0; i < num_children; ++i)
            {
                u32 ci = children[i];
                scene->physics_handles[ci] = child_handles[i];
                scene->physics_data[ci].type = e_physics_type::compound_child;
                scene->entities[ci] |= e_cmp::physics;
            }
        }

        void destroy_physics(ecs_scene* scene, s32 node_index)
        {
            if (!(scene->entities[node_index] & e_cmp::physics))
                return;

            scene->entities[node_index] &= ~e_cmp::physics;

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
            scene->entities[node_index] |= e_cmp::geometry;

            if (gr->p_skin)
                scene->entities[node_index] |= e_cmp::skinned;

            instance->vertex_shader_class = ID_VERTEX_CLASS_BASIC;

            if (scene->entities[node_index] & e_cmp::skinned)
                instance->vertex_shader_class = ID_VERTEX_CLASS_SKINNED;
        }

        void destroy_geometry(ecs_scene* scene, u32 node_index)
        {
            if (!(scene->entities[node_index] & e_cmp::geometry))
                return;

            scene->entities[node_index] &= ~e_cmp::geometry;
            scene->entities[node_index] &= ~e_cmp::material;

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
            scene->entities[node_index] |= e_cmp::pre_skinned;
            scene->entities[node_index] &= ~e_cmp::skinned;

            geom.vertex_shader_class = ID_VERTEX_CLASS_BASIC;
        }

        void instantiate_anim_controller_v2(ecs_scene* scene, s32 node_index)
        {
            cmp_geometry* geom = &scene->geometries[node_index];

            if (geom->p_skin)
            {
                cmp_anim_controller_v2& controller = scene->anim_controller_v2[node_index];

                std::vector<s32> joint_indices;
                build_heirarchy_node_list(scene, node_index, joint_indices);

                controller.joints_offset = -1;
                for (s32 jj = 0; jj < joint_indices.size(); ++jj)
                {
                    s32 jnode = joint_indices[jj];

                    if (jnode > -1 && scene->entities[jnode] & e_cmp::bone)
                    {
                        if (controller.joints_offset == -1)
                            controller.joints_offset = jnode;

                        sb_push(controller.joint_indices, jnode);
                    }
                }

                scene->entities[node_index] |= e_cmp::anim_controller;
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
                dev_console_log_level(dev_ui::console_level::error, "[shadow] %s is not a signed distance field texture",
                                      volume_texture_filename.c_str());
                return;
            }

            scene->transforms[node_index].scale = scale;
            scene->shadows[node_index].texture_handle = volume_texture;
            scene->shadows[node_index].sampler_state = pmfx::get_render_state(id_cl, pmfx::e_render_state::sampler);
            scene->entities[node_index] |= e_cmp::sdf_shadow;
        }

        void instantiate_light(ecs_scene* scene, u32 node_index)
        {
            if (is_valid(scene->cbuffer[node_index]) && scene->cbuffer[node_index] != 0)
                return;

            // cbuffer for draw call, light volume for editor / deferred etc
            scene->entities[node_index] |= e_cmp::light;
            instantiate_model_cbuffer(scene, node_index);

            scene->bounding_volumes[node_index].min_extents = -vec3f::one();
            scene->bounding_volumes[node_index].max_extents = vec3f::one();

            scene->world_matrices[node_index] = mat4::create_identity();
            f32 rad = std::max<f32>(scene->lights[node_index].radius, 1.0f);
            scene->transforms[node_index].scale = vec3f(rad, rad, rad);
            scene->entities[node_index] |= e_cmp::transform;

            // basic defaults
            cmp_light& snl = scene->lights[node_index];
            snl.colour = vec3f::white();
            snl.radius = 1.0f;
            snl.spot_falloff = 0.001f;
            snl.cos_cutoff = 0.1f;

            area_light_resource& alr = scene->area_light_resources[node_index];
            alr.sampler_state_name = "";
            alr.texture_name = "";
            alr.shader_name = "";
            alr.technique_name = "";
        }

        void instantiate_area_light(ecs_scene* scene, u32 node_index)
        {
            geometry_resource* gr = get_geometry_resource(PEN_HASH("quad"));

            material_resource area_light_material;
            area_light_material.id_shader = PEN_HASH("pmfx_utility");
            area_light_material.id_technique = PEN_HASH("area_light_colour");
            area_light_material.material_name = "area_light_colour";
            area_light_material.shader_name = "pmfx_utility";

            instantiate_geometry(gr, scene, node_index);
            instantiate_material(&area_light_material, scene, node_index);
            instantiate_model_cbuffer(scene, node_index);

            scene->entities[node_index] |= e_cmp::light;
            scene->lights[node_index].type = e_light_type::area;
            scene->area_light[node_index].shader = PEN_INVALID_HANDLE;
        }

        void instantiate_area_light_ex(ecs_scene* scene, u32 node_index, area_light_resource& alr)
        {
            geometry_resource* gr = get_geometry_resource(PEN_HASH("quad"));

            material_resource area_light_material;
            area_light_material.id_shader = PEN_HASH("pmfx_utility");
            area_light_material.id_technique = PEN_HASH("area_light_texture");
            area_light_material.material_name = "area_light_texture";
            area_light_material.shader_name = "pmfx_utility";

            instantiate_geometry(gr, scene, node_index);
            instantiate_material(&area_light_material, scene, node_index);
            instantiate_model_cbuffer(scene, node_index);

            scene->entities[node_index] |= e_cmp::light;
            scene->lights[node_index].type = e_light_type::area_ex;

            if (!alr.texture_name.empty())
            {
                scene->area_light[node_index].texture_handle = put::load_texture(alr.texture_name.c_str());
            }

            if (!alr.shader_name.empty())
            {
                scene->area_light[node_index].shader = pmfx::load_shader(alr.shader_name.c_str());
                scene->area_light[node_index].technique = PEN_HASH(alr.technique_name.c_str());
            }
            else
            {
                if (!alr.texture_name.empty())
                {
                    // default to basic texture
                }
            }

            // store for later for save load.
            scene->area_light_resources[node_index] = alr;
        }

        void instantiate_material(material_resource* mr, ecs_scene* scene, u32 node_index)
        {
            scene->id_material[node_index] = mr->hash;
            scene->material_names[node_index] = mr->material_name;

            scene->entities[node_index] |= e_cmp::material;

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

            for (u32 i = 0; i < e_texture::COUNT; ++i)
            {
                if (!mr->id_sampler_state[i])
                    mr->id_sampler_state[i] = id_default_sampler_state;
            }

            scene->material_resources[node_index] = *mr;

            bake_material_handles(scene, node_index);
        }

        void permutation_flags_from_vertex_class(u32& permutation, hash_id vertex_class)
        {
            u32 clear_vertex = ~(e_shader_permutation::skinned | e_shader_permutation::instanced);
            permutation &= clear_vertex;

            if (vertex_class == ID_VERTEX_CLASS_SKINNED)
                permutation |= e_shader_permutation::skinned;

            if (vertex_class == ID_VERTEX_CLASS_INSTANCED)
                permutation |= e_shader_permutation::instanced;
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

            if (!(scene->state_flags[node_index] & e_state::material_initialised))
            {
                pmfx::initialise_constant_defaults(material->shader, material->technique_index,
                                                   scene->material_data[node_index].data);

                scene->state_flags[node_index] |= e_state::material_initialised;
            }

            instantiate_material_cbuffer(scene, node_index, cbuffer_size);

            // material samplers
            if (!(scene->state_flags[node_index] & e_state::samplers_initialised))
            {
                pmfx::initialise_sampler_defaults(material->shader, material->technique_index, samplers);

                // set material texture from source data
                for (u32 t = 0; t < e_texture::COUNT; ++t)
                {
                    if (resource->texture_handles[t] != 0 && is_valid(resource->texture_handles[t]))
                    {
                        for (u32 s = 0; s < e_pmfx_constants::max_technique_sampler_bindings; ++s)
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

                scene->entities[node_index] |= e_cmp::samplers;
                scene->state_flags[node_index] |= e_state::samplers_initialised;
            }

            // bake ss handles
            for (u32 s = 0; s < e_pmfx_constants::max_technique_sampler_bindings; ++s)
                if (samplers.sb[s].id_sampler_state != 0)
                    samplers.sb[s].sampler_state =
                        pmfx::get_render_state(samplers.sb[s].id_sampler_state, pmfx::e_render_state::sampler);
        }

        void bake_material_handles()
        {
            ecs_scene_list* scenes = get_scenes();
            for (auto& si : *scenes)
            {
                ecs_scene* scene = si.scene;

                for (u32 n = 0; n < scene->soa_size; ++n)
                {
                    if (scene->entities[n] & e_cmp::material)
                        bake_material_handles(scene, n);
                }
            }
        }

        anim_handle load_pma(const c8* filename)
        {
            Str pd = put::dev_ui::get_program_preference_filename("project_dir");

            Str stipped_filename = pen::str_replace_string(filename, pd.c_str(), "");

            hash_id filename_hash = PEN_HASH(stipped_filename.c_str());

            // search for existing
            s32 num_anims = s_animation_resources.size();
            for (s32 i = 0; i < num_anims; ++i)
            {
                if (s_animation_resources[i].id_name == filename_hash)
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

            s_animation_resources.push_back(animation_resource());
            animation_resource& new_animation = s_animation_resources.back();

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
                    PEN_UNUSED(type);
                    u32 target = *p_u32reader++;

                    // read float buffer
                    u32 num_elements = *p_u32reader++;

                    if (sematic == e_anim_semantics::interpolation)
                    {
                        PEN_ASSERT(type == e_anim_data::type_int);

                        u32* data = new u32[num_elements];
                        memcpy(data, p_u32reader, sizeof(u32) * num_elements);
                        new_animation.channels[i].interpolation = data;
                    }
                    else if (sematic == e_anim_semantics::time)
                    {
                        PEN_ASSERT(type == e_anim_data::type_float);

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
                            case e_anim_target::transform:
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

                                    //tq[0][m] = normalised(rot);
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

                            case e_anim_target::translate_x:
                            case e_anim_target::translate_y:
                            case e_anim_target::translate_z:
                                new_animation.channels[i].offset[target - e_anim_target::translate_x] = (f32*)data;
                                break;
                            case e_anim_target::rotate_x:
                            case e_anim_target::rotate_y:
                            case e_anim_target::rotate_z:
                            {
                                new_animation.channels[i].rotation[num_rots] = new quat[num_floats];

                                for (u32 q = 0; q < num_floats; ++q)
                                {
                                    vec3f mask[] = {vec3f::unit_x(), vec3f::unit_y(), vec3f::unit_z()};

                                    vec3f vr = vec3f(data[q]) * mask[target - e_anim_target::rotate_x];
                                    new_animation.channels[i].rotation[num_rots][q] = quat(vr.z, vr.y, vr.x);
                                }

                                num_rots++;
                            }
                            break;
                            case e_anim_target::scale_x:
                            case e_anim_target::scale_y:
                            case e_anim_target::scale_z:
                                new_animation.channels[i].scale[target - e_anim_target::scale_x] = (f32*)data;
                                break;
                            case e_anim_target::translate:
                                new_animation.channels[i].offset[sematic - e_anim_semantics::x] = (f32*)data;
                                break;
                            case e_anim_target::rotate:
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
                            case e_anim_target::scale:
                                new_animation.channels[i].scale[sematic - e_anim_semantics::x] = (f32*)data;
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
                        soa.channels[c].element_offset[elm++] = e_anim_output::translate_x + i;

                // scale
                for (u32 i = 0; i < 3; ++i)
                    if (channel.scale[i])
                        soa.channels[c].element_offset[elm++] = e_anim_output::scale_x + i;

                // quaternion
                for (u32 i = 0; i < 3; ++i)
                    if (channel.rotation[i])
                        for (u32 q = 0; q < 4; ++q)
                            soa.channels[c].element_offset[elm++] = e_anim_output::quaternion;

                if (channel.matrices)
                {
                    // baked
                    soa.channels[c].flags = e_anim_flags::baked_quaternion;
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

            return (anim_handle)s_animation_resources.size() - 1;
        }

        void optimise_pmm(const c8* input_filename, const c8* output_filename)
        {
            pmm_contents contents;
            if(!parse_pmm_contents(input_filename, contents))
                return;

            std::vector<pmm_geometry> geom;
            parse_pmm_geometry(contents, geom);

            // perform optimisations on each submesh
            std::vector<intptr_t> reductions;
            u32                   mc = 0;
            for (auto& g : geom)
            {
                // reduction could be negative in theory..
                // ... especially as index size goes from u16 > u32 so handle it with signed types
                intptr_t reduction = 0;
                for (auto& sm : g.submeshes)
                {
                    // alloc space for indices
                    size_t _ib_size = sm.num_indices * sizeof(u32);
                    u32*   remap = (u32*)pen::memory_alloc(_ib_size);
                    u32*   ni = (u32*)pen::memory_alloc(_ib_size);
                    
                    // to 32 bit indices
                    if(sm.index_size == 2)
                    {
                        u16* i16 = (u16*)sm.index_data;
                        u32* i32 = (u32*)pen::memory_alloc(sm.num_indices*sizeof(u32));
                        for(u32 i = 0; i < sm.num_indices; ++i)
                            i32[i] = i16[i];
                            
                        pen::memory_free(sm.index_data);
                        sm.index_data = (void*)i32;
                    }

                    // generate efficient index buffer and reduce vertex count
                    size_t vertex_count = meshopt_generateVertexRemap(&remap[0], (u32*)sm.index_data, sm.num_indices, sm.vertex_data,
                                                                      sm.num_verts, sm.vertex_size);

                    meshopt_remapIndexBuffer(ni, nullptr, sm.num_indices, &remap[0]);

                    // alloc new vertex buffer
                    size_t _vb_size = sm.vertex_size * vertex_count; // optimised / reduced size
                    void*  nv = pen::memory_alloc(sm.vertex_size * vertex_count);

                    // todo, position only, collision

                    // remap
                    meshopt_remapVertexBuffer(nv, sm.vertex_data, sm.num_indices, sm.vertex_size, &remap[0]);

                    // swap winding..
                    for (u32 i = 0; i < sm.num_indices; i += 3)
                        std::swap(ni[i], ni[i + 2]);

                    // reduce index size to u16?
                    sm.index_size = 4;

                    // cleanup the old / temp buffers
                    pen::memory_free(sm.vertex_data);
                    pen::memory_free(sm.index_data);
                    pen::memory_free(remap);

                    // track data reductions
                    intptr_t vbr = (intptr_t)_vb_size - (intptr_t)sm.vertex_data_size;
                    intptr_t ibr = (intptr_t)_ib_size - (intptr_t)sm.index_data_size;

                    reduction += vbr + ibr;

                    // reassign
                    PEN_LOG("    new vertex count: %i, old %i", sm.num_verts, vertex_count);
                    sm.vertex_data = nv;
                    sm.vertex_data_size = _vb_size;
                    sm.index_data = ni;
                    sm.index_data_size = _ib_size;
                    sm.num_verts = vertex_count;
                    sm.num_vertex_floats = _vb_size / sizeof(f32);

                    mc++;
                }
                reductions.push_back(reduction);
            }

            // work out the offset adjustments, geom is at the end so we dont need to bother with the last one.
            for (u32 g = 1; g < contents.num_geometry; ++g)
                contents.geometry_offsets[g] += reductions[g - 1];

            // ..
            // below can be refactored as write pmm

            // array of offsets to cacluate size of a block
            std::vector<size_t> offsets;
            for (u32 i = 0; i < contents.num_scene; ++i)
                offsets.push_back(contents.scene_offsets[i]);
            for (u32 i = 0; i < contents.num_materials; ++i)
                offsets.push_back(contents.material_offsets[i]);
            for (u32 i = 0; i < contents.num_geometry; ++i)
                offsets.push_back(contents.geometry_offsets[i]);
            offsets.push_back(contents.file_size);

            // write the file back
            std::ofstream ofs(output_filename, std::ofstream::binary);
            ofs.write((const c8*)&contents.num_scene, sizeof(u32));
            ofs.write((const c8*)&contents.num_materials, sizeof(u32));
            ofs.write((const c8*)&contents.num_geometry, sizeof(u32));

            // write strings and offsets
            for (u32 s = 0; s < contents.num_scene; ++s)
            {
                ofs.write((const c8*)&contents.scene_offsets[s], sizeof(u32));
            }

            for (u32 m = 0; m < contents.num_materials; ++m)
            {
                write_parsable_string_u32(contents.material_names[m], ofs);
                ofs.write((const c8*)&contents.material_offsets[m], sizeof(u32));
            }

            for (u32 g = 0; g < contents.num_geometry; ++g)
            {
                write_parsable_string_u32(contents.geometry_names[g], ofs);
                ofs.write((const c8*)&contents.geometry_offsets[g], sizeof(u32));
            }

            // scenes
            u32 cur_offset = 0;
            for (u32 s = 0; s < contents.num_scene; ++s)
            {
                const c8* p_scene_data = (const c8*)(contents.data_start + contents.scene_offsets[s]);
                ofs.write(p_scene_data, offsets[cur_offset + 1] - contents.scene_offsets[s]);
                cur_offset++;
            }

            // materials
            for (u32 m = 0; m < contents.num_materials; ++m)
            {
                const c8* p_mat_data = (const c8*)(contents.data_start + contents.material_offsets[m]);
                ofs.write(p_mat_data, offsets[cur_offset + 1] - contents.material_offsets[m]);
                cur_offset++;
            }

            // finally optimised geom we needs to be properly written
            for (u32 g = 0; g < contents.num_geometry; ++g)
            {
                // header and material names
                ofs.write((const c8*)&geom[g].version, sizeof(u32));
                ofs.write((const c8*)&geom[g].num_meshes, sizeof(u32));
                for (auto& mm : geom[g].mat_names)
                    write_parsable_string_u32(mm, ofs);

                // submeshes
                for (auto& sm : geom[g].submeshes)
                {
                    // header
                    ofs.write((const c8*)&sm.min_extents, sizeof(vec3f));
                    ofs.write((const c8*)&sm.max_extents, sizeof(vec3f));
                    ofs.write((const c8*)&sm.num_verts, sizeof(u32));
                    ofs.write((const c8*)&sm.index_size, sizeof(u32));
                    ofs.write((const c8*)&sm.num_pos_floats, sizeof(u32));
                    ofs.write((const c8*)&sm.num_vertex_floats, sizeof(u32));
                    ofs.write((const c8*)&sm.num_indices, sizeof(u32));
                    ofs.write((const c8*)&sm.num_collision_floats, sizeof(u32));
                    ofs.write((const c8*)&sm.skinned, sizeof(u32));
                    ofs.write((const c8*)&sm.num_joint_floats, sizeof(u32));
                    ofs.write((const c8*)&sm.bind_shape_matrix, sizeof(mat4));
                    // data buffers
                    ofs.write((const c8*)sm.joint_data, sm.joint_data_size);
                    ofs.write((const c8*)sm.position_data, sm.position_data_size);
                    ofs.write((const c8*)sm.vertex_data, sm.vertex_data_size);
                    ofs.write((const c8*)sm.index_data, sm.index_data_size);
                    ofs.write((const c8*)sm.collision_data, sm.collision_data_size);
                }
            }

            ofs.close();

            // cleanup memory
            for (auto& g : geom)
            {
                for (auto& sm : g.submeshes)
                {
                    pen::memory_free(sm.vertex_data);
                    pen::memory_free(sm.position_data);
                    pen::memory_free(sm.index_data);
                    pen::memory_free(sm.joint_data);
                    pen::memory_free(sm.collision_data);
                }
            }
            pen::memory_free(contents.file_data);
        }

        void optimise_pma(const c8* input_filename, const c8* output_filename)
        {
            // todo
        }

        s32 load_pmm(const c8* filename, ecs_scene* scene, u32 load_flags)
        {
            // pmm contains scene node, material, and geometry resources
            pmm_contents contents;
            parse_pmm_contents(filename, contents);

            // load material resources
            if (load_flags & e_pmm_load_flags::material)
            {
                for (u32 m = 0; m < contents.num_materials; ++m)
                {
                    u32* p_mat_data = (u32*)(contents.data_start + contents.material_offsets[m]);
                    load_material_resource(filename, contents.material_names[m].c_str(), p_mat_data);
                }
            }

            // load geometry resources
            if (load_flags & e_pmm_load_flags::geometry)
                load_pmm_geometry(filename, contents);

            // load nodes.. we need to do this last because they depend on the material and geometry resources.
            s32 root = PEN_INVALID_HANDLE;
            if (load_flags & e_pmm_load_flags::nodes)
            {
                for (u32 s = 0; s < contents.num_scene; ++s)
                {
                    u32* p_scene_data = (u32*)(contents.data_start + contents.scene_offsets[s]);
                    u32  scene_start = load_nodes_resource(filename, scene, p_scene_data);
                    if (s == 0)
                        root = scene_start;
                }

                // invalidate trees to rebuild
                if (contents.num_scene > 0)
                    if (scene)
                        scene->flags |= e_scene_flags::invalidate_scene_tree;
            }

            pen::memory_free(contents.file_data);
            return root;
        }

        s32 load_pmv(const c8* filename, ecs_scene* scene)
        {
            pen::json pmv = pen::json::load_from_file(filename);

            Str volume_texture_filename = pmv["filename"].as_str();
            u32 volume_texture = put::load_texture(volume_texture_filename.c_str());

            vec3f scale = vec3f(pmv["scale_x"].as_f32(), pmv["scale_y"].as_f32(), pmv["scale_z"].as_f32());

            hash_id id_type = pmv["volume_type"].as_hash_id();

            static volume_instance vi[] = {
                {PEN_HASH("volume_texture"), PEN_HASH("volume_texture"), PEN_HASH("clamp_point"), e_cmp::volume},

                {PEN_HASH("signed_distance_field"), PEN_HASH("volume_sdf"), PEN_HASH("clamp_linear"), e_cmp::sdf_shadow}};

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

            add_material_resource(material);

            geometry_resource* cube = get_geometry_resource(PEN_HASH("cube"));

            vec3f pos = vec3f::zero();

            u32 v = get_new_entity(scene);

            scene->names[v] = "volume";
            scene->names[v].appendf("%i", v);
            scene->transforms[v].rotation = quat();
            scene->transforms[v].scale = scale;
            scene->transforms[v].translation = pos;
            scene->entities[v] |= e_cmp::transform;
            scene->parents[v] = v;

            scene->samplers[v].sb[0].sampler_unit = e_texture::volume;
            scene->samplers[v].sb[0].handle = volume_texture;
            scene->samplers[v].sb[0].sampler_state =
                pmfx::get_render_state(vi[i].id_sampler_state, pmfx::e_render_state::sampler);

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
