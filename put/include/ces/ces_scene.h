#ifndef ces_scene_h__
#define ces_scene_h__

#include "camera.h"
#include "loader.h"
#include "maths/maths.h"
#include "maths/quat.h"
#include "pen.h"
#include "physics.h"
#include "pmfx.h"
#include "str/Str.h"

#include <vector>

namespace put
{
    struct scene_view;
    typedef s32 anim_handle;

    namespace ces
    {
        enum e_scene_view_flags : u32
        {
            SV_NONE     = 0,
            SV_HIDE     = (1 << 0),
            SV_BITS_END = 0
        };

        enum e_scene_flags : u32
        {
            INVALIDATE_NONE       = 0,
            INVALIDATE_SCENE_TREE = 1 << 1,
            PAUSE_UPDATE          = 1 << 2
        };

        enum e_component_flags : u32
        {
            CMP_ALLOCATED       = (1 << 0),
            CMP_GEOMETRY        = (1 << 1),
            CMP_PHYSICS         = (1 << 2),
            CMP_PHYSICS_MULTI   = (1 << 3),
            CMP_MATERIAL        = (1 << 4),
            CMP_HAND            = (1 << 5),
            CMP_SKINNED         = (1 << 6),
            CMP_BONE            = (1 << 7),
            CMP_DYNAMIC         = (1 << 8),
            CMP_ANIM_CONTROLLER = (1 << 9),
            CMP_ANIM_TRAJECTORY = (1 << 10),
            CMP_LIGHT           = (1 << 11),
            CMP_TRANSFORM       = (1 << 12),
            CMP_CONSTRAINT      = (1 << 13),
            CMP_SUB_INSTANCE    = (1 << 14),
            CMP_MASTER_INSTANCE = (1 << 15),
            CMP_PRE_SKINNED     = (1 << 16),
            CMP_SUB_GEOMETRY    = (1 << 17),
            CMP_SDF_SHADOW      = (1 << 18),
            CMP_VOLUME          = (1 << 19)
        };

        enum e_state_flags : u32
        {
            SF_SELECTED             = (1 << 0),
            SF_CHILD_SELECTED       = (1 << 1),
            SF_HIDDEN               = (1 << 2),
            SF_MATERIAL_INITIALISED = (1 << 3),
            SF_NO_SHADOW            = (1 << 4)
        };

        enum e_light_types : u32
        {
            LIGHT_TYPE_DIR      = 0,
            LIGHT_TYPE_POINT    = 1,
            LIGHT_TYPE_SPOT     = 2,
            LIGHT_TYPE_AREA_BOX = 3
        };

        enum e_scene_node_textures
        {
            SN_ALBEDO_MAP = 0,
            SN_NORMAL_MAP,
            SN_SPECULAR_MAP,
            SN_ENV_MAP,
            SN_VOLUME_TEXTURE,
            SN_EMISSIVE_MAP,

            SN_NUM_TEXTURES
        };

        enum e_scene_node_cb
        {
            SN_CB1,
            SN_CB2,
            SN_CB3,

            SN_NUM_CB
        };

        enum e_scene_global_textures
        {
            SHADOW_MAP_UNIT = 15,
            SDF_SHADOW_UNIT = 14
        };

        enum e_physics_type
        {
            PHYSICS_TYPE_RIGID_BODY = 0,
            PHYSICS_TYPE_CONSTRAINT
        };

        enum e_scene_limits
        {
            MAX_FORWARD_LIGHTS = 8,
            MAX_SHADOW_MAPS    = 1,
            MAX_SDF_SHADOWS    = 1
        };

        struct material_resource
        {
            hash_id hash;
            Str     material_name;
            Str     shader_name;
            f32     data[64];
            hash_id id_shader = 0;
            hash_id id_technique = 0;
            hash_id id_sampler_state[SN_NUM_TEXTURES] = { 0 };
            s32     texture_handles[SN_NUM_TEXTURES] = { 0 };
        };

        struct cmp_draw_call
        {
            mat4 world_matrix;
            vec4f v1; // generic data 1
            vec4f v2; // generic data 2
            mat4 world_matrix_inv_transpose;
        };

        struct cmp_skin
        {
            u32  num_joints;
            mat4 bind_shape_matrix;
            mat4 joint_bind_matrices[85];
            u32  bone_cbuffer = PEN_INVALID_HANDLE;
        };

        struct cmp_material
        {
            s32   texture_handles[SN_NUM_TEXTURES]  = {0};
            u32   sampler_states[SN_NUM_TEXTURES]   = {0};
            u32   material_cbuffer = PEN_INVALID_HANDLE;
            u32   material_cbuffer_size = 0;

            pmfx::shader_handle pmfx_shader;
            u32                 technique;
        };

        struct cmp_material_data
        {
            f32 data[64] = { 0 };
        };

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
                pen::memory_cpy(this, &other, sizeof(cmp_physics));
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

        struct cmp_anim_controller
        {
            enum e_play_flags : u8
            {
                STOPPED = 0,
                PLAY    = 1
            };

