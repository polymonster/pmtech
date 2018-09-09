#include <vector>

#include "ces/ces_resources.h"
#include "dev_ui.h"
#include "pmfx.h"
#include "str/Str.h"
#include "str_utilities.h"

#include "console.h"
#include "data_struct.h"
#include "file_system.h"
#include "hash.h"
#include "memory.h"
#include "pen.h"
#include "pen_json.h"
#include "pen_string.h"
#include "renderer.h"

using namespace put;
using namespace pmfx;
using namespace ces;

namespace
{
    // clang-format off
    const c8* semantic_names[] = {
        "SV_POSITION",
        "POSITION",
        "TEXCOORD",
        "NORMAL",
        "TANGENT",
        "BITANGENT",
        "COLOR",
        "BLENDINDICES"
    };
    // clang-format on
    
    shader_program null_shader      = {0};
    
    hash_id id_widgets[] = {PEN_HASH("slider"), PEN_HASH("input"), PEN_HASH("colour")};
    static_assert(PEN_ARRAY_SIZE(id_widgets) == CW_NUM, "mismatched array size");
    
    struct pmfx_shader
    {
        hash_id         id_filename = 0;
        Str             filename    = nullptr;
        bool            invalidated = false;
        pen::json       info;
        u32             info_timestamp = 0;
        shader_program* techniques     = nullptr;
    };
    pmfx_shader* k_pmfx_list = nullptr;
    
    const char**  k_shader_names    = nullptr;
    const char*** k_technique_names = nullptr;
    
    u32 k_num_shader_names = 0;
}

namespace put
{
    namespace pmfx
    {
        void generate_name_lists()
        {
            pen::memory_free(k_shader_names);

            for (u32 i = 0; i < k_num_shader_names; ++i)
                sb_free(k_technique_names[i]);

            pen::memory_free(k_technique_names);

            u32 num_pmfx      = sb_count(k_pmfx_list);
            k_technique_names = (const char***)pen::memory_calloc(num_pmfx, sizeof(k_technique_names));
            k_shader_names    = (const char**)pen::memory_calloc(num_pmfx, sizeof(k_shader_names));

            for (u32 i = 0; i < num_pmfx; ++i)
            {
                k_shader_names[i] = k_pmfx_list[i].filename.c_str();

                u32 num_techniques = sb_count(k_pmfx_list[i].techniques);
                for (u32 t = 0; t < num_techniques; ++t)
                {
                    sb_push(k_technique_names[i], k_pmfx_list[i].techniques[t].name.c_str());
                }
            }

            k_num_shader_names = num_pmfx;
        }

        const c8** get_shader_list(u32& count)
        {
            count = k_num_shader_names;
            return k_shader_names;
        }

        const c8** get_technique_list(shader_handle index, u32& count)
        {
            count = sb_count(k_technique_names[index]);
            return k_technique_names[index];
        }

        const c8* get_shader_name(shader_handle handle)
        {
            if (handle >= sb_count(k_pmfx_list))
                return nullptr;

            return k_pmfx_list[handle].filename.c_str();
        }

        const c8* get_technique_name(shader_handle handle, hash_id id_technique)
        {
            if (handle >= sb_count(k_pmfx_list))
                return nullptr;

            u32 nt = sb_count(k_pmfx_list[handle].techniques);
            for (u32 i = 0; i < nt; ++i)
                if (k_pmfx_list[handle].techniques[i].id_name == id_technique)
                    return k_pmfx_list[handle].techniques[i].name.c_str();

            return nullptr;
        }

