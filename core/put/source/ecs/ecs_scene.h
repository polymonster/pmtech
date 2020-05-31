// ecs_scene.h
// Copyright 2014 - 2019 Alex Dixon.
// License: https://github.com/polymonster/pmtech/blob/master/license.md

#pragma once

#include "camera.h"
#include "loader.h"
#include "physics/physics.h"
#include "pmfx.h"

#include "data_struct.h"
#include "pen.h"

#include "maths/maths.h"
#include "maths/quat.h"

#include "str/Str.h"

#include <vector>

namespace put
{
    struct scene_view;
    typedef s32 anim_handle;

    namespace ecs
    {
        struct anim_instance;
        struct ecs_scene;

        namespace e_scene_view_flags
        {
            enum sv_flags_t
            {
                none = 0,
                hide = 1 << 0,
                hide_debug = 1 << 1,
                node = 1 << 2,
                grid = 1 << 3,
                matrix = 1 << 4,
                bones = 1 << 5,
                aabb = 1 << 6,
                lights = 1 << 7,
                physics = 1 << 8,
                selected_children = 1 << 8,
                camera = 1 << 9,
                geometry = 1 << 10,
                COUNT = 12,

                defaults = (node | grid)
            };
        }
        typedef u32 scene_view_flags;

        namespace e_scene_flags
        {
            enum s_flags_t
            {
                none = 0,
                invalidate_scene_tree = 1 << 1,
                pause_update = 1 << 2
            };
        }
        typedef u32 scene_flags;

        namespace e_state
        {
            enum state_flags_t
            {
                selected = (1 << 0),
                child_selected = (1 << 1),
                hidden = (1 << 2),
                material_initialised = (1 << 3),
                no_shadow = (1 << 4),
                samplers_initialised = (1 << 5),
                apply_anim_transform = (1 << 6),
                sync_physics_transform = (1 << 7),
                alpha_blended = (1 << 0)
            };
        }

        static const f32 k_dir_light_offset = 1000000.0f;
        namespace e_light_type
        {
            enum light_type_t
            {
                dir,
                point,
                spot,
                area,
                area_ex
            };
        }

        namespace e_texture
        {
            enum texture_t
            {
                albedo = 0,
                normal_map,
                specular_map,
                env_map,
                volume,
                emissive_map,
                COUNT
            };
        }

        namespace e_global_textures
        {
            enum global_textures_t
            {
                shadow_map = 15,
                sdf_shadow = 14,
                omni_shadow_map = 13
            };
        }

        namespace e_physics_type
        {
            enum physics_type_t
            {
                rigid_body,
                constraint,
                compound_child
            };
        }

        namespace e_scene_limits
        {
            enum scene_limits_t
            {
                max_forward_lights = 100,
                max_area_lights = 10,
                max_shadow_maps = 100,
                max_sdf_shadows = 1,
                max_omni_shadow_maps = 100
            };
        }

        namespace e_cmp
        {
            enum cmp_t
            {
                allocated = (1 << 0),
                geometry = (1 << 1),
                physics = (1 << 2),
                physics_multi = (1 << 3),
                material = (1 << 4),
                // 1<<5 unused
                skinned = (1 << 6),
                bone = (1 << 7),
                dynamic = (1 << 8),
                anim_controller = (1 << 9),
                anim_trajectory = (1 << 10),
                light = (1 << 11),
                transform = (1 << 12),
                constraint = (1 << 13),
                sub_instance = (1 << 14),
                master_instance = (1 << 15),
                pre_skinned = (1 << 16),
                sub_geometry = (1 << 17),
                sdf_shadow = (1 << 18),
                volume = (1 << 19),
                samplers = (1 << 20)
            };
        }
        
        namespace e_light_flags
        {
            enum light_flags_t : u8
            {
                shadow_map = 1<<0,
                omni_shadow_map = 1<<1,
                global_illumination = 1<<2
            };
        };
        typedef u8 light_flags;

        struct cmp_draw_call
        {
            mat4  world_matrix;
            vec4f v1; // generic data 1
            vec4f v2; // generic data 2
            mat4  world_matrix_inv_transpose;
        };

        struct cmp_skin
        {
            u32  num_joints;
            mat4 bind_shape_matrix;
            mat4 joint_bind_matrices[85];
            u32  bone_cbuffer = PEN_INVALID_HANDLE;
        };

