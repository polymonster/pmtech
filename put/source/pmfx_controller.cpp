#include "pmfx_controller.h"
#include "entry_point.h"
#include "dev_ui.h"
#include "debug_render.h"
#include "pen_json.h"
#include "file_system.h"
#include "hash.h"
#include "pen_string.h"
#include "pmfx.h"

extern pen::window_creation_params pen_window;

namespace put
{    
    namespace pmfx
    {
        static hash_id ID_MAIN_COLOUR = PEN_HASH("main_colour");
        static hash_id ID_MAIN_DEPTH = PEN_HASH("main_depth");
        
        struct mode_map
        {
            const c8* name;
            u32 val;
        };
        
        static mode_map k_cull_mode_map[] =
        {
            "none",             PEN_CULL_NONE,
            "back",             PEN_CULL_BACK,
            "front",            PEN_CULL_FRONT,
            nullptr,            0
        };
        
        static mode_map k_fill_mode_map[] =
        {
            "solid",            PEN_FILL_SOLID,
            "wireframe",        PEN_FILL_WIREFRAME,
            nullptr,            0
        };
        
        static mode_map k_comparison_mode_map[] =
        {
            "never",            PEN_COMPARISON_NEVER,
            "less",             PEN_COMPARISON_LESS,
            "less_equal",       PEN_COMPARISON_LESS_EQUAL,
            "greater",          PEN_COMPARISON_GREATER,
            "not_equal",        PEN_COMPARISON_NOT_EQUAL,
            "greater_equal",    PEN_COMPARISON_GREATER_EQUAL,
            "always",           PEN_COMPARISON_ALWAYS,
            nullptr,            0
        };
        
        static mode_map k_stencil_mode_map[] =
        {
            "keep",             PEN_STENCIL_OP_KEEP,
            "replace",          PEN_STENCIL_OP_REPLACE,
            "incr",             PEN_STENCIL_OP_INCR,
            "incr_sat",         PEN_STENCIL_OP_INCR_SAT,
            "decr",             PEN_STENCIL_OP_DECR,
            "decr_sat",         PEN_STENCIL_OP_DECR_SAT,
            "zero",             PEN_STENCIL_OP_ZERO,
            "invert",           PEN_STENCIL_OP_INVERT,
            nullptr,            0
        };
        
        static mode_map k_filter_mode_map[] =
        {
            "linear",           PEN_FILTER_MIN_MAG_MIP_LINEAR,
            "point",            PEN_FILTER_MIN_MAG_MIP_POINT,
            nullptr,            0
        };
        
        static mode_map k_address_mode_map[] =
        {
            "wrap",             PEN_TEXTURE_ADDRESS_WRAP,
            "clamp",            PEN_TEXTURE_ADDRESS_CLAMP,
            "border",           PEN_TEXTURE_ADDRESS_BORDER,
            "mirror",           PEN_TEXTURE_ADDRESS_MIRROR,
            "mirror_once",      PEN_TEXTURE_ADDRESS_MIRROR_ONCE,
            nullptr,            0
        };
        
        static mode_map k_blend_mode_map[] =
        {
            "zero",             PEN_BLEND_ZERO,
            "one",              PEN_BLEND_ONE,
            "src_colour",       PEN_BLEND_SRC_COLOR,
            "inv_src_colour",   PEN_BLEND_INV_SRC_COLOR,
            "src_alpha",        PEN_BLEND_SRC_ALPHA,
            "inv_src_alpha",    PEN_BLEND_INV_SRC_ALPHA,
            "dest_alpha",       PEN_BLEND_DEST_ALPHA,
            "inv_dest_alpha",   PEN_BLEND_INV_DEST_ALPHA,
            "dest_colour",      PEN_BLEND_DEST_COLOR,
            "inv_dest_colour",  PEN_BLEND_INV_DEST_COLOR,
            "src_alpha_sat",    PEN_BLEND_SRC_ALPHA_SAT,
            "blend_factor",     PEN_BLEND_BLEND_FACTOR,
            "inv_blend_factor", PEN_BLEND_INV_BLEND_FACTOR,
            "src1_colour",      PEN_BLEND_SRC1_COLOR,
            "inv_src1_colour",  PEN_BLEND_INV_SRC1_COLOR,
            "src1_aplha",       PEN_BLEND_SRC1_ALPHA,
            "inv_src1_alpha",   PEN_BLEND_INV_SRC1_ALPHA,
            nullptr,            0
        };
        
        static mode_map k_blend_op_mode_map[] =
        {
            "belnd_op_add",     PEN_BLEND_OP_ADD,
            "belnd_op_add",     PEN_BLEND_OP_ADD,
            "belnd_op_subtract", PEN_BLEND_OP_SUBTRACT,
            "belnd_op_rev_sbtract", PEN_BLEND_OP_REV_SUBTRACT,
            "belnd_op_min",     PEN_BLEND_OP_MIN,
            "belnd_op_max",     PEN_BLEND_OP_MAX,
            nullptr,            0
        };
        
        struct format_info
        {
            Str name;
            s32 format;
            u32 block_size;
            bind_flags flags;
        };
        