        void get_link_params_constants(pen::shader_link_params& link_params,
                                       const pen::json& j_info,
                                       const pen::json& j_technique)
        {
            u32 num_constants         = j_info["cbuffers"].size() +
                                        j_info["texture_samplers"].size() +
                                        j_technique["texture_samplers"].size();
            
            link_params.num_constants = num_constants;

            link_params.constants =
                (pen::constant_layout_desc*)pen::memory_alloc(sizeof(pen::constant_layout_desc) * num_constants);

            // per pmfx constants
            // .. per technique constants go into: material_data(7), todo rename to techhnique_data
            
            u32       cc         = 0;
            pen::json j_cbuffers = j_info["cbuffers"];
            for (s32 i = 0; i < j_cbuffers.size(); ++i)
            {
                pen::json cbuf     = j_cbuffers[i];
                Str       name_str = cbuf["name"].as_str();

                u32 name_len = name_str.length();

                link_params.constants[cc].name = new c8[name_len + 1];

                memcpy(link_params.constants[cc].name, name_str.c_str(), name_len);

                link_params.constants[cc].name[name_len] = '\0';

                link_params.constants[cc].location = cbuf["location"].as_u32();

                link_params.constants[cc].type = pen::CT_CBUFFER;

                cc++;
            }
            
            static Str sampler_type_names[] = {"texture_2d", "texture_3d", "texture_cube", "texture_2dms"};

            // per pmfx textures [0]
            // per technique texture samplers [1]
            pen::json j_samplers_[2];
            
            j_samplers_[0] = j_info["texture_samplers"];
            j_samplers_[1] = j_technique["texture_samplers"];
            
            for(u32 s = 0; s < 2; ++s)
            {
                const pen::json& j_samplers = j_samplers_[s];
                
                for (s32 i = 0; i < j_samplers.size(); ++i)
                {
                    pen::json sampler = j_samplers[i];
                    
                    Str name_str = sampler["name"].as_str();
                    if(!sampler["name"].as_cstr())
                        name_str = sampler.key();
                    
                    u32 name_len = name_str.length();
                    
                    link_params.constants[cc].name = (c8*)pen::memory_alloc(name_len + 1);
                    
                    memcpy(link_params.constants[cc].name, name_str.c_str(), name_len);
                    
                    link_params.constants[cc].name[name_len] = '\0';
                    
                    link_params.constants[cc].location = sampler["unit"].as_u32();
                    
                    for (u32 i = 0; i < 4; ++i)
                    {
                        if (sampler["type"].as_str() == sampler_type_names[i])
                        {
                            link_params.constants[cc].type = (pen::constant_type)i;
                            break;
                        }
                    }
                    
                    cc++;
                }
            }
        }

        void get_input_layout_params(pen::input_layout_creation_params& ilp, pen::json& j_techique)
        {
            u32 vertex_elements = j_techique["vs_inputs"].size();
            ilp.num_elements    = vertex_elements;

            u32 instance_elements = j_techique["instance_inputs"].size();
            ilp.num_elements += instance_elements;

            ilp.input_layout = (pen::input_layout_desc*)pen::memory_alloc(sizeof(pen::input_layout_desc) * ilp.num_elements);

            struct layout
            {
                const c8*            name;
                input_classification iclass;
                u32                  step_rate;
                u32                  num;
            };

            layout layouts[2] = {
                {"vs_inputs", PEN_INPUT_PER_VERTEX, 0, vertex_elements},
                {"instance_inputs", PEN_INPUT_PER_INSTANCE, 1, instance_elements},
            };

            u32 input_index = 0;
            for (u32 l = 0; l < 2; ++l)
            {
                for (u32 i = 0; i < layouts[l].num; ++i)
                {
                    pen::json vj = j_techique[layouts[l].name][i];

                    u32 num_elements  = vj["num_elements"].as_u32();
                    u32 elements_size = vj["element_size"].as_u32();

                    static const s32 float_formats[4] = {PEN_VERTEX_FORMAT_FLOAT1, PEN_VERTEX_FORMAT_FLOAT2,
                                                         PEN_VERTEX_FORMAT_FLOAT3, PEN_VERTEX_FORMAT_FLOAT4};

                    static const s32 byte_formats[4] = {PEN_VERTEX_FORMAT_UNORM1, PEN_VERTEX_FORMAT_UNORM2,
                                                        PEN_VERTEX_FORMAT_UNORM2, PEN_VERTEX_FORMAT_UNORM4};

                    const s32* fomats = float_formats;

                    if (elements_size == 1)
                        fomats = byte_formats;

                    ilp.input_layout[input_index].semantic_index          = vj["semantic_index"].as_u32();
                    ilp.input_layout[input_index].format                  = fomats[num_elements - 1];
                    ilp.input_layout[input_index].semantic_name           = semantic_names[vj["semantic_id"].as_u32()];
                    ilp.input_layout[input_index].input_slot              = l;
                    ilp.input_layout[input_index].aligned_byte_offset     = vj["offset"].as_u32();
                    ilp.input_layout[input_index].input_slot_class        = layouts[l].iclass;
                    ilp.input_layout[input_index].instance_data_step_rate = layouts[l].step_rate;

                    ++input_index;
                }
            }
        }

