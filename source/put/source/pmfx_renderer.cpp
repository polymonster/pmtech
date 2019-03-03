// pmfx_renderer.cpp
// Copyright 2014 - 2019 Alex Dixon. 
// License: https://github.com/polymonster/pmtech/blob/master/license.md

#include "ecs/ecs_editor.h"
#include "ecs/ecs_resources.h"
#include "ecs/ecs_scene.h"
#include "console.h"
#include "data_struct.h"
#include "debug_render.h"
#include "dev_ui.h"
#include "file_system.h"
#include "hash.h"
#include "os.h"
#include "pen_json.h"
#include "pen_string.h"
#include "pmfx.h"
#include "str_utilities.h"
#include "timer.h"

#include <fstream>

extern pen::window_creation_params pen_window;

using namespace put;
using namespace pmfx;
using namespace pen;
using namespace ecs;

namespace
{
    // clang-format off
    const hash_id k_id_main_colour = PEN_HASH("main_colour");
    const hash_id k_id_main_depth  = PEN_HASH("main_depth");
    const hash_id k_id_wrap_linear = PEN_HASH("wrap_linear"); // todo rename

    struct mode_map
    {
        const c8* name;
        u32       val;
    };

    const mode_map k_cull_mode_map[] = {
        "none",     PEN_CULL_NONE,
        "back",     PEN_CULL_BACK,
        "front",    PEN_CULL_FRONT,
        nullptr, 0
    };

    const mode_map k_fill_mode_map[] = {
        "solid",        PEN_FILL_SOLID,
        "wireframe",    PEN_FILL_WIREFRAME,
        nullptr, 0
    };

    const mode_map k_comparison_mode_map[] = {
        "never",            PEN_COMPARISON_NEVER,
        "less",             PEN_COMPARISON_LESS,
        "less_equal",       PEN_COMPARISON_LESS_EQUAL,
        "greater",          PEN_COMPARISON_GREATER,
        "not_equal",        PEN_COMPARISON_NOT_EQUAL,
        "greater_equal",    PEN_COMPARISON_GREATER_EQUAL,
        "always",           PEN_COMPARISON_ALWAYS,
        "equal",            PEN_COMPARISON_EQUAL,
        nullptr,            0
    };

    const mode_map k_stencil_mode_map[] = {
        "keep",     PEN_STENCIL_OP_KEEP,     "replace", PEN_STENCIL_OP_REPLACE, "incr",     PEN_STENCIL_OP_INCR,
        "incr_sat", PEN_STENCIL_OP_INCR_SAT, "decr",    PEN_STENCIL_OP_DECR,    "decr_sat", PEN_STENCIL_OP_DECR_SAT,
        "zero",     PEN_STENCIL_OP_ZERO,     "invert",  PEN_STENCIL_OP_INVERT,  nullptr,    0
    };

    const mode_map k_filter_mode_map[] = {
        "linear", PEN_FILTER_MIN_MAG_MIP_LINEAR,
        "point", PEN_FILTER_MIN_MAG_MIP_POINT,
        nullptr, 0
    };

    const mode_map k_address_mode_map[] = {
        "wrap",        PEN_TEXTURE_ADDRESS_WRAP,        "clamp",  PEN_TEXTURE_ADDRESS_CLAMP,
        "border",      PEN_TEXTURE_ADDRESS_BORDER,      "mirror", PEN_TEXTURE_ADDRESS_MIRROR,
        "mirror_once", PEN_TEXTURE_ADDRESS_MIRROR_ONCE, nullptr,  0
    };

    const mode_map k_blend_mode_map[] = {
        "zero",             PEN_BLEND_ZERO,             "one",              PEN_BLEND_ONE,
        "src_colour",       PEN_BLEND_SRC_COLOR,        "inv_src_colour",   PEN_BLEND_INV_SRC_COLOR,
        "src_alpha",        PEN_BLEND_SRC_ALPHA,        "inv_src_alpha",    PEN_BLEND_INV_SRC_ALPHA,
        "dest_alpha",       PEN_BLEND_DEST_ALPHA,       "inv_dest_alpha",   PEN_BLEND_INV_DEST_ALPHA,
        "dest_colour",      PEN_BLEND_DEST_COLOR,       "inv_dest_colour",  PEN_BLEND_INV_DEST_COLOR,
        "src_alpha_sat",    PEN_BLEND_SRC_ALPHA_SAT,    "blend_factor",     PEN_BLEND_BLEND_FACTOR,
        "inv_blend_factor", PEN_BLEND_INV_BLEND_FACTOR, "src1_colour",      PEN_BLEND_SRC1_COLOR,
        "inv_src1_colour",  PEN_BLEND_INV_SRC1_COLOR,   "src1_aplha",       PEN_BLEND_SRC1_ALPHA,
        "inv_src1_alpha",   PEN_BLEND_INV_SRC1_ALPHA,   nullptr, 0
    };

    const mode_map k_blend_op_mode_map[] = {
        "blend_op_add",             PEN_BLEND_OP_ADD,      
        "blend_op_subtract",        PEN_BLEND_OP_SUBTRACT,     
        "blend_op_rev_subtract",    PEN_BLEND_OP_REV_SUBTRACT, 
        "blend_op_min",             PEN_BLEND_OP_MIN,           
        "blend_op_max",             PEN_BLEND_OP_MAX,
        nullptr, 0
    };

    struct format_info
    {
        Str        name;
        hash_id    id_name;
        s32        format;
        u32        block_size;
        bind_flags flags;
    };
    
    const format_info rt_format[] = {
        {"rgba8",   PEN_HASH("rgba8"),      PEN_TEX_FORMAT_RGBA8_UNORM,         32,     PEN_BIND_RENDER_TARGET},
        {"bgra8",   PEN_HASH("bgra8"),      PEN_TEX_FORMAT_BGRA8_UNORM,         32,     PEN_BIND_RENDER_TARGET},
        {"rgba32f", PEN_HASH("rgba32f"),    PEN_TEX_FORMAT_R32G32B32A32_FLOAT,  32 * 4, PEN_BIND_RENDER_TARGET},
        {"rgba16f", PEN_HASH("rgba16f"),    PEN_TEX_FORMAT_R16G16B16A16_FLOAT,  16 * 4, PEN_BIND_RENDER_TARGET},
        {"r32f",    PEN_HASH("r32f"),       PEN_TEX_FORMAT_R32_FLOAT,           32,     PEN_BIND_RENDER_TARGET},
        {"r16f",    PEN_HASH("r16f"),       PEN_TEX_FORMAT_R16_FLOAT,           16,     PEN_BIND_RENDER_TARGET},
        {"r32u",    PEN_HASH("r32u"),       PEN_TEX_FORMAT_R32_UINT,            32,     PEN_BIND_RENDER_TARGET},
        {"d24s8",   PEN_HASH("d24s8"),      PEN_TEX_FORMAT_D24_UNORM_S8_UINT,   32,     PEN_BIND_DEPTH_STENCIL}
    };

    const Str rt_ratio[] = {
        "none",
        "equal",
        "half",
        "quarter",
        "eighth",
        "sixteenth"
    };
    
    const mode_map render_flags_map[] = {
        "forward_lit", ecs::RENDER_FORWARD_LIT,
        nullptr, 0
    };
    
    const mode_map sampler_bind_flags[] = {
        "ps", pen::TEXTURE_BIND_PS,
        "vs", pen::TEXTURE_BIND_VS,
        "msaa", pen::TEXTURE_BIND_MSAA,
        nullptr, 0
    };
    // clang-format on

    enum e_post_process_flags
    {
        PP_NONE = 0,
        PP_ENABLED = (1 << 0),
        PP_EDITED = (1 << 1),
        PP_WRITE_NON_AUX = (1 << 2) // writes directly to the specified target bypassing aux / virtual buffers
    };

    struct view_params
    {
        Str     name;
        Str     group;
        Str     info_json;
        hash_id id_name;
        hash_id id_group;
        hash_id id_render_target[pen::MAX_MRT] = {0};
        hash_id id_depth_target = 0;
        hash_id id_filter = 0;

        // draw / update
        ecs::ecs_scene* scene;
        put::camera*       camera;

        std::vector<void (*)(const put::scene_view&)> render_functions;

        // targets
        u32 render_targets[pen::MAX_MRT] = {PEN_INVALID_HANDLE, PEN_INVALID_HANDLE, PEN_INVALID_HANDLE, PEN_INVALID_HANDLE,
                                            PEN_INVALID_HANDLE, PEN_INVALID_HANDLE, PEN_INVALID_HANDLE, PEN_INVALID_HANDLE};

        u32 depth_target = PEN_INVALID_HANDLE;

        // viewport
        s32 rt_width;
        s32 rt_height;
        f32 rt_ratio;

        f32 viewport[4] = {0};

        // render state
        u32 num_arrays = 1; // ie. 6 for cubemap
        u32 num_colour_targets = 0;
        u32 clear_state = 0;
        u32 raster_state = 0;
        u32 depth_stencil_state = 0;
        u32 blend_state = 0;
        u32 cbuffer_filter = PEN_INVALID_HANDLE;
        u32 cbuffer_technique = PEN_INVALID_HANDLE;

        // shader and technique
        u32     pmfx_shader;
        hash_id technique; // todo rename to id_technique
        u32     render_flags;

        technique_constant_data technique_constants;
        sampler_set             technique_samplers;
        u32                     technique_permutation;

        std::vector<sampler_binding> sampler_bindings;
        vec4f*                       sampler_info;

        // post process
        u32                      post_process_flags = PP_NONE;
        Str                      post_process_name;
        std::vector<Str>         post_process_chain;
        std::vector<view_params> post_process_views;

        // for debug
        bool stash_output = false;
        u32  stashed_output_rt = PEN_INVALID_HANDLE;
        f32  stashed_rt_aspect = 0.0f;

        bool viewport_correction = true; // todo put this into flags?
    };

    struct edited_post_process
    {
        hash_id                  id_name;
        std::vector<Str>         chain;
        std::vector<view_params> views;
    };

    struct render_state
    {
        Str     name;
        hash_id id_name;

        hash_id hash;
        u32     handle;

        e_render_state_type type;
    };

    struct filter_kernel
    {
        hash_id id_name;
        Str     name;

        // cbuffer friendly data
        vec4f info;              // xy = direction, z = num samples, w = pad
        vec4f offset_weight[16]; // x = offset, y = weight;
    };

    struct textured_vertex
    {
        float x, y, z, w;
        float u, v;
    };

    struct geometry_utility
    {
        u32 screen_quad_vb;
        u32 screen_quad_ib;
    };

    // Views / Sets
    std::vector<Str>                 s_post_process_names;    // List of post process names (ie bloom, dof.. etc)
    std::vector<view_params>         s_views;                 // List of all view parameters
    std::vector<Str>                 s_view_sets;             // list of view set names (ie. forward, deferred.. etc)
    std::vector<Str>                 s_view_set;              // list of view names in the current set
    Str                              s_view_set_name;         // Name of the current view set
    Str                              s_edited_view_set_name;  // Name of the user selected view
    std::vector<edited_post_process> s_edited_post_processes; // User edited post processes

    // Render Resources
    std::vector<scene_controller>        s_controllers;
    std::vector<scene_view_renderer>     s_scene_view_renderers;
    std::vector<render_target>           s_render_targets;
    std::vector<texture_creation_params> s_render_target_tcp;
    std::vector<const c8*>               s_render_target_names;
    std::vector<render_state>            s_render_states;
    std::vector<sampler_binding>         s_sampler_bindings;
    std::vector<filter_kernel>           s_filter_kernels;
    geometry_utility                     s_geometry;

    // ids
} // namespace

namespace put
{
    namespace pmfx
    {
        void register_scene_controller(const scene_controller& controller)
        {
            s_controllers.push_back(controller);
            s_controllers.back().camera->name = controller.name;
        }

        void register_scene_view_renderer(const scene_view_renderer& svr)
        {
            s_scene_view_renderers.push_back(svr);
        }

        s32 calc_num_mips(s32 width, s32 height)
        {
            s32 num = 0;

            while (width > 1 && height > 1)
            {
                ++num;

                width /= 2;
                height /= 2;

                width = std::max<s32>(1, width);
                height = std::max<s32>(1, height);
            }

            return num;
        }

        void get_rt_dimensions(s32 rt_w, s32 rt_h, f32 rt_r, f32& w, f32& h)
        {
            w = (f32)pen_window.width;
            h = (f32)pen_window.height;

            if (rt_r != 0)
            {
                w /= (f32)rt_r;
                h /= (f32)rt_r;
            }
            else
            {
                w = (f32)rt_w;
                h = (f32)rt_h;
            }
        }

