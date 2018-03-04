#include <vector>

#include "definitions.h"
#include "pmfx.h"
#include "memory.h"
#include "file_system.h"
#include "renderer.h"
#include "pen_string.h"
#include "pen_json.h"
#include "str_utilities.h"
#include "str/Str.h"
#include "hash.h"
#include "dev_ui.h"

namespace put
{
    namespace pmfx
    {
        const c8* semantic_names[] =
        {
            "SV_POSITION",
            "POSITION",
            "TEXCOORD",
            "NORMAL",
            "TANGENT",
            "BITANGENT",
            "COLOR",
            "BLENDINDICES"
        };
        shader_program null_shader = { 0 };
        
        struct pmfx
        {
            hash_id         id_filename;
			Str				filename;
            bool            invalidated = false;
            pen::json       info;
            u32             info_timestamp = 0;
			std::vector<shader_program> techniques;
        };
        std::vector<pmfx> s_pmfx_list;
        
        void get_link_params_constants( pen::shader_link_params& link_params, const pen::json& j_info )
        {
            u32 num_constants = j_info["cbuffers"].size() + j_info["texture_samplers"].size();
            link_params.num_constants = num_constants;
            
            link_params.constants = (pen::constant_layout_desc*)pen::memory_alloc(sizeof(pen::constant_layout_desc) * num_constants);
            
            u32 cc = 0;
            pen::json j_cbuffers = j_info["cbuffers"];
            for( s32 i = 0; i < j_cbuffers.size(); ++i )
            {
                pen::json cbuf = j_cbuffers[i];
                Str name_str = cbuf["name"].as_str();
                
                u32 name_len = name_str.length();
                
                link_params.constants[cc].name = new c8[name_len+1];
                
                pen::memory_cpy(link_params.constants[cc].name, name_str.c_str(), name_len );
                
                link_params.constants[cc].name[name_len] = '\0';
                
                link_params.constants[cc].location = cbuf["location"].as_u32();
                
                link_params.constants[cc].type = pen::CT_CBUFFER;
                
                cc++;
            }
            
            pen::json j_samplers = j_info["texture_samplers"];
            for( s32 i = 0; i < j_samplers.size(); ++i )
            {
                pen::json sampler = j_samplers[i];
                
                Str name_str = sampler["name"].as_str();
                u32 name_len = name_str.length();
                
                link_params.constants[cc].name = (c8*)pen::memory_alloc(name_len+1);
                
                pen::memory_cpy(link_params.constants[cc].name, name_str.c_str(), name_len );
                
                link_params.constants[cc].name[name_len] = '\0';
                
                link_params.constants[cc].location = sampler["location"].as_u32();
                
                static Str sampler_type_names[] =
                {
                    "TEXTURE_2D",
                    "TEXTURE_3D",
                    "texture_cube",
                    "TEXTURE_2DMS"
                };
                
                for( u32 i = 0; i < 4; ++i )
                {
                    if( sampler["type"].as_str() == sampler_type_names[i] )
                    {
                        link_params.constants[cc].type = (pen::constant_type)i;
                        break;
                    }
                }
                
                cc++;
            }
        }
        
        void get_input_layout_params( pen::input_layout_creation_params& ilp, pen::json& j_techique )
        {
            u32 vertex_elements = j_techique["vs_inputs"].size();
            ilp.num_elements = vertex_elements;
            
            u32 instance_elements = j_techique["instance_inputs"].size();
            ilp.num_elements += instance_elements;
            
            ilp.input_layout = (pen::input_layout_desc*)pen::memory_alloc(sizeof(pen::input_layout_desc) * ilp.num_elements);
            
            struct layout
            {
                const c8* name;
                input_classification iclass;
                u32 step_rate;
                u32 num;
            };
            
            layout layouts[2] =
            {
                { "vs_inputs", PEN_INPUT_PER_VERTEX, 0, vertex_elements},
                { "instance_inputs", PEN_INPUT_PER_INSTANCE, 1, instance_elements},
            };
            
            u32 input_index = 0;
            for(u32 l = 0; l < 2; ++l)
            {
                for (u32 i = 0; i < layouts[l].num; ++i)
                {
                    pen::json vj = j_techique[layouts[l].name][i];
                    
                    u32 num_elements = vj["num_elements"].as_u32();
                    u32 elements_size = vj["element_size"].as_u32();
                    
                    static const s32 float_formats[4] =
                    {
                        PEN_VERTEX_FORMAT_FLOAT1,
                        PEN_VERTEX_FORMAT_FLOAT2,
                        PEN_VERTEX_FORMAT_FLOAT3,
                        PEN_VERTEX_FORMAT_FLOAT4
                    };
                    
                    static const s32 byte_formats[4] =
                    {
                        PEN_VERTEX_FORMAT_UNORM1,
                        PEN_VERTEX_FORMAT_UNORM2,
                        PEN_VERTEX_FORMAT_UNORM2,
                        PEN_VERTEX_FORMAT_UNORM4
                    };
                    
                    const s32* fomats = float_formats;
                    
                    if (elements_size == 1)
                        fomats = byte_formats;
                    
                    ilp.input_layout[input_index].semantic_index = vj["semantic_index"].as_u32();
                    ilp.input_layout[input_index].format = fomats[num_elements-1];
                    ilp.input_layout[input_index].semantic_name = semantic_names[vj["semantic_id"].as_u32()];
                    ilp.input_layout[input_index].input_slot = l;
                    ilp.input_layout[input_index].aligned_byte_offset = vj["offset"].as_u32();
                    ilp.input_layout[input_index].input_slot_class = layouts[l].iclass;
                    ilp.input_layout[input_index].instance_data_step_rate = layouts[l].step_rate;
                    
                    ++input_index;
                }
            }
        }
        