        // contains handles and data to re-create a material from scratch
        // material resources could be re-used created and shared
        struct material_resource
        {
            hash_id hash;
            Str     material_name;
            Str     shader_name;
            f32     data[64];
            hash_id id_shader = 0;
            hash_id id_technique = 0;
            hash_id id_sampler_state[e_texture::COUNT] = {0};
            s32     texture_handles[e_texture::COUNT] = {0};
        };

        // contains baked handles for o(1) time setting of technique / shader
        // unique per-instance cbuffer
        struct cmp_material
        {
            u32 material_cbuffer = PEN_INVALID_HANDLE;
            u32 material_cbuffer_size = 0;
            u32 shader;
            u32 technique_index;
        };

        // from pmfx per instance material cbuffer data and samplers
        typedef technique_constant_data cmp_material_data; // upto 64 floats of data stored in material cbuffer
        typedef sampler_set             cmp_samplers;      // 8 samplers which can bind to any slots

        struct cmp_physics
        {
            s32 type;

            union {
                physics::rigid_body_params rigid_body;
                physics::constraint_params constraint;
            };

            cmp_physics(){};
            ~cmp_physics(){};

            cmp_physics& operator=(const cmp_physics& other)
            {
                memcpy(this, &other, sizeof(cmp_physics));
                return *this;
            }
        };

        struct cmp_bounding_volume
        {
            vec3f min_extents;
            vec3f max_extents;
            vec3f transformed_min_extents;
            vec3f transformed_max_extents;
            f32   radius;
        };

        struct extents
        {
            vec3f min;
            vec3f max;
        };
        
        struct cmp_geometry
        {
            u32       position_buffer;
            u32       vertex_buffer;
            u32       index_buffer;
            u32       num_indices;
            u32       num_vertices;
            u32       index_type;
            u32       vertex_size;
            cmp_skin* p_skin;
            hash_id   vertex_shader_class;
        };

        struct cmp_pre_skin
        {
            u32 vertex_buffer;
            u32 position_buffer;
            u32 vertex_size;
            u32 num_verts;
        };

        struct cmp_master_instance
        {
            u32 num_instances;
            u32 instance_buffer;
            u32 instance_stride;
        };

        struct anim_blend
        {
            u32 anim_a = 0;
            u32 anim_b = 0;
            f32 ratio = 0.0f;
        };

        struct cmp_anim_controller_v2
        {
            anim_instance* anim_instances = nullptr;
            u32*           joint_indices = nullptr; // indices into the scene hierarchy
            u8*            joint_flags = nullptr;
            anim_blend     blend;
            u32            joints_offset;
        };

        struct cmp_light
        {
            u32         type;
            vec3f       colour;
            f32         radius = 10.0f;
            f32         spot_falloff = 0.05f;
            f32         cos_cutoff = -(f32)M_PI / 4.0f;
            f32         azimuth;
            f32         altitude;
            vec3f       direction;
            light_flags flags;
        };

        // run time shader / texture for area light textures
        struct cmp_area_light
        {
            u32     texture_handle = PEN_INVALID_HANDLE;
            u32     sampler_state = PEN_INVALID_HANDLE;
            u32     shader = PEN_INVALID_HANDLE;
            hash_id technique = PEN_INVALID_HANDLE;
        };

        // data to save / load and reconstruct area light textures
        struct area_light_resource
        {
            Str shader_name;
            Str technique_name;
            Str texture_name;
            Str sampler_state_name;
        };

        typedef maths::transform cmp_transform;

        struct cmp_shadow
        {
            u32 texture_handle; // texture handle for sdf
            u32 sampler_state;
        };

        struct light_data
        {
            vec4f pos_radius; // radius = point radius and spot length
            vec4f dir_cutoff; // spot dir and cos cutoff
            vec4f colour;     // w = boolean cast shadow
            vec4f data;       // x = spot falloff, yzw reserved
        };

        struct forward_light_buffer
        {
            vec4f      info;
            light_data lights[e_scene_limits::max_forward_lights];
        };

        struct distance_field_shadow
        {
            mat4 world_matrix;
            mat4 world_matrix_inverse;
        };

        struct distance_field_shadow_buffer
        {
            distance_field_shadow shadows;
        };

        struct area_light
        {
            vec4f corners[4];
            vec4f colour; // w can hold the index of a texture array slice to sample.
        };

