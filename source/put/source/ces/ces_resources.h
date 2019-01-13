#ifndef ces_resources_h__
#define ces_resources_h__

#include "ces/ces_scene.h"

namespace put
{
    namespace ces
    {
        enum e_animation_semantics
        {
            A_TIME = 0,
            A_TRANSFORM,
            A_OFFSET_X,
            A_OFFSET_Y,
            A_OFFSET_Z,
            A_ANGLE,
            A_INTERPOLATION
        };

        enum e_animation_interpolation_types
        {
            A_LINEAR = 0,
            A_BEZIER,
            A_CARDINAL,
            A_HERMITE,
            A_BSPLINE,
            A_STEP
        };

        enum e_animation_data_types
        {
            A_FLOAT = 0,
            A_FLOAT4x4,
            A_INT
        };

        enum e_animation_targets
        {
            A_TRANSLATE_TARGET = 0,
            A_TRANSFORM_TARGET,
            A_ROTATE_TARGET,
            A_TRANSLATE_X_TARGET,
            A_TRANSLATE_Y_TARGET,
            A_TRANSLATE_Z_TARGET,
            A_ROTATE_X_TARGET,
            A_ROTATE_Y_TARGET,
            A_ROTATE_Z_TARGET
        };

        enum e_pmm_load_flags : u32
        {
            PMM_GEOMETRY = (1 << 0),
            PMM_MATERIAL = (1 << 1),
            PMM_NODES = (1 << 2),
            PMM_ALL = 7
        };

        struct animation_channel
        {
            u32     num_frames;
            hash_id target;
            f32*    times;
            mat4*   matrices;
            f32*    offset[3];
            f32*    angle[3];
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
            f32 step;

            Str name;
        };

        struct geometry_resource
        {
            hash_id file_hash;
            hash_id hash;
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

        void save_scene(const c8* filename, entity_scene* scene);
        void load_scene(const c8* filename, entity_scene* scene, bool merge = false);

        s32         load_pmm(const c8* model_scene_name, entity_scene* scene = nullptr, u32 load_flags = PMM_ALL);
        anim_handle load_pma(const c8* model_scene_name);
        s32         load_pmv(const c8* filename, entity_scene* scene);

        void instantiate_rigid_body(entity_scene* scene, u32 node_index);
        void instantiate_constraint(entity_scene* scene, u32 node_index);
        void instantiate_geometry(geometry_resource* gr, entity_scene* scene, s32 node_index);
        void instantiate_model_pre_skin(entity_scene* scene, s32 node_index);
        void instantiate_model_cbuffer(entity_scene* scene, s32 node_index);
        void instantiate_material_cbuffer(entity_scene* scene, s32 node_index, s32 size);
        void instantiate_anim_controller(entity_scene* scene, s32 node_index);
        void instantiate_material(material_resource* mr, entity_scene* scene, u32 node_index);
        void instantiate_sdf_shadow(const c8* pmv_filename, entity_scene* scene, u32 node_index);
        void instantiate_light(entity_scene* scene, u32 node_index);

        void destroy_geometry(entity_scene* scene, u32 node_index);
        void destroy_physics(entity_scene* scene, s32 node_index);

        void bake_material_handles(entity_scene* scene, u32 node_index);
        void bake_material_handles();

        void create_geometry_primitives();

        void add_geometry_resource(geometry_resource* gr);
        void add_material_resource(material_resource* mr);

        material_resource*  get_material_resource(hash_id hash);
        animation_resource* get_animation_resource(anim_handle h);
        geometry_resource*  get_geometry_resource(hash_id h);
    } // namespace ces
} // namespace put
#endif