        void get_rt_viewport(s32 rt_w, s32 rt_h, f32 rt_r, const f32* vp_in, pen::viewport& vp_out)
        {
            f32 w, h;
            get_rt_dimensions(rt_w, rt_h, rt_r, w, h);

            vp_out = {vp_in[0] * w, vp_in[1] * h, vp_in[2] * w, vp_in[3] * h, 0.0f, 1.0f};
        }

        u32 mode_from_string(const mode_map* map, const c8* str, u32 default_value)
        {
            if (!str)
                return default_value;

            while (map->name)
                if (pen::string_compare(str, map->name) == 0)
                    return map->val;
                else
                    map++;

            return default_value;
        }

        u32 sampler_bind_flags_from_json(const json& sampler_binding)
        {
            u32 res = 0;
            res = pen::TEXTURE_BIND_PS; // todo remove once all configs have been updated
            json flags = sampler_binding["bind_flags"];
            u32  num_flags = flags.size();
            for (u32 i = 0; i < num_flags; ++i)
                res |= mode_from_string(sampler_bind_flags, flags[i].as_cstr(), 0);

            return res;
        }

        render_state* get_state_by_hash(hash_id hash)
        {
            size_t num = s_render_states.size();
            for (s32 i = 0; i < num; ++i)
                if (s_render_states[i].hash == hash)
                    return &s_render_states[i];

            return nullptr;
        }

        render_state* _get_render_state(hash_id id_name, u32 type)
        {
            size_t num = s_render_states.size();
            for (s32 i = 0; i < num; ++i)
                if (s_render_states[i].type == type)
                    if (s_render_states[i].id_name == id_name)
                        return &s_render_states[i];

            return nullptr;
        }

        u32 get_render_state(hash_id id_name, u32 type)
        {
            size_t num = s_render_states.size();
            for (s32 i = 0; i < num; ++i)
                if (s_render_states[i].type == type)
                    if (s_render_states[i].id_name == id_name)
                        return s_render_states[i].handle;

            return 0;
        }

        Str get_render_state_name(u32 handle)
        {
            size_t num = s_render_states.size();
            for (s32 i = 0; i < num; ++i)
                if (s_render_states[i].handle == handle)
                    return s_render_states[i].name;

            return "";
        }

        c8** get_render_state_list(u32 type)
        {
            c8** list = nullptr;

            size_t num = s_render_states.size();
            for (s32 i = 0; i < num; ++i)
                if (s_render_states[i].type == type)
                    sb_push(list, s_render_states[i].name.c_str());

            return list;
        }

        hash_id* get_render_state_id_list(u32 type)
        {
            hash_id* list = nullptr;

            size_t num = s_render_states.size();
            for (s32 i = 0; i < num; ++i)
                if (s_render_states[i].type == type)
                    sb_push(list, s_render_states[i].id_name);

            return list;
        }

        void create_geometry_utilities()
        {
            // buffers
            // create vertex buffer for a quad
            textured_vertex quad_vertices[] = {
                -1.0f, -1.0f, 0.5f, 1.0f, // p1
                0.0f,  1.0f,              // uv1

                -1.0f, 1.0f,  0.5f, 1.0f, // p2
                0.0f,  0.0f,              // uv2

                1.0f,  1.0f,  0.5f, 1.0f, // p3
                1.0f,  0.0f,              // uv3

                1.0f,  -1.0f, 0.5f, 1.0f, // p4
                1.0f,  1.0f,              // uv4
            };

            pen::buffer_creation_params bcp;
            bcp.usage_flags = PEN_USAGE_DEFAULT;
            bcp.bind_flags = PEN_BIND_VERTEX_BUFFER;
            bcp.cpu_access_flags = 0;

            bcp.buffer_size = sizeof(textured_vertex) * 4;
            bcp.data = (void*)&quad_vertices[0];

            s_geometry.screen_quad_vb = pen::renderer_create_buffer(bcp);

            // create index buffer
            u16 indices[] = {0, 1, 2, 2, 3, 0};

            bcp.usage_flags = PEN_USAGE_IMMUTABLE;
            bcp.bind_flags = PEN_BIND_INDEX_BUFFER;
            bcp.cpu_access_flags = 0;
            bcp.buffer_size = sizeof(u16) * 6;
            bcp.data = (void*)&indices[0];

            s_geometry.screen_quad_ib = pen::renderer_create_buffer(bcp);
        }

        void parse_sampler_bindings(pen::json render_config, view_params& vp)
        {
            std::vector<sampler_binding>& bindings = vp.sampler_bindings;
            pen::json                     j_sampler_bindings = render_config["sampler_bindings"];
            s32                           num = j_sampler_bindings.size();

            if (num > 0)
                vp.sampler_info = new vec4f[num];

            for (s32 i = 0; i < num; ++i)
            {
                pen::json binding = j_sampler_bindings[i];

                sampler_binding sb;

                // texture id and handle from render targets.. todo add global textures
                sb.id_texture = binding["texture"].as_hash_id();
                const render_target* rt = get_render_target(sb.id_texture);

                if (!rt)
                {
                    dev_console_log_level(dev_ui::CONSOLE_WARNING,
                                          "[warning] pmfx: view '%s' expects sampler '%s' but it does not exist",
                                          vp.name.c_str(), binding["texture"].as_cstr());

                    continue;
                }

                sb.handle = rt->handle;

                // sampler state from name
                Str ss = binding["state"].as_str();
                sb.sampler_state = get_render_state(PEN_HASH(ss.c_str()), RS_SAMPLER);

                // unit
                sb.sampler_unit = binding["unit"].as_u32();

                // sampler / texture bind flags
                sb.bind_flags = sampler_bind_flags_from_json(binding);

                // sample info for sampling in shader
                f32 w, h;
                get_render_target_dimensions(rt, w, h);

                vp.sampler_info[i].x = 1.0f / w;
                vp.sampler_info[i].y = 1.0f / h;

                bindings.push_back(sb);
            }
        }

        void parse_sampler_states(pen::json render_config)
        {
            pen::json j_sampler_states = render_config["sampler_states"];
            s32       num = j_sampler_states.size();
            for (s32 i = 0; i < num; ++i)
            {
                pen::sampler_creation_params scp;

                pen::json state = j_sampler_states[i];

                scp.filter = mode_from_string(k_filter_mode_map, state["filter"].as_cstr(), PEN_FILTER_MIN_MAG_MIP_LINEAR);
                scp.address_u = mode_from_string(k_address_mode_map, state["address"].as_cstr(), PEN_TEXTURE_ADDRESS_WRAP);
                scp.address_v = scp.address_w = scp.address_u;

                scp.address_u = mode_from_string(k_address_mode_map, state["address_u"].as_cstr(), PEN_TEXTURE_ADDRESS_WRAP);
                scp.address_v = mode_from_string(k_address_mode_map, state["address_v"].as_cstr(), PEN_TEXTURE_ADDRESS_WRAP);
                scp.address_w = mode_from_string(k_address_mode_map, state["address_w"].as_cstr(), PEN_TEXTURE_ADDRESS_WRAP);

                scp.mip_lod_bias = state["mip_lod_bias"].as_f32(0.0f);
                scp.max_anisotropy = state["max_anisotropy"].as_u32(0);

                scp.comparison_func =
                    mode_from_string(k_comparison_mode_map, state["comparison_func"].as_cstr(), PEN_COMPARISON_ALWAYS);

                pen::json border = state["border"];
                if (border.type() == JSMN_ARRAY)
                {
                    if (border.size() == 4)
                        for (s32 i = 0; i < 4; ++i)
                            scp.border_color[i] = border[i].as_f32();
                }

                scp.min_lod = state["min_lod"].as_f32(0.0f);
                scp.max_lod = state["max_lod"].as_f32(FLT_MAX);

                hash_id hh = PEN_HASH(scp);

                render_state rs;
                rs.hash = hh;
                rs.name = state.name();
                rs.id_name = PEN_HASH(rs.name);
                rs.type = RS_SAMPLER;

                render_state* existing_state = get_state_by_hash(hh);
                if (existing_state)
                    rs.handle = existing_state->handle;
                else
                    rs.handle = pen::renderer_create_sampler(scp);

                s_render_states.push_back(rs);
            }
        }

        void parse_raster_states(pen::json& render_config)
        {
            pen::json j_raster_states = render_config["raster_states"];
            s32       num = j_raster_states.size();
            for (s32 i = 0; i < num; ++i)
            {
                pen::rasteriser_state_creation_params rcp;

                pen::json state = j_raster_states[i];

                rcp.fill_mode = mode_from_string(k_fill_mode_map, state["fill_mode"].as_cstr(), PEN_FILL_SOLID);
                rcp.cull_mode = mode_from_string(k_cull_mode_map, state["cull_mode"].as_cstr(), PEN_CULL_BACK);
                rcp.front_ccw = state["front_ccw"].as_bool(false) ? 1 : 0;
                rcp.depth_bias = state["depth_bias"].as_s32(0);
                rcp.depth_bias_clamp = state["depth_bias_clamp"].as_f32(0.0f);
                rcp.sloped_scale_depth_bias = state["sloped_scale_depth_bias"].as_f32(0.0f);
                rcp.depth_clip_enable = state["depth_clip_enable"].as_bool(true) ? 1 : 0;
                rcp.scissor_enable = state["scissor_enable"].as_bool(false) ? 1 : 0;
                rcp.multisample = state["multisample"].as_bool(true) ? 1 : 0;
                rcp.aa_lines = state["aa_lines"].as_bool(false) ? 1 : 0;

                hash_id hh = PEN_HASH(rcp);

                render_state rs;
                rs.hash = hh;
                rs.name = state.name();
                rs.id_name = PEN_HASH(rs.name);
                rs.type = RS_RASTERIZER;

                render_state* existing_state = get_state_by_hash(hh);
                if (existing_state)
                    rs.handle = existing_state->handle;
                else
                    rs.handle = pen::renderer_create_rasterizer_state(rcp);

                s_render_states.push_back(rs);
            }
        }

        struct partial_blend_state
        {
            hash_id                  id_name;
            pen::render_target_blend rtb;
        };
        static std::vector<partial_blend_state> s_partial_blend_states;

        void parse_partial_blend_states(pen::json& render_config)
        {
            pen::json j_blend_states = render_config["blend_states"];
            s32       num = j_blend_states.size();
            for (s32 i = 0; i < num; ++i)
            {
                pen::render_target_blend rtb;

                pen::json state = j_blend_states[i];

                rtb.blend_enable = state["blend_enable"].as_bool(false);
                rtb.src_blend = mode_from_string(k_blend_mode_map, state["src_blend"].as_cstr(), PEN_BLEND_ONE);
                rtb.dest_blend = mode_from_string(k_blend_mode_map, state["dest_blend"].as_cstr(), PEN_BLEND_ZERO);
                rtb.blend_op = mode_from_string(k_blend_op_mode_map, state["blend_op"].as_cstr(), PEN_BLEND_OP_ADD);

                rtb.src_blend_alpha = mode_from_string(k_blend_mode_map, state["src_blend_alpha"].as_cstr(), PEN_BLEND_ONE);
                rtb.dest_blend_alpha =
                    mode_from_string(k_blend_mode_map, state["dest_blend_alpha"].as_cstr(), PEN_BLEND_ZERO);
                rtb.blend_op_alpha =
                    mode_from_string(k_blend_op_mode_map, state["alpha_blend_op"].as_cstr(), PEN_BLEND_OP_ADD);

                /// make partial blend states for per rt blending
                s_partial_blend_states.push_back({PEN_HASH(state.name().c_str()), rtb});
                
                // create a generic single blend for code use
                pen::blend_creation_params bcp;
                bcp.alpha_to_coverage_enable = false; // todo atoc
                bcp.independent_blend_enable = false; // todo independent blend
                bcp.num_render_targets = 1;
                bcp.render_targets = new pen::render_target_blend[1];
                bcp.render_targets[0] = rtb;

                render_state rs;
                rs.name = state.name();
                rs.id_name = PEN_HASH(state.name().c_str());
                rs.type = RS_BLEND;
                rs.handle = pen::renderer_create_blend_state(bcp);
                s_render_states.push_back(rs);
            }
        }

        void parse_stencil_state(pen::json& depth_stencil_state, pen::stencil_op* front, pen::stencil_op* back)
        {
            pen::stencil_op op;

            op.stencil_failop =
                mode_from_string(k_stencil_mode_map, depth_stencil_state["stencil_fail"].as_cstr(), PEN_STENCIL_OP_KEEP);

            op.stencil_depth_failop =
                mode_from_string(k_stencil_mode_map, depth_stencil_state["depth_fail"].as_cstr(), PEN_STENCIL_OP_KEEP);

            op.stencil_passop =
                mode_from_string(k_stencil_mode_map, depth_stencil_state["stencil_pass"].as_cstr(), PEN_STENCIL_OP_REPLACE);

            op.stencil_func =
                mode_from_string(k_comparison_mode_map, depth_stencil_state["stencil_func"].as_cstr(), PEN_COMPARISON_ALWAYS);

            if (front)
                memcpy(front, &op, sizeof(op));

            if (back)
                memcpy(back, &op, sizeof(op));
        }