        format_info rt_format[] =
        {
            {"rgba8",   PEN_TEX_FORMAT_RGBA8_UNORM,         32,     PEN_BIND_RENDER_TARGET },
            {"bgra8",   PEN_TEX_FORMAT_BGRA8_UNORM,         32,     PEN_BIND_RENDER_TARGET },
            {"rgba32f", PEN_TEX_FORMAT_R32G32B32A32_FLOAT,  32*4,   PEN_BIND_RENDER_TARGET },
            {"rgba16f", PEN_TEX_FORMAT_R16G16B16A16_FLOAT,  16*4,   PEN_BIND_RENDER_TARGET },
            {"r32f",    PEN_TEX_FORMAT_R32_FLOAT,           32,     PEN_BIND_RENDER_TARGET },
            {"r16f",    PEN_TEX_FORMAT_R16_FLOAT,           16,     PEN_BIND_RENDER_TARGET },
            {"r32u",    PEN_TEX_FORMAT_R32_UINT,            32,     PEN_BIND_RENDER_TARGET },
            {"d24s8",   PEN_TEX_FORMAT_D24_UNORM_S8_UINT,   32,     PEN_BIND_DEPTH_STENCIL }
        };
        s32 num_formats = (s32)sizeof(rt_format)/sizeof(format_info);
        
        Str rt_ratio[] =
        {
            "none",
            "equal",
            "half",
            "quater",
            "eighth"
            "sixteenth"
        };
        s32 num_ratios = (s32)sizeof(rt_ratio)/sizeof(Str);
        
        struct view_params
        {
            Str     name;
            hash_id id_name;
            
            s32     rt_width, rt_height;
            f32     rt_ratio;
            
            u32     render_targets[PEN_MAX_MRT] = { 0 };
            hash_id id_render_target[PEN_MAX_MRT] = { };
            
            u32     depth_target = 0;
            u32     num_colour_targets = 0;
            
            bool    viewport_correction = true;
            
            f32     viewport[4] = { 0 };
            
            u32     clear_state = 0;
            u32     raster_state = 0;
            u32     depth_stencil_state = 0;
            u32     blend_state = 0;
            
            hash_id technique;
            
            ces::entity_scene* scene;
            put::camera* camera;
            
            std::vector<void(*)(const put::ces::scene_view&)> render_functions;
        };
        
        static std::vector<view_params>         k_views;
        static std::vector<scene_controller>    k_scenes;
        static std::vector<scene_view_renderer> k_scene_view_renderers;
        static std::vector<camera_controller>   k_cameras;
        static std::vector<render_target>       k_render_targets;
        
        void register_scene( const scene_controller& scene )
        {
            k_scenes.push_back(scene);
        }
        
        void register_scene_view_renderer( const scene_view_renderer& svr )
        {
            k_scene_view_renderers.push_back(svr);
        }

        void register_camera( const camera_controller& cam )
        {
            k_cameras.push_back(cam);
        }
        
        s32 calc_num_mips( s32 width, s32 height )
        {
            s32 num = 0;
            
            while( width > 1 && height > 1 )
            {
                ++num;
                
                width /= 2;
                height /= 2;
                
                width = std::max<s32>(1, width);
                height = std::max<s32>(1, height);
            }
            
            return num;
        }
        
        void get_rt_dimensions( s32 rt_w, s32 rt_h, f32 rt_r, f32& w, f32& h )
        {
            w = (f32)pen_window.width;
            h = (f32)pen_window.height;
            
            if( rt_r != 0 )
            {
                w *= (f32)rt_r;
                h *= (f32)rt_r;
            }
            else
            {
                w = (f32)rt_w;
                h = (f32)rt_h;
            }
        }
        
        void get_rt_viewport( s32 rt_w, s32 rt_h, f32 rt_r, const f32* vp_in, pen::viewport& vp_out )
        {
            f32 w, h;
            get_rt_dimensions( rt_w, rt_h, rt_r, w, h );
            
            vp_out =
            {
                vp_in[0] * w, vp_in[1] * h,
                vp_in[2] * w, vp_in[3] * h,
                0.0f, 1.0f
            };
        }
        
        u32 mode_from_string( const mode_map* map, const c8* str, u32 default_value )
        {
            if(!str)
                return default_value;
            
            while( map->name )
                if( pen::string_compare(str, map->name) == 0)
                    return map->val;
                else
                    map++;
            
            return default_value;
        }
        
        struct render_state
        {
            hash_id id_name;
            hash_id hash;
            u32     handle;
        };
        static std::vector<render_state> k_render_states;
        
        render_state* get_state_by_name( hash_id id_name )
        {
            s32 num = k_render_states.size();
            for( s32 i = 0; i < num; ++i )
                if( k_render_states[i].id_name == id_name )
                    return &k_render_states[i];
            
            return nullptr;
        }
        
        render_state* get_state_by_hash( hash_id hash )
        {
            s32 num = k_render_states.size();
            for( s32 i = 0; i < num; ++i )
                if( k_render_states[i].hash == hash )
                    return &k_render_states[i];
            
            return nullptr;
        }
        
        u32 get_render_state_by_name( hash_id id_name )
        {
            render_state* rs = nullptr;
            rs = get_state_by_name( id_name );
            if( rs )
                return rs->handle;
            
            return 0;
        }
        
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
        geometry_utility k_geometry;
        
        void create_geometry_utilities( )
        {
            //buffers
            //create vertex buffer for a quad
            textured_vertex quad_vertices[] =
            {
                -1.0f, -1.0f, 0.5f, 1.0f,       //p1
                0.0f, 1.0f,                     //uv1
                
                -1.0f, 1.0f, 0.5f, 1.0f,         //p2
                0.0f, 0.0f,                     //uv2
                
                1.0f, 1.0f, 0.5f, 1.0f,         //p3
                1.0f, 0.0f,                     //uv3
                
                1.0f, -1.0f, 0.5f, 1.0f,         //p4
                1.0f, 1.0f,                     //uv4
            };
            
            pen::buffer_creation_params bcp;
            bcp.usage_flags = PEN_USAGE_DEFAULT;
            bcp.bind_flags = PEN_BIND_VERTEX_BUFFER;
            bcp.cpu_access_flags = 0;
            
            bcp.buffer_size = sizeof(textured_vertex) * 4;
            bcp.data = (void*)&quad_vertices[0];
            
            k_geometry.screen_quad_vb = pen::renderer_create_buffer(bcp);
            
            //create index buffer
            u16 indices[] =
            {
                0, 1, 2,
                2, 3, 0
            };
            
            bcp.usage_flags = PEN_USAGE_IMMUTABLE;
            bcp.bind_flags = PEN_BIND_INDEX_BUFFER;
            bcp.cpu_access_flags = 0;
            bcp.buffer_size = sizeof(u16) * 6;
            bcp.data = (void*)&indices[0];
            
            k_geometry.screen_quad_ib = pen::renderer_create_buffer(bcp);
        }
        
