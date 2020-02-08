// ecs_resources.h
// Copyright 2014 - 2019 Alex Dixon.
// License: https://github.com/polymonster/pmtech/blob/master/license.md

#pragma once

#include "ecs/ecs_scene.h"

namespace put
{
    namespace ecs
    {
        namespace e_anim_flags
        {
            enum anim_flags_t
            {
                play = 1,
                apply_root_motion = 1 << 1,
                baked_quaternion = 1 << 2,
                looped = 1 << 3,
                paused = 1 << 4
            };
        }
        typedef u32 anim_flags;
        
        namespace e_anim_semantics
        {
            enum anim_semantics_t
            {
                time = 0,
                transform,
                x,
                y,
                z,
                angle,
                interpolation
            };
        }
        typedef e_anim_semantics::anim_semantics_t anim_semantics;
        
        namespace e_anim_interpolation
        {
            enum anim_interpolation_t
            {
                linear = 0,
                bezier,
                cardinal,
                hermite,
                bspline,
                step
            };
        }
        typedef e_anim_interpolation::anim_interpolation_t anim_interpolation;
        
        namespace e_anim_data
        {
            enum anim_data_t
            {
                type_float = 0,
                type_float4x4,
                type_int,
            };
        }
        typedef e_anim_data::anim_data_t anim_data;
        
        namespace e_anim_target
        {
            enum anim_target_t
            {
                transform = 0,
                translate,
                rotate,
                scale,
                translate_x,
                translate_y,
                translate_z,
                rotate_x,
                rotate_y,
                rotate_z,
                scale_x,
                scale_y,
                scale_z
            };
        }
        
        namespace e_anim_output
        {
            enum anim_output_t
            {
                translate_x = 0,
                translate_y = 1,
                translate_z = 2,
                scale_x = 6,
                scale_y = 7,
                scale_z = 8,
                quaternion = 9
            };
        }
        
        namespace e_pmm_load_flags
        {
            enum pmm_load_flags_t
            {
                geometry = 1<<0,
                material = 1<<1,
                nodes = 1<<2,
                all = (geometry | material | nodes)
            };
        }
        typedef u32 pmm_load_flags;
        
        struct anim_info
        {
            f32 time;
            u32 interpolation;
            u32 offset;
        };

        struct anim_channel
        {
            u32 num_frames;
            u32 element_count;
            u32 element_offset[21];
            u32 flags = 0;
        };

        struct soa_anim
        {
            u32           num_channels = 0;
            anim_channel* channels = nullptr;
            anim_info**   info = nullptr; // [frame][samplers]
            f32**         data = nullptr; // [frame][sampler offset]
        };

        struct anim_sampler
        {
            u32 pos;
            u32 joint;
            u32 flags;
            f32 cur_t;
            f32 prev_t;
        };

        struct anim_target
        {
            f32  t[9]; // translate xyz, scale xyz.
            quat q;
            u32  flags = 0;
        };

        struct anim_instance
        {
            u32            flags = 0;
            soa_anim       soa;
            f32            time = 0.0f;
            f32            length = 0.0f; // length in time
            anim_target*   targets = nullptr;
            cmp_transform* joints = nullptr;
            anim_sampler*  samplers = nullptr;
            vec3f          root_translation;
            vec3f          root_delta = vec3f::zero();
        };

        struct animation_channel
        {
            u32     num_frames;
            hash_id target;
            f32*    times;
            mat4*   matrices;
            f32*    offset[3];
            f32*    scale[3];
            quat*   rotation[3];
            u32*    interpolation;
            Str     target_name;
            u32     target_node_index;
            s32     processed_frame = -1;
        };

        struct animation_resource
        {
            hash_id id_name;
            u32     node;

            u32                num_channels;
            animation_channel* channels;
            bool               remap_channels = false;

            f32 length;
            Str name;

            soa_anim soa;
        };

        struct geometry_resource
        {
            hash_id file_hash;
            hash_id geom_hash; // mesh
            hash_id hash;      // submesh
            hash_id material_id_name;

            Str filename;
            Str geometry_name;
            Str material_name;
            u32 submesh_index;
            u32 position_buffer;
            u32 vertex_buffer;
            u32 index_buffer;
            u32 num_indices;
            u32 num_vertices;
            u32 index_type;
            u32 material_index;
            u32 vertex_size;

            vec3f min_extents;
            vec3f max_extents;

            void* cpu_index_buffer;
            void* cpu_position_buffer;
            void* cpu_vertex_buffer;

            cmp_skin* p_skin;
        };

        struct vertex_2d
        {
            vec4f pos;
            vec4f texcoord;
        };

        struct vertex_model
        {
            vec4f pos;
            vec4f normal;
            vec4f uv12;
            vec4f tangent;
            vec4f bitangent;

            vertex_model(){};
        };

        struct vertex_model_skinned
        {
            vec4f pos;
            vec4f normal;
            vec4f uv12;
            vec4f tangent;
            vec4f bitangent;
            vec4i blend_indices;
            vec4f blend_weights;

            vertex_model_skinned(){};
        };

        struct vertex_position
        {
            f32 x, y, z, w;
        };

        void save_scene(const c8* filename, ecs_scene* scene);
        void save_sub_scene(ecs_scene* scene, u32 root);
        void load_scene(const c8* filename, ecs_scene* scene, bool merge = false);

        s32 load_pmm(const c8* model_scene_name, ecs_scene* scene = nullptr, u32 load_flags = e_pmm_load_flags::all);
        s32 load_pma(const c8* model_scene_name);
        s32 load_pmv(const c8* filename, ecs_scene* scene);

        void instantiate_rigid_body(ecs_scene* scene, u32 node_index);
        void instantiate_compound_rigid_body(ecs_scene* scene, u32 parent, u32* children, u32 num_children);
        void instantiate_constraint(ecs_scene* scene, u32 node_index);
        void instantiate_geometry(geometry_resource* gr, ecs_scene* scene, s32 node_index);
        void instantiate_geometry_ref(geometry_resource* gr, ecs_scene* scene, s32 node_index);
        void instantiate_model_pre_skin(ecs_scene* scene, s32 node_index);
        void instantiate_model_cbuffer(ecs_scene* scene, s32 node_index);
        void instantiate_material_cbuffer(ecs_scene* scene, s32 node_index, s32 size);
        void instantiate_anim_controller_v2(ecs_scene* scene, s32 node_index);
        void instantiate_material(material_resource* mr, ecs_scene* scene, u32 node_index);
        void instantiate_sdf_shadow(const c8* pmv_filename, ecs_scene* scene, u32 node_index);
        void instantiate_light(ecs_scene* scene, u32 node_index);
        void instantiate_area_light(ecs_scene* scene, u32 node_index);
        void instantiate_area_light_ex(ecs_scene* scene, u32 node_index, area_light_resource& alr);

        void destroy_geometry(ecs_scene* scene, u32 node_index);
        void destroy_physics(ecs_scene* scene, s32 node_index);

        void bake_rigid_body_params(ecs_scene* scene, u32 node_index);
        void bake_material_handles(ecs_scene* scene, u32 node_index);
        void bake_material_handles();

        void create_geometry_primitives();

        void add_geometry_resource(geometry_resource* gr);
        void add_material_resource(material_resource* mr);

        material_resource*  get_material_resource(hash_id hash);
        animation_resource* get_animation_resource(anim_handle h);
        geometry_resource*  get_geometry_resource(hash_id h);
        geometry_resource*  get_geometry_resource_by_index(hash_id id_filename, u32 index);
    } // namespace ecs
} // namespace put