        void parse_depth_stencil_states(pen::json& render_config)
        {
            pen::json j_ds_states = render_config["depth_stencil_states"];
            s32       num = j_ds_states.size();
            for (s32 i = 0; i < num; ++i)
            {
                pen::depth_stencil_creation_params dscp;

                pen::json state = j_ds_states[i];

                dscp.depth_enable = state["depth_enable"].as_bool(false) ? 1 : 0;
                dscp.depth_write_mask = state["depth_write"].as_bool(false) ? 1 : 0;

                dscp.depth_func =
                    mode_from_string(k_comparison_mode_map, state["depth_func"].as_cstr(), PEN_COMPARISON_ALWAYS);

                dscp.stencil_enable = state["stencil_enable"].as_bool(false) ? 1 : 0;
                dscp.stencil_read_mask = state["stencil_read_mask"].as_u8_hex(0);
                dscp.stencil_write_mask = state["stencil_write_mask"].as_u8_hex(0);

                pen::json op = state["stencil_op"];
                parse_stencil_state(op, &dscp.front_face, &dscp.back_face);

                pen::json op_front = state["stencil_op_front"];
                parse_stencil_state(op_front, &dscp.front_face, nullptr);

                pen::json op_back = state["stencil_op_back"];
                parse_stencil_state(op_back, nullptr, &dscp.back_face);

                hash_id hh = PEN_HASH(dscp);

                render_state rs;
                rs.hash = hh;
                rs.name = state.name();
                rs.id_name = PEN_HASH(rs.name);
                rs.type = RS_DEPTH_STENCIL;

                render_state* existing_state = get_state_by_hash(hh);
                if (existing_state)
                    rs.handle = existing_state->handle;
                else
                    rs.handle = pen::renderer_create_depth_stencil_state(dscp);

                s_render_states.push_back(rs);
            }
        }

        void parse_filters(pen::json& render_config)
        {
            pen::json j_filters = render_config["filter_kernels"];
            s32       num = j_filters.size();
            for (s32 i = 0; i < num; ++i)
            {
                pen::json jfk = j_filters[i];

                filter_kernel fk;
                fk.name = j_filters[i].name();
                fk.id_name = PEN_HASH(fk.name.c_str());

                // weights / offsets
                pen::json jweights = jfk["weights"];
                pen::json joffsets = jfk["offsets"];
                pen::json joffsets_xy = jfk["offsets_xy"];

                u32 num_xy = joffsets_xy.size();
                u32 num_w = jweights.size();
                u32 num_o = joffsets.size();

                u32 num_samples = 0;

                if (num_xy > 0)
                {
                    // offsets xy
                    for (u32 s = 0; s < num_xy; s += 2)
                    {
                        f32 x = joffsets_xy[s].as_f32();
                        f32 y = joffsets_xy[s + 1].as_f32();

                        fk.offset_weight[num_samples] = vec4f(x, y, 0.0f, 0.0f);
                        num_samples++;
                    }

                    for (u32 s = 0; s < num_w; ++s)
                    {
                        f32 w = jweights[s].as_f32();
                        fk.offset_weight[s].z = w;
                    }
                }
                else
                {
                    // offsets weights
                    num_samples = std::max<u32>(num_w, num_o);
                    for (u32 s = 0; s < num_samples; ++s)
                    {
                        f32 w = 0.0f;
                        f32 o = 0.0f;

                        if (s < num_w)
                            w = jweights[s].as_f32();

                        if (s < num_o)
                            o = joffsets[s].as_f32();

                        fk.offset_weight[s] = vec4f(o, w, 0.0f, 0.0f);
                    }
                }

                fk.info.z = num_samples;
                s_filter_kernels.push_back(fk);
            }
        }

        u32 create_blend_state(const c8* view_name, pen::json& blend_state, pen::json& write_mask, bool alpha_to_coverage)
        {
            std::vector<pen::render_target_blend> rtb;

            if (blend_state.type() != JSMN_UNDEFINED)
            {
                if (blend_state.type() == JSMN_ARRAY)
                {
                    for (s32 i = 0; i < blend_state.size(); ++i)
                    {
                        hash_id hh = PEN_HASH(blend_state[i].as_cstr());

                        for (auto& b : s_partial_blend_states)
                        {
                            if (hh == b.id_name)
                                rtb.push_back(b.rtb);
                        }
                    }
                }
                else
                {
                    hash_id hh = PEN_HASH(blend_state.as_cstr());

                    for (auto& b : s_partial_blend_states)
                    {
                        if (hh == b.id_name)
                            rtb.push_back(b.rtb);
                    }
                }
            }

            std::vector<u8> masks;
            if (write_mask.type() != JSMN_UNDEFINED)
            {
                if (write_mask.type() == JSMN_ARRAY)
                {
                    for (s32 i = 0; i < write_mask.size(); ++i)
                        masks.push_back(write_mask[i].as_u8_hex());
                }
                else
                {
                    masks.push_back(write_mask.as_u8_hex(0x0F));
                }
            }

            if (masks.size() == 0)
                masks.push_back(0x0F);

            bool   multi_blend = rtb.size() > 1 || write_mask.size() > 1;
            size_t num_rt = std::max<size_t>(rtb.size(), write_mask.size());

            // splat
            size_t rtb_start = rtb.size();
            rtb.resize(num_rt);
            for (s32 i = rtb_start; i < num_rt; ++i)
                rtb[i] = rtb[i - 1];

            size_t mask_start = masks.size();
            masks.resize(num_rt);
            for (s32 i = mask_start; i < num_rt; ++i)
                masks[i] = masks[i - 1];

            pen::blend_creation_params bcp;
            bcp.alpha_to_coverage_enable = alpha_to_coverage;
            bcp.independent_blend_enable = multi_blend;
            bcp.num_render_targets = num_rt;
            bcp.render_targets = new pen::render_target_blend[num_rt];

            for (s32 i = 0; i < num_rt; ++i)
            {
                bcp.render_targets[i] = rtb[i];
                bcp.render_targets[i].render_target_write_mask = masks[i];
            }

            pen::hash_murmur hm;
            hm.begin();
            hm.add(&bcp.alpha_to_coverage_enable, sizeof(bcp.alpha_to_coverage_enable));
            hm.add(&bcp.independent_blend_enable, sizeof(bcp.independent_blend_enable));
            hm.add(&bcp.num_render_targets, sizeof(bcp.num_render_targets));
            for (s32 i = 0; i < num_rt; ++i)
                hm.add(&bcp.render_targets[i], sizeof(bcp.render_targets[i]));

            hash_id hh = hm.end();

            render_state rs;
            rs.hash = hh;
            rs.name = view_name;
            rs.id_name = PEN_HASH(view_name);
            rs.type = RS_BLEND;

            render_state* existing_state = get_state_by_hash(hh);
            if (existing_state)
                rs.handle = existing_state->handle;
            else
                rs.handle = pen::renderer_create_blend_state(bcp);

            s_render_states.push_back(rs);

            return rs.handle;
        }

        void add_backbuffer_targets()
        {
            // add 2 defaults
            render_target main_colour;
            main_colour.id_name = k_id_main_colour;
            main_colour.name = "Backbuffer Colour";
            main_colour.ratio = 1;
            main_colour.format = PEN_TEX_FORMAT_RGBA8_UNORM;
            main_colour.handle = PEN_BACK_BUFFER_COLOUR;
            main_colour.num_mips = 1;
            main_colour.pp = VRT_WRITE;
            main_colour.flags = RT_WRITE_ONLY;

            s_render_targets.push_back(main_colour);
            s_render_target_tcp.push_back(texture_creation_params());

            render_target main_depth;
            main_depth.id_name = k_id_main_depth;
            main_depth.name = "Backbuffer Depth";
            main_depth.ratio = 1;
            main_depth.format = PEN_TEX_FORMAT_D24_UNORM_S8_UINT;
            main_depth.handle = PEN_BACK_BUFFER_DEPTH;
            main_depth.num_mips = 1;
            main_depth.pp = VRT_WRITE;
            main_depth.flags = RT_WRITE_ONLY;

            s_render_targets.push_back(main_depth);
            s_render_target_tcp.push_back(texture_creation_params());
        }

        void parse_render_targets(const pen::json& render_config, Str* include_targets)
        {
            pen::json j_render_targets = render_config["render_targets"];

            s32 num = j_render_targets.size();

            for (s32 i = 0; i < num; ++i)
            {
                pen::json r = j_render_targets[i];

                hash_id id_format = r["format"].as_hash_id();

                if (include_targets)
                {
                    // only parse specific targets in list
                    bool include = false;
                    u32  num_includes = sb_count(include_targets);
                    for (int j = 0; j < num_includes; ++j)
                    {
                        // check for existing
                        bool exists = false;
                        for (auto& er : s_render_targets)
                        {
                            if (er.name == r.name())
                            {
                                exists = true;
                                break;
                            }
                        }

                        if (exists)
                            continue;

                        if (r.name() == include_targets[j])
                        {
                            include = true;
                            break;
                        }
                    }

                    if (!include)
                        continue;
                }

                for (s32 f = 0; f < PEN_ARRAY_SIZE(rt_format); ++f)
                {
                    if (rt_format[f].id_name == id_format)
                    {
                        s_render_targets.push_back(render_target());
                        render_target& new_info = s_render_targets.back();

                        s_render_target_tcp.push_back(texture_creation_params());
                        pen::texture_creation_params& tcp = s_render_target_tcp.back();

                        new_info.ratio = 0;
                        new_info.name = r.name();
                        new_info.id_name = PEN_HASH(r.name().c_str());

                        pen::json size = r["size"];
                        if (size.size() == 2)
                        {
                            // explicit size
                            new_info.width = size[0].as_s32();
                            new_info.height = size[1].as_s32();
                        }
                        else
                        {
                            // ratio
                            new_info.width = 0;
                            new_info.height = 0;

                            Str ratio_str = size.as_str();

                            for (s32 rr = 0; rr < PEN_ARRAY_SIZE(rt_ratio); ++rr)
                                if (rt_ratio[rr] == ratio_str)
                                {
                                    new_info.width = pen::BACK_BUFFER_RATIO;
                                    new_info.height = rr;
                                    new_info.ratio = rr;
                                    break;
                                }
                        }

                        new_info.num_mips = 1;
                        new_info.format = rt_format[f].format;

                        tcp.data = nullptr;
                        tcp.width = new_info.width;
                        tcp.height = new_info.height;
                        tcp.format = new_info.format;
                        tcp.pixels_per_block = 1;
                        tcp.block_size = rt_format[f].block_size;
                        tcp.usage = PEN_USAGE_DEFAULT;
                        tcp.flags = 0;
                        tcp.num_mips = 1;
                        tcp.num_arrays = 1;
                        tcp.collection_type = pen::TEXTURE_COLLECTION_NONE;

                        // mips
                        if (r["mips"].as_bool(false))
                        {
                            new_info.num_mips = calc_num_mips(new_info.width, new_info.height);
                            tcp.num_mips = new_info.num_mips;
                        }

                        // cubes and arrays
                        Str type = r["type"].as_str("");
                        if (!type.empty())
                        {
                            if (type == "cube")
                            {
                                new_info.collection = pen::TEXTURE_COLLECTION_CUBE;
                                tcp.collection_type = pen::TEXTURE_COLLECTION_CUBE;
                                tcp.flags |= PEN_RESOURCE_MISC_TEXTURECUBE;
                                tcp.num_arrays = 6;
                            }
                            else if (type == "array")
                            {
                                new_info.collection = pen::TEXTURE_COLLECTION_ARRAY;
                                tcp.collection_type = pen::TEXTURE_COLLECTION_CUBE;
                                tcp.num_arrays = r["num_arrays"].as_u32(1);
                            }
                        }

                        // flags
                        tcp.cpu_access_flags = 0;

                        if (r["cpu_read"].as_bool(false))
                            tcp.cpu_access_flags |= PEN_CPU_ACCESS_READ;

                        if (r["cpu_write"].as_bool(false))
                            tcp.cpu_access_flags |= PEN_CPU_ACCESS_WRITE;

                        static hash_id id_write = PEN_HASH("write");
                        if (r["pp"].as_hash_id() == id_write)
                        {
                            new_info.pp = VRT_WRITE;
                            new_info.flags |= RT_AUX;
                        }

                        hash_id idr = r["init_read"].as_hash_id();
                        u32     hr = 0;
                        if (idr != 0)
                        {
                            // load any init read targets
                            Str* include_rt = nullptr;
                            sb_push(include_rt, r["init_read"].as_str());
                            parse_render_targets(render_config, include_rt);
                            sb_free(include_rt);

                            for (auto& rt : s_render_targets)
                            {
                                if (rt.id_name == idr)
                                {
                                    new_info.pp_read = hr;
                                    break;
                                }
                                ++hr;
                            }
                        }

                        tcp.bind_flags = rt_format[f].flags | PEN_BIND_SHADER_RESOURCE;

                        // msaa
                        tcp.sample_count = r["samples"].as_u32(1);
                        tcp.sample_quality = 0;

                        new_info.samples = tcp.sample_count;

                        new_info.handle = pen::renderer_create_render_target(tcp);
                    }
                }
            }
        }