        void parse_sampler_states( pen::json render_config )
        {
            pen::json j_sampler_states = render_config["sampler_states"];
            s32 num = j_sampler_states.size();
            for( s32 i = 0; i < num; ++i )
            {
                pen::sampler_creation_params scp;
                
                pen::json state = j_sampler_states[i];
                
                scp.filter = mode_from_string( k_filter_mode_map, state["filter"].as_cstr(), PEN_FILTER_MIN_MAG_MIP_LINEAR );
                scp.address_v = scp.address_w = scp.address_u = mode_from_string( k_address_mode_map, state["address"].as_cstr(), PEN_TEXTURE_ADDRESS_WRAP );
                scp.address_u = mode_from_string( k_address_mode_map, state["address_u"].as_cstr(), PEN_TEXTURE_ADDRESS_WRAP );
                scp.address_v = mode_from_string( k_address_mode_map, state["address_v"].as_cstr(), PEN_TEXTURE_ADDRESS_WRAP );
                scp.address_w = mode_from_string( k_address_mode_map, state["address_w"].as_cstr(), PEN_TEXTURE_ADDRESS_WRAP );

                scp.mip_lod_bias = state["mip_lod_bias"].as_f32( 0.0f );
                scp.max_anisotropy = state["max_anisotropy"].as_u32( 0 );
                
                scp.comparison_func = mode_from_string( k_comparison_mode_map, state["comparison_func"].as_cstr(), PEN_COMPARISON_ALWAYS );
                
                pen::json border = state["border"];
                if( border.type() == JSMN_ARRAY )
                {
                    if( border.size() == 4 )
                        for( s32 i = 0; i < 4; ++i )
                            scp.border_color[i] = border[i].as_f32();
                }

                scp.min_lod = state["min_lod"].as_f32( 0.0f );
                scp.max_lod = state["max_lod"].as_f32( PEN_F32_MAX );
                
                hash_id hh = PEN_HASH(scp);
                
                render_state rs;
                rs.hash = hh;
                Str typed_name = state.name();
                typed_name.append("_sampler_state");
                rs.id_name =  PEN_HASH(typed_name.c_str());
            
                render_state* existing_state = get_state_by_hash( hh );
                if( existing_state )
                    rs.handle = existing_state->handle;
                else
                    rs.handle = pen::renderer_create_sampler(scp);
                
                k_render_states.push_back(rs);
            }
        }
        
        void parse_raster_states( pen::json& render_config )
        {
            pen::json j_raster_states = render_config["raster_states"];
            s32 num = j_raster_states.size();
            for( s32 i = 0; i < num; ++i )
            {
                pen::rasteriser_state_creation_params rcp;
                
                pen::json state = j_raster_states[i];
                
                rcp.fill_mode = mode_from_string( k_fill_mode_map, state["fill_mode"].as_cstr(), PEN_FILL_SOLID );
                rcp.cull_mode = mode_from_string( k_cull_mode_map, state["cull_mode"].as_cstr(), PEN_CULL_BACK );
                rcp.front_ccw = state["front_ccw"].as_bool( false ) ? 1 : 0;
                rcp.depth_bias = state["depth_bias"].as_s32(0);
                rcp.depth_bias_clamp = state["depth_bias_clamp"].as_f32(0.0f);
                rcp.sloped_scale_depth_bias = state["sloped_scale_depth_bias"].as_f32(0.0f);
                rcp.depth_clip_enable = state["depth_clip_enable"].as_bool( true ) ? 1 : 0;
                rcp.scissor_enable = state["scissor_enable"].as_bool( false ) ? 1 : 0;
                rcp.multisample = state["multisample"].as_bool( false ) ? 1 : 0;
                rcp.aa_lines = state["aa_lines"].as_bool( false ) ? 1 : 0;
                
                hash_id hh = PEN_HASH(rcp);
                
                render_state rs;
                rs.hash = hh;
                Str typed_name = state.name();
                typed_name.append("_raster_state");
                rs.id_name =  PEN_HASH(typed_name.c_str());
                
                render_state* existing_state = get_state_by_hash( hh );
                if( existing_state )
                    rs.handle = existing_state->handle;
                else
                    rs.handle = pen::renderer_create_rasterizer_state(rcp);
                
                k_render_states.push_back(rs);
            }
        }
        
        struct partial_blend_state
        {
            hash_id id_name;
            pen::render_target_blend rtb;
        };
        static std::vector<partial_blend_state> k_partial_blend_states;
        
