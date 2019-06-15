// loader.cpp
// Copyright 2014 - 2019 Alex Dixon.
// License: https://github.com/polymonster/pmtech/blob/master/license.md

#include "loader.h"
#include "console.h"
#include "data_struct.h"
#include "dev_ui.h"
#include "file_system.h"
#include "hash.h"
#include "memory.h"
#include "pen.h"
#include "pen_json.h"
#include "pen_string.h"
#include "renderer.h"
#include "str/Str.h"
#include "str_utilities.h"

#include <fstream>
#include <vector>

using namespace put;

namespace
{
    enum texture_type
    {
        DDS_RGBA = 0x01,
        DDS_BC = 0x04,
        DDS_R32_FLOAT = 114,
        DDS_DX10 = PEN_FOURCC('D', 'X', '1', '0')
    };

    enum compression_format
    {
        BC1 = PEN_FOURCC('D', 'X', 'T', '1'),
        BC2 = PEN_FOURCC('D', 'X', 'T', '3'),
        BC3 = PEN_FOURCC('D', 'X', 'T', '5'),
        BC4 = PEN_FOURCC('A', 'T', 'I', '1'),
        BC5 = PEN_FOURCC('A', 'T', 'I', '2'),
    };

    enum ddpf_flags
    {
        DDPF_ALPHAPIXELS = 0x1,
        DDPF_ALPHA = 0x2,
        DDPF_FOURCC = 0x4,
        DDPF_RGB = 0x40,
        DDPF_YUV = 0x200,
        DDPF_LUMINANCE = 0x20000
    };

    enum dds_flags
    {
        DDS_CAPS = 0x1,
        DDS_HEIGHT = 0x2,
        DDS_WIDTH = 0x4,
        DDS_PITCH = 0x8,
        DDS_PIXELFORMAT = 0x1000,
        DDS_MIPMAPCOUNT = 0x20000,
        DDS_LINEARSIZE = 0x80000,
        DDS_DEPTH = 0x8000000,

        DDSCAPS_COMPLEX = 0x8,
        DDSCAPS_TEXTURE = 0x1000,
        DDSCAPS_MIPMAP = 0x400000,

        DDSCAPS2_CUBEMAP = 0x200,
        DDSCAPS2_CUBEMAP_POSITIVEX = 0x400,
        DDSCAPS2_CUBEMAP_NEGATIVEX = 0x800,
        DSCAPS2_CUBEMAP_POSITIVEY = 0x1000,
        DDSCAPS2_CUBEMAP_NEGATIVEY = 0x2000,
        DSCAPS2_CUBEMAP_POSITIVEZ = 0x4000,
        DDSCAPS2_CUBEMAP_NEGATIVEZ = 0x8000,
        DDSCAPS2_VOLUME = 0x200000,

        DDS_CUBEMAP_ALLFACES = (DDSCAPS2_CUBEMAP_POSITIVEX | DDSCAPS2_CUBEMAP_NEGATIVEX | DSCAPS2_CUBEMAP_POSITIVEY |
                                DDSCAPS2_CUBEMAP_NEGATIVEY | DSCAPS2_CUBEMAP_POSITIVEZ | DDSCAPS2_CUBEMAP_NEGATIVEZ)
    };

    // dds documentation on MSDN defines all these data types as ulongs.. but they are only 4 bytes in actual data..
    struct ddspf
    {
        u32 size;
        u32 flags;
        u32 four_cc;
        u32 rgb_bit_count;
        u32 r_mask;
        u32 g_mask;
        u32 b_mask;
        u32 a_mask;
    };

    struct dds_header
    {
        u32   magic;
        u32   size;
        u32   flags;
        u32   height;
        u32   width;
        u32   pitch_or_linear_size;
        u32   depth;
        u32   mip_map_count;
        u32   reserved[11];
        ddspf pixel_format;
        u32   caps;
        u32   caps2;
        u32   caps3;
        u32   caps4;
        u32   reserved2;
    };

