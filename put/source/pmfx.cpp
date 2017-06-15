#include <fstream>

#include "definitions.h"
#include "pmfx.h"
#include "memory.h"
#include "file_system.h"
#include "renderer.h"
#include "pen_string.h"
#include "json.hpp"

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
        
        struct managed_shader
        {
            c8 shader_name[64];
            json metadata;
            shader_program program;
            shader_program invalidated_program;
            u32 info_timestamp;
            bool invalidated;
        };
        std::vector<managed_shader> s_managed_shaders;
        
        const u32 max_techniques_per_fx = 8;
        struct pmfx
        {
            json            info;
            u32             info_timestamp;
            u32             num_techniques;
            shader_program  techniques[max_techniques_per_fx];
        };
        std::vector<pmfx> s_pmfx_list;
        
        shader_program load_shader_technique( const c8* fx_filename, json& j_techique, json& j_info )
        {
            shader_program program = { 0 };
            const c8* sfp = pen::renderer_get_shader_platform();
            
            //vertex shader
            c8* vs_file_buf = (c8*)pen::memory_alloc(256);
            std::string vs_filename_str = j_techique["vs_file"];
            pen::string_format( vs_file_buf, 256, "data/proto_shaders/%s/%s/%s", sfp, fx_filename, vs_filename_str.c_str() );
            
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
            pen::string_format( ps_file_buf, 256, "data/proto_shaders/%s/%s/%s", sfp, fx_filename, ps_filename_str.c_str() );
            
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

        void load( const c8* filename )
        {
            //load info file for description
            c8 info_file_buf[ 256 ];
            pen::string_format( info_file_buf, 256, "data/proto_shaders/%s/%s/info.json", pen::renderer_get_shader_platform(), filename );
            
            //read shader info json
            pmfx new_pmfx;

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
        }
        
        shader_program* load_shader_program( const c8* shader_name, managed_shader* ms )
        {
            if( s_managed_shaders.size() == 0 )
            {
                s_managed_shaders.reserve(1024);
            }
            
            c8 vs_file_buf[ 256 ];
            c8 ps_file_buf[ 256 ];
            c8 info_file_buf[ 256 ];

            pen::string_format( vs_file_buf, 256, "data/shaders/%s/%s.vsc", pen::renderer_get_shader_platform(), shader_name );
            pen::string_format( ps_file_buf, 256, "data/shaders/%s/%s.psc", pen::renderer_get_shader_platform(), shader_name );
            pen::string_format( info_file_buf, 256, "data/shaders/%s/%s.json", pen::renderer_get_shader_platform(), shader_name );

            //shaders
            pen::shader_load_params vs_slp;
            vs_slp.type = PEN_SHADER_TYPE_VS;

            pen::shader_load_params ps_slp;
            ps_slp.type = PEN_SHADER_TYPE_PS;

            //textured
            pen_error err = pen::filesystem_read_file_to_buffer( vs_file_buf, &vs_slp.byte_code, vs_slp.byte_code_size );

            if ( err != PEN_ERR_OK  )
            {
                //we must have a vertex shader, if this has failed, so will have the input layout.
                return nullptr;
            }

            bool hl = false;
            
            if (!ms)
            {
                //add a new managed shader
                u32 ms_index = s_managed_shaders.size();
                s_managed_shaders.push_back(managed_shader{});
                ms = &s_managed_shaders[ms_index];

                u32 name_len = pen::string_length(shader_name);
                pen::memory_cpy(ms->shader_name, shader_name, name_len);
            }
            else
            {
                hl = true;
                
                //otherwise we are hot loading
                ms->invalidated_program = ms->program;

                //compare timestamps of the .info files to see if we have actually updated
                u32 current_info_ts;
                pen_error err = pen::filesystem_getmtime( info_file_buf, current_info_ts );
                
                if ( err == PEN_ERR_OK && (u32)current_info_ts <= ms->info_timestamp)
                {
                    //return ourselves, so we remain invalid until the newly compiled shader is ready
                    return &ms->program;
                }
            }

            //read shader info json
            std::ifstream ifs(info_file_buf);
            ms->metadata = json::parse(ifs);
            u32 ts;
            err = pen::filesystem_getmtime(info_file_buf, ts);
            if( err == PEN_ERR_OK )
            {
                ms->info_timestamp = ts;
            }
            
            //create input layout from json
            pen::input_layout_creation_params ilp;
            ilp.vs_byte_code = vs_slp.byte_code;
            ilp.vs_byte_code_size = vs_slp.byte_code_size;
            ilp.num_elements = ms->metadata ["vs_inputs"].size();

            ilp.input_layout = (pen::input_layout_desc*)pen::memory_alloc(sizeof(pen::input_layout_desc) * ilp.num_elements);

            for (u32 i = 0; i < ilp.num_elements; ++i)
            {
                json vj = ms->metadata["vs_inputs"][i];

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

            ms->program.input_layout = pen::renderer_create_input_layout(ilp);

            if ( err != PEN_ERR_OK  )
            {
                //this shader set does not have a pixel shader which is valid, to allow fast z-only writes.
                ps_slp.byte_code = NULL;
                ps_slp.byte_code_size = 0;
            }
            
            err = pen::filesystem_read_file_to_buffer(ps_file_buf, &ps_slp.byte_code, ps_slp.byte_code_size);

            ms->program.vertex_shader = pen::renderer_load_shader( vs_slp );
            ms->program.pixel_shader = pen::renderer_load_shader( ps_slp );
            
            //link the shader to allow opengl to match d3d constant and texture bindings
            pen::shader_link_params link_params;
            link_params.input_layout = ms->program.input_layout;
            link_params.vertex_shader = ms->program.vertex_shader;
            link_params.pixel_shader = ms->program.pixel_shader;
            
            u32 num_constants = ms->metadata["cbuffers"].size() + ms->metadata["texture_samplers"].size();
            
            link_params.constants = (pen::constant_layout_desc*)pen::memory_alloc(sizeof(pen::constant_layout_desc) * num_constants);
            
            u32 cc = 0;
            
            for( auto& cbuf : ms->metadata["cbuffers"])
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
            
            for( auto& samplers : ms->metadata["texture_samplers"])
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
            
            ms->program.program_index = pen::renderer_link_shader_program(link_params);
            
            //free the temp mem
            for( u32 c = 0; c < num_constants; ++c )
            {
                pen::memory_free(link_params.constants[c].name);
            }
            
            pen::memory_free( link_params.constants );
            pen::memory_free( vs_slp.byte_code );
            pen::memory_free( ps_slp.byte_code );
            pen::memory_free( ilp.input_layout );

            ms->invalidated = false;
            
            return &ms->program;
        }
        
        void release_shader_program( shader_program* shader_program )
        {
            pen::renderer_release_shader( shader_program->vertex_shader, PEN_SHADER_TYPE_VS );
            pen::renderer_release_shader( shader_program->pixel_shader, PEN_SHADER_TYPE_PS );
            pen::renderer_release_input_layout( shader_program->input_layout );
            pen::renderer_release_program( shader_program->program_index );
        }

        void poll_for_changes()
        {
            static bool s_invalidated = false;

            if (s_invalidated)
            {
                bool awaiting_rebuild = false;

                s32 num = s_managed_shaders.size();
                for (s32 i = 0; i < num; ++i)
                {
                    //reload the shaders
                    if ( s_managed_shaders[i].invalidated )
                    {
                        awaiting_rebuild = true;
                        load_shader_program( s_managed_shaders[i].shader_name, &s_managed_shaders[i] );
                    }
                }

                if (!awaiting_rebuild)
                {
                    s_invalidated = false;
                }
            }
            else
            {
                for (auto& ms : s_managed_shaders)
                {
                    for (auto& file : ms.metadata["files"])
                    {
                        std::string fn = file["name"];
                        u32 shader_ts = file["timestamp"];
                        u32 current_ts;
                        pen_error err = pen::filesystem_getmtime(fn.c_str(), current_ts);

                        if ( err == PEN_ERR_OK && current_ts > shader_ts)
                        {
                            system( PEN_SHADER_COMPILE_CMD );
                            
                            ms.invalidated = true;
                            s_invalidated = true;
                            break;
                        }
                    }

                    if (s_invalidated)
                    {
                        break;
                    }
                }
            }
        }
    }
}