        void parse_partial_blend_states( pen::json& render_config )
        {
            pen::json j_blend_states = render_config["blend_states"];
            s32 num = j_blend_states.size();
            for( s32 i = 0; i < num; ++i )
            {
                pen::render_target_blend rtb;
                
                pen::json state = j_blend_states[i];
                
                rtb.blend_enable = state["blend_enable"].as_bool(false);
                rtb.src_blend = mode_from_string( k_blend_mode_map, state["src_blend"].as_cstr(), PEN_BLEND_ONE );
                rtb.dest_blend = mode_from_string( k_blend_mode_map, state["dest_blend"].as_cstr(), PEN_BLEND_ZERO );
                rtb.blend_op = mode_from_string( k_blend_op_mode_map, state["blend_op"].as_cstr(), PEN_BLEND_OP_ADD );
                
                rtb.src_blend_alpha = mode_from_string( k_blend_mode_map, state["src_blend_alpha"].as_cstr(), PEN_BLEND_ONE );
                rtb.dest_blend_alpha = mode_from_string( k_blend_mode_map, state["dest_blend_alpha"].as_cstr(), PEN_BLEND_ZERO );
                rtb.blend_op_alpha = mode_from_string( k_blend_op_mode_map, state["alpha_blend_op"].as_cstr(), PEN_BLEND_OP_ADD );
                
                k_partial_blend_states.push_back({PEN_HASH(state.name().c_str()), rtb});
            }
        }
        
        void parse_stencil_state( pen::json& depth_stencil_state, pen::stencil_op* front, pen::stencil_op* back )
        {
            pen::stencil_op op;
            
            op.stencil_failop = mode_from_string( k_stencil_mode_map, depth_stencil_state["stencil_fail"].as_cstr(), PEN_STENCIL_OP_KEEP );
            op.stencil_depth_failop = mode_from_string( k_stencil_mode_map, depth_stencil_state["depth_fail"].as_cstr(), PEN_STENCIL_OP_KEEP );
            op.stencil_passop = mode_from_string( k_stencil_mode_map, depth_stencil_state["stencil_pass"].as_cstr(), PEN_STENCIL_OP_REPLACE );
            op.stencil_func = mode_from_string( k_comparison_mode_map, depth_stencil_state["stencil_func"].as_cstr(), PEN_COMPARISON_ALWAYS );
            
            if( front )
                pen::memory_cpy(front, &op, sizeof(op));
            
            if( back )
                pen::memory_cpy(back, &op, sizeof(op));
        }
        
        void parse_depth_stencil_states( pen::json& render_config )
        {
            pen::json j_ds_states = render_config["depth_stencil_states"];
            s32 num = j_ds_states.size();
            for( s32 i = 0; i < num; ++i )
            {
                pen::depth_stencil_creation_params dscp;
                
                pen::json state = j_ds_states[i];
                
                dscp.depth_enable = state["depth_enable"].as_bool( false ) ? 1 : 0;
                dscp.depth_write_mask = state["depth_write"].as_bool( false ) ? 1 : 0;
                dscp.depth_func = mode_from_string( k_comparison_mode_map, state["depth_func"].as_cstr(), PEN_COMPARISON_ALWAYS );

                dscp.stencil_enable = state["stencil_enable"].as_bool( false ) ? 1 : 0;
                dscp.stencil_read_mask = state["stencil_read_mask"].as_u8_hex( 0 );
                dscp.stencil_write_mask = state["stencil_write_mask"].as_u8_hex( 0 );
                
                pen::json op = state["stencil_op"];
                parse_stencil_state( op, &dscp.front_face, &dscp.back_face );
                
                pen::json op_front = state["stencil_op_front"];
                parse_stencil_state( op_front, &dscp.front_face, nullptr );
                
                pen::json op_back = state["stencil_op_back"];
                parse_stencil_state( op_back, nullptr, &dscp.back_face );
                
                hash_id hh = PEN_HASH(dscp);
                
                render_state rs;
                rs.hash = hh;
                Str typed_name = state.name();
                typed_name.append("_depth_stencil_state");
                rs.id_name =  PEN_HASH(typed_name.c_str());
                
                render_state* existing_state = get_state_by_hash( hh );
                if( existing_state )
                    rs.handle = existing_state->handle;
                else
                    rs.handle = pen::renderer_create_depth_stencil_state(dscp);
                
                k_render_states.push_back(rs);
            }
        }
        
        u32 create_blend_state( const c8* view_name, pen::json& blend_state, pen::json& write_mask, bool alpha_to_coverage )
        {
            std::vector<pen::render_target_blend> rtb;
            
            if( blend_state.type() != JSMN_UNDEFINED )
            {
                if( blend_state.type() == JSMN_ARRAY )
                {
                    for( s32 i = 0; i < blend_state.size(); ++i )
                    {
                        hash_id hh = PEN_HASH(blend_state[i].as_cstr());
                        
                        for( auto& b : k_partial_blend_states )
                        {
                            if( hh == b.id_name )
                                rtb.push_back(b.rtb);
                                
                        }
                    }
                }
                else
                {
                    hash_id hh = PEN_HASH(blend_state.as_cstr());
                    
                    for( auto& b : k_partial_blend_states )
                    {
                        if( hh == b.id_name )
                            rtb.push_back(b.rtb);
                    }
                }
            }
                
            std::vector<u8> masks;
            if( write_mask.type() != JSMN_UNDEFINED )
            {
                if( write_mask.type() == JSMN_ARRAY )
                {
                    for( s32 i = 0; i < write_mask.size(); ++i )
                    {
                        masks.push_back(write_mask[i].as_u8_hex());
                    }
                }
                else
                {
                    masks.push_back(write_mask.as_u8_hex(0x0F));
                }
            }
            
            if( masks.size() == 0 )
                masks.push_back(0x0F);
                
            bool multi_blend = rtb.size() > 1 || write_mask.size() > 1;
            u32 num_rt = std::max<u32>(rtb.size(), write_mask.size());
            
            //splat
            s32 rtb_start = rtb.size();
            rtb.resize(num_rt);
            for( s32 i = rtb_start; i < num_rt; ++i )
                rtb[i] = rtb[i-1];
                
                
            s32 mask_start = masks.size();
            rtb.resize(num_rt);
            for( s32 i = mask_start; i < num_rt; ++i )
                masks[i] = masks[i-1];
            
            pen::blend_creation_params bcp;
            bcp.alpha_to_coverage_enable = alpha_to_coverage;
            bcp.independent_blend_enable = multi_blend;
            bcp.num_render_targets = num_rt;
            bcp.render_targets = new pen::render_target_blend[num_rt];
            
            for( s32 i = 0; i < num_rt; ++i )
            {
                bcp.render_targets[i] = rtb[i];
                bcp.render_targets[i].render_target_write_mask = masks[i];
			}
            
            pen::hash_murmur hm;
            hm.begin();
            hm.add(&bcp.alpha_to_coverage_enable, sizeof(bcp.alpha_to_coverage_enable));
            hm.add(&bcp.independent_blend_enable, sizeof(bcp.independent_blend_enable));
            hm.add(&bcp.num_render_targets, sizeof(bcp.num_render_targets));
            for( s32 i = 0; i < num_rt; ++i)
                hm.add(&bcp.render_targets[i], sizeof(bcp.render_targets[i]));
            
            hash_id hh = hm.end();
            
            render_state rs;
            rs.hash = hh;
            Str typed_name = view_name;
            typed_name.append("_blend_state");
            rs.id_name =  PEN_HASH(typed_name.c_str());
            
            render_state* existing_state = get_state_by_hash( hh );
            if( existing_state )
                rs.handle = existing_state->handle;
            else
                rs.handle = pen::renderer_create_blend_state(bcp);
            
            k_render_states.push_back(rs);
            
            return rs.handle;
        }
        