    struct dx10_header
    {
        u32 dxgi_format;
        u32 resource_dimension;
        u32 misc_flag;
        u32 array_size;
        u32 misc_flags2;
    };

    struct texture_reference
    {
        hash_id                      id_name;
        Str                          filename;
        u32                          handle;
        pen::texture_creation_params tcp;
    };

    struct file_watch
    {
        hash_id              id_name;
        Str                  filename;
        pen::json            dependencies;
        bool                 invalidated = false;
        std::vector<hash_id> changes;

        void (*build_callback)();
        void (*hotload_callback)(std::vector<hash_id>& dirty);
    };

    // static vars
    std::vector<file_watch*>       k_file_watches;
    std::vector<texture_reference> k_texture_references;

    u32 calc_level_size(u32 width, u32 height, bool compressed, u32 block_size)
    {
        if (compressed)
        {
            u32 block_width = max<u32>(1, ((width + 3) / 4));
            u32 block_height = max<u32>(1, ((width + 3) / 4));
            return block_width * block_height * block_size;
        }

        return width * height * block_size;
    }

    u32 dxgi_format_to_texture_format(const dx10_header* dxh, u32& block_size)
    {
        switch (dxh->dxgi_format)
        {
            case 2:
                block_size = 16;
                return PEN_TEX_FORMAT_R32G32B32A32_FLOAT;
            case 16:
                block_size = 8;
                return PEN_TEX_FORMAT_R32G32_FLOAT;
        }

        PEN_ASSERT_MSG(0, "Unsupported Image Format");
        return 0;
    }

    u32 dds_pixel_format_to_texture_format(const dds_header* ddsh, bool& compressed, u32& block_size,
                                           bool& dx10_header_present)
    {
        const ddspf& pixel_format = ddsh->pixel_format;

        dx10_header_present = false;
        compressed = false;

        if (pixel_format.four_cc)
        {
            switch (pixel_format.four_cc)
            {
                case BC1:
                    compressed = true;
                    block_size = 8;
                    return PEN_TEX_FORMAT_BC1_UNORM;
                case BC2:
                    block_size = 16;
                    compressed = true;
                    return PEN_TEX_FORMAT_BC2_UNORM;
                case BC3:
                    block_size = 16;
                    compressed = true;
                    return PEN_TEX_FORMAT_BC3_UNORM;
                case BC4:
                    compressed = true;
                    block_size = 8;
                    return PEN_TEX_FORMAT_BC4_UNORM;
                case BC5:
                    compressed = true;
                    block_size = 16;
                    return PEN_TEX_FORMAT_BC5_UNORM;
                case DDS_R32_FLOAT:
                    block_size = 4;
                    return PEN_TEX_FORMAT_R32_FLOAT;
                case DDS_DX10:
                    dx10_header_present = true;
                    return DDS_DX10;
            }
        }
        else
        {
            u32 rgba = (pixel_format.r_mask | pixel_format.g_mask | pixel_format.b_mask | pixel_format.a_mask);
            if (rgba == 0xffffffff)
            {
                block_size = pixel_format.size / 8;
                return PEN_TEX_FORMAT_BGRA8_UNORM;
            }
            else if (rgba == 0xffffff)
            {
                block_size = pixel_format.size / 8;
                return PEN_TEX_FORMAT_BGRA8_UNORM;
            }
        }

        // supported formats are RGBA, BC1-BC5
        PEN_ASSERT_MSG(0, "Unsupported Image Format");
        return 0;
    }