        shader_program load_shader_technique(const c8* fx_filename, pen::json& j_techique, pen::json& j_info)
        {
            shader_program program = {0};

            Str name = j_techique["name"].as_str();

            static const c8* k_sub_types[] = {"_skinned", "_instanced"};

            // default sub type
            program.name        = name;
            program.id_name     = PEN_HASH(name.c_str());
            program.id_sub_type = PEN_HASH("");

            for (s32 i = 0; i < PEN_ARRAY_SIZE(k_sub_types); ++i)
            {
                // override subtype
                if (pen::str_find(name, k_sub_types[i]) != -1)
                {
                    Str name_base = pen::str_replace_string(name, k_sub_types[i], "");

                    program.id_name     = PEN_HASH(name_base.c_str());
                    program.id_sub_type = PEN_HASH(k_sub_types[i]);

                    break;
                }
            }

            const c8* sfp = pen::renderer_get_shader_platform();

            // vertex shader
            c8* vs_file_buf     = (c8*)pen::memory_alloc(256);
            Str vs_filename_str = j_techique["vs_file"].as_str();
            pen::string_format(vs_file_buf, 256, "data/pmfx/%s/%s/%s", sfp, fx_filename, vs_filename_str.c_str());

            pen::shader_load_params vs_slp;
            vs_slp.type = PEN_SHADER_TYPE_VS;

            pen_error err = pen::filesystem_read_file_to_buffer(vs_file_buf, &vs_slp.byte_code, vs_slp.byte_code_size);

            pen::memory_free(vs_file_buf);

            if (err != PEN_ERR_OK)
            {
                pen::memory_free(vs_slp.byte_code);
                return program;
            }

            // vertex stream out shader
            bool stream_out = j_techique["stream_out"].as_bool();
            if (stream_out)
            {
                vs_slp.type = PEN_SHADER_TYPE_SO;

                u32 num_vertex_outputs = j_techique["vs_outputs"].size();

                u32                         decl_size_bytes = sizeof(pen::stream_out_decl_entry) * num_vertex_outputs;
                pen::stream_out_decl_entry* so_decl         = (pen::stream_out_decl_entry*)pen::memory_alloc(decl_size_bytes);

                pen::shader_link_params slp;
                slp.stream_out_shader = program.stream_out_shader;
                slp.pixel_shader      = 0;
                slp.vertex_shader     = 0;

                slp.stream_out_names     = new c8*[num_vertex_outputs];
                slp.num_stream_out_names = num_vertex_outputs;

                for (u32 vo = 0; vo < num_vertex_outputs; ++vo)
                {
                    pen::json voj = j_techique["vs_outputs"][vo];

                    so_decl[vo].stream          = 0;
                    so_decl[vo].semantic_name   = semantic_names[voj["semantic_id"].as_u32()];
                    so_decl[vo].semantic_index  = voj["semantic_index"].as_u32();
                    so_decl[vo].start_component = 0;
                    so_decl[vo].component_count = voj["num_elements"].as_u32();
                    so_decl[vo].output_slot     = 0;

                    Str gl_name = voj["name"].as_str();
                    gl_name.append("_vs_output");

                    u32 name_len             = gl_name.length();
                    slp.stream_out_names[vo] = new c8[name_len + 1];
                    memcpy(slp.stream_out_names[vo], gl_name.c_str(), name_len);
                    slp.stream_out_names[vo][name_len] = '\0';
                }

                vs_slp.so_decl_entries = so_decl;
                vs_slp.so_num_entries  = num_vertex_outputs;

                program.stream_out_shader = pen::renderer_load_shader(vs_slp);
                program.vertex_shader     = 0;
                program.pixel_shader      = 0;

                pen::memory_free(vs_slp.so_decl_entries);

                get_link_params_constants(slp, j_info, j_techique);

                slp.stream_out_shader = program.stream_out_shader;
                slp.vertex_shader     = 0;
                slp.pixel_shader      = 0;

                program.program_index = pen::renderer_link_shader_program(slp);

                // create input layout from json
                pen::input_layout_creation_params ilp;
                ilp.vs_byte_code      = vs_slp.byte_code;
                ilp.vs_byte_code_size = vs_slp.byte_code_size;

                get_input_layout_params(ilp, j_techique);

                program.input_layout = pen::renderer_create_input_layout(ilp);

                return program;
            }

            // traditional vs / ps combo
            program.vertex_shader = pen::renderer_load_shader(vs_slp);
            pen::memory_free(vs_slp.so_decl_entries);

            // pixel shader
            c8* ps_file_buf     = (c8*)pen::memory_alloc(256);
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

            // create input layout from json
            pen::input_layout_creation_params ilp;
            ilp.vs_byte_code      = vs_slp.byte_code;
            ilp.vs_byte_code_size = vs_slp.byte_code_size;

            get_input_layout_params(ilp, j_techique);

            program.input_layout = pen::renderer_create_input_layout(ilp);

            pen::memory_free(ilp.input_layout);

            // link the shader to allow opengl to match d3d constant and texture bindings
            pen::shader_link_params link_params;
            link_params.input_layout      = program.input_layout;
            link_params.vertex_shader     = program.vertex_shader;
            link_params.pixel_shader      = program.pixel_shader;
            link_params.stream_out_shader = 0;
            link_params.stream_out_names  = nullptr;

            get_link_params_constants(link_params, j_info, j_techique);

            program.program_index = pen::renderer_link_shader_program(link_params);

            pen::memory_free(link_params.constants);
            
            // generate technique textures meta data
            program.textures                = nullptr;
            u32 num_technique_textues       = j_techique["texture_samplers"].size();
            
            for (u32 i = 0; i < num_technique_textues; ++i)
            {
                pen::json jt = j_techique["texture_samplers"][i];
                
                technique_sampler tt;
                
                tt.name = jt.name();
                tt.sampler_state_name = jt["sampler_state"].as_str();
                tt.unit = jt["unit"].as_u32();
                tt.type_name = jt["type"].as_str();
                tt.default_name = jt["default"].as_str();
                tt.filename = tt.default_name;
                tt.handle = put::load_texture(tt.filename.c_str());
                
                sb_push(program.textures, tt);
            }

            // generate technique constants meta data
            program.constants               = nullptr;
            u32 num_technique_constants     = j_techique["constants"].size();
            program.technique_constant_size = j_techique["constants_size_bytes"].as_u32(0);

            if (program.technique_constant_size > 0)
                program.constant_defaults = (f32*)pen::memory_alloc(program.technique_constant_size);

            u32 constant_offset = 0;
            for (u32 i = 0; i < num_technique_constants; ++i)
            {
                pen::json jc = j_techique["constants"][i];

                technique_constant tc;
                tc.name = jc.name();

                hash_id widget = jc["widget"].as_hash_id(PEN_HASH("input"));
                for (u32 j = 0; j < CW_NUM; ++j)
                {
                    if (widget == id_widgets[j])
                    {
                        tc.widget = j;
                        break;
                    }
                }

                tc.min          = jc["min"].as_f32(tc.min);
                tc.max          = jc["max"].as_f32(tc.max);
                tc.step         = jc["step"].as_f32(tc.step);
                tc.cb_offset    = jc["offset"].as_u32();
                tc.num_elements = jc["num_elements"].as_u32();

                if (tc.num_elements > 1)
                {
                    // initialise defaults to 0
                    for (u32 d = 0; d < tc.num_elements; ++d)
                        program.constant_defaults[constant_offset + d] = 0.0f;

                    // set from json
                    u32 nd = jc["default"].size();
                    for (u32 d = 0; d < nd; ++d)
                        program.constant_defaults[constant_offset + d] = jc["default"][i].as_f32(0.0f);
                }
                else
                {
                    // intialise from json
                    program.constant_defaults[constant_offset] = jc["default"].as_f32(0.0f);
                }

                constant_offset += tc.num_elements;

                sb_push(program.constants, tc);
            }

            return program;
        }