        void parse_render_targets( pen::json& render_config )
        {
            //add 2 defaults
            render_target main_colour;
            main_colour.id_name = ID_MAIN_COLOUR;
			main_colour.name = "Backbuffer Colour";
            main_colour.ratio = 1;
            main_colour.format = PEN_TEX_FORMAT_RGBA8_UNORM;
            main_colour.handle = PEN_BACK_BUFFER_COLOUR;
            main_colour.num_mips = 1;
            
            k_render_targets.push_back(main_colour);
            
            render_target main_depth;
            main_depth.id_name = ID_MAIN_DEPTH;
			main_depth.name = "Backbuffer Depth";
            main_depth.ratio = 1;
            main_depth.format = PEN_TEX_FORMAT_D24_UNORM_S8_UINT;
            main_depth.handle = PEN_BACK_BUFFER_DEPTH;
            main_depth.num_mips = 1;
            
            k_render_targets.push_back(main_depth);
            
            pen::json j_render_targets = render_config["render_targets"];
            
            s32 num = j_render_targets.size();
            
            for( s32 i = 0; i < num; ++i )
            {
                pen::json r = j_render_targets[i];
                
                for( s32 f = 0; f < num_formats; ++f)
                {
                    if( rt_format[f].name == r["format"].as_str() )
                    {
                        k_render_targets.push_back(render_target());
                        render_target& new_info = k_render_targets.back();
                        new_info.ratio = 0;
                        
                        new_info.name = r.name();
                        new_info.id_name = PEN_HASH(r.name().c_str());
                        
                        pen::json size = r["size"];
                        if( size.size() == 2 )
                        {
                            //explicit size
                            new_info.width = size[0].as_s32();
                            new_info.height = size[1].as_s32();
                        }
                        else
                        {
                            //ratio
                            new_info.width = 0;
                            new_info.height = 0;
                            
                            Str ratio_str = size.as_str();
                            
                            for( s32 rr = 0; rr < num_ratios; ++rr )
                                if( rt_ratio[rr] == ratio_str )
                                {
                                    new_info.width = PEN_BACK_BUFFER_RATIO;
                                    new_info.height = rr;
                                    
                                    new_info.ratio = 1.0f / (f32)rr;
                                    break;
                                }
                        }
                        
                        new_info.num_mips = 1;
                        new_info.format = rt_format[f].format;
                        
                        pen::texture_creation_params tcp;
                        tcp.data = nullptr;
                        tcp.width = new_info.width;
                        tcp.height = new_info.height;
                        tcp.format = new_info.format;
                        tcp.pixels_per_block = 1;
                        tcp.block_size = rt_format[f].block_size;
                        tcp.usage = PEN_USAGE_DEFAULT;
                        tcp.flags = 0;
                        tcp.num_mips = 1;
                        
                        //arays and mips
                        tcp.num_arrays = r["num_arrays"].as_u32(1);
                        if( r["mips"].as_bool(false) )
                        {
                            new_info.num_mips = calc_num_mips(new_info.width, new_info.height);
                            tcp.num_mips = new_info.num_mips;
                        }
                        
                        //flags
                        tcp.cpu_access_flags = 0;

						if (r["cpu_read"].as_bool(false))
							tcp.cpu_access_flags |= PEN_CPU_ACCESS_READ;

						if (r["cpu_write"].as_bool(false))
							tcp.cpu_access_flags |= PEN_CPU_ACCESS_WRITE;

                        tcp.bind_flags = rt_format[f].flags | PEN_BIND_SHADER_RESOURCE;
                        
                        //msaa
                        tcp.sample_count = r["samples"].as_u32(1);
                        tcp.sample_quality = 0;
                        
                        new_info.samples = tcp.sample_count;
                        
                        new_info.handle = pen::renderer_create_render_target( tcp );
                    }
                }
            }
        }
        
        const render_target* get_render_target( hash_id h )
        {
            u32 num = k_render_targets.size();
            for( u32 i = 0; i < num; ++i )
            {
                if( k_render_targets[i].id_name == h )
                {
                    return &k_render_targets[i];
                }
            }
            
            return nullptr;
        }
        
