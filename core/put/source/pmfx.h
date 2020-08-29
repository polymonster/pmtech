// pmfx.h
// Copyright 2014 - 2019 Alex Dixon.
// License: https://github.com/polymonster/pmtech/blob/master/license.md

#pragma once

#include "hash.h"
#include "renderer.h"
#include "types.h"
#include "pen_json.h"

#include "str/Str.h"

static const hash_id ID_VERTEX_CLASS_INSTANCED = PEN_HASH("_instanced");
static const hash_id ID_VERTEX_CLASS_SKINNED = PEN_HASH("_skinned");
static const hash_id ID_VERTEX_CLASS_BASIC = PEN_HASH("");

namespace put
{
    namespace ecs
    {
        struct ecs_scene;
    }

    struct camera;
} // namespace put

namespace put
{
    namespace e_update_order
    {
        enum update_order_t
        {
            pre_update = 0,
            main_update,
            post_update,
            COUNT
        };
    }
    typedef e_update_order::update_order_t update_order;

    namespace e_pmfx_constants
    {
        enum pmfx_constants_t
        {
            max_technique_sampler_bindings = 8,
            max_sampler_bindings = 16
        };
    }

    namespace e_shader_permutation
    {
        enum shader_permutation_t
        {
            skinned = 1 << 31,
            instanced = 1 << 30
        };
    }
    typedef u32 shader_permutation;

    struct scene_view
    {
        u32             cb_view = PEN_INVALID_HANDLE;
        u32             cb_2d_view = PEN_INVALID_HANDLE;
        u32             render_flags = 0;
        u32             depth_stencil_state = 0;
        u32             blend_state = 0;
        u32             raster_state = 0;
        u32             array_index = 0;
        u32             num_arrays = 1;
        put::camera*    camera = nullptr;
        pen::viewport*  viewport = nullptr;
        u32             pmfx_shader = PEN_INVALID_HANDLE;
        hash_id         id_technique = 0;
        u32             permutation = 0;
        ecs::ecs_scene* scene = nullptr;
    };

    struct scene_view_renderer
    {
        Str     name;
        hash_id id_name = 0;

        void (*render_function)(const scene_view&) = nullptr;
    };

    struct technique_constant_data
    {
        f32 data[64] = {0};
    };

    struct sampler_binding
    {
        hash_id id_texture = 0;
        hash_id id_sampler_state = 0;
        u32     handle = 0;
        u32     sampler_unit = 0;
        u32     sampler_state = 0;
        u32     bind_flags = 0;
    };

    struct sampler_set
    {
        // 8 generic samplers for draw calls, can bind to any slots.. if more are required use a second set.
        // global samplers (shadow map, render targets etc, will still be bound in addition to these)
        sampler_binding sb[e_pmfx_constants::max_technique_sampler_bindings];
    };

} // namespace put

namespace put
{
    namespace pmfx
    {
        namespace e_constant_widget
        {
            enum constant_widget_t
            {
                slider,
                input,
                colour,
                COUNT
            };
        }

        namespace e_permutation_widget
        {
            enum permutation_widget_t
            {
                checkbox,
                input,
                COUNT
            };
        }

        namespace e_cbuffer_location
        {
            enum cbuffer_location_t
            {
                per_pass_view = 0,
                per_draw_call = 1,
                filter_kernel = 2,
                per_pass_lights = 3,
                per_pass_shadow = 4,
                per_pass_sdf_shadow = 5,
                per_pass_area_lights = 6,
                material_constants = 7,
                sampler_info = 10,
                post_process_info = 4,
                taa_resolve_info = 3
            };
        }

        namespace e_render_state
        {
            enum render_state_t
            {
                rasterizer,
                sampler,
                blend,
                depth_stencil
            };
        }
        typedef e_render_state::render_state_t render_state_t;

        namespace e_rt_flags
        {
            enum rt_flags_t
            {
                aux = 1 << 1,
                aux_used = 1 << 2,
                write_only = 1 << 3,
                resolve = 1 << 4
            };
        }

        namespace e_vrt_mode
        {
            enum vrt_mode_t
            {
                read,
                write,
                COUNT
            };
        }
        typedef e_vrt_mode::vrt_mode_t vrt_mode;

        namespace e_scene_render_flags
        {
            enum scene_render_flags_t
            {
                opaque = 0,
                forward_lit = 1,
                shadow_map = 1 << 1,
                alpha_blended = 1 << 2,
                COUNT
            };
        }
        typedef e_scene_render_flags::scene_render_flags_t scene_render_flags;

        struct technique_constant
        {
            Str     name;
            hash_id id_name;
            u32     widget = e_constant_widget::slider;
            f32     min = 0.0f;
            f32     max = 1.0f;
            f32     step = 0.01f;
            u32     cb_offset = 0;
            u32     num_elements = 0;
        };

        struct technique_sampler
        {
            Str     name;
            hash_id id_name;
            Str     sampler_state_name;
            Str     type_name;
            Str     default_name;
            Str     filename;
            u32     handle;
            u32     unit;
            u32     sampler_state;
            u32     bind_flags;
        };