        void initialise_constant_defaults(shader_handle handle, u32 index, f32* data)
        {
            if (handle >= sb_count(k_pmfx_list))
                return;

            if (index >= sb_count(k_pmfx_list[handle].techniques))
                return;

            memcpy(data, k_pmfx_list[handle].techniques[index].constant_defaults,
                   k_pmfx_list[handle].techniques[index].technique_constant_size);
        }

        technique_constant* get_technique_constants(shader_handle handle, u32 index)
        {
            if (handle >= sb_count(k_pmfx_list))
                return nullptr;

            if (index >= sb_count(k_pmfx_list[handle].techniques))
                return nullptr;

            return k_pmfx_list[handle].techniques[index].constants;
        }
        
        technique_sampler* get_technique_samplers(shader_handle handle, u32 index)
        {
            if (handle >= sb_count(k_pmfx_list))
                return nullptr;
            
            if (index >= sb_count(k_pmfx_list[handle].techniques))
                return nullptr;
            
            return k_pmfx_list[handle].techniques[index].textures;
        }

        u32 get_technique_cbuffer_size(shader_handle handle, u32 index)
        {
            if (handle >= sb_count(k_pmfx_list))
                return 0;

            if (index >= sb_count(k_pmfx_list[handle].techniques))
                return 0;

            return k_pmfx_list[handle].techniques[index].technique_constant_size;
        }