        void get_render_target_dimensions( const render_target* rt, f32& w, f32& h)
        {
            get_rt_dimensions( rt->width, rt->height, rt->ratio, w, h );
        }
        
        void parse_clear_colour( pen::json& render_config )
        {
            
        }
        
        void parse_views( pen::json& render_config )
        {
            pen::json j_views = render_config["views"];
            
            s32 num = j_views.size();
            
            for( s32 i = 0; i < num; ++i )
            {
                bool valid = true;
                
                view_params new_view;
                
                pen::json view = j_views[i];
                
                new_view.name = j_views[i].name();
                
                new_view.id_name = PEN_HASH(j_views[i].name());
                
                //render targets
                pen::json targets = view["target"];
                
                s32 num_targets = targets.size();
         
                s32 cur_rt = 0;
                for( s32 t = 0; t < num_targets; ++t )
                {
                    Str target_str = targets[t].as_str();
                    hash_id target_hash = PEN_HASH(target_str.c_str());
                    
                    if( target_hash == ID_MAIN_COLOUR || target_hash == ID_MAIN_DEPTH )
                        new_view.viewport_correction = false;
                    
                    bool found = false;
                    for( auto& r : k_render_targets )
                    {
                        if( target_hash == r.id_name )
                        {
                            found = true;
                            
                            s32 w = r.width;
                            s32 h = r.height;
                            s32 rr = r.ratio;
                            
                            new_view.id_render_target[cur_rt] = target_hash;
                            
                            if( cur_rt == 0 )
                            {
                                new_view.rt_width = w;
                                new_view.rt_height = h;
                                new_view.rt_ratio = rr;
                            }
                            else
                            {
                                if( new_view.rt_width != w || new_view.rt_height != h || new_view.rt_ratio != rr )
                                {
                                    PEN_PRINTF("render controller error: render target %s is incorrect dimension\n", target_str.c_str() );
                                    valid = false;
                                }
                            }
                            
                            if( r.format == PEN_TEX_FORMAT_D24_UNORM_S8_UINT )
                                new_view.depth_target = r.handle;
                            else
                                new_view.render_targets[cur_rt++] = r.handle;
                            
                            new_view.num_colour_targets = cur_rt;
                            
                            break;
                        }
                    }
                    
                    if(!found)
                    {
                        PEN_PRINTF("render controller error: missing render target - %s\n", target_str.c_str() );
                        valid = false;
                    }
                }
                
                //clear colour
                pen::json clear_colour = view["clear_colour"];
                
                f32 clear_colour_f[4] = { 0 };
                u8 clear_stencil_val = 0;
                
                u32 clear_flags = 0;
                
                if( clear_colour.size() == 4 )
                {
                    for( s32 c = 0; c < 4; ++c )
                        clear_colour_f[c] = clear_colour[c].as_f32();
                    
                    clear_flags |= PEN_CLEAR_COLOUR_BUFFER;
                }
                
                //clear depth
                pen::json clear_depth = view["clear_depth"];
                
                f32 clear_depth_f;
                
                if( clear_depth.type() != JSMN_UNDEFINED )
                {
                    clear_depth_f = clear_depth.as_f32();
                    
                    clear_flags |= PEN_CLEAR_DEPTH_BUFFER;
                }
                
                //clear stencil
                pen::json clear_stencil = view["clear_stencil"];
                
                if( clear_stencil.type() != JSMN_UNDEFINED )
                {
                    clear_stencil_val = clear_stencil.as_u8_hex();
                    
                    clear_flags |= PEN_CLEAR_STENCIL_BUFFER;
                }
                
                //clear state
                pen::clear_state cs_info =
                {
                    clear_colour_f[0], clear_colour_f[1], clear_colour_f[2], clear_colour_f[3], clear_depth_f, clear_stencil_val, clear_flags,
                };
                
                //clear mrt
                pen::json clear_mrt = view["clear"];
                for( s32 m = 0; m < clear_mrt.size(); ++m )
                {
                    pen::json jmrt = clear_mrt[m];
                    
                    hash_id rt_id = PEN_HASH(jmrt.name().c_str());
                    
                    for( s32 t = 0; t < num_targets; ++t )
                    {
                        if( new_view.id_render_target[t] == rt_id )
                        {
                            cs_info.num_colour_targets++;
                            
                            pen::json colour_f = jmrt["clear_colour_f"];
                            if( colour_f.size() == 4 )
                            {
                                for( s32 j = 0; j < 4; ++j )
                                {
                                    cs_info.mrt[t].type = pen::CLEAR_F32;
                                    cs_info.mrt[t].f[j] = colour_f[j].as_f32();
                                }
                                
                                break;
                            }
                            
                            pen::json colour_u = jmrt["clear_colour_u"];
                            if( colour_u.size() == 4 )
                            {
                                for( s32 j = 0; j < 4; ++j )
                                {
                                    cs_info.mrt[t].type = pen::CLEAR_U32;
                                    cs_info.mrt[t].u[j] = colour_u[j].as_u32();
                                }
                                
                                break;
                            }
                        }
                    }
                }
                
                new_view.clear_state = pen::renderer_create_clear_state( cs_info );
                
                //viewport
                pen::json viewport = view["viewport"];
                
                if( viewport.size() == 4 )
                {
                    for( s32 v = 0; v < 4; ++v )
                        new_view.viewport[v] = viewport[v].as_f32();
                }
                else
                {
                    new_view.viewport[0] = 0.0f;
                    new_view.viewport[1] = 0.0f;
                    new_view.viewport[2] = 1.0f;
                    new_view.viewport[3] = 1.0f;
                }
                
                //render state
                render_state* state = nullptr;
                
                //raster
                Str raster_state = view["raster_state"].as_cstr("default");
                raster_state.append("_raster_state");
                state = get_state_by_name( PEN_HASH( raster_state.c_str() ) );
                if( state )
                    new_view.raster_state = state->handle;
                
                //depth stencil
                Str depth_stencil_state = view["depth_stencil_state"].as_cstr("default");
                depth_stencil_state.append("_depth_stencil_state");
                state = get_state_by_name( PEN_HASH( depth_stencil_state.c_str() ) );
                if( state )
                    new_view.depth_stencil_state = state->handle;
                
                //blend
                bool alpha_to_coverage = view["alpha_to_coverage"].as_bool();
                pen::json colour_write_mask = view["colour_write_mask"];
                pen::json blend_state = view["blend_state"];
                
                new_view.blend_state = create_blend_state( j_views[i].name().c_str(), blend_state, colour_write_mask, alpha_to_coverage);
                
                //scene and camera
                Str scene_str = view["scene"].as_str();
                Str camera_str = view["camera"].as_str();
                
                hash_id scene_id = PEN_HASH(scene_str.c_str());
                hash_id camera_id = PEN_HASH(camera_str.c_str());
                
                bool found_scene = false;
                for( auto& s : k_scenes )
                {
                    if(s.id_name == scene_id)
                    {
                        new_view.scene = s.scene;
                        found_scene = true;
                        break;
                    }
                }
                
                if(!found_scene)
                {
                    PEN_PRINTF("render controller error: missing scene - %s\n", scene_str.c_str() );
                    valid = false;
                }

                bool found_camera = false;
                for( auto& c : k_cameras )
                {
                    if(c.id_name == camera_id)
                    {
                        new_view.camera = c.camera;
                        found_camera = true;
                        break;
                    }
                }
                
                if(!found_camera)
                {
                    PEN_PRINTF("render controller error: missing camera - %s\n", camera_str.c_str() );
                    valid = false;
                }
                
                Str technique_str = view["technique"].as_str();
                
                new_view.technique = PEN_HASH(technique_str.c_str());
                
                //scene views
                pen::json scene_views = view["scene_views"];
                for( s32 ii = 0; ii < scene_views.size(); ++ii )
                {
                    hash_id id = scene_views[ii].as_hash_id();
                    for( auto& sv : k_scene_view_renderers )
                        if( id == sv.id_name )
                            new_view.render_functions.push_back(sv.render_function);
                }
                
                if(valid)
                    k_views.push_back(new_view);
            }
        }
        