        shader_program load_shader_technique( const c8* fx_filename, pen::json& j_techique, pen::json& j_info )
        {
            shader_program program = { 0 };
            
            Str name = j_techique["name"].as_str();
            
            static const c8* k_sub_types[] =
            {
                "_skinned",
                "_instanced"
            };
			            
            //default sub type
            program.id_name = PEN_HASH(name.c_str());
            program.id_sub_type = PEN_HASH("");
            
            for( s32 i = 0; i < PEN_ARRAY_SIZE(k_sub_types); ++i )
            {
                //override subtype
                if( put::str_find(name, k_sub_types[i]) != -1 )
                {
                    Str name_base = put::str_replace_string(name, k_sub_types[i], "");
                    
                    program.id_name = PEN_HASH(name_base.c_str());
                    program.id_sub_type = PEN_HASH(k_sub_types[i]);
                    
                    break;
                }
            }

            const c8* sfp = pen::renderer_get_shader_platform();
            
            //vertex shader
            c8* vs_file_buf = (c8*)pen::memory_alloc(256);
            Str vs_filename_str = j_techique["vs_file"].as_str();
            pen::string_format( vs_file_buf, 256, "data/pmfx/%s/%s/%s", sfp, fx_filename, vs_filename_str.c_str() );
            
            pen::shader_load_params vs_slp;
            vs_slp.type = PEN_SHADER_TYPE_VS;
        
            pen_error err = pen::filesystem_read_file_to_buffer( vs_file_buf, &vs_slp.byte_code, vs_slp.byte_code_size );
            
            pen::memory_free(vs_file_buf);
            
            if ( err != PEN_ERR_OK  )
            {
                pen::memory_free(vs_slp.byte_code);
                return program;
            }
            
            //vertex stream out shader
            bool stream_out = j_techique["stream_out"].as_bool();
            if( stream_out )
            {
                vs_slp.type = PEN_SHADER_TYPE_SO;
                
                u32 num_vertex_outputs = j_techique["vs_outputs"].size();
                
                u32 decl_size_bytes = sizeof(pen::stream_out_decl_entry) * num_vertex_outputs;
                pen::stream_out_decl_entry* so_decl = (pen::stream_out_decl_entry*)pen::memory_alloc(decl_size_bytes);
                
                pen::shader_link_params slp;
                slp.stream_out_shader = program.stream_out_shader;
                slp.pixel_shader = 0;
                slp.vertex_shader = 0;
                
                slp.stream_out_names = new c8*[num_vertex_outputs];
                slp.num_stream_out_names = num_vertex_outputs;
                
                for( u32 vo = 0; vo < num_vertex_outputs; ++vo )
                {
                    pen::json voj = j_techique["vs_outputs"][vo];
                    
                    so_decl[vo].stream = 0;
                    so_decl[vo].semantic_name = semantic_names[voj["semantic_id"].as_u32()];
                    so_decl[vo].semantic_index = voj["semantic_index"].as_u32();
                    so_decl[vo].start_component = 0;
                    so_decl[vo].component_count = voj["num_elements"].as_u32();
                    so_decl[vo].output_slot = 0;
                    
                    Str gl_name = voj["name"].as_str();
                    gl_name.append("_vs_output");
                    
                    u32 name_len = gl_name.length();
                    slp.stream_out_names[vo] = new c8[name_len+1];
                    pen::memory_cpy(slp.stream_out_names[vo], gl_name.c_str(), name_len);
                    slp.stream_out_names[vo][name_len] = '\0';
                }
                
                vs_slp.so_decl_entries = so_decl;
                vs_slp.so_num_entries = num_vertex_outputs;
                
                program.stream_out_shader = pen::renderer_load_shader(vs_slp);
                program.vertex_shader = 0;
                program.pixel_shader = 0;
                
                pen::memory_free(vs_slp.so_decl_entries);
                
                get_link_params_constants(slp, j_info);
                
                slp.stream_out_shader = program.stream_out_shader;
                slp.vertex_shader = 0;
                slp.pixel_shader = 0;

                program.program_index = pen::renderer_link_shader_program(slp);
                
                //create input layout from json
                pen::input_layout_creation_params ilp;
                ilp.vs_byte_code = vs_slp.byte_code;
                ilp.vs_byte_code_size = vs_slp.byte_code_size;
                
                get_input_layout_params(ilp, j_techique);
                
                program.input_layout = pen::renderer_create_input_layout(ilp);
                
                return program;
            }
            
            //traditional vs / ps combo
            program.vertex_shader = pen::renderer_load_shader(vs_slp);
            pen::memory_free(vs_slp.so_decl_entries);
            
            //pixel shader
            c8* ps_file_buf = (c8*)pen::memory_alloc(256);
            Str ps_filename_str = j_techique["ps_file"].as_str();

			pen::string_format(ps_file_buf, 256, "data/pmfx/%s/%s/%s", sfp, fx_filename, ps_filename_str.c_str());

			pen::shader_load_params ps_slp;
			ps_slp.type = PEN_SHADER_TYPE_PS;

			err = pen::filesystem_read_file_to_buffer(ps_file_buf, &ps_slp.byte_code, ps_slp.byte_code_size);

			pen::memory_free(ps_file_buf);

			if (err != PEN_ERR_OK)
			{
				pen::memory_free(ps_slp.byte_code);
				return program;
			}

			program.pixel_shader = pen::renderer_load_shader(ps_slp);

            //create input layout from json
            pen::input_layout_creation_params ilp;
            ilp.vs_byte_code = vs_slp.byte_code;
            ilp.vs_byte_code_size = vs_slp.byte_code_size;
           
            get_input_layout_params(ilp, j_techique);
           
            program.input_layout = pen::renderer_create_input_layout(ilp);
            
            pen::memory_free(ilp.input_layout);
            
            //link the shader to allow opengl to match d3d constant and texture bindings
            pen::shader_link_params link_params;
            link_params.input_layout = program.input_layout;
            link_params.vertex_shader = program.vertex_shader;
            link_params.pixel_shader = program.pixel_shader;
            link_params.stream_out_shader = 0;
            link_params.stream_out_names = nullptr;
            
            get_link_params_constants( link_params, j_info );
            
            program.program_index = pen::renderer_link_shader_program(link_params);
            
            pen::memory_free(link_params.constants);
            
            return program;
        }
        
