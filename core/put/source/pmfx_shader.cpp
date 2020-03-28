// pmfx_shader.cpp
// Copyright 2014 - 2019 Alex Dixon.
// License: https://github.com/polymonster/pmtech/blob/master/license.md

#include <vector>

#include "dev_ui.h"
#include "ecs/ecs_resources.h"
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
using namespace ecs;

namespace
{
    const c8* semantic_names[] = {
        "SV_POSITION",
        "POSITION",
        "TEXCOORD",
        "NORMAL",
        "TANGENT",
        "BITANGENT",
		"BLENDWEIGHTS",
        "COLOR",
        "BLENDINDICES"
    };

    shader_program null_shader = {0};

    hash_id id_widgets[] = {
		PEN_HASH("slider"), 
		PEN_HASH("input"), 
		PEN_HASH("colour")
	};
    static_assert(PEN_ARRAY_SIZE(id_widgets) == e_constant_widget::COUNT, "mismatched array size");

    struct pmfx_shader
    {
        hash_id         id_filename = 0;
        Str             filename = nullptr;
        bool            invalidated = false;
        pen::json       info;
        u32             info_timestamp = 0;
        shader_program* techniques = nullptr;
        u32             rebuild_ts = 0;
    };

    pmfx_shader*  s_pmfx_list = nullptr;
    const char**  s_shader_names = nullptr;
    const char*** s_technique_names = nullptr;
    hash_id**     s_technique_id_names = nullptr;
    u32           s_num_shader_names = 0;
} // namespace

namespace put
{
    namespace pmfx
    {
        void generate_name_lists()
        {
            pen::memory_free(s_shader_names);

            for (u32 i = 0; i < s_num_shader_names; ++i)
                sb_free(s_technique_names[i]);

            pen::memory_free(s_technique_names);
            pen::memory_free(s_technique_id_names);

            u32 num_pmfx = sb_count(s_pmfx_list);
            s_technique_names = (const char***)pen::memory_calloc(num_pmfx, sizeof(s_technique_names));
            s_shader_names = (const char**)pen::memory_calloc(num_pmfx, sizeof(s_shader_names));
            s_technique_id_names = (hash_id**)pen::memory_calloc(num_pmfx, sizeof(s_technique_id_names));

            for (u32 i = 0; i < num_pmfx; ++i)
            {
                s_shader_names[i] = s_pmfx_list[i].filename.c_str();

                u32 num_techniques = sb_count(s_pmfx_list[i].techniques);
                for (u32 t = 0; t < num_techniques; ++t)
                {
                    if (s_pmfx_list[i].techniques[t].permutation_id != 0)
                        continue;

                    sb_push(s_technique_names[i], s_pmfx_list[i].techniques[t].name.c_str());
                    sb_push(s_technique_id_names[i], PEN_HASH(s_pmfx_list[i].techniques[t].name.c_str()));
                }
            }

            s_num_shader_names = num_pmfx;
        }

        const c8** get_shader_list(u32& count)
        {
            count = s_num_shader_names;
            return s_shader_names;
        }

        const c8** get_technique_list(u32 shader, u32& count)
        {
            count = sb_count(s_technique_names[shader]);
            return s_technique_names[shader];
        }

        u32 get_technique_list_index(u32 shader, hash_id id_technique)
        {
            u32 num_id = sb_count(s_technique_id_names[shader]);
            for (u32 i = 0; i < num_id; ++i)
            {
                if (s_technique_id_names[shader][i] == id_technique)
                {
                    return i;
                }
            }

            return PEN_INVALID_HANDLE;
        }

        const c8* get_shader_name(u32 shader)
        {
            if (shader >= sb_count(s_pmfx_list))
                return nullptr;

            return s_pmfx_list[shader].filename.c_str();
        }

        const c8* get_technique_name(u32 shader, hash_id id_technique)
        {
            if (shader >= sb_count(s_pmfx_list))
                return nullptr;

            u32 nt = sb_count(s_pmfx_list[shader].techniques);
            for (u32 i = 0; i < nt; ++i)
                if (s_pmfx_list[shader].techniques[i].id_name == id_technique)
                    return s_pmfx_list[shader].techniques[i].name.c_str();

            return nullptr;
        }

