#include <fstream>

#include "definitions.h"
#include "pmfx.h"
#include "memory.h"
#include "file_system.h"
#include "renderer.h"
#include "pen_string.h"
#include "json.hpp"
#include "pen_json.h"
#include "str/Str.h"

using json = nlohmann::json;

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
            const c8*       filename = nullptr;
            bool            invalidated = false;
            json            info;
            u32             info_timestamp = 0;
            u32             num_techniques = 0;
            shader_program  techniques[max_techniques_per_fx] = { 0 };
        };
        std::vector<pmfx> s_pmfx_list;
        
        shader_program load_shader_technique( const c8* fx_filename, json& j_techique, json& j_info )
        {
            shader_program program = { 0 };
            const c8* sfp = pen::renderer_get_shader_platform();
            
            //vertex shader
            c8* vs_file_buf = (c8*)pen::memory_alloc(256);
            std::string vs_filename_str = j_techique["vs_file"];
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
            std::string ps_filename_str = j_techique["ps_file"];
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
                json vj = j_techique["vs_inputs"][i];
                
                u32 num_elements = vj["num_elements"];
                u32 elements_size = vj["element_size"];
                
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
                
                ilp.input_layout[i].semantic_index = vj["semantic_index"];
                ilp.input_layout[i].format = fomats[num_elements-1];
                ilp.input_layout[i].semantic_name = &semantic_names[vj["semantic_id"]][0];
                ilp.input_layout[i].input_slot = 0;
                ilp.input_layout[i].aligned_byte_offset = vj["offset"];
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
            
            for( auto& cbuf : j_info["cbuffers"])
            {
                std::string name_str = cbuf["name"];
                u32 name_len = name_str.length();
                
                link_params.constants[cc].name = new c8[name_len+1];
                
                pen::memory_cpy(link_params.constants[cc].name, name_str.c_str(), name_len );
                
                link_params.constants[cc].name[name_len] = '\0';
                
                link_params.constants[cc].location = cbuf["location"];
                
                link_params.constants[cc].type = pen::CT_CBUFFER;
                
                cc++;
            }
            
            for( auto& samplers : j_info["texture_samplers"])
            {
                std::string name_str = samplers["name"];
                u32 name_len = name_str.length();
                
                link_params.constants[cc].name = (c8*)pen::memory_alloc(name_len+1);
                
                pen::memory_cpy(link_params.constants[cc].name, name_str.c_str(), name_len );
                
                link_params.constants[cc].name[name_len] = '\0';
                
                link_params.constants[cc].location = samplers["location"];
                
                static std::string sampler_type_names[] =
                {
                    "TEXTURE_2D",
                    "TEXTURE_3D",
                    "TEXTURE_CUBE"
                };
                
                for( u32 i = 0; i < 3; ++i )
                {
                    if( samplers["type"] == sampler_type_names[i] )
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
        
        void set_technique( pmfx_handle handle, u32 technique_index )
        {
            u32 ps = s_pmfx_list[ handle ].techniques[ technique_index ].pixel_shader;
            u32 vs = s_pmfx_list[ handle ].techniques[ technique_index ].vertex_shader;
            u32 il = s_pmfx_list[ handle ].techniques[ technique_index ].input_layout;
            
            pen::renderer_set_shader( vs, PEN_SHADER_TYPE_VS );
            pen::renderer_set_shader( ps, PEN_SHADER_TYPE_PS );
            pen::renderer_set_input_layout( il );
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
            
            pen::test_json(info_file_buf);
            
            //read shader info json
            pmfx new_pmfx;
            new_pmfx.num_techniques = 0;
            
            new_pmfx.filename = filename;
            
            std::ifstream ifs(info_file_buf);
            new_pmfx.info = json::parse(ifs);
            
            u32 ts;
            pen_error err = pen::filesystem_getmtime(info_file_buf, ts);
            if( err == PEN_ERR_OK )
            {
                new_pmfx.info_timestamp = ts;
            }
            
            //find num techiques
            json techniques = new_pmfx.info["techniques"];
            
            for( auto& t : techniques )
            {
                PEN_ASSERT( new_pmfx.num_techniques < max_techniques_per_fx );
                
                new_pmfx.techniques[new_pmfx.num_techniques++] = load_shader_technique( filename, t, new_pmfx.info );
            }
            
            return new_pmfx;
        }

        pmfx_handle load( const c8* filename )
        {
            pmfx new_pmfx = load_internal(filename);
            
            pmfx_handle ph = 0;
            for( auto& p : s_pmfx_list )
            {
                if( p.filename == nullptr )
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
                    get_pmfx_info_filename( info_file_buf, pmfx_set.filename );
                    
                    u32 current_ts;
                    pen_error err = pen::filesystem_getmtime(info_file_buf, current_ts);
                    
                    //wait until info is newer than the current info file,
                    //to know compilation is completed.
                    if( err == PEN_ERR_OK && current_ts > pmfx_set.info_timestamp )
                    {
                        //load new one
                        pmfx pmfx_new = load_internal( pmfx_set.filename );
                        
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
                    for (auto& file : pmfx_set.info["files"])
                    {
                        std::string fn = file["name"];
                        u32 shader_ts = file["timestamp"];
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