        void set_technique( pmfx_handle handle, u32 index )
        {
            auto& t = s_pmfx_list[ handle ].techniques[ index ];
            
            if( t.stream_out_shader )
            {
                pen::renderer_set_shader( t.stream_out_shader, PEN_SHADER_TYPE_SO );
            }
            else
            {
                pen::renderer_set_shader( t.vertex_shader, PEN_SHADER_TYPE_VS );
                pen::renderer_set_shader( t.pixel_shader, PEN_SHADER_TYPE_PS );
            }
            
            pen::renderer_set_input_layout( t.input_layout );
        }
        
        bool set_technique( pmfx_handle handle, hash_id id_technique, hash_id id_sub_type )
        {
            for( auto& t : s_pmfx_list[ handle ].techniques )
            {
                if( t.id_name != id_technique )
                    continue;
                
                if( t.id_sub_type != id_sub_type )
                    continue;
                
                if( t.stream_out_shader )
                {
                    pen::renderer_set_shader( t.stream_out_shader, PEN_SHADER_TYPE_SO );
                }
                else
                {
                    pen::renderer_set_shader( t.vertex_shader, PEN_SHADER_TYPE_VS );
                    pen::renderer_set_shader( t.pixel_shader, PEN_SHADER_TYPE_PS );
                }
                
                pen::renderer_set_input_layout( t.input_layout );
                
                return true;
            }
            
            return false;
        }
        
        void get_pmfx_info_filename( c8* file_buf, const c8* pmfx_filename )
        {
            pen::string_format( file_buf, 256, "data/pmfx/%s/%s/info.json", pen::renderer_get_shader_platform(), pmfx_filename );
        }
        
        void release( pmfx_handle handle )
        {
			s_pmfx_list[handle].filename = nullptr;
            
            for( auto& t : s_pmfx_list[handle].techniques )
            {
                pen::renderer_release_shader(t.pixel_shader, PEN_SHADER_TYPE_PS);
				pen::renderer_release_shader(t.vertex_shader, PEN_SHADER_TYPE_VS);
				pen::renderer_release_input_layout(t.input_layout );
            }
        }
        