    ddspf dds_pixel_format_from_texture_format(u32 fmt)
    {
        ddspf pf = {0};
        pf.size = 32;

        switch (fmt)
        {
            case PEN_TEX_FORMAT_R32_FLOAT:
                pf.flags |= DDPF_FOURCC;
                pf.four_cc = DDS_R32_FLOAT;
                break;
            case PEN_TEX_FORMAT_BGRA8_UNORM:
            case PEN_TEX_FORMAT_RGBA8_UNORM:
                pf.size = 32;
                pf.a_mask = 0xffffffff;
                pf.r_mask = 0xffffffff;
                pf.g_mask = 0xffffffff;
                pf.b_mask = 0xffffffff;
                pf.rgb_bit_count = 32;
                break;
        }

        return pf;
    }

    u32 load_texture_internal(const c8* filename, hash_id hh, pen::texture_creation_params& tcp)
    {
        // load a texture file from disk.
        void* file_data = nullptr;
        u32   file_data_size = 0;

        u32 pen_err = pen::filesystem_read_file_to_buffer(filename, &file_data, file_data_size);

        if (pen_err != PEN_ERR_OK)
        {
            dev_console_log_level(dev_ui::CONSOLE_ERROR, "[error] texture - unabled to find file: %s", filename);
            pen::memory_free(file_data);
            return 0;
        }

        // parse dds header
        dds_header* ddsh = (dds_header*)file_data;

        bool dx10_header_present;
        bool compressed;
        u32  block_size;

        u32 format = dds_pixel_format_to_texture_format(ddsh, compressed, block_size, dx10_header_present);

        u8* top_image_start = (u8*)file_data + sizeof(dds_header);
        if (dx10_header_present)
        {
            dx10_header* dxh = (dx10_header*)top_image_start;

            format = dxgi_format_to_texture_format(dxh, block_size);

            top_image_start += sizeof(dx10_header);
        }

        // fill out texture_creation_params
        tcp.width = ddsh->width;
        tcp.height = ddsh->height;
        tcp.format = format;
        tcp.num_mips = std::max<u32>(ddsh->mip_map_count, 1);
        tcp.num_arrays = 1;
        tcp.sample_count = 1;
        tcp.sample_quality = 0;
        tcp.usage = PEN_USAGE_DEFAULT;
        tcp.bind_flags = PEN_BIND_SHADER_RESOURCE;
        tcp.cpu_access_flags = 0;
        tcp.flags = 0;
        tcp.block_size = block_size;
        tcp.pixels_per_block = compressed ? 4 : 1;
        tcp.collection_type = pen::TEXTURE_COLLECTION_NONE;

        if (ddsh->caps & DDSCAPS_COMPLEX)
        {
            if (ddsh->caps2 & DDS_CUBEMAP_ALLFACES)
            {
                tcp.collection_type = pen::TEXTURE_COLLECTION_CUBE;
                tcp.num_arrays = 6;
            }

            if (ddsh->caps2 & DDSCAPS2_VOLUME)
            {
                tcp.collection_type = pen::TEXTURE_COLLECTION_VOLUME;
                tcp.num_arrays = ddsh->depth;
            }
        }

        // calculate total data size
        tcp.data_size = 0;

        // faces / slices / depths
        for (s32 a = 0; a < tcp.num_arrays; ++a)
        {
            // top level
            u32 data_size = calc_level_size(tcp.width, tcp.height, compressed, block_size);

            // mips
            u32 ext_data_size = 0;

            u32 mip_width = tcp.width >> 1;
            u32 mip_height = tcp.height >> 1;

            for (s32 i = 0; i < tcp.num_mips - 1; ++i)
            {
                ext_data_size += calc_level_size(mip_width, mip_height, compressed, block_size);

                mip_width = mip_width > 1 ? mip_width >> 1 : 1;
                mip_height = mip_height > 1 ? mip_height >> 1 : 1;
            }

            tcp.data_size += data_size + ext_data_size;
        }

        // allocate mem and copy
        tcp.data = pen::memory_alloc(tcp.data_size);

        // copy texture data into the tcp storage
        memcpy(tcp.data, top_image_start, tcp.data_size);

        // free the files contents
        pen::memory_free(file_data);

        u32 texture_index = pen::renderer_create_texture(tcp);

        pen::memory_free(tcp.data);

        return texture_index;
    }