            anim_handle*             handles = nullptr;
            s32                      joints_offset;
            anim_handle              current_animation;
            f32                      current_time;
            s32                      current_frame     = 0;
            u8                       play_flags        = STOPPED;
            bool                     apply_root_motion = true;
        };

        struct cmp_light
        {
            u32   type;
            vec3f colour;

            f32 radius = 10.0f;
            f32 spot_falloff = 0.05f;
            f32 cos_cutoff = -M_PI/4.0f;
            f32 azimuth;
            f32 altitude;

            vec3f direction;

            bool shadow_map = false;
        };

        struct cmp_transform
        {
            vec3f translation = vec3f::zero();
            quat  rotation    = quat();
            vec3f scale       = vec3f::one();
        };


        struct cmp_shadow
        {
            u32 texture_handle; // texture handle for sdf
            u32 sampler_state; 
        };

        struct forward_light
        {
            vec4f pos_radius;
            vec4f dir_cutoff;
            vec4f colour;
        };

        struct forward_light_buffer
        {
            forward_light lights[MAX_FORWARD_LIGHTS];
            vec4f         info;
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

        struct area_box_light
        {
            mat4 world_matrix;
            mat4 world_matrix_inverse;
        };

        struct area_box_light_buffer
        {
            area_box_light area_lights;
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

            pen_inline T& operator[](size_t index)
            {
                return data[index];
            }

            pen_inline const T& operator[](size_t index) const
            {
                return data[index];
            }
        };

        struct generic_cmp_array
        {
            u32   size;
            void* data;

            pen_inline void* operator[](size_t index)
            {
                u8* d  = (u8*)data;
                u8* di = &d[index * size];
                return (void*)(di);
            }
        };

        struct entity_scene
        {
            entity_scene()
            {
                num_components = (((size_t)&num_components) - ((size_t)&entities)) / sizeof(generic_cmp_array);
            };

            // Components
            cmp_array<u64>                 entities;
            cmp_array<u64>                 state_flags;
            cmp_array<hash_id>             id_name;
            cmp_array<hash_id>             id_geometry;
            cmp_array<hash_id>             id_material;
            cmp_array<Str>                 names;
            cmp_array<Str>                 geometry_names;
            cmp_array<Str>                 material_names;
            cmp_array<u32>                 parents;
            cmp_array<cmp_transform>       transforms;
            cmp_array<mat4>                local_matrices;
            cmp_array<mat4>                world_matrices;
            cmp_array<mat4>                offset_matrices;
            cmp_array<mat4>                physics_matrices;
            cmp_array<cmp_bounding_volume> bounding_volumes;
            cmp_array<cmp_light>           lights;
            cmp_array<u32>                 physics_handles;
            cmp_array<cmp_master_instance> master_instances;
            cmp_array<cmp_geometry>        geometries;
            cmp_array<cmp_pre_skin>        pre_skin;
            cmp_array<cmp_physics>         physics_data;
            cmp_array<cmp_anim_controller> anim_controller;
            cmp_array<u32>                 cbuffer;
            cmp_array<cmp_draw_call>       draw_call_data;
            cmp_array<free_node_list>      free_list;
            cmp_array<cmp_material>        materials;
            cmp_array<cmp_material_data>   material_data;
            cmp_array<material_resource>   material_resources;
            cmp_array<cmp_shadow>          shadows;

            // Ensure num_components is the next to calc size
            u32 num_components;

            // Scene Data
            u32             num_nodes             = 0;
            u32             nodes_size            = 0;
            free_node_list* free_list_head        = nullptr;
            u32             forward_light_buffer  = PEN_INVALID_HANDLE;
            u32             sdf_shadow_buffer     = PEN_INVALID_HANDLE;
            u32             area_box_light_buffer = PEN_INVALID_HANDLE;
            s32             selected_index        = -1;
            u32             flags                 = 0;
            u32             view_flags            = 0;
            extents         renderable_extents;

            // Access to component data in a generic way
            pen_inline generic_cmp_array& get_component_array(u32 index)
            {
                generic_cmp_array* begin = (generic_cmp_array*)this;
                return begin[index];
            }
        };

        struct entity_scene_instance
        {
            u32           id_name;
            const c8*     name;
            entity_scene* scene;
        };

        enum e_scene_render_flags
        {
            RENDER_FORWARD_LIT = 1
        };

        entity_scene* create_scene(const c8* name);
        void          destroy_scene(entity_scene* scene);

        void render_scene_view(const scene_view& view);
        void update_scene(entity_scene* scene, f32 dt);

        void clear_scene(entity_scene* scene);
        void default_scene(entity_scene* scene);

        void resize_scene_buffers(entity_scene* scene, s32 size = 1024);
        void zero_entity_components(entity_scene* scene, u32 node_index);

        void delete_entity(entity_scene* scene, u32 node_index);
        void delete_entity_first_pass(entity_scene* scene, u32 node_index);
        void delete_entity_second_pass(entity_scene* scene, u32 node_index);

        void update_view_flags(entity_scene* scene, bool error);

        void initialise_free_list(entity_scene* scene);
    } // namespace ces
} // namespace put

#endif
