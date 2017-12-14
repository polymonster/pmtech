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

namespace put
{
    namespace pmfx
    {
        c8 semantic_names[7][16] =
        {
            "POSITION",
            "TEXCOORD",
            "NORMAL",
            "TANGENT",
            "BITANGENT",
            "COLOR",
            "BLENDINDICES"
        };
        shader_program null_shader = { 0 };
        
        const u32 max_techniques_per_fx = 8;
        struct pmfx
        {
			Str				filename;
            bool            invalidated = false;
            pen::json       info;
            u32             info_timestamp = 0;
			std::vector<shader_program> techniques;
        };
        std::vector<pmfx> s_pmfx_list;
        
        shader_program load_shader_technique( const c8* fx_filename, pen::json& j_techique, pen::json& j_info )
        {
            shader_program program = { 0 };
            
            Str name = j_techique["name"].as_str();
            
            if( put::str_find(name, "_skinned") != -1 )
            {
                Str name_base = put::str_replace_string(name, "_skinned", "");
                
                program.id_name = PEN_HASH(name_base.c_str());
                program.id_sub_type = PEN_HASH("_skinned");
            }
            else
            {
                program.id_name = PEN_HASH(name.c_str());
                program.id_sub_type = PEN_HASH("");
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
            
            program.vertex_shader = pen::renderer_load_shader(vs_slp);
            
            //pixel shader
            c8* ps_file_buf = (c8*)pen::memory_alloc(256);
            Str ps_filename_str = j_techique["ps_file"].as_str();
            pen::string_format( ps_file_buf, 256, "data/pmfx/%s/%s/%s", sfp, fx_filename, ps_filename_str.c_str() );
            
            pen::shader_load_params ps_slp;
            ps_slp.type = PEN_SHADER_TYPE_PS;
            
            err = pen::filesystem_read_file_to_buffer( ps_file_buf, &ps_slp.byte_code, ps_slp.byte_code_size );
            
            pen::memory_free(ps_file_buf);
            
            if ( err != PEN_ERR_OK  )
            {
                pen::memory_free(ps_slp.byte_code);
                
                return program;
            }
            
            program.pixel_shader = pen::renderer_load_shader(ps_slp);
            
            //create input layout from json
            pen::input_layout_creation_params ilp;
            ilp.vs_byte_code = vs_slp.byte_code;
            ilp.vs_byte_code_size = vs_slp.byte_code_size;
            ilp.num_elements = j_techique["vs_inputs"].size();
            
            ilp.input_layout = (pen::input_layout_desc*)pen::memory_alloc(sizeof(pen::input_layout_desc) * ilp.num_elements);
            
            for (u32 i = 0; i < ilp.num_elements; ++i)
            {
                pen::json vj = j_techique["vs_inputs"][i];
                
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
                
                ilp.input_layout[i].semantic_index = vj["semantic_index"].as_u32();
                ilp.input_layout[i].format = fomats[num_elements-1];
                ilp.input_layout[i].semantic_name = &semantic_names[vj["semantic_id"].as_u32()][0];
                ilp.input_layout[i].input_slot = 0;
                ilp.input_layout[i].aligned_byte_offset = vj["offset"].as_u32();
                ilp.input_layout[i].input_slot_class = PEN_INPUT_PER_VERTEX;
                ilp.input_layout[i].instance_data_step_rate = 0;
            }
            
            program.input_layout = pen::renderer_create_input_layout(ilp);
            
            pen::memory_free(ilp.input_layout);
            
            //link the shader to allow opengl to match d3d constant and texture bindings
            pen::shader_link_params link_params;
            link_params.input_layout = program.input_layout;
            link_params.vertex_shader = program.vertex_shader;
            link_params.pixel_shader = program.pixel_shader;
            
            u32 num_constants = j_info["cbuffers"].size() + j_info["texture_samplers"].size();
            
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
                    "TEXTURE_CUBE"
                };
                
                for( u32 i = 0; i < 3; ++i )
                {
                    if( sampler["type"].as_str() == sampler_type_names[i] )
                    {
                        link_params.constants[cc].type = (pen::constant_type)i;
                        break;
                    }
                }
                
                cc++;
            }
            
            link_params.num_constants = num_constants;
            
            program.program_index = pen::renderer_link_shader_program(link_params);
            
            pen::memory_free(link_params.constants);
            
            return program;
        }
        
        void set_technique( pmfx_handle handle, u32 index )
        {
            auto& t = s_pmfx_list[ handle ].techniques[ index ];
            
            pen::renderer_set_shader( t.vertex_shader, PEN_SHADER_TYPE_VS );
            pen::renderer_set_shader( t.pixel_shader, PEN_SHADER_TYPE_PS );
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
                
                pen::renderer_set_shader( t.vertex_shader, PEN_SHADER_TYPE_VS );
                pen::renderer_set_shader( t.pixel_shader, PEN_SHADER_TYPE_PS );
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
           // s_pmfx_list[handle].filename = nullptr;
            
            for( auto& t : s_pmfx_list[handle].techniques )
            {
                //pen::renderer_release_shader(t.pixel_shader, PEN_SHADER_TYPE_PS);
                //pen::renderer_release_shader(t.vertex_shader, PEN_SHADER_TYPE_VS);
                //pen::renderer_release_input_layout(t.input_layout );
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
        
        void poll_for_changes( )
        {
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
                            system( PEN_SHADER_COMPILE_CMD );
                            pmfx_set.invalidated = true;
                        }
                    }
                }
                
                current_counter++;
            }
        }
    }
}