        void set_technique(shader_handle handle, u32 index)
        {
            if (index >= sb_count(k_pmfx_list[handle].techniques))
                return;

            auto& t = k_pmfx_list[handle].techniques[index];

            if (t.stream_out_shader)
            {
                pen::renderer_set_shader(t.stream_out_shader, PEN_SHADER_TYPE_SO);
            }
            else
            {
                pen::renderer_set_shader(t.vertex_shader, PEN_SHADER_TYPE_VS);
                pen::renderer_set_shader(t.pixel_shader, PEN_SHADER_TYPE_PS);
            }

            pen::renderer_set_input_layout(t.input_layout);
        }

        bool set_technique(shader_handle handle, hash_id id_technique, hash_id id_sub_type)
        {
            u32 technique_index = get_technique_index(handle, id_technique, id_sub_type);

            if (!is_valid(technique_index))
                return false;

            set_technique(handle, technique_index);

            return true;
        }

        u32 get_technique_index(shader_handle handle, hash_id id_technique, hash_id id_sub_type)
        {
            u32 num_techniques = sb_count(k_pmfx_list[handle].techniques);
            for (u32 i = 0; i < num_techniques; ++i)
            {
                auto& t = k_pmfx_list[handle].techniques[i];

                if (t.id_name != id_technique)
                    continue;

                if (t.id_sub_type != id_sub_type)
                    continue;

                return i;
            }

            return PEN_INVALID_HANDLE;
        }

        void get_pmfx_info_filename(c8* file_buf, const c8* pmfx_filename)
        {
            pen::string_format(file_buf, 256, "data/pmfx/%s/%s/info.json", pen::renderer_get_shader_platform(),
                               pmfx_filename);
        }

        void release_shader(shader_handle handle)
        {
            k_pmfx_list[handle].filename = nullptr;

            u32 num_techniques = sb_count(k_pmfx_list[handle].techniques);

            for (u32 i = 0; i < num_techniques; ++i)
            {
                auto& t = k_pmfx_list[handle].techniques[i];

                pen::renderer_release_shader(t.pixel_shader, PEN_SHADER_TYPE_PS);
                pen::renderer_release_shader(t.vertex_shader, PEN_SHADER_TYPE_VS);
                pen::renderer_release_input_layout(t.input_layout);
            }
        }

        pmfx_shader load_internal(const c8* filename)
        {
            // load info file for description
            c8 info_file_buf[256];
            get_pmfx_info_filename(info_file_buf, filename);

            // read shader info json
            pmfx_shader new_pmfx;

            new_pmfx.filename    = filename;
            new_pmfx.id_filename = PEN_HASH(filename);

            new_pmfx.info = pen::json::load_from_file(info_file_buf);

            u32       ts;
            pen_error err = pen::filesystem_getmtime(info_file_buf, ts);
            if (err == PEN_ERR_OK)
            {
                new_pmfx.info_timestamp = ts;
            }

            pen::json _techniques = new_pmfx.info["techniques"];

            for (s32 i = 0; i < _techniques.size(); ++i)
            {
                pen::json      t             = _techniques[i];
                shader_program new_technique = load_shader_technique(filename, t, new_pmfx.info);

                sb_push(new_pmfx.techniques, new_technique);
            }

            return new_pmfx;
        }