        const render_target* get_render_target(hash_id h)
        {
            size_t num = s_render_targets.size();
            for (u32 i = 0; i < num; ++i)
            {
                if (s_render_targets[i].id_name == h)
                {
                    return &s_render_targets[i];
                }
            }

            return nullptr;
        }

        void resize_render_target(hash_id target, u32 width, u32 height, const c8* format)
        {
            render_target* current_target = nullptr;
            size_t         num = s_render_targets.size();
            for (u32 i = 0; i < num; ++i)
            {
                if (s_render_targets[i].id_name == target)
                {
                    current_target = &s_render_targets[i];
                    break;
                }
            }

            s32 new_format = current_target->format;
            u32 format_index = 0;

            if (format)
            {
                hash_id id_format = PEN_HASH(format);
                for (auto& fmt : rt_format)
                {
                    if (fmt.id_name == id_format)
                    {
                        new_format = fmt.format;
                        break;
                    }

                    format_index++;
                }
            }
            else
            {
                for (auto& fmt : rt_format)
                {
                    if (fmt.format == new_format)
                        break;

                    format_index++;
                }
            }

            if (current_target->width == width && current_target->height == height && current_target->format == new_format)
            {
                return;
            }

            pen::texture_creation_params tcp;
            tcp.data = nullptr;
            tcp.width = width;
            tcp.height = height;
            tcp.format = new_format;
            tcp.pixels_per_block = 1;
            tcp.block_size = rt_format[format_index].block_size;
            tcp.usage = PEN_USAGE_DEFAULT;
            tcp.flags = 0;
            tcp.num_mips = 1;
            tcp.num_arrays = 1;
            tcp.sample_count = 1;
            tcp.cpu_access_flags = PEN_CPU_ACCESS_READ;
            tcp.sample_quality = 0;
            tcp.bind_flags = rt_format[format_index].flags | PEN_BIND_SHADER_RESOURCE;
            tcp.collection_type = pen::TEXTURE_COLLECTION_NONE;

            u32 h = pen::renderer_create_render_target(tcp);
            pen::renderer_replace_resource(current_target->handle, h, pen::RESOURCE_RENDER_TARGET);

            current_target->width = width;
            current_target->height = height;
            current_target->format = new_format;
        }

        void resize_viewports()
        {
            size_t num = s_views.size();
            for (s32 i = 0; i < num; ++i)
            {
                s32 target_w;
                s32 target_h;
                f32 target_r;

                bool first = true;

                for (s32 j = 0; j < pen::MAX_MRT; ++j)
                {
                    if (s_views[i].id_render_target[j] == 0)
                        continue;

                    const render_target* rt = get_render_target(s_views[i].id_render_target[j]);

                    if (!first)
                    {
                        bool valid = true;

                        if (rt->width != target_w)
                            valid = false;

                        if (rt->height != target_h)
                            valid = false;

                        if (rt->ratio != target_r)
                            valid = false;

                        if (!valid)
                        {
                            dev_console_log_level(dev_ui::CONSOLE_ERROR,
                                                  "[error] render controller: render target %s is incorrect dimension",
                                                  rt->name.c_str());
                        }
                    }

                    target_w = rt->width;
                    target_h = rt->height;
                    target_r = rt->ratio;
                    first = false;
                }

                s_views[i].rt_width = target_w;
                s_views[i].rt_height = target_h;
                s_views[i].rt_ratio = target_r;
            }
        }

        void get_render_target_dimensions(const render_target* rt, f32& w, f32& h)
        {
            get_rt_dimensions(rt->width, rt->height, rt->ratio, w, h);
        }

        void parse_clear_colour(pen::json& view, view_params& new_view, s32 num_targets)
        {
            // clear colour
            pen::json clear_colour = view["clear_colour"];

            f32 clear_colour_f[4] = {0};
            u8  clear_stencil_val = 0;

            u32 clear_flags = 0;

            if (clear_colour.size() == 4)
            {
                for (s32 c = 0; c < 4; ++c)
                {
                    if (!(clear_colour[c].as_str() == "false"))
                    {
                        clear_colour_f[c] = clear_colour[c].as_f32();
                        clear_flags |= PEN_CLEAR_COLOUR_BUFFER;
                    }
                }
            }

            // clear depth
            pen::json clear_depth = view["clear_depth"];

            f32 clear_depth_f;

            if (clear_depth.type() != JSMN_UNDEFINED)
            {
                if (!(clear_depth.as_str() == "false"))
                {
                    clear_depth_f = clear_depth.as_f32();

                    clear_flags |= PEN_CLEAR_DEPTH_BUFFER;
                }
            }

            // clear stencil
            pen::json clear_stencil = view["clear_stencil"];

            if (clear_stencil.type() != JSMN_UNDEFINED)
            {
                if (!(clear_stencil.as_str() == "false"))
                {
                    clear_stencil_val = clear_stencil.as_u8_hex();

                    clear_flags |= PEN_CLEAR_STENCIL_BUFFER;
                }
            }

            // clear state
            pen::clear_state cs_info = {
                clear_colour_f[0], clear_colour_f[1], clear_colour_f[2], clear_colour_f[3],
                clear_depth_f,     clear_stencil_val, clear_flags,
            };

            // clear mrt
            pen::json clear_mrt = view["clear"];
            for (s32 m = 0; m < clear_mrt.size(); ++m)
            {
                pen::json jmrt = clear_mrt[m];

                hash_id rt_id = PEN_HASH(jmrt.name().c_str());

                for (s32 t = 0; t < num_targets; ++t)
                {
                    if (new_view.id_render_target[t] == rt_id)
                    {
                        cs_info.num_colour_targets++;

                        pen::json colour_f = jmrt["clear_colour_f"];
                        if (colour_f.size() == 4)
                        {
                            for (s32 j = 0; j < 4; ++j)
                            {
                                cs_info.mrt[t].type = pen::CLEAR_F32;
                                cs_info.mrt[t].f[j] = colour_f[j].as_f32();
                            }

                            break;
                        }

                        pen::json colour_u = jmrt["clear_colour_u"];
                        if (colour_u.size() == 4)
                        {
                            for (s32 j = 0; j < 4; ++j)
                            {
                                cs_info.mrt[t].type = pen::CLEAR_U32;
                                cs_info.mrt[t].u[j] = colour_u[j].as_u32();
                            }

                            break;
                        }
                    }
                }
            }

            new_view.clear_state = pen::renderer_create_clear_state(cs_info);
        }

        void parse_views(pen::json& j_views, const pen::json& all_views, std::vector<view_params>& view_array,
                         const char* group = nullptr)
        {
            s32 num = j_views.size();
            for (s32 i = 0; i < num; ++i)
            {
                bool valid = true;

                view_params new_view;

                pen::json view = j_views[i];

                if (group)
                {
                    new_view.id_group = PEN_HASH(group);
                    new_view.group = group;
                }
                else
                {
                    new_view.name = view.name();
                    new_view.id_name = PEN_HASH(view.name());
                }

                // inherit and combine
                Str ihv = view["inherit"].as_str();
                for (;;)
                {
                    if (ihv == "")
                        break;

                    pen::json inherit_view = all_views[ihv.c_str()];

                    // inherit name if we dont have one (group view in post process view array)
                    if (new_view.name.empty())
                    {
                        new_view.name = ihv.c_str();
                        new_view.id_name = PEN_HASH(new_view.name);
                    }

                    view = pen::json::combine(inherit_view, view);

                    ihv = inherit_view["inherit"].as_str();
                }

                // render targets
                pen::json targets = view["target"];

                s32 num_targets = targets.size();

                s32 cur_rt = 0;

                new_view.depth_target = PEN_INVALID_HANDLE;
                new_view.info_json = view.dumps();

                for (s32 t = 0; t < num_targets; ++t)
                {
                    Str     target_str = targets[t].as_str();
                    hash_id target_hash = PEN_HASH(target_str.c_str());

                    new_view.viewport_correction = pen::renderer_viewport_vup();
                    if (target_hash == k_id_main_colour || target_hash == k_id_main_depth)
                        new_view.viewport_correction = false;

                    bool found = false;
                    for (auto& r : s_render_targets)
                    {
                        if (target_hash == r.id_name)
                        {
                            found = true;

                            s32 w = r.width;
                            s32 h = r.height;
                            s32 rr = r.ratio;

                            if (r.collection == pen::TEXTURE_COLLECTION_CUBE)
                                new_view.num_arrays = 6;

                            if (cur_rt == 0)
                            {
                                new_view.rt_width = w;
                                new_view.rt_height = h;
                                new_view.rt_ratio = rr;
                            }
                            else
                            {
                                if (new_view.rt_width != w || new_view.rt_height != h || new_view.rt_ratio != rr)
                                {
                                    dev_console_log_level(
                                        dev_ui::CONSOLE_ERROR,
                                        "[error] render controller: render target %s is incorrect dimension",
                                        target_str.c_str());

                                    valid = false;
                                }
                            }

                            if (r.format == PEN_TEX_FORMAT_D24_UNORM_S8_UINT)
                            {
                                new_view.depth_target = r.handle;
                                new_view.id_depth_target = target_hash;
                            }
                            else
                            {
                                new_view.id_render_target[cur_rt] = target_hash;
                                new_view.render_targets[cur_rt++] = r.handle;
                            }

                            new_view.num_colour_targets = cur_rt;

                            break;
                        }
                    }

                    if (!found)
                    {
                        dev_console_log_level(dev_ui::CONSOLE_ERROR, "[error] render controller: missing render target - %s",
                                              target_str.c_str());
                        valid = false;
                    }
                }

                parse_clear_colour(view, new_view, num_targets);

                // viewport
                pen::json viewport = view["viewport"];

                if (viewport.size() == 4)
                {
                    for (s32 v = 0; v < 4; ++v)
                        new_view.viewport[v] = viewport[v].as_f32();
                }
                else
                {
                    new_view.viewport[0] = 0.0f;
                    new_view.viewport[1] = 0.0f;
                    new_view.viewport[2] = 1.0f;
                    new_view.viewport[3] = 1.0f;
                }

                // render state
                render_state* state = nullptr;

                // raster
                Str raster_state = view["raster_state"].as_cstr("default");
                state = _get_render_state(PEN_HASH(raster_state.c_str()), RS_RASTERIZER);
                if (state)
                    new_view.raster_state = state->handle;

                // depth stencil
                Str depth_stencil_state = view["depth_stencil_state"].as_cstr("default");
                state = _get_render_state(PEN_HASH(depth_stencil_state.c_str()), RS_DEPTH_STENCIL);
                if (state)
                    new_view.depth_stencil_state = state->handle;

                // blend
                bool      alpha_to_coverage = view["alpha_to_coverage"].as_bool();
                pen::json colour_write_mask = view["colour_write_mask"];
                pen::json blend_state = view["blend_state"];

                new_view.blend_state =
                    create_blend_state(view.name().c_str(), blend_state, colour_write_mask, alpha_to_coverage);

                // scene
                Str scene_str = view["scene"].as_str();

                new_view.scene = nullptr;
                if (scene_str.length() > 0)
                {
                    hash_id scene_id = PEN_HASH(scene_str.c_str());

                    bool found_scene = false;
                    for (auto& s : s_controllers)
                    {
                        if (s.id_name == scene_id)
                        {
                            new_view.scene = s.scene;
                            found_scene = true;
                            break;
                        }
                    }

                    if (!found_scene)
                    {
                        dev_console_log_level(dev_ui::CONSOLE_ERROR, "[error] render controller: missing scene - %s",
                                              scene_str.c_str());
                        valid = false;
                    }
                }

                // camera
                Str camera_str = view["camera"].as_str();

                new_view.camera = nullptr;
                if (camera_str.length() > 0)
                {
                    hash_id camera_id = PEN_HASH(camera_str.c_str());

                    bool found_camera = false;
                    for (auto& c : s_controllers)
                    {
                        if (c.id_name == camera_id)
                        {
                            new_view.camera = c.camera;
                            found_camera = true;
                            break;
                        }
                    }

                    if (!found_camera)
                    {
                        dev_console_log_level(dev_ui::CONSOLE_ERROR, "[error] render controller: missing camera - %s",
                                              camera_str.c_str());
                        valid = false;
                    }
                }

                // shader and technique
                Str technique_str = view["technique"].as_str();
                new_view.technique = PEN_HASH(technique_str.c_str());
                new_view.pmfx_shader = pmfx::load_shader(view["pmfx_shader"].as_cstr());
                new_view.technique_permutation = view["permutation"].as_u32();

                if (view["pmfx_shader"].as_cstr() && !is_valid(new_view.pmfx_shader))
                {
                    dev_console_log_level(dev_ui::CONSOLE_ERROR, "[error] render controller: missing shader %s",
                                          view["pmfx_shader"].as_cstr());
                    valid = false;
                }

                // render flags
                pen::json render_flags = view["render_flags"];
                new_view.render_flags = 0;
                for (s32 f = 0; f < render_flags.size(); ++f)
                {
                    new_view.render_flags |= mode_from_string(render_flags_map, render_flags[f].as_cstr(), 0);
                }

                // scene views
                pen::json scene_views = view["scene_views"];
                for (s32 ii = 0; ii < scene_views.size(); ++ii)
                {
                    hash_id id = scene_views[ii].as_hash_id();
                    bool    found = false;
                    for (auto& sv : s_scene_view_renderers)
                    {
                        if (id == sv.id_name)
                        {
                            found = true;
                            new_view.render_functions.push_back(sv.render_function);
                        }
                    }

                    if (!found)
                    {
                        dev_console_log_level(dev_ui::CONSOLE_ERROR,
                                              "[error] render controller: missing scene view - '%s' required by view: '%s'",
                                              scene_views[ii].as_cstr(), new_view.name.c_str());
                    }
                }

                // sampler bindings
                parse_sampler_bindings(view, new_view);

                new_view.post_process_name = view["post_process"].as_cstr();
                if (!new_view.post_process_name.empty())
                    new_view.post_process_flags |= PP_ENABLED;

                if (view["pp_write_non_aux"].as_bool())
                    new_view.post_process_flags |= PP_WRITE_NON_AUX;

                // filter id for post process passes
                Str fk = view["filter_kernel"].as_str();

                if (!fk.empty())
                {
                    new_view.id_filter = view["filter_kernel"].as_hash_id();

                    // create filter for cbuffer
                    struct dir_preset
                    {
                        hash_id id;
                        vec2f   dir;
                    };

                    static dir_preset presets[] = {{PEN_HASH("horizontal"), vec2f(1.0, 0.0)},
                                                   {PEN_HASH("vertical"), vec2f(0.0, 1.0)}};

                    vec2f vdir = vec2f::zero();

                    if (view["filter_direction"].size() == 2)
                    {
                        for (u32 i = 0; i < 2; ++i)
                            vdir[i] = view["filter_direction"][i].as_f32();
                    }
                    else
                    {
                        hash_id id_dir = view["filter_direction"].as_hash_id();
                        for (auto& p : presets)
                            if (p.id == id_dir)
                                vdir = p.dir;
                    }

                    filter_kernel fk;
                    for (auto& f : s_filter_kernels)
                        if (f.id_name == new_view.id_filter)
                            fk = f;

                    fk.info.x = vdir.x;
                    fk.info.y = vdir.y;

                    // create the cbuffer itself
                    static const u32            filter_cbuffer_size = sizeof(fk.offset_weight) + sizeof(fk.info);
                    pen::buffer_creation_params bcp;
                    bcp.usage_flags = PEN_USAGE_DYNAMIC;
                    bcp.bind_flags = PEN_BIND_CONSTANT_BUFFER;
                    bcp.cpu_access_flags = PEN_CPU_ACCESS_WRITE;
                    bcp.buffer_size = filter_cbuffer_size;
                    bcp.data = nullptr;

                    new_view.cbuffer_filter = pen::renderer_create_buffer(bcp);

                    pen::renderer_update_buffer(new_view.cbuffer_filter, &fk.info, filter_cbuffer_size);
                }

                if (is_valid(new_view.pmfx_shader))
                {
                    u32 ti = get_technique_index_perm(new_view.pmfx_shader, new_view.technique);
                    if (has_technique_constants(new_view.pmfx_shader, ti))
                    {
                        pen::buffer_creation_params bcp;
                        bcp.usage_flags = PEN_USAGE_DYNAMIC;
                        bcp.bind_flags = PEN_BIND_CONSTANT_BUFFER;
                        bcp.cpu_access_flags = PEN_CPU_ACCESS_WRITE;
                        bcp.buffer_size = sizeof(technique_constant_data);
                        bcp.data = nullptr;

                        new_view.cbuffer_technique = pen::renderer_create_buffer(bcp);
                    }
                }

                if (valid)
                    view_array.push_back(new_view);
            }
        }

