#include "render_controller.h"
#include "entry_point.h"
#include "dev_ui.h"
#include "debug_render.h"
#include "pen_json.h"
#include "file_system.h"
#include "hash.h"
#include "pen_string.h"

extern pen::window_creation_params pen_window;

namespace put
{    
    namespace render_controller
    {
        static std::vector<scene_controller> k_scenes;
        static std::vector<camera_controller> k_cameras;
        
        void register_scene( const scene_controller& scene )
        {
            k_scenes.push_back(scene);
        }
        
        void register_camera( const camera_controller& cam )
        {
            k_cameras.push_back(cam);
        }
        
        struct format_info
        {
            Str name;
            s32 format;
            u32 block_size;
            bind_flags flags;
        };
        
        format_info rt_format[] =
        {
            //colour formats
            {"rgba8", PEN_TEX_FORMAT_RGBA8_UNORM, 32, PEN_BIND_RENDER_TARGET },
            {"bgra8", PEN_TEX_FORMAT_BGRA8_UNORM, 32, PEN_BIND_RENDER_TARGET },
            {"rgba32f", PEN_TEX_FORMAT_R32G32B32A32_FLOAT, 32*4, PEN_BIND_RENDER_TARGET },
            
            //depth stencil formats
            {"d24s8", PEN_TEX_FORMAT_D24_UNORM_S8_UINT, 32, PEN_BIND_DEPTH_STENCIL },
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
        
        static hash_id ID_MAIN_COLOUR = PEN_HASH("main_colour");
        static hash_id ID_MAIN_DEPTH = PEN_HASH("main_depth");
        
        struct view_params
        {
            s32     rt_width, rt_height;
            f32     rt_ratio;
            
            u32     render_targets[8] = { 0 };
            u32     depth_target = 0;
            u32     num_render_targets = 0;
            
            bool    viewport_correction = true;
            
            f32     viewport[4] = { 0 };
            
            u32     clear_state = 0;
            u32     raster_state = 0;
            
            hash_id technique;
        
            ces::entity_scene* scene;
            put::camera* camera;
        };
        
        static std::vector<render_target>   k_render_targets;
        static std::vector<view_params>     k_views;
        
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
        
        struct mode_map
        {
            const c8* name;
            u32 val;
        };
        
        static mode_map k_render_mode_map[] =
        {
            "none", PEN_CULL_NONE,
            "back", PEN_CULL_BACK,
            "front", PEN_CULL_FRONT,
            "solid", PEN_FILL_SOLID,
            "wireframe", PEN_FILL_WIREFRAME
        };
        static const u32 num_mode_maps = sizeof(k_render_mode_map) / sizeof(k_render_mode_map[0]);
        
        u32 mode_from_string( const c8* str, u32 default_value )
        {
            for( s32 i = 0; i < num_mode_maps; ++i )
                if( pen::string_compare(str, k_render_mode_map[i].name) == 0)
                    return k_render_mode_map[i].val;
            
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
        
        void parse_raster_state( pen::json& render_config )
        {
            pen::rasteriser_state_creation_params rcp;
            
            pen::json j_raster_states = render_config["raster_states"];
            s32 num = j_raster_states.size();
            for( s32 i = 0; i < num; ++i )
            {
                rcp.fill_mode = mode_from_string( j_raster_states[i]["fill_mode"].as_str().c_str(), PEN_FILL_SOLID );
                rcp.cull_mode = mode_from_string( j_raster_states[i]["cull_mode"].as_str().c_str(), PEN_CULL_BACK );
                rcp.front_ccw = j_raster_states[i]["front_ccw"].as_bool( false ) ? 0 : 1;
                rcp.depth_bias = j_raster_states[i]["depth_bias"].as_s32(0);
                rcp.depth_bias_clamp = j_raster_states[i]["depth_bias_clamp"].as_f32(0.0f);
                rcp.sloped_scale_depth_bias = j_raster_states[i]["sloped_scale_depth_bias"].as_f32(0.0f);
                rcp.depth_clip_enable = j_raster_states[i]["depth_clip_enable"].as_bool( true ) ? 0 : 1;
                rcp.scissor_enable = j_raster_states[i]["scissor_enable"].as_bool( false ) ? 0 : 1;
                rcp.multisample = j_raster_states[i]["multisample"].as_bool( false ) ? 0 : 1;
                rcp.aa_lines = j_raster_states[i]["aa_lines"].as_bool( false ) ? 0 : 1;
                
                hash_id hh = PEN_HASH(rcp);
                if( !get_state_by_hash( hh ) )
                {
                    render_state rs;
                    rs.hash = hh;
                    rs.id_name =  PEN_HASH(j_raster_states[i].name().c_str());
                    rs.handle = pen::renderer_create_rasterizer_state(rcp);
                    
                    k_render_states.push_back(rs);
                }
            }
        }
        
        void parse_render_targets( pen::json& render_config )
        {
            //add 2 defaults
            render_target main_colour;
            main_colour.id_name = ID_MAIN_COLOUR;
            main_colour.ratio = 1;
            main_colour.format = PEN_TEX_FORMAT_RGBA8_UNORM;
            main_colour.handle = PEN_DEFAULT_RT;
            
            k_render_targets.push_back(main_colour);
            
            render_target main_depth;
            main_depth.id_name = ID_MAIN_DEPTH;
            main_depth.ratio = 1;
            main_depth.format = PEN_TEX_FORMAT_D24_UNORM_S8_UINT;
            main_depth.handle = PEN_DEFAULT_DS;
            
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
                        
                        new_info.num_mips = calc_num_mips(new_info.width, new_info.height);
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
                        
                        //arays and mips
                        tcp.num_arrays = r["num_arrays"].as_u32(1);
                        tcp.num_mips = r["num_mips"].as_u32(1);
                        
                        //flags
                        tcp.cpu_access_flags = 0;
                        tcp.bind_flags = rt_format[f].flags | PEN_BIND_SHADER_RESOURCE;
                        
                        //msaa
                        tcp.sample_count = r["samples"].as_u32(1);
                        tcp.sample_quality = 0;
                        
                        new_info.msaa = tcp.sample_count > 1;
                        
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
        
        void parse_views( pen::json& render_config )
        {
            pen::json j_views = render_config["views"];
            
            s32 num = j_views.size();
            
            for( s32 i = 0; i < num; ++i )
            {
                bool valid = true;
                
                view_params new_view;
                
                pen::json view = j_views[i];
                
                //clear colour
                pen::json clear_colour = view["clear_colour"];
                
                f32 clear_data[5] = { 0 };
                
                u32 clear_flags = 0;
                
                if( clear_colour.size() == 4 )
                {
                    for( s32 c = 0; c < 4; ++c )
                    {
                        clear_data[c] = clear_colour[c].as_f32();
                    }
                    
                    clear_flags |= PEN_CLEAR_COLOUR_BUFFER;
                }
                
                //clear depth
                pen::json clear_depth = view["clear_depth"];
                
                if( clear_depth.type() != JSMN_UNDEFINED )
                {
                    clear_data[4] = clear_depth.as_f32();
                    
                    clear_flags |= PEN_CLEAR_DEPTH_BUFFER;
                }
            
                //clear state
                pen::clear_state cs_info =
                {
                    clear_data[0], clear_data[1], clear_data[2], clear_data[3], clear_data[4], clear_flags,
                };
                
                new_view.clear_state = pen::renderer_create_clear_state( cs_info );
                
                //render targets
                pen::json targets = view["target"];
                
                new_view.num_render_targets = targets.size();
         
                s32 cur_rt = 0;
                for( s32 t = 0; t < new_view.num_render_targets; ++t )
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
                            
                            break;
                        }
                    }
                    
                    if(!found)
                    {
                        PEN_PRINTF("render controller error: missing render target - %s\n", target_str.c_str() );
                        valid = false;
                    }
                }
                
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
                hash_id hh = PEN_HASH(view["raster_state"].as_str("default").c_str());
                render_state* state = get_state_by_name(hh);
                if( state )
                    new_view.raster_state = state->handle;
                
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
            
            pen::json render_config = pen::json::load((const c8*)config_data);
            
            parse_raster_state(render_config);
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
            for( auto& v : k_views )
            {
                //viewport and scissor
                pen::viewport vp = { 0 };
                get_rt_viewport( v.rt_width, v.rt_height, v.rt_ratio, v.viewport, vp );
                pen::renderer_set_viewport( vp );
                pen::renderer_set_scissor_rect({vp.x, vp.y, vp.width, vp.height});
                pen::renderer_set_targets(v.render_targets[0], v.depth_target);
                
                //clear
                pen::renderer_clear( v.clear_state );
                
                //set render state
                pen::renderer_set_rasterizer_state(v.raster_state);
                
                //generate camera matrices
                put::camera_update_shader_constants(v.camera, v.viewport_correction);
                
                ces::scene_view sv;
                sv.scene = v.scene;
                sv.cb_view = v.camera->cbuffer;
                sv.scene_node_flags = 0;
                sv.technique = v.technique;
                
                put::ces::render_scene_view(sv);
                put::ces::render_scene_debug(sv);
            }
        }
        
        void show_dev_ui()
        {
            ImGui::BeginMainMenuBar();
            
            static bool open_renderer = false;
            if (ImGui::Button(ICON_FA_PICTURE_O))
            {
                open_renderer = true;
            }
        
            ImGui::EndMainMenuBar();
            
            if( open_renderer )
            {
                ImGui::Begin("Render Controller", &open_renderer );
                
                static s32 display_ratio = 4;
                ImGui::InputInt("Buffer Ratio", &display_ratio);
                
                if( ImGui::CollapsingHeader("Render Targets") )
                {
                    for( auto& rt : k_render_targets )
                    {
                        if(rt.id_name == ID_MAIN_COLOUR || rt.id_name == ID_MAIN_DEPTH )
                            continue;
                        
                        if( rt.msaa )
                        {
                            pen::renderer_resolve_target(rt.handle);
                        }

                        f32 w, h;
                        get_rt_dimensions(rt.width, rt.height, rt.ratio, w, h);
                        
                        ImGui::Image((void*)&rt.handle, ImVec2(w / display_ratio, h / display_ratio));
                        
                        ImGui::Text("%i, %i", (s32)h, (s32)w );
                    }
                }
            
                ImGui::End();
            }
        }
    }
}