        shader_handle load_shader(const c8* pmfx_name)
        {
            // return existing
            shader_handle ph = PEN_INVALID_HANDLE;
            if (!pmfx_name)
                return ph;

            u32 num_pmfx = sb_count(k_pmfx_list);

            ph = 0;
            // for (auto& p : s_pmfx_list)
            for (u32 i = 0; i < num_pmfx; ++i)
                if (k_pmfx_list[i].filename == pmfx_name)
                    return ph;
                else
                    ph++;

            pmfx_shader new_pmfx = load_internal(pmfx_name);

            ph = 0;
            // for (auto& p : s_pmfx_list)
            for (u32 i = 0; i < num_pmfx; ++i)
            {
                auto& p = k_pmfx_list[i];
                if (p.filename.length() == 0)
                {
                    p = new_pmfx;
                    return ph;
                }

                ++ph;
            }

            sb_push(k_pmfx_list, new_pmfx);

            // s_pmfx_list.push_back(new_pmfx);
            generate_name_lists();

            return ph;
        }

        shader_handle get_shader_handle(hash_id id_filename)
        {
            u32 num_pmfx = sb_count(k_pmfx_list);

            shader_handle ph = 0;
            // for (auto& p : s_pmfx_list)
            for (u32 i = 0; i < num_pmfx; ++i)
                if (k_pmfx_list[i].id_filename == id_filename)
                    return ph;
                else
                    ph++;

            return PEN_INVALID_HANDLE;
        }

        void poll_for_changes()
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

                    pmtech_dir = pen::str_replace_chars(pmtech_dir, '/', PEN_DIR);

                    shader_compiler_str.append(PEN_PYTHON3);
                    shader_compiler_str.append(pmtech_dir.c_str());
                    shader_compiler_str.append(PEN_SHADER_COMPILE_CMD);