        namespace
        {
            struct virtual_rt
            {
                hash_id id = 0;
                u32     rt_index[VRT_NUM] = {PEN_INVALID_HANDLE, PEN_INVALID_HANDLE};
                u32     rt_read = PEN_INVALID_HANDLE;
                bool    swap;
            };
            std::vector<virtual_rt> s_virtual_rt;

            void add_virtual_target(hash_id id)
            {
                // check existing
                for (auto& vrt : s_virtual_rt)
                    if (vrt.id == id)
                        return;

                // find rt
                u32 rt_index;
                for (u32 i = 0; i < s_render_targets.size(); ++i)
                {
                    if (s_render_targets[i].id_name == id)
                    {
                        rt_index = i;
                        break;
                    }
                }

                // add new
                virtual_rt vrt;
                vrt.id = id;
                vrt.rt_index[VRT_READ] = PEN_INVALID_HANDLE;
                vrt.rt_index[VRT_WRITE] = PEN_INVALID_HANDLE;
                vrt.swap = false;

                u32 pp = s_render_targets[rt_index].pp;

                // flag aux in use
                if (s_render_targets[rt_index].flags & RT_AUX)
                    s_render_targets[rt_index].flags |= RT_AUX_USED;

                vrt.rt_index[pp] = rt_index;

                // first read from a scene view render target
                u32 ppr = s_render_targets[rt_index].pp_read;
                if (is_valid(ppr) && pp == VRT_WRITE)
                {
                    vrt.rt_index[VRT_READ] = ppr;
                }

                s_virtual_rt.push_back(vrt);
            }

            u32 get_aux_buffer(hash_id id)
            {
                s32 rt_index = -1;
                for (u32 i = 0; i < s_render_targets.size(); ++i)
                {
                    if (id == s_render_targets[i].id_name)
                    {
                        rt_index = i;
                        break;
                    }
                }

                if (rt_index == -1)
                {
                    dev_console_log_level(dev_ui::CONSOLE_ERROR, "%s",
                                          "[error] missing render target when baking post processes");

                    return PEN_INVALID_HANDLE;
                }

                render_target* rt = &s_render_targets[rt_index];

                // find a suitable size / format aux buffer
                for (u32 i = 0; i < s_render_targets.size(); ++i)
                {
                    if (!(s_render_targets[i].flags & RT_AUX))
                        continue;

                    if (s_render_targets[i].flags & RT_AUX_USED)
                        continue;

                    bool match = true;
                    match &= rt->width == s_render_targets[i].width;
                    match &= rt->height == s_render_targets[i].height;
                    match &= rt->ratio == s_render_targets[i].ratio;
                    match &= rt->num_mips == s_render_targets[i].num_mips;
                    match &= rt->format == s_render_targets[i].format;
                    match &= rt->samples == s_render_targets[i].samples;

                    if (match)
                    {
                        s_render_targets[i].flags |= RT_AUX_USED;
                        return i;
                    }
                }

                // create an aux copy from the original rt
                render_target aux_rt = *rt;
                aux_rt.flags |= RT_AUX;
                aux_rt.flags |= RT_AUX_USED;
                aux_rt.handle = pen::renderer_create_render_target(s_render_target_tcp[rt_index]);
                aux_rt.name = rt->name;
                aux_rt.name.append("_aux");
                s_render_targets.push_back(aux_rt);
                return s_render_targets.size() - 1;
            }

            u32 get_virtual_target(hash_id id, e_rt_mode mode, bool non_aux)
            {
                u32 result = PEN_INVALID_HANDLE;

                for (auto& vrt : s_virtual_rt)
                {
                    if (vrt.id == id)
                    {
                        u32 rt_index = vrt.rt_index[mode];

                        if (is_valid(rt_index))
                        {
                            result = s_render_targets[rt_index].handle;
                        }
                        else if (mode == VRT_WRITE)
                        {
                            if (non_aux)
                            {
                                // find buffer
                                for (u32 i = 0; i < s_render_targets.size(); ++i)
                                    if (id == s_render_targets[i].id_name)
                                        return s_render_targets[i].handle;
                            }
                            else
                            {
                                rt_index = get_aux_buffer(id);
                                vrt.rt_index[mode] = rt_index;
                                result = s_render_targets[rt_index].handle;
                            }
                        }

                        if (mode == VRT_WRITE)
                            vrt.swap = true;

                        break;
                    }
                }

                PEN_ASSERT(result != PEN_INVALID_HANDLE);

                return result;
            }

            void virtual_rt_swap_buffers()
            {
                for (auto& vrt : s_virtual_rt)
                {
                    if (vrt.swap)
                    {
                        u32 wrt = vrt.rt_index[VRT_WRITE];
                        if (is_valid(wrt))
                        {
                            if (s_render_targets[wrt].flags & RT_WRITE_ONLY)
                                continue;
                        }

                        std::swap(vrt.rt_index[VRT_WRITE], vrt.rt_index[VRT_READ]);

                        // remark aux buffer usable or remove a read only rt from the write slot
                        wrt = vrt.rt_index[VRT_WRITE];

                        if (is_valid(wrt))
                        {
                            render_target& rt = s_render_targets[wrt];

                            rt.flags &= ~RT_AUX_USED;
                            vrt.rt_index[VRT_WRITE] = PEN_INVALID_HANDLE;
                        }

                        vrt.swap = false;
                    }
                }
            }

            void virtual_rt_reset()
            {
                // start any virtual rt with appropriate read only buffers in the read slot
                for (auto& vrt : s_virtual_rt)
                    vrt.rt_index[VRT_READ] = vrt.rt_read;
            }

            void bake_post_process_targets(std::vector<view_params>& pp_views)
            {
                // find render target aliases and generate automatic ping pongs

                // first create virtual rt for each unique target / texture
                for (auto& v : pp_views)
                {
                    for (u32 i = 0; i < v.num_colour_targets; ++i)
                        add_virtual_target(v.id_render_target[i]);

                    if (is_valid(v.depth_target))
                        add_virtual_target(v.id_depth_target);

                    for (auto& sb : v.sampler_bindings)
                        add_virtual_target(sb.id_texture);
                }

                u32 pass_counter = 0;
                for (auto& p : pp_views)
                {
                    bool non_aux = p.post_process_flags & PP_WRITE_NON_AUX;
                    for (u32 i = 0; i < p.num_colour_targets; ++i)
                        p.render_targets[i] = get_virtual_target(p.id_render_target[i], VRT_WRITE, non_aux);

                    if (is_valid(p.depth_target))
                        p.depth_target = get_virtual_target(p.id_depth_target, VRT_WRITE, non_aux);
                    else
                        p.depth_target = PEN_NULL_DEPTH_BUFFER;

                    for (auto& sb : p.sampler_bindings)
                        sb.handle = get_virtual_target(sb.id_texture, VRT_READ, false);

                    // material for pass
                    pmfx::initialise_constant_defaults(p.pmfx_shader, p.technique, p.technique_constants.data);

                    // swap buffers
                    if (!non_aux)
                        virtual_rt_swap_buffers();

                    ++pass_counter;
                }

                // get technique sampler bindings
                for (auto& p : pp_views)
                {
                    u32 ti = get_technique_index_perm(p.pmfx_shader, p.technique);
                    if (has_technique_samplers(p.pmfx_shader, ti))
                    {
                        technique_sampler* ts = get_technique_samplers(p.pmfx_shader, ti);

                        u32 num_tt = sb_count(ts);
                        for (u32 i = 0; i < num_tt; ++i)
                        {
                            sampler_binding sb;
                            sb.handle = ts[i].handle;
                            sb.sampler_unit = ts[i].unit;
                            sb.bind_flags = PEN_SHADER_TYPE_PS;
                            sb.sampler_state = get_render_state(k_id_wrap_linear, RS_SAMPLER);

                            p.technique_samplers.sb[i] = sb;
                        }
                    }
                }
            }
        } // namespace

        void load_always_create_render_targets(const pen::json& render_config)
        {
            Str* targets = nullptr;

            json rts = render_config["render_targets"];
            for (u32 i = 0; i < rts.size(); ++i)
            {
                if (rts[i]["always_create"].as_bool() == true)
                {
                    sb_push(targets, rts[i].key());
                }
            }

            parse_render_targets(render_config, targets);
        }