        void init( const c8* filename )
        {
            void* config_data;
            u32   config_data_size;
            
            pen_error err = pen::filesystem_read_file_to_buffer(filename, &config_data, config_data_size);
            
            if( err != PEN_ERR_OK || config_data_size == 0 )
            {
                //failed load file
                pen::memory_free(config_data);
                PEN_ASSERT(0);
            }
            
            create_geometry_utilities();
            
            pen::json render_config = pen::json::load((const c8*)config_data);
            
            parse_sampler_states(render_config);
            parse_raster_states(render_config);
            parse_depth_stencil_states(render_config);
            parse_partial_blend_states(render_config);
            
            parse_render_targets(render_config);
            
            parse_views(render_config);
        }
        
        void update()
        {
            s32 num_cameras = k_cameras.size();
            for( s32 i = 0; i < num_cameras; ++i )
            {
                k_cameras[i].update_function( &k_cameras[i] );
            }
            
            s32 num_scenes = k_scenes.size();
            for( s32 i = 0; i < num_scenes; ++i )
            {
                k_scenes[i].update_function( &k_scenes[i] );
            }
        }

        void render()
        {
            pen::buffer_creation_params bcp;
            bcp.usage_flags = PEN_USAGE_DYNAMIC;
            bcp.bind_flags = PEN_BIND_CONSTANT_BUFFER;
            bcp.cpu_access_flags = PEN_CPU_ACCESS_WRITE;
            bcp.buffer_size = sizeof(float) * 16;
            bcp.data = (void*)nullptr;
            
            static u32 cb_2d = pen::renderer_create_buffer(bcp);
            
            for( auto& v : k_views )
            {
                //viewport and scissor
                pen::viewport vp = { 0 };
                get_rt_viewport( v.rt_width, v.rt_height, v.rt_ratio, v.viewport, vp );
                
                //create 2d view proj matrix
                float W = 2.0f / vp.width;
                float H = 2.0f / vp.height;
                float mvp[4][4] =
                {
                    { W, 0.0, 0.0, 0.0 },
                    { 0.0, H, 0.0, 0.0 },
                    { 0.0, 0.0, 1.0, 0.0 },
                    { -1.0, -1.0, 0.0, 1.0 }
                };
                pen::renderer_update_buffer(cb_2d, mvp, sizeof(mvp), 0);
                
                //generate 3d view proj matrix
                put::camera_update_shader_constants(v.camera, v.viewport_correction);
                
                //target
                pen::renderer_set_viewport( vp );
                pen::renderer_set_scissor_rect({vp.x, vp.y, vp.width, vp.height});
                
                pen::renderer_set_targets( v.render_targets, v.num_colour_targets, v.depth_target);
                
                pen::renderer_clear( v.clear_state );
                
                //render state
                pen::renderer_set_rasterizer_state(v.raster_state);
                pen::renderer_set_depth_stencil_state(v.depth_stencil_state);
                pen::renderer_set_blend_state(v.blend_state);
                
                //build view info
                ces::scene_view sv;
                sv.scene = v.scene;
                sv.cb_view = v.camera->cbuffer;
                sv.scene_node_flags = 0;
                sv.technique = v.technique;
                sv.raster_state = v.raster_state;
                sv.depth_stencil_state = v.depth_stencil_state;
                sv.blend_state_state = v.blend_state;
                sv.camera = v.camera;
                sv.viewport = &vp;
                sv.cb_2d_view = cb_2d;
                
                //render passes
                for( s32 rf = 0; rf < v.render_functions.size(); ++rf )
                    v.render_functions[rf](sv);
            }

			//resolve
			pen::renderer_set_targets(PEN_BACK_BUFFER_COLOUR, PEN_BACK_BUFFER_DEPTH);

			for (s32 i = 0; i < 8; ++i)
			{
				pen::renderer_set_texture(0, 0, i, PEN_SHADER_TYPE_PS);
				pen::renderer_set_texture(0, 0, i, PEN_SHADER_TYPE_VS);
			}

			for (auto& rt : k_render_targets)
			{
				if (rt.samples > 1)
				{
					static pmfx_handle pmfx_resolve = pmfx::load("msaa_resolve");
					pmfx::set_technique(pmfx_resolve, PEN_HASH("average_4x"), 0);

					pen::renderer_resolve_target(rt.handle, pen::RESOLVE_CUSTOM);
				}
			}

			pen::renderer_set_targets(PEN_BACK_BUFFER_COLOUR, PEN_BACK_BUFFER_DEPTH);

			for (s32 i = 0; i < 8; ++i)
			{
				pen::renderer_set_texture(0, 0, i, PEN_SHADER_TYPE_PS);
				pen::renderer_set_texture(0, 0, i, PEN_SHADER_TYPE_VS);
			}
        }