                    dev_console_log_level(dev_ui::CONSOLE_MESSAGE, "[shader compiler cmd] %s", shader_compiler_str.c_str());
                }
                else
                {
                    dev_console_log_level(dev_ui::CONSOLE_ERROR, "%s", "[error] unable to find pmtech dir");
                }
            }

            if (shader_compiler_str == "")
                return;

            shader_handle current_counter = 0;

            u32 num_pmfx = sb_count(k_pmfx_list);

            // for( auto& pmfx_set : s_pmfx_list )
            for (u32 i = 0; i < num_pmfx; ++i)
            {
                auto& pmfx_set = k_pmfx_list[i];

                if (pmfx_set.invalidated)
                {
                    c8 info_file_buf[256];
                    get_pmfx_info_filename(info_file_buf, pmfx_set.filename.c_str());

                    u32       current_ts;
                    pen_error err = pen::filesystem_getmtime(info_file_buf, current_ts);

                    // wait until info is newer than the current info file,
                    // to know compilation is completed.
                    if (err == PEN_ERR_OK && current_ts > pmfx_set.info_timestamp)
                    {
                        // load new one
                        pmfx_shader pmfx_new = load_internal(pmfx_set.filename.c_str());

                        // release existing
                        release_shader(current_counter);

                        // set exisiting to the new one
                        pmfx_set = pmfx_new;

                        // no longer invalidated
                        pmfx_set.invalidated = false;

                        ces::bake_material_handles();
                        generate_name_lists();
                    }
                }
                else
                {
                    pen::json files = pmfx_set.info["files"];

                    s32 num_files = files.size();
                    for (s32 i = 0; i < num_files; ++i)
                    {
                        pen::json file      = files[i];
                        Str       fn        = file["name"].as_str();
                        u32       shader_ts = file["timestamp"].as_u32();
                        u32       current_ts;
                        pen_error err = pen::filesystem_getmtime(fn.c_str(), current_ts);

                        // trigger re-compile if files on disk are newer
                        if (err == PEN_ERR_OK && current_ts > shader_ts)
                        {
                            PEN_SYSTEM(shader_compiler_str.c_str());
                            pmfx_set.invalidated = true;
                        }
                    }
                }

                current_counter++;
            }
        }

        bool has_technique_constants(shader_handle shader, u32 technique_index)
        {
            return get_technique_constants(shader, technique_index);
        }
        
        bool has_technique_samplers(shader_handle shader, u32 technique_index)
        {
            return get_technique_samplers(shader, technique_index);
        }
        
        bool has_technique_params(shader_handle shader, u32 technique_index)
        {
            return get_technique_constants(shader, technique_index) || get_technique_samplers(shader, technique_index);
        }
        
        bool constant_ui(shader_handle shader, u32 technique_index, f32* material_data)
        {
            technique_constant* tc = get_technique_constants(shader, technique_index);
            
            bool rv = false;
            
            if (!tc)
                return rv;
            
            static bool colour_edit[64] = {0};
            u32         num_constants   = sb_count(tc);
            for (u32 i = 0; i < num_constants; ++i)
            {
                f32* f = &material_data[tc[i].cb_offset];
                
                switch (tc[i].widget)
                {
                    case CW_INPUT:
                        rv |= ImGui::InputFloatN(tc[i].name.c_str(), f, tc[i].num_elements, 3, 0);
                        break;
                    case CW_SLIDER:
                        rv |= ImGui::SliderFloatN(tc[i].name.c_str(), f, tc[i].num_elements, tc[i].min, tc[i].max, "%.3f", 1.0f);
                        break;
                    case CW_COLOUR:
                        
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(f[0] * 0.5f, f[1] * 0.5f, f[2] * 0.5f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(f[0], f[1], f[2], 1.0f));
                        
                        if (ImGui::Button(tc[i].name.c_str()))
                        {
                            colour_edit[i] = true;
                            rv = true;
                        }
                        
                        if (colour_edit[i])
                        {
                            ImGui::PushID("col_window");
                            
                            ImGui::Begin(tc[i].name.c_str(), &colour_edit[i]);
                            
                            if (tc[i].num_elements == 3)
                            {
                                rv |= ImGui::ColorPicker3(tc[i].name.c_str(), f);
                            }
                            else
                            {
                                rv |= ImGui::ColorPicker4(tc[i].name.c_str(), f);
                            }
                            
                            ImGui::End();
                            
                            ImGui::PopID();
                        }
                        
                        ImGui::PopStyleColor(2);
                        break;
                }
            }
            
            return rv;
        }
        
        bool texture_ui(shader_handle shader, u32 technique_index, cmp_samplers& samplers)
        {
            technique_sampler* tt = get_technique_samplers(shader, technique_index);
            
            bool rv = false;
            
            if (!tt)
                return false;
            
            u32 num_textures = sb_count(tt);
            
            ImGui::Columns(num_textures);
            
            static bool open_fb = false;
            static s32 select_index = -1;
            
            for(u32 i = 0; i < num_textures; ++i)
            {
                technique_sampler& t = tt[i];
                
                ImGui::Text("unit: %i [%s]", t.unit, t.name.c_str());
                if( ImGui::ImageButton((void*)&samplers.sb[i].handle, ImVec2(64, 64)) )
                {
                    if (select_index == -1)
                    {
                        open_fb = true;
                        select_index = i;
                    }
                }
                
                ImGui::Text("file: %s", put::get_texture_filename(samplers.sb[i].handle).c_str());
                
                ImGui::NextColumn();
            }
            
            if(open_fb)
            {
                const c8* fn = dev_ui::file_browser(open_fb, dev_ui::FB_OPEN);
                if(fn)
                {
                    samplers.sb[select_index].handle = put::load_texture(fn);

                    select_index = -1;
                    
                    rv = true;
                }
            }
            
            ImGui::Columns(1);
            return rv;
        }

        bool show_technique_ui(shader_handle shader,
                               u32 technique_index,
                               f32* material_data,
                               cmp_samplers& samplers)
        {
            bool rv = false;
            rv |= constant_ui(shader, technique_index, material_data);
            rv |= texture_ui(shader, technique_index, samplers);
            
            return rv;
        }
    } // namespace pmfx
} // namespace put