    void texture_build()
    {
        Str build_cmd = get_build_cmd();

        build_cmd.append(" -actions textures");

        PEN_SYSTEM(build_cmd.c_str());
    }

    void texture_hotload(std::vector<hash_id>& dirty)
    {
        for (auto& d : dirty)
        {
            for (auto& tr : k_texture_references)
            {
                if (tr.id_name == d)
                {
                    u32 new_handle = load_texture_internal(tr.filename.c_str(), tr.id_name, tr.tcp);

                    pen::renderer_replace_resource(tr.handle, new_handle, pen::RESOURCE_TEXTURE);
                }
            }
        }
    }
} // namespace

namespace put
{
    Str get_build_cmd()
    {
        static Str build_tool_str = "";
        if (build_tool_str == "")
        {
            pen::json j_build_config = pen::json::load_from_file("../../build_config.json");

            if (j_build_config.type() != JSMN_UNDEFINED)
            {
                Str pmtech_dir = "../../";
                pmtech_dir.append(j_build_config["pmtech_dir"].as_cstr());
                pmtech_dir.append(PEN_DIR);

                pmtech_dir = pen::str_replace_chars(pmtech_dir, '/', PEN_DIR);

                build_tool_str.append(PEN_PYTHON3);
                build_tool_str.append(pmtech_dir.c_str());
                build_tool_str.append(PEN_BUILD_CMD);

                dev_console_log_level(dev_ui::CONSOLE_MESSAGE, "[build tool cmd] %s", build_tool_str.c_str());
            }
            else
            {
                dev_console_log_level(dev_ui::CONSOLE_ERROR, "%s", "[error] unable to find pmtech dir");
            }
        }

        return build_tool_str;
    }

    void save_texture(const c8* filename, const texture_info& info)
    {
        // dds header
        dds_header hdr = {0};
        hdr.magic = 0x20534444;
        hdr.size = 124;
        hdr.flags |= (DDS_CAPS | DDS_HEIGHT | DDS_WIDTH | DDS_PIXELFORMAT);
        hdr.caps |= DDSCAPS_TEXTURE;

        hdr.width = info.width;
        hdr.height = info.height;
        hdr.mip_map_count = info.num_mips;
        hdr.depth = 1;
        hdr.pitch_or_linear_size = (info.width * info.block_size + 7) / 8;

        // conditional flags
        if (info.num_mips > 1)
        {
            hdr.flags |= DDS_MIPMAPCOUNT;
            hdr.caps |= (DDSCAPS_MIPMAP | DDSCAPS_COMPLEX);
        }

        if (info.collection_type != pen::TEXTURE_COLLECTION_NONE)
            hdr.caps |= DDSCAPS_COMPLEX;

        if (info.collection_type == pen::TEXTURE_COLLECTION_VOLUME)
        {
            hdr.depth = info.num_arrays;
            hdr.caps2 |= DDSCAPS2_VOLUME;
            hdr.caps |= DDS_DEPTH;
        }
        else if (info.collection_type == pen::TEXTURE_COLLECTION_CUBE)
        {
            hdr.caps2 |= DDSCAPS2_CUBEMAP;
            hdr.caps2 |= DDS_CUBEMAP_ALLFACES;
        }

        // pixel format
        ddspf pf = dds_pixel_format_from_texture_format(info.format);
        hdr.pixel_format = pf;

        std::ofstream ofs(filename, std::ofstream::binary);

        ofs.write((const c8*)&hdr, sizeof(dds_header));
        ofs.write((const c8*)info.data, info.data_size);

        ofs.close();
    }

    u32 load_texture(const c8* filename)
    {
        // check for existing
        hash_id hh = PEN_HASH(filename);
        for (auto& t : k_texture_references)
            if (t.id_name == hh)
                return t.handle;

        add_file_watcher(filename, texture_build, texture_hotload);

        pen::texture_creation_params tcp;
        u32                          texture_index = load_texture_internal(filename, hh, tcp);

        k_texture_references.push_back({hh, filename, texture_index, tcp});

        return texture_index;
    }