        struct technique_permutation
        {
            Str name = "";
            u32 val = 0;
            u32 widget = e_permutation_widget::checkbox;
        };

        struct shader_program
        {
            hash_id id_name;
            hash_id id_sub_type;
            Str     name;
            bool    loaded = false;
            pen::json info;

            u32 vertex_shader;
            u32 pixel_shader;
            u32 stream_out_shader;
            u32 compute_shader;
            u32 input_layout;
            u32 program_index;
            u32 technique_constant_size; // bytes
            u32 permutation_id;          // bitmask
            u32 permutation_option_mask; // bits which this permutation parent technique accepts

            f32*                   constant_defaults;
            technique_constant*    constants;
            technique_sampler*     textures;
            technique_permutation* permutations;
        };

        struct render_target
        {
            hash_id id_name;

            // todo replace with tcp..?
            s32 width = 0;
            s32 height = 0;
            s32 depth = 0;
            f32 ratio = 0;
            s32 num_mips = 0;
            u32 num_arrays = 0;
            u32 format = 0;
            u32 handle = PEN_INVALID_HANDLE;
            u32 samples = 1;
            Str name = "";
            u32 flags = 0;
            u32 pp = e_vrt_mode::read;
            u32 pp_read = PEN_INVALID_HANDLE;
            u32 collection = pen::TEXTURE_COLLECTION_NONE;
            u32 bind_flags = 0;
        };

        struct rt_resize_params
        {
            u32       width = 0;
            u32       height = 0;
            u32       num_mips = 0;
            u32       num_arrays = 0; // num array slices
            u32       collection = pen::TEXTURE_COLLECTION_NONE;
            u32       flags = 0;
            const c8* format = nullptr;
        };

        // pmfx renderer ---------------------------------------------------------------------------------------------------

        void init(const c8* filename);
        void shutdown();
        void show_dev_ui();

        void release_script_resources();
        void render();
        void render_view(hash_id id_name);
        void register_scene(ecs::ecs_scene* scene, const char* name);
        void register_camera(camera* cam, const char* name);
        void register_scene_view_renderer(const scene_view_renderer& svr);

        void resize_render_target(hash_id target, u32 width, u32 height, const c8* format = nullptr);
        void resize_render_target(hash_id target, const rt_resize_params& params);
        void resize_viewports();

        void set_view_set(const c8* name);

        camera*              get_camera(hash_id id_name);
        camera**             get_cameras(); // call sb_free on return value when done
        const render_target* get_render_target(hash_id h);
        void                 get_render_target_dimensions(const render_target* rt, f32& w, f32& h);
        u32                  get_render_state(hash_id id_name, u32 type);
        Str                  get_render_state_name(u32 handle);
        c8**                 get_render_state_list(u32 type);    // call sb_free on return value when done
        hash_id*             get_render_state_id_list(u32 type); // call sb_free on return value when don

        // render funcs
        void                 fullscreen_quad(const scene_view& sv);
        void                 render_taa_resolve(const scene_view& view);

        // pmfx shader -----------------------------------------------------------------------------------------------------

        u32  load_shader(const c8* pmfx_name);
        void release_shader(u32 shader);

        void set_technique(u32 shader, u32 technique_index);
        bool set_technique_perm(u32 shader, hash_id id_technique, u32 permutation = 0);

        void initialise_constant_defaults(u32 shader, u32 technique_index, f32* data);
        void initialise_sampler_defaults(u32 handle, u32 technique_index, sampler_set& samplers);

        u32                 get_technique_list_index(u32 shader, hash_id id_technique); // return index in name list
        const c8**          get_shader_list(u32& count);
        const c8**          get_technique_list(u32 shader, u32& count);
        const c8*           get_shader_name(u32 shader);
        const c8*           get_technique_name(u32 shader, hash_id id_technique);
        hash_id             get_technique_id(u32 shader, u32 technique_index);
        u32                 get_technique_index_perm(u32 shader, hash_id id_technique, u32 permutation = 0);
        technique_constant* get_technique_constants(u32 shader, u32 technique_index);
        technique_constant* get_technique_constant(hash_id id_constant, u32 shader, u32 technique_index);
        u32                 get_technique_cbuffer_size(u32 shader, u32 technique_index);
        technique_sampler*  get_technique_samplers(u32 shader, u32 technique_index);
        technique_sampler*  get_technique_sampler(hash_id id_sampler, u32 shader, u32 technique_index);

        bool show_technique_ui(u32 shader, u32 technique_index, f32* data, sampler_set& samplers, u32* permutation);
        bool has_technique_permutations(u32 shader, u32 technique_index);
        bool has_technique_constants(u32 shader, u32 technique_index);
        bool has_technique_samplers(u32 shader, u32 technique_index);
        bool has_technique_params(u32 shader, u32 technique_index);

        void poll_for_changes();
    } // namespace pmfx
} // namespace put