        pmfx load_internal( const c8* filename )
        {
            //load info file for description
            c8 info_file_buf[ 256 ];
            get_pmfx_info_filename( info_file_buf, filename );
            
            //read shader info json
            pmfx new_pmfx;
            
            new_pmfx.filename = filename;
            new_pmfx.id_filename = PEN_HASH(filename);
            
            new_pmfx.info = pen::json::load_from_file(info_file_buf);
            
            u32 ts;
            pen_error err = pen::filesystem_getmtime(info_file_buf, ts);
            if( err == PEN_ERR_OK )
            {
                new_pmfx.info_timestamp = ts;
            }
            
            pen::json _techniques = new_pmfx.info["techniques"];
            
            for( s32 i = 0; i < _techniques.size(); ++i )
            {
                pen::json t = _techniques[i];
                shader_program new_technique = load_shader_technique( filename, t, new_pmfx.info );

				new_pmfx.techniques.push_back(new_technique);
            }
            
            return new_pmfx;
        }

        pmfx_handle load( const c8* pmfx_name )
        {
			//return existing
			pmfx_handle ph = 0;
			for (auto& p : s_pmfx_list)
				if (p.filename == pmfx_name)
					return ph;
                else
                    ph++;

			pmfx new_pmfx = load_internal(pmfx_name);

			ph = 0;
			for (auto& p : s_pmfx_list)
			{
				if (p.filename.length() == 0)
				{
					p = new_pmfx;
					return ph;
				}

				++ph;
			}
                        
            s_pmfx_list.push_back(new_pmfx);
            
            return ph;
        }
        
        pmfx_handle get_pmfx_handle( hash_id id_filename )
        {
            pmfx_handle ph = 0;
            for (auto& p : s_pmfx_list)
                if (p.id_filename == id_filename)
                    return ph;
                else
                    ph++;
            
            return PEN_INVALID_HANDLE;
        }
        
        void poll_for_changes( )
        {
			static bool initialised = false;

			static Str shader_compiler_str = "";
			if (shader_compiler_str == "")
			{
				initialised = true;

				pen::json j_build_config = pen::json::load_from_file("../../build_config.json");

				if (j_build_config.type() != JSMN_UNDEFINED)
				{
					Str pmtech_dir = "../../";
					pmtech_dir.append(j_build_config["pmtech_dir"].as_cstr());
					pmtech_dir.append(PEN_DIR);

					put::str_replace_chars(pmtech_dir, '/', PEN_DIR);

					shader_compiler_str.append(PEN_SHADER_COMPILE_PRE_CMD);
					shader_compiler_str.append(pmtech_dir.c_str());
					shader_compiler_str.append(PEN_SHADER_COMPILE_CMD);

					dev_console_log_level(dev_ui::CONSOLE_MESSAGE, "[shader compiler cmd] %s", shader_compiler_str.c_str());
				}
				else
				{
					dev_console_log_level( dev_ui::CONSOLE_ERROR, "%s", "[error] unable to find pmtech dir" );
				}
			}

			if (shader_compiler_str == "")
				return;

            pmfx_handle current_counter = 0;
            
            for( auto& pmfx_set : s_pmfx_list )
            {
                if( pmfx_set.invalidated )
                {
                    c8 info_file_buf[ 256 ];
                    get_pmfx_info_filename( info_file_buf, pmfx_set.filename.c_str() );

                    u32 current_ts;
                    pen_error err = pen::filesystem_getmtime(info_file_buf, current_ts);
                    
                    //wait until info is newer than the current info file,
                    //to know compilation is completed.
                    if( err == PEN_ERR_OK && current_ts > pmfx_set.info_timestamp )
                    {
                        //load new one
                        pmfx pmfx_new = load_internal( pmfx_set.filename.c_str() );
                        
                        //release existing
                        release(current_counter);
                        
                        //set exisiting to the new one
                        pmfx_set = pmfx_new;
                        
                        //no longer invalidated
                        pmfx_set.invalidated = false;
                    }
                }
                else
                {
                    pen::json files = pmfx_set.info["files"];
                    
                    s32 num_files = files.size();
                    for( s32 i = 0; i < num_files; ++i )
                    {
                        pen::json file = files[i];
                        Str fn = file["name"].as_str();
                        u32 shader_ts = file["timestamp"].as_u32();
                        u32 current_ts;
                        pen_error err = pen::filesystem_getmtime(fn.c_str(), current_ts);
                        
                        //trigger re-compile if files on disk are newer
                        if ( err == PEN_ERR_OK && current_ts > shader_ts)
                        {
                            system(shader_compiler_str.c_str());
                            pmfx_set.invalidated = true;
                        }
                    }
                }
                
                current_counter++;
            }
        }
    }
}