        struct area_light_buffer
        {
            vec4f      info;
            area_light lights[e_scene_limits::max_area_lights];
        };
        
        struct gi_volume_info
        {
            vec4f   scene_size;
            vec4f   volume_size;
        };

        struct free_node_list
        {
            u32             node;
            free_node_list* next;
            free_node_list* prev;
        };

        template <typename T>
        struct cmp_array
        {
            u32 size = sizeof(T);
            T*  data = nullptr;

            T&       operator[](size_t index);
            const T& operator[](size_t index) const;
        };

        struct generic_cmp_array
        {
            u32   size;
            void* data;

            void* operator[](size_t index);
        };

        struct ecs_extension
        {
            Str                name;
            hash_id            id_name = 0;
            void*              context;
            generic_cmp_array* components;
            u32                num_components;

            void* (*ext_func)(ecs_scene*) = nullptr;                    // must implement.. registers ext with scene
            void (*shutdown)(ecs_extension&) = nullptr;                 // must implement.. frees mem
            void (*browser_func)(ecs_extension&, ecs_scene*) = nullptr; // component editor ui
            void (*load_func)(ecs_extension&, ecs_scene*) = nullptr;    // fix up any loaded resources and read lookup strings
            void (*save_func)(ecs_extension&, ecs_scene*) = nullptr;    // fix down any save info.. write lookup strings etc
            void (*update_func)(ecs_extension&, ecs_scene*, f32) = nullptr; // update with dt
        };

        struct ecs_controller
        {
            Str          name;
            hash_id      id_name = 0;
            put::camera* camera = nullptr;
            void*        context = nullptr;

            void (*update_func)(ecs_controller&, ecs_scene* scene, f32 dt) = nullptr;
            void (*post_update_func)(ecs_controller&, ecs_scene* scene, f32 dt) = nullptr;
        };

        struct ecs_scene
        {
            static const u32 k_version = 9;

            ecs_scene()
            {
                num_base_components = (u32)(((size_t)&num_base_components) - ((size_t)&entities)) / sizeof(generic_cmp_array);
                num_components = num_base_components;
            };

            // Components version 4
            cmp_array<u64>                          entities;
            cmp_array<u64>                          state_flags;
            cmp_array<hash_id>                      id_name;
            cmp_array<hash_id>                      id_geometry;
            cmp_array<hash_id>                      id_material;
            cmp_array<Str>                          names;
            cmp_array<Str>                          geometry_names;
            cmp_array<Str>                          material_names;
            cmp_array<u32>                          parents;
            cmp_array<cmp_transform>                transforms;
            cmp_array<mat4>                         local_matrices;
            cmp_array<mat4>                         world_matrices;
            cmp_array<mat4>                         offset_matrices;
            cmp_array<mat4>                         physics_matrices;
            cmp_array<cmp_bounding_volume>          bounding_volumes;
            cmp_array<cmp_light>                    lights;
            cmp_array<u32>                          physics_handles;
            cmp_array<cmp_master_instance>          master_instances;
            cmp_array<cmp_geometry>                 geometries;
            cmp_array<cmp_pre_skin>                 pre_skin;
            cmp_array<cmp_physics>                  physics_data;
            cmp_array<cmp_geometry>                 position_geometries;
            cmp_array<u32>                          cbuffer;
            cmp_array<cmp_draw_call>                draw_call_data;
            cmp_array<free_node_list>               free_list;
            cmp_array<cmp_material>                 materials;
            cmp_array<cmp_material_data>            material_data;
            cmp_array<material_resource>            material_resources;
            cmp_array<cmp_shadow>                   shadows;
            cmp_array<cmp_samplers>                 samplers;             // version 5
            cmp_array<u32>                          material_permutation; // version 8
            cmp_array<cmp_transform>                initial_transform;    // version 9
            cmp_array<cmp_anim_controller_v2>       anim_controller_v2;
            cmp_array<cmp_transform>                physics_offset;
            cmp_array<u32>                          physics_debug_cbuffer;
            cmp_array<cmp_area_light>               area_light;
            cmp_array<area_light_resource>          area_light_resources;
            cmp_array<pmfx::scene_render_flags>     render_flags;

            // num base components calculates value based on its address - entities address.
            u32 num_base_components;
            u32 num_components;