        void load_view_render_targets(const pen::json& render_config, const pen::json& view)
        {
            Str* targets = nullptr;

            u32 num_targets = view["target"].size();
            for (u32 t = 0; t < num_targets; ++t)
                sb_push(targets, view["target"][t].as_str());

            u32 num_samplers = view["sampler_bindings"].size();
            for (u32 s = 0; s < num_samplers; ++s)
                sb_push(targets, view["sampler_bindings"][s]["texture"].as_str());

            if (num_targets > 0)
                parse_render_targets(render_config, targets);

            sb_free(targets);
        }

        bool load_edited_post_process(const pen::json& render_config, const pen::json& pp_config, const pen::json& j_views,
                                      view_params& v)
        {
            for (auto& epp : s_edited_post_processes)
            {
                v.post_process_chain = epp.chain;

                for (auto& ppc : v.post_process_chain)
                {
                    pen::json ppv = pp_config[ppc.c_str()];

                    // subview
                    u32 num_sub_views = ppv.size();
                    for (u32 i = 0; i < num_sub_views; ++i)
                        load_view_render_targets(render_config, ppv[i]);

                    parse_views(ppv, j_views, v.post_process_views, ppc.c_str());
                }

                bake_post_process_targets(v.post_process_views);

                // find edited settings
                for (auto& ev : epp.views)
                {
                    for (auto& pv : v.post_process_views)
                    {
                        if (pv.id_group != ev.id_group)
                            continue;

                        if (pv.id_name != ev.id_name)
                            continue;

                        // found group / view matching
                        pv.technique_samplers = ev.technique_samplers;
                        pv.technique_constants = ev.technique_constants;

                        break;
                    }
                }

                return true;
            }

            return false;
        }

        void load_post_process(const pen::json& render_config, const pen::json& pp_config, const pen::json& j_views,
                               view_params& v)
        {
            v.post_process_chain.clear();

            pen::json pp_set;
            if (str_ends_with(v.post_process_name.c_str(), ".jsn"))
            {
                pp_set = pen::json::load_from_file(v.post_process_name.c_str());
            }
            else
            {
                // look in embedded post_process_sets
                pp_set = render_config["post_process_sets"][v.post_process_name.c_str()];
            }

            if (pp_set.is_null())
            {
                dev_console_log_level(dev_ui::CONSOLE_ERROR, "[error] pmfx - missing post process set %s'",
                                      v.post_process_name.c_str());
                return;
            }

            pen::json pp_chain = pp_set["chain"];
            u32       num_pp = pp_chain.size();

            for (u32 i = 0; i < num_pp; ++i)
                v.post_process_chain.push_back(pp_chain[i].as_str());

            for (auto& ppc : v.post_process_chain)
            {
                pen::json ppv = pp_config[ppc.c_str()];

                // subview
                u32 num_sub_views = ppv.size();
                for (u32 i = 0; i < num_sub_views; ++i)
                    load_view_render_targets(render_config, ppv[i]);

                parse_views(ppv, j_views, v.post_process_views, ppc.c_str());
            }

            bake_post_process_targets(v.post_process_views);

            // load constants and samplers from data
            pen::json params = pp_set["parameters"];
            u32       num_params = params.size();
            for (u32 i = 0; i < num_params; ++i)
            {
                for (auto& pv : v.post_process_views)
                {
                    pen::json tech_params = params[i];
                    if (pv.id_name == PEN_HASH(tech_params.key()))
                    {
                        u32 ti = get_technique_index_perm(pv.pmfx_shader, pv.technique);

                        u32 num_c = tech_params.size();
                        for (u32 c = 0; c < num_c; ++c)
                        {
                            pen::json tp = tech_params[c];

                            hash_id              id_c = PEN_HASH(tp.key());
                            static const hash_id id_textures = PEN_HASH("textures");

                            if (id_c == id_textures)
                            {
                                u32 num_samplers = tp.size();
                                for (u32 s = 0; s < num_samplers; ++s)
                                {
                                    pen::json j_sampler = tp[s];

                                    hash_id            id_s = PEN_HASH(j_sampler.key());
                                    technique_sampler* ts = get_technique_sampler(id_s, pv.pmfx_shader, ti);

                                    if (!ts)
                                        continue;

                                    sampler_binding& sb = pv.technique_samplers.sb[s];

                                    sb.sampler_unit = ts->unit;
                                    sb.handle = put::load_texture(j_sampler["filename"].as_cstr());
                                    sb.sampler_state = j_sampler["filename"].as_u32();
                                    sb.id_texture = PEN_HASH(j_sampler["filename"].as_cstr());
                                    sb.bind_flags = ts->bind_flags;
                                }
                            }
                            else
                            {
                                technique_constant* tc = get_technique_constant(id_c, pv.pmfx_shader, ti);

                                if (!tc)
                                    continue;

                                PEN_ASSERT(tc->num_elements == tp.size());

                                for (u32 e = 0; e < tc->num_elements; ++e)
                                    pv.technique_constants.data[tc->cb_offset + e] = tp[e].as_f32();
                            }
                        }

                        break;
                    }
                }
            }
        }

        void load_script_internal(const c8* filename)
        {
            pen::renderer_consume_cmd_buffer();

            create_geometry_utilities();

            void* config_data;
            u32   config_data_size;

            pen_error err = pen::filesystem_read_file_to_buffer(filename, &config_data, config_data_size);

            if (err != PEN_ERR_OK || config_data_size == 0)
            {
                // failed load file
                pen::memory_free(config_data);
                dev_console_log_level(dev_ui::CONSOLE_ERROR, "[error] pmfx - failed to open %s'", filename);
                return;
            }

            // load render config
            pen::json render_config = pen::json::load((const c8*)config_data);

            // add includes
            u32 num_includes = render_config["include"].size();
            for (u32 i = 0; i < num_includes; ++i)
            {
                Str include_filename = render_config["include"][i].as_str();

                Str config_path = (const c8*)config_data;
                config_path = pen::str_normalise_filepath(filename);

                u32 last_dir = pen::str_find_reverse(config_path, "/");
                Str include_dir = pen::str_substr(config_path, 0, last_dir + 1);

                include_dir.append(include_filename.c_str());

                pen::json include_json = pen::json::load_from_file(include_dir.c_str());

                render_config = pen::json::combine(include_json, render_config);
            }

            // add main_colour and main_depth backbuffer target
            add_backbuffer_targets();
            load_always_create_render_targets(render_config); // load render targets with always_create

            // parse info
            parse_sampler_states(render_config);
            parse_raster_states(render_config);
            parse_depth_stencil_states(render_config);
            parse_partial_blend_states(render_config);
            parse_filters(render_config);

            pen::renderer_consume_cmd_buffer();

            pen::json j_views = render_config["views"];
            pen::json j_view_sets = render_config["view_sets"];

            s_view_set_name = render_config["view_set"].as_str();
            if (!s_edited_view_set_name.empty())
                s_view_set_name = s_edited_view_set_name;

            // get view set names
            u32 num_view_sets = j_view_sets.size();
            for (u32 i = 0; i < num_view_sets; ++i)
            {
                s_view_sets.push_back(j_view_sets[i].name());
            }

            // get views for view set
            pen::json j_view_set = j_view_sets[s_view_set_name.c_str()];
            u32       num_views_in_set = j_view_set.size();
            if (num_views_in_set > 0)
            {
                // views from "view_set"
                pen::json view_set;

                Str* used_targets = nullptr;

                for (u32 i = 0; i < num_views_in_set; ++i)
                {
                    Str vs = j_view_set[i].as_str();
                    s_view_set.push_back(vs);

                    pen::json v = j_views[vs.c_str()];

                    if (v.type() == JSMN_UNDEFINED)
                    {
                        dev_console_log_level(dev_ui::CONSOLE_ERROR, "[error] pmfx - view '%s' not found", vs.c_str());
                        return;
                    }

                    // inherit and combine
                    Str ihv = v["inherit"].as_str();
                    for (;;)
                    {
                        if (ihv == "")
                            break;

                        pen::json inherit_view = j_views[ihv.c_str()];

                        v = pen::json::combine(inherit_view, v);

                        ihv = inherit_view["inherit"].as_str();
                    }

                    view_set.set(vs.c_str(), v);

                    u32 num_targets = v["target"].size();
                    for (u32 t = 0; t < num_targets; ++t)
                    {
                        sb_push(used_targets, v["target"][t].as_str());
                    }
                }

                // only add targets in use
                parse_render_targets(render_config, used_targets);

                parse_views(view_set, j_views, s_views);

                sb_free(used_targets);
            }
            else
            {
                dev_console_log_level(dev_ui::CONSOLE_ERROR, "[error] pmfx - no views in view set");
            }

            pen::renderer_consume_cmd_buffer();

            // parse post process info
            pen::json pp_config = render_config["post_processes"];
            for (u32 i = 0; i < pp_config.size(); ++i)
                s_post_process_names.push_back(pp_config[i].name());

            // per view post processes
            for (auto& v : s_views)
            {
                if (v.post_process_flags & PP_ENABLED)
                {
                    if (!load_edited_post_process(render_config, pp_config, j_views, v))
                        load_post_process(render_config, pp_config, j_views, v);
                }
            }

            pen::renderer_consume_cmd_buffer();

            // rebake material handles
            ecs::bake_material_handles();
        }

        std::vector<Str> k_script_files;

        void pmfx_config_build()
        {
            Str build_cmd = get_build_cmd();

            build_cmd.append(" -actions configs");

            PEN_SYSTEM(build_cmd.c_str());
        }

        void pmfx_config_hotload()
        {
            release_script_resources();

            for (auto& s : k_script_files)
                load_script_internal(s.c_str());
        }

        void pmfx_config_hotload(std::vector<hash_id>& dirty)
        {
            pmfx_config_hotload();
        }

        void init(const c8* filename)
        {
            load_script_internal(filename);

            k_script_files.push_back(filename);

            put::add_file_watcher(filename, pmfx_config_build, pmfx_config_hotload);
        }

        void release_script_resources()
        {
            // release render states
            for (auto& rs : s_render_states)
            {
                // dev_console_log("release state %i : %s (%i)", rs.type, rs.name.c_str(), rs.handle);

                switch (rs.type)
                {
                    case RS_RASTERIZER:
                        pen::renderer_release_raster_state(rs.handle);
                        break;
                    case RS_SAMPLER:
                        pen::renderer_release_sampler(rs.handle);
                        break;
                    case RS_BLEND:
                        pen::renderer_release_blend_state(rs.handle);
                        break;
                    case RS_DEPTH_STENCIL:
                        pen::renderer_release_depth_stencil_state(rs.handle);
                        break;
                }
            }

            // release render targets
            for (auto& rt : s_render_targets)
            {
                if (rt.id_name == k_id_main_colour)
                    continue;

                if (rt.id_name == k_id_main_depth)
                    continue;

                pen::renderer_release_render_target(rt.handle);
            }

            // release clear state and clear views
            for (auto& v : s_views)
            {
                pen::renderer_release_clear_state(v.clear_state);
            }

            s_render_states.clear();
            s_render_targets.clear();
            s_render_target_tcp.clear();
            s_render_target_names.clear();
            s_view_set.clear();
            s_views.clear();
            s_view_sets.clear();
            s_post_process_names.clear();
            s_virtual_rt.clear();
            s_partial_blend_states.clear();
        }

        void shutdown()
        {
            release_script_resources();

            // clear vectors of remaining stuff
            s_controllers.clear();
            s_scene_view_renderers.clear();
        }

        void update()
        {
            static u32 dt_timer = pen::timer_create("sc_dt");
            f32 dt = pen::timer_elapsed_ms(dt_timer) * 0.001f;
            pen::timer_start(dt_timer);

            size_t num_controllers = s_controllers.size();
            for (u32 u = 0; u < put::UPDATES_NUM; ++u)
                for (u32 i = 0; i < num_controllers; ++i)
                    if (s_controllers[i].order == u)
                        if (s_controllers[i].update_function)
                        {
                            s_controllers[i].dt = dt;
                            s_controllers[i].update_function(&s_controllers[i]);
                        }
                            
        }

        struct post_process_per_view
        {
            vec4f viewport_correction;
        };

        void fullscreen_quad(const scene_view& sv)
        {
            static ecs::geometry_resource* quad = ecs::get_geometry_resource(PEN_HASH("full_screen_quad"));

            if (!is_valid(sv.pmfx_shader))
                return;

            pen::renderer_set_constant_buffer(sv.cb_view, CB_PER_PASS_VIEW, pen::CBUFFER_BIND_PS | pen::CBUFFER_BIND_VS);

            pen::renderer_set_index_buffer(quad->index_buffer, quad->index_type, 0);
            pen::renderer_set_vertex_buffer(quad->vertex_buffer, 0, quad->vertex_size, 0);

            if (!pmfx::set_technique_perm(sv.pmfx_shader, sv.technique))
                PEN_ASSERT(0);

            pen::renderer_draw_indexed(quad->num_indices, 0, 0, PEN_PT_TRIANGLELIST);
        }