        hash_id get_technique_id(u32 shader, u32 technique_index)
        {
            if (shader >= sb_count(s_pmfx_list))
                return 0;

            u32 nt = sb_count(s_pmfx_list[shader].techniques);
            if (technique_index >= nt)
                return 0;

            return s_pmfx_list[shader].techniques[technique_index].id_name;
        }

        void get_link_params_constants(pen::shader_link_params& link_params, const pen::json& j_info,
                                       const pen::json& j_technique)
        {
            u32 num_constants = j_technique["cbuffers"].size() + j_technique["texture_sampler_bindings"].size();

            link_params.num_constants = num_constants;

            link_params.constants =
                (pen::constant_layout_desc*)pen::memory_alloc(sizeof(pen::constant_layout_desc) * num_constants);

            // per pmfx constants
            // .. per technique constants go into: material_data(7), todo rename to techhnique_data

            u32       cc = 0;
            pen::json j_cbuffers = j_technique["cbuffers"];
            for (s32 i = 0; i < j_cbuffers.size(); ++i)
            {
                pen::json cbuf = j_cbuffers[i];
                Str       name_str = cbuf["name"].as_str();

                u32 name_len = name_str.length();

                link_params.constants[cc].name = new c8[name_len + 1];

                memcpy(link_params.constants[cc].name, name_str.c_str(), name_len);

                link_params.constants[cc].name[name_len] = '\0';

                link_params.constants[cc].location = cbuf["location"].as_u32();

                link_params.constants[cc].type = pen::CT_CBUFFER;

                cc++;
            }

            static Str sampler_type_names[] = {"texture_2d", "texture_3d", "texture_cube", "texture_2dms",
                                               "texture_2d_array"};

            const pen::json& j_samplers = j_technique["texture_sampler_bindings"];

            for (s32 i = 0; i < j_samplers.size(); ++i)
            {
                pen::json sampler = j_samplers[i];

                Str name_str = sampler["name"].as_str();
                if (!sampler["name"].as_cstr())
                    name_str = sampler.key();

                u32 name_len = name_str.length();

                link_params.constants[cc].name = (c8*)pen::memory_alloc(name_len + 1);

                memcpy(link_params.constants[cc].name, name_str.c_str(), name_len);

                link_params.constants[cc].name[name_len] = '\0';

                link_params.constants[cc].location = sampler["unit"].as_u32();

                for (u32 i = 0; i < PEN_ARRAY_SIZE(sampler_type_names); ++i)
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

        void get_input_layout_params(pen::input_layout_creation_params& ilp, pen::json& j_techique)
        {
            u32 vertex_elements = j_techique["vs_inputs"].size();
            ilp.num_elements = vertex_elements;

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

                    u32 num_elements = vj["num_elements"].as_u32();
                    u32 elements_size = vj["element_size"].as_u32();

                    static const s32 float_formats[4] = {
						PEN_VERTEX_FORMAT_FLOAT1, 
						PEN_VERTEX_FORMAT_FLOAT2,
						PEN_VERTEX_FORMAT_FLOAT3, 
						PEN_VERTEX_FORMAT_FLOAT4
					};

                    static const s32 byte_formats[4] = {
						PEN_VERTEX_FORMAT_UNORM1, 
						PEN_VERTEX_FORMAT_UNORM2,
						PEN_VERTEX_FORMAT_UNORM2, 
						PEN_VERTEX_FORMAT_UNORM4
					};

                    const s32* fomats = float_formats;

                    if (elements_size == 1)
                        fomats = byte_formats;

                    ilp.input_layout[input_index].semantic_index = vj["semantic_index"].as_u32();
                    ilp.input_layout[input_index].format = fomats[num_elements - 1];
                    ilp.input_layout[input_index].semantic_name = semantic_names[vj["semantic_id"].as_u32()];
                    ilp.input_layout[input_index].input_slot = l;
                    ilp.input_layout[input_index].aligned_byte_offset = vj["offset"].as_u32();
                    ilp.input_layout[input_index].input_slot_class = layouts[l].iclass;
                    ilp.input_layout[input_index].instance_data_step_rate = layouts[l].step_rate;

                    ++input_index;
                }
            }
        }