            // extensions and controllers
            ecs_extension*  extensions = nullptr;
            ecs_controller* controllers = nullptr;

            // Scene Data
            size_t           num_entities = 0;
            u32              soa_size = 0;
            free_node_list*  free_list_head = nullptr;
            u32              forward_light_buffer = PEN_INVALID_HANDLE;
            u32              sdf_shadow_buffer = PEN_INVALID_HANDLE;
            u32              area_light_buffer = PEN_INVALID_HANDLE;
            u32              shadow_map_buffer = PEN_INVALID_HANDLE;
            u32              gi_volume_buffer = PEN_INVALID_HANDLE;
            s32              selected_index = -1;
            scene_flags      flags = 0;
            scene_view_flags view_flags = 0;
            extents          renderable_extents;
            u32*             selection_list = nullptr;
            u32              version = k_version;
            Str              filename = "";

            generic_cmp_array& get_component_array(u32 index);
        };

        struct ecs_scene_instance
        {
            u32        id_name;
            const c8*  name;
            ecs_scene* scene;
        };
        typedef std::vector<ecs_scene_instance> ecs_scene_list;

        void            init();
        ecs_scene*      create_scene(const c8* name);
        void            destroy_scene(ecs_scene* scene);
        ecs_scene_list* get_scenes();

        void update(f32 dt);
        void update_scene(ecs_scene* scene, f32 dt);

        void render_scene_view(const scene_view& view);
        void render_light_volumes(const scene_view& view);
        void render_shadow_views(const scene_view& view);
        void render_omni_shadow_views(const scene_view& view);
        void render_area_light_textures(const scene_view& view);
        void compute_volume_gi(const scene_view& view);

        void clear_scene(ecs_scene* scene);
        void default_scene(ecs_scene* scene);

        void resize_scene_buffers(ecs_scene* scene, s32 size = 1024);
        void zero_entity_components(ecs_scene* scene, u32 node_index);

        void delete_entity(ecs_scene* scene, u32 node_index);
        void delete_entity_first_pass(ecs_scene* scene, u32 node_index);
        void delete_entity_second_pass(ecs_scene* scene, u32 node_index);

        void initialise_free_list(ecs_scene* scene);

        void register_ecs_extentsions(ecs_scene* scene, const ecs_extension& ext);
        void unregister_ecs_extensions(ecs_scene* scene);

        void register_ecs_controller(ecs_scene* scene, const ecs_controller& controller);

        // separate implementations to make clang always inline
        template <typename T>
        pen_inline T& cmp_array<T>::operator[](size_t index)
        {
            return data[index];
        }

        template <typename T>
        pen_inline const T& cmp_array<T>::operator[](size_t index) const
        {
            return data[index];
        }

        pen_inline void* generic_cmp_array::operator[](size_t index)
        {
            u8* d = (u8*)data;
            u8* di = &d[index * size];
            return (void*)(di);
        }

        pen_inline u32 get_extension_component_offset(ecs_scene* scene, u32 extension)
        {
            u32 offset = scene->num_base_components;

            for (u32 i = 0; i < extension; ++i)
            {
                if (i == extension)
                    break;

                offset += scene->extensions[i].num_components;
            }

            return offset;
        }

        inline u32 get_extension_component_offset_from_id(ecs_scene* scene, hash_id extension_id)
        {
            u32 offset = scene->num_base_components;

            u32 num_extensions = sb_count(scene->extensions);
            for (u32 i = 0; i < num_extensions; ++i)
            {
                if (scene->extensions[i].id_name == extension_id)
                    break;

                offset += scene->extensions[i].num_components;
            }

            return offset;
        }

        inline generic_cmp_array& ecs_scene::get_component_array(u32 index)
        {
            if (index >= num_base_components)
            {
                // extension components
                u32 num_ext = sb_count(extensions);
                u32 ext_component_start = num_base_components;
                for (u32 e = 0; e < num_ext; ++e)
                {
                    u32 num_components = extensions[e].num_components;
                    if (index < ext_component_start + num_components)
                    {
                        u32 component_offset = index - ext_component_start;
                        return extensions[e].components[component_offset];
                    }

                    ext_component_start += num_components;
                }
            }

            generic_cmp_array* begin = (generic_cmp_array*)this;
            return begin[index];
        }
    } // namespace ecs
} // namespace put