        void render_view(view_params& v)
        {
            // early out.. nothing to render
            if (v.num_colour_targets == 0 && v.depth_target == PEN_INVALID_HANDLE)
                return;

            static u32 cb_2d = PEN_INVALID_HANDLE;
            static u32 cb_sampler_info = PEN_INVALID_HANDLE;
            if (!is_valid(cb_2d))
            {
                pen::buffer_creation_params bcp;
                bcp.usage_flags = PEN_USAGE_DYNAMIC;
                bcp.bind_flags = PEN_BIND_CONSTANT_BUFFER;
                bcp.cpu_access_flags = PEN_CPU_ACCESS_WRITE;
                bcp.buffer_size = sizeof(float) * 16;
                bcp.data = (void*)nullptr;

                cb_2d = pen::renderer_create_buffer(bcp);

                bcp.buffer_size = sizeof(vec4f) * 16; // 16 samplers worth, x = 1.0 / width, y = 1.0 / height
                cb_sampler_info = pen::renderer_create_buffer(bcp);
            }

            // unbind samplers to stop d3d debug layer moaning
            for (s32 i = 0; i < MAX_SAMPLER_BINDINGS; ++i)
                pen::renderer_set_texture(0, 0, i, pen::TEXTURE_BIND_PS | pen::TEXTURE_BIND_VS);

            // render state
            pen::viewport vp = {0};
            get_rt_viewport(v.rt_width, v.rt_height, v.rt_ratio, v.viewport, vp);
            pen::renderer_set_viewport(vp);
            pen::renderer_set_scissor_rect({vp.x, vp.y, vp.width, vp.height});
            pen::renderer_set_depth_stencil_state(v.depth_stencil_state);
            pen::renderer_set_rasterizer_state(v.raster_state);
            pen::renderer_set_blend_state(v.blend_state);

            // create 2d view proj matrix
            float W = 2.0f / vp.width;
            float H = 2.0f / vp.height;
            float mvp[4][4] = {{W, 0.0, 0.0, 0.0}, {0.0, H, 0.0, 0.0}, {0.0, 0.0, 1.0, 0.0}, {-1.0, -1.0, 0.0, 1.0}};
            pen::renderer_update_buffer(cb_2d, mvp, sizeof(mvp), 0);

            // bind any per view cbuffers
            u32 num_samplers = v.sampler_bindings.size();
            if (num_samplers > 0)
            {
                pen::renderer_update_buffer(cb_sampler_info, v.sampler_info, num_samplers * sizeof(vec4f));
                pen::renderer_set_constant_buffer(cb_sampler_info, CB_SAMPLER_INFO, pen::CBUFFER_BIND_PS);
            }

            // filters
            if (is_valid(v.cbuffer_filter))
                pen::renderer_set_constant_buffer(v.cbuffer_filter, CB_FILTER_KERNEL, pen::CBUFFER_BIND_PS);

            // technique cbuffer
            if (is_valid(v.cbuffer_technique))
            {
                pen::renderer_update_buffer(v.cbuffer_technique, v.technique_constants.data, sizeof(technique_constant_data));
                pen::renderer_set_constant_buffer(v.cbuffer_technique, CB_MATERIAL_CONSTANTS, pen::CBUFFER_BIND_PS);
            }

            // build scene view info
            scene_view sv;
            sv.scene = v.scene;
            sv.render_flags = v.render_flags;
            sv.technique = v.technique;
            sv.raster_state = v.raster_state;
            sv.depth_stencil_state = v.depth_stencil_state;
            sv.blend_state = v.blend_state;
            sv.camera = v.camera;
            sv.viewport = &vp;
            sv.viewport_correction = v.viewport_correction;
            sv.cb_2d_view = cb_2d;
            sv.pmfx_shader = v.pmfx_shader;
            sv.permutation = v.technique_permutation;

            // render passes.. multi pass for cubemaps or arrays
            for (u32 a = 0; a < v.num_arrays; ++a)
            {
                // generate 3d view proj matrix
                if (v.camera)
                {
                    if (v.num_arrays == 6)
                    {
                        put::camera_set_cubemap_face(v.camera, a);
                    }

                    put::camera_update_shader_constants(v.camera, v.viewport_correction);
                    sv.cb_view = v.camera->cbuffer;
                }
                else
                {
                    static put::camera c;
                    put::camera_create_orthographic(&c, vp.x, vp.y, vp.width, vp.height, 0.0f, 1.0f);
                    put::camera_update_shader_constants(&c, v.viewport_correction);
                    sv.cb_view = c.cbuffer;
                }

                // bind targets before samplers..
                // so that ping-pong buffers get unbound from rt before being bound on samplers
                pen::renderer_set_targets(v.render_targets, v.num_colour_targets, v.depth_target, a);
                pen::renderer_clear(v.clear_state, a);

                // bind view samplers.. render targets, global textures
                for (auto& sb : v.sampler_bindings)
                    pen::renderer_set_texture(sb.handle, sb.sampler_state, sb.sampler_unit, sb.bind_flags);

                // bind technique samplers
                for (u32 i = 0; i < MAX_TECHNIQUE_SAMPLER_BINDINGS; ++i)
                {
                    auto& sb = v.technique_samplers.sb[i];
                    if (sb.handle == 0)
                        continue;

                    pen::renderer_set_texture(sb.handle, sb.sampler_state, sb.sampler_unit, sb.bind_flags);
                }

                // call render functions and make draw calls
                for (s32 rf = 0; rf < v.render_functions.size(); ++rf)
                    v.render_functions[rf](sv);
            }

            // stash output for debug
            if (v.stash_output && v.render_targets[0] != PEN_INVALID_HANDLE)
            {
                struct stash_rt
                {
                    u32 handle;
                    f32 width;
                    f32 height;
                    f32 aspect;
                };

                static std::vector<stash_rt> s_stash_rt;

                if (v.stashed_output_rt == PEN_INVALID_HANDLE)
                {
                    for (auto& r : s_stash_rt)
                    {
                        if (r.width == vp.width && r.height == vp.height)
                        {
                            v.stashed_output_rt = r.handle;
                            v.stashed_rt_aspect = r.aspect;
                        }

                        break;
                    }

                    if (v.stashed_output_rt == PEN_INVALID_HANDLE)
                    {
                        pen::texture_creation_params tcp;
                        tcp.data = nullptr;
                        tcp.width = vp.width;
                        tcp.height = vp.height;
                        tcp.format = PEN_TEX_FORMAT_RGBA8_UNORM;
                        tcp.pixels_per_block = 1;
                        tcp.block_size = 4;
                        tcp.usage = PEN_USAGE_DEFAULT;
                        tcp.flags = 0;
                        tcp.num_mips = 1;
                        tcp.num_arrays = 1;
                        tcp.sample_count = 1;
                        tcp.cpu_access_flags = PEN_CPU_ACCESS_READ;
                        tcp.sample_quality = 0;
                        tcp.bind_flags = PEN_BIND_RENDER_TARGET | PEN_BIND_SHADER_RESOURCE;
                        tcp.collection_type = pen::TEXTURE_COLLECTION_NONE;
                        u32 h = pen::renderer_create_render_target(tcp);

                        v.stashed_output_rt = h;
                        v.stashed_rt_aspect = vp.width / vp.height;
                        s_stash_rt.push_back({h, vp.width, vp.height, v.stashed_rt_aspect});
                    }
                }

                // render
                static u32                     pp_shader = pmfx::load_shader("post_process");
                static ecs::geometry_resource* quad = ecs::get_geometry_resource(PEN_HASH("full_screen_quad"));

                pen::renderer_set_targets(&v.stashed_output_rt, 1, PEN_NULL_DEPTH_BUFFER);
                pen::renderer_set_viewport(vp);
                pen::renderer_set_scissor_rect({vp.x, vp.y, vp.width, vp.height});

                u32 wlss = get_render_state(k_id_wrap_linear, RS_SAMPLER);

                pen::renderer_set_texture(v.render_targets[0], wlss, 0, pen::TEXTURE_BIND_PS);

                pen::renderer_set_index_buffer(quad->index_buffer, quad->index_type, 0);
                pen::renderer_set_vertex_buffer(quad->vertex_buffer, 0, quad->vertex_size, 0);

                static hash_id id_technique = PEN_HASH("blit");
                if (!pmfx::set_technique_perm(pp_shader, id_technique))
                    PEN_ASSERT(0);

                pen::renderer_draw_indexed(quad->num_indices, 0, 0, PEN_PT_TRIANGLELIST);

                v.stash_output = false;
            }
        }

        void resolve_targets(bool aux)
        {
            // resolve.. todo only resolve if we have rendered
            pen::renderer_set_targets(PEN_BACK_BUFFER_COLOUR, PEN_BACK_BUFFER_DEPTH);

            for (s32 i = 0; i < MAX_SAMPLER_BINDINGS; ++i)
            {
                pen::renderer_set_texture(0, 0, i, pen::TEXTURE_BIND_PS | pen::TEXTURE_BIND_VS);
            }

            for (auto& rt : s_render_targets)
            {
                if (!(rt.flags & RT_AUX) && aux)
                    continue;

                if (rt.samples > 1)
                {
                    static u32 pmfx_resolve = pmfx::load_shader("msaa_resolve");
                    pmfx::set_technique_perm(pmfx_resolve, PEN_HASH("average_4x"));

                    pen::renderer_resolve_target(rt.handle, pen::RESOLVE_CUSTOM);
                }
            }

            pen::renderer_set_targets(PEN_BACK_BUFFER_COLOUR, PEN_BACK_BUFFER_DEPTH);

            for (s32 i = 0; i < MAX_SAMPLER_BINDINGS; ++i)
            {
                pen::renderer_set_texture(0, 0, i, pen::TEXTURE_BIND_PS | pen::TEXTURE_BIND_VS);
            }
        }

        void render()
        {
            for (auto& v : s_views)
            {
                render_view(v);

                resolve_targets(false);

                if (v.post_process_flags & PP_ENABLED)
                {
                    virtual_rt_reset();

                    for (auto& v : v.post_process_views)
                    {
                        v.render_functions.clear();
                        v.render_functions.push_back(&fullscreen_quad);

                        render_view(v);
                    }
                }
            }

            resolve_targets(true);
        }

        void render_target_info_ui(const render_target& rt)
        {
            f32 w, h;
            get_rt_dimensions(rt.width, rt.height, rt.ratio, w, h);

            const c8* format_str = nullptr;
            s32       byte_size = 0;
            for (s32 f = 0; f < PEN_ARRAY_SIZE(rt_format); ++f)
            {
                if (rt_format[f].format == rt.format)
                {
                    format_str = rt_format[f].name.c_str();
                    byte_size = rt_format[f].block_size / 8;
                    break;
                }
            }

            ImGui::Text("%s", rt.name.c_str());
            ImGui::Text("%ix%i, %s", (s32)w, (s32)h, format_str);

            s32 image_size = byte_size * w * h * rt.samples;

            if (rt.samples > 1)
            {
                ImGui::Text("Msaa Samples: %i", rt.samples);
                image_size += byte_size * w * h;
            }

            ImGui::Text("Size: %f (mb)", (f32)image_size / 1024.0f / 1024.0f);
        }

        void view_info_ui(const view_params& v)
        {
            if (!ImGui::CollapsingHeader(v.name.c_str()))
                return;

            for (u32 i = 0; i < v.num_colour_targets; ++i)
            {
                const render_target* rt = get_render_target(v.id_render_target[i]);
                ImGui::Text("colour target %i: %s (%i)", i, rt->name.c_str(), v.render_targets[i]);
            }

            if (is_valid(v.depth_target) && v.depth_target)
            {
                const render_target* rt = get_render_target(v.id_depth_target);
                ImGui::Text("depth target: %s (%i)", rt->name.c_str(), v.depth_target);
            }

            int isb = 0;
            for (auto& sb : v.sampler_bindings)
            {
                const render_target* rt = get_render_target(sb.id_texture);
                ImGui::Text("input sampler %i: %s (%i)", isb, rt->name.c_str(), sb.handle);
                ++isb;
            }

            ImGui::TextWrapped("%s", v.info_json.c_str());
        }