    Str get_texture_filename(u32 handle)
    {
        for (auto& t : k_texture_references)
            if (t.handle == handle)
                return t.filename;

        return "";
    }

    void get_texture_info(u32 handle, texture_info& info)
    {
        for (auto& t : k_texture_references)
        {
            if (t.handle == handle)
                info = t.tcp;
        }
    }

    void texture_browser_ui()
    {
        ImGui::Columns(4);

        for (auto& t : k_texture_references)
        {
            ImGui::PushID(t.filename.c_str());
            image_ex(t.handle, vec2f(256.0f, 256.0f), (dev_ui::e_shader)t.tcp.collection_type);
            ImGui::NextColumn();
            ImGui::PopID();
        }

        ImGui::Columns(1);
    }

    void add_file_watcher(const c8* filename, void (*build_callback)(), void (*hotload_callback)(std::vector<hash_id>& dirty))
    {
        Str fn = filename;

        // replace filename with dependencies.json
        u32 loc = pen::str_find_reverse(fn, "/");

        fn.appendf_from(loc + 1, "%s", "dependencies.json");

        hash_id id_name = PEN_HASH(fn.c_str());

        // search for existing
        for (auto* fw : k_file_watches)
        {
            if (id_name == fw->id_name)
            {
                return;
            }
        }

        // add new
        file_watch* fw = new file_watch();
        fw->dependencies = pen::json::load_from_file(fn.c_str());
        fw->filename = fn;
        fw->id_name = id_name;
        fw->hotload_callback = hotload_callback;
        fw->build_callback = build_callback;

        k_file_watches.push_back(fw);
    }

    void poll_hot_loader()
    {
        // print build cmd to console first time init
        get_build_cmd();

        for (auto* fw : k_file_watches)
        {
            if (fw->invalidated)
            {
                fw->dependencies = pen::json::load_from_file(fw->filename.c_str());
            }

            bool      invalidated = false;
            pen::json files = fw->dependencies["files"];
            s32       num_files = files.size();
            for (s32 i = 0; i < num_files; ++i)
            {
                pen::json outputs = files[i];
                s32       num_outputs = outputs.size();
                for (s32 j = 0; j < num_outputs; ++j)
                {
                    pen::json inputs = outputs[j];
                    s32       num_inputs = outputs.size();
                    for (s32 k = 0; k < num_inputs; ++k)
                    {
                        pen::json input = inputs[k];

                        u32 built_ts = input["timestamp"].as_u32();
                        Str fn = input["name"].as_filename();

                        u32       current_ts = 0;
                        pen_error err = pen::filesystem_getmtime(fn.c_str(), current_ts);

                        // dev_console_log("%s: %i,%i", fn.c_str(), current_ts, built_ts);

                        if (current_ts > built_ts && err == PEN_ERR_OK)
                        {
                            invalidated = true;

                            if (!fw->invalidated)
                            {
                                dev_console_log("[file watcher] source file %s has changed", fn.c_str());
                                dev_console_log("[file watcher] dest file is %s", outputs[j].name().c_str());

                                fw->changes.push_back(PEN_HASH(outputs[j].name().c_str()));
                            }
                        }
                    }
                }
            }

            if (invalidated)
            {
                if (!fw->invalidated)
                {
                    // trigger rebuild
                    fw->build_callback();
                }

                fw->invalidated = true;
            }
            else
            {
                if (fw->invalidated)
                {
                    // rebuild has succeeded
                    dev_console_log("[file watcher] rebuild for %s complete", fw->filename.c_str());

                    fw->hotload_callback(fw->changes);
                    fw->changes.clear();

                    fw->invalidated = false;
                }
            }
        }
    }
} // namespace put