		void debug_viewport( )
		{
			pen::renderer_set_targets(PEN_BACK_BUFFER_COLOUR, PEN_BACK_BUFFER_DEPTH);

			u32 h2 = 0;
			for (auto& rt : k_render_targets)
			{
				if (rt.id_name == PEN_HASH("gbuffer_albedo"))
				{
					h2 = rt.handle;
					break;
				}
			}

			static pmfx_handle pmfx_debug = pmfx::load("msaa_resolve");
			pmfx::set_technique(pmfx_debug, PEN_HASH("average_4x"), 0);

			pen::renderer_resolve_target(h2, pen::RESOLVE_CUSTOM);
		}

		void render_target_info_ui( const render_target& rt )
		{
			f32 w, h;
			get_rt_dimensions(rt.width, rt.height, rt.ratio, w, h);

			bool is_depth = rt.format == PEN_TEX_FORMAT_D24_UNORM_S8_UINT;

			const c8* format_str = nullptr;
			s32 byte_size = 0;
			for (s32 f = 0; f < num_formats; ++f)
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

			static bool k_dbg_vp = false;

			static u32 current_render_target = 0;

			if(k_dbg_vp)
				debug_viewport();
            
            if( open_renderer )
            {
                if( ImGui::Begin("Render Targets", &open_renderer, ImGuiWindowFlags_AlwaysAutoResize ) )
                {
					s32 num_rt = k_render_targets.size();

					if (ImGui::Button(ICON_FA_MINUS))
						current_render_target++;
					ImGui::SameLine();

					if (ImGui::Button(ICON_FA_PLUS))
						current_render_target--;
					ImGui::SameLine();

					if (current_render_target >= num_rt)
					{
						current_render_target = 0;
					}

					static s32 display_ratio = 1;
					ImGui::InputInt("Buffer Ratio", &display_ratio);
					display_ratio = std::max<s32>(1, display_ratio);

					render_target& rt = k_render_targets[current_render_target];

					f32 w, h;
					get_rt_dimensions(rt.width, rt.height, rt.ratio, w, h);

					bool unsupported_display = rt.id_name == ID_MAIN_COLOUR || rt.id_name == ID_MAIN_DEPTH;
					unsupported_display |= rt.format == PEN_TEX_FORMAT_R32_UINT;

					if (!unsupported_display)
					{
						f32 aspect = w / h;
						ImGui::Image((void*)&rt.handle, ImVec2(256 / display_ratio * aspect, 256 / display_ratio));
					}

					render_target_info_ui(rt);

                    if( ImGui::CollapsingHeader("Render Targets") )
                    {
                        for( auto& rt : k_render_targets )
                        {                                     
                            f32 w, h;
                            get_rt_dimensions(rt.width, rt.height, rt.ratio, w, h);

							bool is_depth = rt.format == PEN_TEX_FORMAT_D24_UNORM_S8_UINT;
                                                        
                            const c8* format_str = nullptr;
                            s32 byte_size = 0;
                            for( s32 f = 0; f < num_formats; ++f )
                            {
                                if( rt_format[f].format == rt.format )
                                {
                                    format_str = rt_format[f].name.c_str();
                                    byte_size = rt_format[f].block_size / 8;
                                    break;
                                }
                            }
                            
                            ImGui::Text("%s", rt.name.c_str() );
                            ImGui::Text("%ix%i, %s", (s32)w, (s32)h, format_str);
                            
                            s32 image_size = byte_size * w * h * rt.samples;
                            
                            if( rt.samples > 1 )
                            {
                                ImGui::Text("Msaa Samples: %i", rt.samples);
                                image_size += byte_size * w * h;
                            }
                            
                            ImGui::Text("Size: %f (mb)", (f32)image_size / 1024.0f / 1024.0f );

							if (!unsupported_display)
								ImGui::Image((void*)&rt.handle, ImVec2(w / display_ratio, h / display_ratio));
                            
                            ImGui::Separator();
                        }
                    }

                    ImGui::End();
                }
            }
        }
    }
}