        shader_program load_shader_technique(const c8* fx_filename, pen::json& j_technique, pen::json& j_info)
        {
            shader_program program = {0};

            Str name = j_technique["name"].as_str();

            program.name = name;
            program.id_name = PEN_HASH(name.c_str());
            program.id_sub_type = PEN_HASH("");
            program.permutation_id = j_technique["permutation_id"].as_u32();
            program.permutation_option_mask = j_technique["permutation_option_mask"].as_u32();

            const c8* sfp = pen::renderer_get_shader_platform();

            // compute shader
            Str cs_name = j_technique["cs"].as_str();
            if (!cs_name.empty())
            {
                c8* cs_file_buf = (c8*)pen::memory_alloc(256);
                Str cs_filename_str = j_technique["cs_file"].as_str();
                pen::string_format(cs_file_buf, 256, "data/pmfx/%s/%s/%s", sfp, fx_filename, cs_filename_str.c_str());

                pen::shader_load_params cs_slp;
                cs_slp.type = PEN_SHADER_TYPE_CS;

                pen_error err = pen::filesystem_read_file_to_buffer(cs_file_buf, &cs_slp.byte_code, cs_slp.byte_code_size);

                pen::memory_free(cs_file_buf);

                if (err != PEN_ERR_OK)
                {
                    pen::memory_free(cs_slp.byte_code);
                    return program;
                }

                program.compute_shader = pen::renderer_load_shader(cs_slp);

                return program;
            }

            // vertex shader
            c8* vs_file_buf = (c8*)pen::memory_alloc(256);
            Str vs_filename_str = j_technique["vs_file"].as_str();
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
            bool stream_out = j_technique["stream_out"].as_bool();
            if (stream_out)
            {
                vs_slp.type = PEN_SHADER_TYPE_SO;

                u32 num_vertex_outputs = j_technique["vs_outputs"].size();

                u32                         decl_size_bytes = sizeof(pen::stream_out_decl_entry) * num_vertex_outputs;
                pen::stream_out_decl_entry* so_decl = (pen::stream_out_decl_entry*)pen::memory_alloc(decl_size_bytes);

                pen::shader_link_params slp;
                slp.stream_out_shader = program.stream_out_shader;
                slp.pixel_shader = 0;
                slp.vertex_shader = 0;

                slp.stream_out_names = new c8*[num_vertex_outputs];
                slp.num_stream_out_names = num_vertex_outputs;

                for (u32 vo = 0; vo < num_vertex_outputs; ++vo)
                {
                    pen::json voj = j_technique["vs_outputs"][vo];

                    so_decl[vo].stream = 0;
                    so_decl[vo].semantic_name = semantic_names[voj["semantic_id"].as_u32()];
                    so_decl[vo].semantic_index = voj["semantic_index"].as_u32();
                    so_decl[vo].start_component = 0;
                    so_decl[vo].component_count = voj["num_elements"].as_u32();
                    so_decl[vo].output_slot = 0;

                    Str gl_name = voj["name"].as_str();
                    gl_name.append("_vs_output");

                    u32 name_len = gl_name.length();
                    slp.stream_out_names[vo] = new c8[name_len + 1];
                    memcpy(slp.stream_out_names[vo], gl_name.c_str(), name_len);
                    slp.stream_out_names[vo][name_len] = '\0';
                }

                vs_slp.so_decl_entries = so_decl;
                vs_slp.so_num_entries = num_vertex_outputs;

                program.stream_out_shader = pen::renderer_load_shader(vs_slp);
                program.vertex_shader = 0;
                program.pixel_shader = 0;

                pen::memory_free(vs_slp.so_decl_entries);

                get_link_params_constants(slp, j_info, j_technique);

                slp.stream_out_shader = program.stream_out_shader;
                slp.vertex_shader = 0;
                slp.pixel_shader = 0;

                program.program_index = pen::renderer_link_shader_program(slp);

                // create input layout from json
                pen::input_layout_creation_params ilp;
                ilp.vs_byte_code = vs_slp.byte_code;
                ilp.vs_byte_code_size = vs_slp.byte_code_size;

                get_input_layout_params(ilp, j_technique);

                program.input_layout = pen::renderer_create_input_layout(ilp);

                return program;
            }

            // traditional vs / ps combo
            program.vertex_shader = pen::renderer_load_shader(vs_slp);
            pen::memory_free(vs_slp.so_decl_entries);

            // pixel shader
            c8* ps_file_buf = (c8*)pen::memory_alloc(256);
            Str ps_filename_str = j_technique["ps_file"].as_str();

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
            ilp.vs_byte_code = vs_slp.byte_code;
            ilp.vs_byte_code_size = vs_slp.byte_code_size;

            get_input_layout_params(ilp, j_technique);

            program.input_layout = pen::renderer_create_input_layout(ilp);

            pen::memory_free(ilp.input_layout);

            // link the shader to allow opengl to match d3d constant and texture bindings
            pen::shader_link_params link_params;
            link_params.input_layout = program.input_layout;
            link_params.vertex_shader = program.vertex_shader;
            link_params.pixel_shader = program.pixel_shader;
            link_params.stream_out_shader = 0;
            link_params.stream_out_names = nullptr;

            get_link_params_constants(link_params, j_info, j_technique);

            program.program_index = pen::renderer_link_shader_program(link_params);

            pen::memory_free(link_params.constants);

            // generate technique textures meta data
            program.textures = nullptr;
            u32 num_technique_textues = j_technique["texture_samplers"].size();

            for (u32 i = 0; i < num_technique_textues; ++i)
            {
                pen::json jt = j_technique["texture_samplers"][i];

                technique_sampler tt;

                tt.name = jt.name();
                tt.id_name = PEN_HASH(tt.name);
                tt.sampler_state_name = jt["sampler_state"].as_str();
                tt.unit = jt["unit"].as_u32();
                tt.type_name = jt["type"].as_str();
                tt.default_name = jt["default"].as_str();
                tt.filename = tt.default_name;
                tt.handle = put::load_texture(tt.filename.c_str());

                extern u32 sampler_bind_flags_from_json(const pen::json& sampler_binding);
                tt.bind_flags = sampler_bind_flags_from_json(jt);

                sb_push(program.textures, tt);
            }

            // generate technique constants meta data
            program.constants = nullptr;
            u32 num_technique_constants = j_technique["constants"].size();
            program.technique_constant_size = j_technique["constants_size_bytes"].as_u32(0);

            if (program.technique_constant_size > 0)
                program.constant_defaults = (f32*)pen::memory_alloc(program.technique_constant_size);

            u32 constant_offset = 0;
            for (u32 i = 0; i < num_technique_constants; ++i)
            {
                pen::json jc = j_technique["constants"][i];

                technique_constant tc;
                tc.name = jc.name();
                tc.id_name = PEN_HASH(tc.name);

                hash_id widget = jc["widget"].as_hash_id(PEN_HASH("input"));
                for (u32 j = 0; j < e_constant_widget::COUNT; ++j)
                {
                    if (widget == id_widgets[j])
                    {
                        tc.widget = j;
                        break;
                    }
                }

                tc.min = jc["min"].as_f32(tc.min);
                tc.max = jc["max"].as_f32(tc.max);
                tc.step = jc["step"].as_f32(tc.step);
                tc.cb_offset = jc["offset"].as_u32();
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

            // generate permutation metadata
            program.permutations = nullptr;
            u32 num_permutations = j_technique["permutations"].size();

            for (u32 i = 0; i < num_permutations; ++i)
            {
                technique_permutation tp;
                tp.name = j_technique["permutations"][i].key();
                tp.val = j_technique["permutations"][i]["val"].as_u32();

                hash_id id_widget = j_technique["permutations"][i]["type"].as_hash_id();

                static hash_id id_checkbox = PEN_HASH("checkbox");
                static hash_id id_input = PEN_HASH("input");

                if (id_widget == id_checkbox)
                {
                    tp.widget = e_permutation_widget::checkbox;
                }
                else if (id_widget == id_input)
                {
                    tp.widget = e_permutation_widget::input;
                }

                sb_push(program.permutations, tp);
            }

            return program;
        }

        void initialise_constant_defaults(u32 shader, u32 technique_index, f32* data)
        {
            if (shader >= sb_count(s_pmfx_list))
                return;

            if (technique_index >= sb_count(s_pmfx_list[shader].techniques))
                return;

            memcpy(data, s_pmfx_list[shader].techniques[technique_index].constant_defaults,
                   s_pmfx_list[shader].techniques[technique_index].technique_constant_size);
        }

        void initialise_sampler_defaults(u32 shader, u32 technique_index, sampler_set& samplers)
        {
            if (shader >= sb_count(s_pmfx_list))
                return;

            if (technique_index >= sb_count(s_pmfx_list[shader].techniques))
                return;

            if (pmfx::has_technique_samplers(shader, technique_index))
            {
                pmfx::technique_sampler* ts = pmfx::get_technique_samplers(shader, technique_index);

                static hash_id id_wrap_linear = PEN_HASH("wrap_linear");

                u32 num_tt = sb_count(ts);
                for (u32 i = 0; i < num_tt; ++i)
                {
                    sampler_binding sb;
                    sb.handle = ts[i].handle;
                    sb.sampler_unit = ts[i].unit;
                    sb.bind_flags = pen::TEXTURE_BIND_PS;
                    sb.id_sampler_state = id_wrap_linear;
                    sb.sampler_state = pmfx::get_render_state(id_wrap_linear, e_render_state::sampler);

                    samplers.sb[i] = sb;
                }
            }
        }

        technique_constant* get_technique_constants(u32 shader, u32 technique_index)
        {
            if (shader >= sb_count(s_pmfx_list))
                return nullptr;

            if (technique_index >= sb_count(s_pmfx_list[shader].techniques))
                return nullptr;

            return s_pmfx_list[shader].techniques[technique_index].constants;
        }

        technique_constant* get_technique_constant(hash_id id_constant, u32 shader, u32 technique_index)
        {
            technique_constant* tc = get_technique_constants(shader, technique_index);

            for (u32 i = 0; i < sb_count(tc); ++i)
            {
                if (tc[i].id_name == id_constant)
                {
                    return &tc[i];
                }
            }

            return nullptr;
        }

        technique_sampler* get_technique_samplers(u32 shader, u32 technique_index)
        {
            if (shader >= sb_count(s_pmfx_list))
                return nullptr;

            if (technique_index >= sb_count(s_pmfx_list[shader].techniques))
                return nullptr;

            return s_pmfx_list[shader].techniques[technique_index].textures;
        }

        technique_sampler* get_technique_sampler(hash_id id_sampler, u32 shader, u32 technique_index)
        {
            technique_sampler* ts = get_technique_samplers(shader, technique_index);

            for (u32 i = 0; i < sb_count(ts); ++i)
            {
                if (ts[i].id_name == id_sampler)
                {
                    return &ts[i];
                }
            }

            return nullptr;
        }

        technique_permutation* get_technique_permutations(u32 shader, u32 technique_index)
        {
            if (shader >= sb_count(s_pmfx_list))
                return nullptr;

            if (technique_index >= sb_count(s_pmfx_list[shader].techniques))
                return nullptr;

            return s_pmfx_list[shader].techniques[technique_index].permutations;
        }

        u32 get_technique_cbuffer_size(u32 shader, u32 technique_index)
        {
            if (shader >= sb_count(s_pmfx_list))
                return 0;

            if (technique_index >= sb_count(s_pmfx_list[shader].techniques))
                return 0;

            return s_pmfx_list[shader].techniques[technique_index].technique_constant_size;
        }

        void set_technique(u32 shader, u32 technique_index)
        {
            if (technique_index >= sb_count(s_pmfx_list[shader].techniques))
                return;

            auto& t = s_pmfx_list[shader].techniques[technique_index];

            if (t.stream_out_shader)
            {
                // stream out gs
                pen::renderer_set_shader(t.stream_out_shader, PEN_SHADER_TYPE_SO);
            }
            else if (t.compute_shader)
            {
                // cs
                pen::renderer_set_shader(t.compute_shader, PEN_SHADER_TYPE_CS);
            }
            else
            {
                // traditional ps / vs combo.. ps can be null
                pen::renderer_set_shader(t.vertex_shader, PEN_SHADER_TYPE_VS);
                pen::renderer_set_shader(t.pixel_shader, PEN_SHADER_TYPE_PS);
            }

            pen::renderer_set_input_layout(t.input_layout);
        }

        bool set_technique_perm(u32 shader, hash_id id_technique, u32 permutation)
        {
            u32 technique_index = get_technique_index_perm(shader, id_technique, permutation);

            if (!is_valid(technique_index))
                return false;

            set_technique(shader, technique_index);

            return true;
        }

        u32 get_technique_index_perm(u32 shader, hash_id id_technique, u32 permutation)
        {
            u32 num_techniques = sb_count(s_pmfx_list[shader].techniques);
            for (u32 i = 0; i < num_techniques; ++i)
            {
                auto& t = s_pmfx_list[shader].techniques[i];

                if (t.id_name != id_technique)
                    continue;

                u32 masked_permutation = permutation & t.permutation_option_mask;

                if (t.permutation_id != masked_permutation)
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

        void release_shader(u32 shader)
        {
            s_pmfx_list[shader].filename = nullptr;

            u32 num_techniques = sb_count(s_pmfx_list[shader].techniques);

            for (u32 i = 0; i < num_techniques; ++i)
            {
                auto& t = s_pmfx_list[shader].techniques[i];

                pen::renderer_release_shader(t.pixel_shader, PEN_SHADER_TYPE_PS);
                pen::renderer_release_shader(t.vertex_shader, PEN_SHADER_TYPE_VS);
                pen::renderer_release_input_layout(t.input_layout);
            }
        }

        bool pmfx_ready(const c8* filename)
        {
            static c8 info_file_buf[256];
            get_pmfx_info_filename(info_file_buf, filename);
            pen::json j = pen::json::load_from_file(info_file_buf);
            if (j.is_null())
                return false;

            return true;
        }

        pmfx_shader load_internal(const c8* filename)
        {
            // load info file for description
            static c8 info_file_buf[256];
            get_pmfx_info_filename(info_file_buf, filename);

            // read shader info json
            pmfx_shader new_pmfx;

            new_pmfx.filename = filename;
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
                pen::json      t = _techniques[i];
                shader_program new_technique = load_shader_technique(filename, t, new_pmfx.info);

                sb_push(new_pmfx.techniques, new_technique);
            }

            return new_pmfx;
        }

        u32 load_shader(const c8* pmfx_name)
        {
            // return existing
            u32 ph = PEN_INVALID_HANDLE;
            if (!pmfx_name)
                return ph;

            u32 num_pmfx = sb_count(s_pmfx_list);

            ph = 0;
            for (u32 i = 0; i < num_pmfx; ++i)
                if (s_pmfx_list[i].filename == pmfx_name)
                    return ph;
                else
                    ph++;

            pmfx_shader new_pmfx = load_internal(pmfx_name);

            // check shader worked
            if (new_pmfx.techniques == nullptr)
                return PEN_INVALID_HANDLE;

            ph = 0;
            for (u32 i = 0; i < num_pmfx; ++i)
            {
                auto& p = s_pmfx_list[i];
                if (p.filename.length() == 0)
                {
                    p = new_pmfx;
                    return ph;
                }

                ++ph;
            }

            sb_push(s_pmfx_list, new_pmfx);

            generate_name_lists();

            return ph;
        }

        u32 get_shader_handle(hash_id id_filename)
        {
            u32 num_pmfx = sb_count(s_pmfx_list);

            u32 ph = 0;
            // for (auto& p : s_pmfx_list)
            for (u32 i = 0; i < num_pmfx; ++i)
                if (s_pmfx_list[i].id_filename == id_filename)
                    return ph;
                else
                    ph++;

            return PEN_INVALID_HANDLE;
        }

        void poll_for_changes()
        {
            PEN_HOTLOADING_ENABLED;

            static c8 info_file_buf[256];
            
            Str shader_compiler_str = put::get_build_cmd();
            shader_compiler_str.append("-pmfx");

            u32 current_counter = 0;

            u32 num_pmfx = sb_count(s_pmfx_list);
            u32* reload_list = nullptr;
            
            for (u32 i = 0; i < num_pmfx; ++i)
            {
                auto& pmfx_set = s_pmfx_list[i];

                if (pmfx_set.invalidated)
                {
                    get_pmfx_info_filename(info_file_buf, pmfx_set.filename.c_str());

                    u32       current_ts;
                    pen_error err = pen::filesystem_getmtime(info_file_buf, current_ts);

                    // wait until info is newer than the current info file,
                    // to know compilation is completed.
                    if (err == PEN_ERR_OK && current_ts >= pmfx_set.rebuild_ts)
                    {
                        bool complete = pmfx_ready(pmfx_set.filename.c_str());
                        if (complete)
                        {
                            sb_push(reload_list, i);
                            pmfx_set.invalidated = false;
                        }
                    }
                }
                else
                {
                    pen::json files = pmfx_set.info["files"];

                    s32 num_files = files.size();
                    for (s32 i = 0; i < num_files; ++i)
                    {
                        pen::json file = files[i];
                        Str       fn = file["name"].as_str();
                        
                        u32 dep_ts = 0;
                        get_pmfx_info_filename(info_file_buf, pmfx_set.filename.c_str());
                        if(pen::filesystem_getmtime(info_file_buf, dep_ts) == PEN_ERR_OK)
                        {
                            u32 input_ts = 0;
                            if(pen::filesystem_getmtime(fn.c_str(), input_ts) == PEN_ERR_OK)
                            {
                                if(input_ts > dep_ts)
                                {
                                    put::trigger_hot_loader(shader_compiler_str);
                                    pmfx_set.invalidated = true;
                                    pmfx_set.rebuild_ts = dep_ts;
                                }
                            }
                        }
                    }
                }

                current_counter++;
            }
            
            u32 num_reload = sb_count(reload_list);
            if(num_reload)
            {                
                // load modified
                for(u32 i = 0; i < num_reload; ++i)
                {
                    auto& pmfx_set = s_pmfx_list[reload_list[i]];
                    pmfx_shader pmfx_new = load_internal(pmfx_set.filename.c_str());
                    release_shader(current_counter);
                    pmfx_set = pmfx_new;
                }
                
                // fixup resources / references
                ecs::bake_material_handles();
                generate_name_lists();
        
                sb_free(reload_list);
            }
        }

        bool has_technique_permutations(u32 shader, u32 technique_index)
        {
            return get_technique_permutations(shader, technique_index);
        }

        bool has_technique_constants(u32 shader, u32 technique_index)
        {
            return get_technique_constants(shader, technique_index);
        }

        bool has_technique_samplers(u32 shader, u32 technique_index)
        {
            return get_technique_samplers(shader, technique_index);
        }

        bool has_technique_params(u32 shader, u32 technique_index)
        {
            return get_technique_constants(shader, technique_index) || get_technique_samplers(shader, technique_index) ||
                   get_technique_permutations(shader, technique_index);
        }

        bool permutation_ui(u32 shader, u32 technique_index, u32* permutation_flags)
        {
            technique_permutation* tp = get_technique_permutations(shader, technique_index);

            bool rv = false;

            if (!tp)
            {
                return rv;
            }

            ImGui::Separator();
            ImGui::Text("Permutation");
            ImGui::Separator();

            u32 num_permutations = sb_count(tp);
            for (u32 i = 0; i < num_permutations; ++i)
            {
                switch (tp[i].widget)
                {
                    case e_permutation_widget::checkbox:
                        rv |= ImGui::CheckboxFlags(tp[i].name.c_str(), permutation_flags, tp[i].val);
                        break;
                    default:
                        break;
                }

                if (i % 3 != 0 && i < num_permutations - 1)
                    ImGui::SameLine();
            }

            return rv;
        }

        bool constant_ui(u32 shader, u32 technique_index, f32* material_data)
        {
            technique_constant* tc = get_technique_constants(shader, technique_index);

            bool rv = false;

            if (!tc)
                return rv;

            ImGui::Separator();
            ImGui::Text("Constants");
            ImGui::Separator();

            static bool colour_edit[64] = {0};
            u32         num_constants = sb_count(tc);
            for (u32 i = 0; i < num_constants; ++i)
            {
                f32* f = &material_data[tc[i].cb_offset];

                ImGui::PushID(f);

                switch (tc[i].widget)
                {
                    case e_constant_widget::input:
                        if (ImGui::Button(ICON_FA_SLIDERS))
                        {
                            tc[i].widget = e_constant_widget::slider;
                        }
                        ImGui::SameLine();
                        rv |= ImGui::InputFloatN(tc[i].name.c_str(), f, tc[i].num_elements, 3, 0);
                        break;
                    case e_constant_widget::slider:
                        if (ImGui::Button(ICON_FA_PENCIL))
                        {
                            tc[i].widget = e_constant_widget::input;
                        }
                        ImGui::SameLine();
                        rv |= ImGui::SliderFloatN(tc[i].name.c_str(), f, tc[i].num_elements, tc[i].min, tc[i].max, "%.3f",
                                                  1.0f);
                        break;
                    case e_constant_widget::colour:

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
                ImGui::PopID();
            }

            return rv;
        }

        bool texture_ui(u32 shader, u32 technique_index, cmp_samplers& samplers)
        {
            technique_sampler* tt = get_technique_samplers(shader, technique_index);

            bool rv = false;

            if (!tt)
                return false;

            ImGui::Separator();
            ImGui::Text("Texture Samplers");
            ImGui::Separator();

            u32 num_textures = sb_count(tt);

            ImGui::Columns(num_textures);

            static bool open_fb = false;
            static s32  select_index = -1;

            // sampler state list
            c8**     sampler_state_list = pmfx::get_render_state_list(pmfx::e_render_state::sampler);
            hash_id* sampler_state_id_list = pmfx::get_render_state_id_list(pmfx::e_render_state::sampler);

            for (u32 i = 0; i < num_textures; ++i)
            {
                technique_sampler& t = tt[i];

                ImGui::Text("unit: %i [%s]", t.unit, t.name.c_str());

                if (ImGui::ImageButton(IMG(samplers.sb[i].handle), ImVec2(64, 64)))
                {
                    if (!open_fb)
                    {
                        open_fb = true;
                        select_index = i;
                    }
                }

                ImGui::Text("file: %s", put::get_texture_filename(samplers.sb[i].handle).c_str());

                s32 ss_index = -1;
                for (u32 j = 0; j < sb_count(sampler_state_id_list); ++j)
                {
                    if (sampler_state_id_list[j] == samplers.sb[i].id_sampler_state)
                    {
                        ss_index = j;
                        break;
                    }
                }

                if (ImGui::Combo("sampler state", &ss_index, &sampler_state_list[0], sb_count(sampler_state_list)))
                {
                    samplers.sb[i].id_sampler_state = sampler_state_id_list[ss_index];
                    rv = true;
                }

                ImGui::NextColumn();
            }

            sb_free(sampler_state_list);

            if (open_fb)
            {
                const c8* fn = dev_ui::file_browser(open_fb, dev_ui::e_file_browser_flags::open);
                if (fn)
                {
                    samplers.sb[select_index].handle = put::load_texture(fn);

                    select_index = -1;

                    rv = true;
                }
            }

            ImGui::Columns(1);
            return rv;
        }

        bool show_technique_ui(u32 shader, u32 technique_index, f32* material_data, sampler_set& samplers, u32* permutation)
        {
            bool rv = false;
            rv |= permutation_ui(shader, technique_index, permutation);
            rv |= constant_ui(shader, technique_index, material_data);
            rv |= texture_ui(shader, technique_index, samplers);

            return rv;
        }
    } // namespace pmfx
} // namespace put