        void generate_post_process_config(json& j_pp, const std::vector<Str>& pp_chain,
                                          const std::vector<view_params>& pp_views)
        {
            j_pp.set_array("chain", &pp_chain[0], pp_chain.size());

            pen::json j_pp_parameters;

            for (auto& pp : pp_views)
            {
                u32 ti = get_technique_index_perm(pp.pmfx_shader, pp.technique);

                if (!has_technique_params(pp.pmfx_shader, ti))
                    continue;

                pen::json j_technique;

                technique_constant* tc = get_technique_constants(pp.pmfx_shader, ti);
                if (tc)
                {
                    u32 num_constants = sb_count(tc);
                    for (u32 i = 0; i < num_constants; ++i)
                    {
                        u32 cb_offset = tc[i].cb_offset;
                        j_technique.set_array(tc[i].name.c_str(), &pp.technique_constants.data[cb_offset],
                                              tc[i].num_elements);
                    }
                }

                technique_sampler* ts = get_technique_samplers(pp.pmfx_shader, ti);
                if (ts)
                {
                    pen::json j_textures;

                    u32 num_textures = sb_count(ts);
                    for (u32 i = 0; i < num_textures; ++i)
                    {
                        sampler_binding sb = pp.technique_samplers.sb[i];

                        Str fn = put::ecs::strip_project_dir(put::get_texture_filename(sb.handle));

                        pen::json j_texture;
                        j_texture.set("filename", fn);
                        j_texture.set("sampler_state", "default");

                        j_textures.set(ts[i].name.c_str(), j_texture);
                    }

                    j_technique.set("textures", j_textures);
                }

                j_pp_parameters.set(pp.name.c_str(), j_technique);
            }

            j_pp.set("parameters", j_pp_parameters);
        }

        void set_post_process_edited(view_params& input_view)
        {
            for (auto& epp : s_edited_post_processes)
            {
                if (epp.id_name == input_view.id_name)
                {
                    epp.chain = input_view.post_process_chain;
                    epp.views = input_view.post_process_views;
                    return;
                }
            }

            // add new
            s_edited_post_processes.push_back(edited_post_process());
            s_edited_post_processes.back().id_name = input_view.id_name;
            s_edited_post_processes.back().chain = input_view.post_process_chain;
            s_edited_post_processes.back().views = input_view.post_process_views;
        }

        void set_view_set(const c8* name)
        {
            s_edited_view_set_name = name;
            pmfx_config_hotload();
        }

        void show_dev_ui()
        {
            ImGui::BeginMainMenuBar();

            static bool open_renderer = false;
            if (ImGui::Button(ICON_FA_PICTURE_O))
            {
                open_renderer = true;
            }
            put::dev_ui::set_tooltip("Pmfx");

            ImGui::EndMainMenuBar();

            static s32 current_render_target = 0;

            if (!open_renderer)
                return;

            if (ImGui::Begin("Pmfx", &open_renderer))
            {
                if (ImGui::CollapsingHeader("Render Targets"))
                {
                    if (s_render_target_names.size() != s_render_targets.size())
                    {
                        s_render_target_names.clear();

                        for (auto& rr : s_render_targets)
                        {
                            s_render_target_names.push_back(rr.name.c_str());
                        }
                    }

                    ImGui::Combo("", &current_render_target, (const c8* const*)&s_render_target_names[0],
                                 (s32)s_render_target_names.size(), 10);

                    static s32 display_ratio = 3;
                    ImGui::InputInt("Buffer Size", &display_ratio);
                    display_ratio = std::max<s32>(1, display_ratio);
                    display_ratio = std::min<s32>(4, display_ratio);

                    render_target& rt = s_render_targets[current_render_target];

                    f32 w, h;
                    get_rt_dimensions(rt.width, rt.height, rt.ratio, w, h);

                    bool unsupported_display = rt.id_name == k_id_main_colour || rt.id_name == k_id_main_depth;
                    unsupported_display |= rt.format == PEN_TEX_FORMAT_R32_UINT;

                    if (!unsupported_display)
                    {
                        f32 aspect = w / h;

                        dev_ui::image_ex(rt.handle, vec2f(1024 / display_ratio * aspect, 1024 / display_ratio),
                                         (dev_ui::e_shader)rt.collection);
                    }

                    render_target_info_ui(rt);
                }

                if (ImGui::CollapsingHeader("Views"))
                {
                    ImGui::Indent();

                    bool invalidated = false;

                    static std::vector<c8*> view_set_items;

                    static s32 s_selected_view_set = 1;

                    view_set_items.clear();

                    u32 i = 0;
                    for (auto& vs : s_view_sets)
                    {
                        if (vs == s_view_set_name)
                            s_selected_view_set = i;

                        view_set_items.push_back(vs.c_str());
                        ++i;
                    }

                    if (ImGui::Combo("View Set", &s_selected_view_set, &view_set_items[0], view_set_items.size()))
                    {
                        invalidated = true;
                        s_edited_view_set_name = view_set_items[s_selected_view_set];
                    }

                    for (auto& v : s_views)
                        view_info_ui(v);

                    if (invalidated)
                        pmfx_config_hotload();

                    ImGui::Unindent();
                }

                if (ImGui::CollapsingHeader("Post Processing"))
                {
                    ImGui::Indent();

                    static s32 s_selected_input_view = 0;
                    static s32 s_selected_chain_pp = 0;
                    static s32 s_selected_process = 0;
                    static s32 s_selected_pp_view = 0;

                    static std::vector<c8*> view_items;
                    static std::vector<c8*> chain_items;
                    static std::vector<c8*> process_items;
                    static std::vector<c8*> pass_items;

                    bool invalidated = false;

                    // input views which has post processing
                    view_items.clear();
                    for (auto& v : s_views)
                        if (v.post_process_flags & PP_ENABLED)
                            view_items.push_back(v.name.c_str());

                    if (view_items.size() == 0)
                    {
                        ImGui::Text("No views have 'post_process' set\n");
                    }
                    else
                    {
                        ImGui::Combo("Input View", &s_selected_input_view, &view_items[0], view_items.size());
                    }

                    std::vector<Str>&         s_post_process_chain = s_views[s_selected_input_view].post_process_chain;
                    std::vector<view_params>& s_post_process_passes = s_views[s_selected_input_view].post_process_views;
                    view_params&              edit_view = s_views[s_selected_input_view];

                    // all available post processes from config (ie bloom, dof, colour_lut)
                    process_items.clear();
                    for (auto& pp : s_post_process_names)
                        process_items.push_back(pp.c_str());

                    // selected input view post process chain
                    chain_items.clear();
                    for (auto& pp : s_post_process_chain)
                        chain_items.push_back(pp.c_str());

                    // selected input view, view passes which make up the post process chain
                    pass_items.clear();
                    for (auto& pp : s_post_process_passes)
                        pass_items.push_back(pp.name.c_str());

                    // Main Toolbar
                    static bool s_save_dialog_open = false;
                    if (ImGui::Button(ICON_FA_FLOPPY_O))
                        s_save_dialog_open = true;

                    dev_ui::set_tooltip("Save post process preset");
                    ImGui::SameLine();

                    static bool s_load_dialog_open = false;
                    if (ImGui::Button(ICON_FA_FOLDER_OPEN_O))
                        s_load_dialog_open = true;

                    dev_ui::set_tooltip("Load post process preset");
                    ImGui::Separator();

                    if (s_save_dialog_open)
                    {
                        const c8* res = dev_ui::file_browser(s_save_dialog_open, dev_ui::FB_SAVE);
                        if (res)
                        {
                            json j_pp;
                            generate_post_process_config(j_pp, s_post_process_chain, s_post_process_passes);

                            Str basename = pen::str_remove_ext(res);
                            basename.append(".jsn");

                            std::ofstream ofs(basename.c_str());
                            ofs << j_pp.dumps().c_str();
                            ofs.close();
                        }
                    }

                    if (s_load_dialog_open)
                    {
                        const c8* res = dev_ui::file_browser(s_load_dialog_open, dev_ui::FB_OPEN);
                        if (res)
                        {
                            // perform load: todo
                        }
                    }

                    ImGui::Columns(2);

                    ImGui::SetColumnWidth(0, 300);
                    ImGui::SetColumnOffset(1, 300);
                    ImGui::SetColumnWidth(1, 500);

                    // Edit Toolbar
                    if (ImGui::Button(ICON_FA_ARROW_UP) && s_selected_chain_pp > 0)
                    {
                        invalidated = true;
                        Str tmp = s_post_process_chain[s_selected_chain_pp];
                        s_post_process_chain[s_selected_chain_pp] = s_post_process_chain[s_selected_chain_pp - 1].c_str();
                        s_post_process_chain[s_selected_chain_pp - 1] = tmp.c_str();

                        s_selected_chain_pp--;
                    }
                    dev_ui::set_tooltip("Move selected post process up");

                    ImGui::SameLine();
                    if (ImGui::Button(ICON_FA_ARROW_DOWN) && s_selected_chain_pp < s_post_process_chain.size() - 1)
                    {
                        invalidated = true;
                        Str tmp = s_post_process_chain[s_selected_chain_pp];
                        s_post_process_chain[s_selected_chain_pp] = s_post_process_chain[s_selected_chain_pp + 1].c_str();
                        s_post_process_chain[s_selected_chain_pp + 1] = tmp.c_str();

                        s_selected_chain_pp++;
                    }
                    dev_ui::set_tooltip("Move selected post process down");

                    ImGui::SameLine();
                    if (ImGui::Button(ICON_FA_TRASH))
                    {
                        invalidated = true;
                        s_post_process_chain.erase(s_post_process_chain.begin() + s_selected_chain_pp);
                        s_selected_pp_view = -1;
                    }
                    dev_ui::set_tooltip("Remove selected post process");

                    ImGui::Text("Post Process Chain");
                    dev_ui::set_tooltip("A chain of post processes executed top to bottom");

                    // List box
                    if (!invalidated)
                    {
                        ImGui::PushID("Post Process Chain");
                        if (chain_items.size() > 0)
                            ImGui::ListBox("", &s_selected_chain_pp, &chain_items[0], chain_items.size());

                        ImGui::PopID();
                    }

                    ImGui::NextColumn();

                    if (ImGui::Button(ICON_FA_ARROW_LEFT))
                    {
                        invalidated = true;

                        s_post_process_chain.insert(s_post_process_chain.begin() + s_selected_chain_pp,
                                                    process_items[s_selected_process]);
                    }

                    dev_ui::set_tooltip("Insert selected post process to chain");

                    ImGui::Text("Post Processes");
                    dev_ui::set_tooltip("A view or collections of post process views make up a post process");

                    if (process_items.size() > 0)
                    {
                        ImGui::PushID("Post Processes");
                        ImGui::ListBox("", &s_selected_process, &process_items[0], process_items.size());
                        ImGui::PopID();
                    }

                    ImGui::Columns(1);

                    if (invalidated)
                    {
                        set_post_process_edited(edit_view);
                        pmfx_config_hotload();
                    }
                    else if (s_selected_input_view >= 0 && s_post_process_passes.size() > 0)
                    {
                        if (s_selected_pp_view == -1)
                            s_selected_pp_view = 0;

                        ImGui::Separator();

                        ImGui::Text("Parameters");

                        for (auto& pp : s_post_process_passes)
                        {
                            u32 ti = get_technique_index_perm(pp.pmfx_shader, pp.technique);

                            if (has_technique_params(pp.pmfx_shader, ti))
                            {
                                ImGui::Separator();
                                ImGui::Text("Technique: %s", pp.name.c_str());

                                show_technique_ui(pp.pmfx_shader, ti, &pp.technique_constants.data[0], pp.technique_samplers,
                                                  &pp.technique_permutation);
                            }
                        }

                        ImGui::Separator();

                        if (ImGui::CollapsingHeader("Passes"))
                        {
                            ImGui::Columns(2);

                            ImGui::SetColumnWidth(0, 300);
                            ImGui::SetColumnOffset(1, 300);
                            ImGui::SetColumnWidth(1, 500);

                            ImGui::PushID("Passes_");
                            ImGui::ListBox("", &s_selected_pp_view, &pass_items[0], pass_items.size());
                            ImGui::PopID();

                            ImGui::NextColumn();

                            ImGui::Text("Input / Output");

                            view_params& selected_chain_pp = s_post_process_passes[s_selected_pp_view];
                            view_info_ui(selected_chain_pp);

                            selected_chain_pp.stash_output = true;

                            if (selected_chain_pp.stashed_output_rt != PEN_INVALID_HANDLE)
                            {
                                f32 aspect = selected_chain_pp.stashed_rt_aspect;
                                ImGui::Image(IMG(selected_chain_pp.stashed_output_rt), ImVec2(128.0f * aspect, 128.0f));
                            }

                            ImGui::Columns(1);

                            ImGui::Separator();
                        }
                    }

                    ImGui::Unindent();
                }

                ImGui::End();
            }
        }

        camera* get_camera(hash_id id_name)
        {
            for (auto& c : s_controllers)
            {
                if (c.id_name == id_name)
                    return c.camera;
            }

            return nullptr;
        }

        camera** get_cameras()
        {
            camera** list = nullptr;

            for (auto& c : s_controllers)
                sb_push(list, c.camera);

            return list;
        }
    } // namespace pmfx
} // namespace put
