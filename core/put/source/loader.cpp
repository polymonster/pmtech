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
#include "timer.h"

#include <fstream>
#include <vector>

using namespace put;

// dxgi formats
#define DXGI_RG32_FLOAT 16
#define DXGI_RGBA32_FLOAT 2
#define DXGI_RGBA8_UNORM 28
#define DXGI_R8_UNORM 61
#define DXGI_A8_UNORM 65
#define DXGI_BC1_UNORM 71
#define DXGI_BC2_UNORM 74
#define DXGI_BC3_UNORM 77
#define DXGI_BC4_UNORM 80
#define DXGI_BC5_UNORM 83

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
        u32                  rebuild_ts = 0;

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

    u32 dxgi_format_to_texture_format(const dx10_header* dxh, bool& compressed, u32& block_size)
    {
        switch (dxh->dxgi_format)
        {
            case DXGI_RGBA32_FLOAT:
                block_size = 16;
                return PEN_TEX_FORMAT_R32G32B32A32_FLOAT;
            case DXGI_RG32_FLOAT:
                block_size = 8;
                return PEN_TEX_FORMAT_R32G32_FLOAT;
            case DXGI_RGBA8_UNORM:
                block_size = 4;
                return PEN_TEX_FORMAT_RGBA8_UNORM;
            case DXGI_R8_UNORM:
                block_size = 1;
                return PEN_TEX_FORMAT_R8_UNORM;
            case DXGI_A8_UNORM:
                block_size = 1;
                return PEN_TEX_FORMAT_R8_UNORM;
            case DXGI_BC1_UNORM:
                block_size = 8;
                compressed = true;
                return PEN_TEX_FORMAT_BC1_UNORM;
            case DXGI_BC2_UNORM:
                block_size = 16;
                compressed = true;
                return PEN_TEX_FORMAT_BC2_UNORM;
            case DXGI_BC3_UNORM:
                block_size = 16;
                compressed = true;
                return PEN_TEX_FORMAT_BC3_UNORM;
            case DXGI_BC4_UNORM:
                block_size = 8;
                compressed = true;
                return PEN_TEX_FORMAT_BC4_UNORM;
            case DXGI_BC5_UNORM:
                block_size = 16;
                compressed = true;
                return PEN_TEX_FORMAT_BC5_UNORM;
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
                return PEN_TEX_FORMAT_RGBA8_UNORM;
            }
            else if (rgba == 0xffffff)
            {
                block_size = pixel_format.size / 8;
                return PEN_TEX_FORMAT_RGBA8_UNORM;
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
            dev_console_log_level(dev_ui::console_level::error, "[error] texture - unabled to find file: %s", filename);
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
        u32 array_size = 1;
        if (dx10_header_present)
        {
            dx10_header* dxh = (dx10_header*)top_image_start;

            format = dxgi_format_to_texture_format(dxh, compressed, block_size);

            array_size = dxh->array_size;
            top_image_start += sizeof(dx10_header);
        }

        // fill out texture_creation_params
        tcp.width = ddsh->width;
        tcp.height = ddsh->height;
        tcp.format = format;
        tcp.num_mips = std::max<u32>(ddsh->mip_map_count, 1);
        tcp.num_arrays = array_size;
        tcp.sample_count = 1;
        tcp.sample_quality = 0;
        tcp.usage = PEN_USAGE_DEFAULT;
        tcp.bind_flags = PEN_BIND_SHADER_RESOURCE;
        tcp.cpu_access_flags = 0;
        tcp.flags = 0;
        tcp.block_size = block_size;
        tcp.pixels_per_block = compressed ? 4 : 1;
        tcp.collection_type = array_size > 1 ? pen::TEXTURE_COLLECTION_ARRAY : pen::TEXTURE_COLLECTION_NONE;

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

    //
    // Hot loading thread
    //

    Str s_pmbuild_cmd = "";

    enum hot_loader_cmd_id
    {
        HOT_LOADER_CMD_CALL_SYSTEM,
        HOT_LOADER_CMD_CALL_SYSTEM_WATCHER
    };

    struct hot_loader_cmd
    {
        u32 cmd_index;
        c8* cmdline = nullptr;
    };

    pen::ring_buffer<hot_loader_cmd> s_hot_loader_cmd_buffer;

    void* hot_loader_thread(void* params)
    {
        pen::job_thread_params* job_params = (pen::job_thread_params*)params;

        pen::job* p_thread_info = job_params->job_info;
        pen::semaphore_post(p_thread_info->p_sem_continue, 1);

        s_hot_loader_cmd_buffer.create(32);

        for (;;)
        {
            hot_loader_cmd* cmd = s_hot_loader_cmd_buffer.get();
            while (cmd)
            {
                // process cmd
                switch (cmd->cmd_index)
                {
                    case HOT_LOADER_CMD_CALL_SYSTEM:
                    {
                        PEN_SYSTEM(cmd->cmdline);
                        pen::memory_free(cmd->cmdline);
                    }
                    break;
                    default:
                        break;
                }

                // get next
                cmd = s_hot_loader_cmd_buffer.get();
            }

            if(pen::semaphore_try_wait(p_thread_info->p_sem_exit))
                break;
                
            // plenty of sleep
            pen::thread_sleep_ms(16);
        }

        pen::semaphore_post(p_thread_info->p_sem_continue, 1);
        pen::semaphore_post(p_thread_info->p_sem_terminated, 1);
        return PEN_THREAD_OK;
    }

    void texture_build()
    {
        Str build_cmd = get_build_cmd();
        build_cmd.append(" -textures");
        put::trigger_hot_loader(build_cmd);
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
    void init_hot_loader()
    {
        pen::jobs_create_job(hot_loader_thread, 1024 * 1024, nullptr, pen::e_thread_start_flags::detached);

        pen::json pmbuild_config = pen::json::load_from_file("data/pmbuild_config.json");
        s_pmbuild_cmd = pmbuild_config["pmbuild"].as_str();

        dev_console_log_level(dev_ui::console_level::message, "[pmbuild cmd] %s", s_pmbuild_cmd.c_str());
    }

    void trigger_hot_loader(const Str& cmdline)
    {
        static f64         s_timeout = 0.0f;
        static pen::timer* t = pen::timer_create();
        s_timeout -= pen::timer_elapsed_ms(t);

        if (s_timeout <= 0.0)
        {
            hot_loader_cmd cmd;
            cmd.cmd_index = HOT_LOADER_CMD_CALL_SYSTEM;
            u32 len = cmdline.length();
            cmd.cmdline = (c8*)pen::memory_alloc(len + 1);
            memcpy(cmd.cmdline, cmdline.c_str(), len);
            cmd.cmdline[len] = '\0';
            s_hot_loader_cmd_buffer.put(cmd);

            // wait 10 seconds
            s_timeout = 1000.0f * 10.0f;
            pen::timer_start(t);
        }
    }

    Str get_build_cmd()
    {
        return s_pmbuild_cmd;
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
            {
                info = t.tcp;
                return;
            }
        }

        // not found, not a texture handle.
        PEN_ASSERT(0);
    }

    void texture_browser_ui()
    {
        ImGui::Columns(4);

        for (auto& t : k_texture_references)
        {
            ImGui::PushID(t.filename.c_str());
            dev_ui::image_ex(t.handle, vec2f(256.0f, 256.0f), (dev_ui::ui_shader)t.tcp.collection_type);
            ImGui::NextColumn();
            ImGui::PopID();
        }

        ImGui::Columns(1);
    }

    void add_file_watcher(const c8* filename, void (*build_callback)(), void (*hotload_callback)(std::vector<hash_id>& dirty))
    {
        Str     fn = filename;
        hash_id id_name = PEN_HASH(fn.c_str());

        // replace filename with dependencies.json
        u32 loc = pen::str_find_reverse(fn, ".");
        fn.appendf_from(loc + 1, "%s", "dep");

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
        PEN_HOTLOADING_ENABLED;

        // print build cmd to console first time init
        get_build_cmd();

        for (auto* fw : k_file_watches)
        {
            if (fw->invalidated)
            {
                u32 dep_ts;
                if(pen::filesystem_getmtime(fw->filename.c_str(), dep_ts) == PEN_ERR_OK)
                {
                    if(dep_ts >= fw->rebuild_ts)
                    {
                        fw->dependencies = pen::json::load_from_file(fw->filename.c_str());
                        
                        // rebuild has succeeded
                        dev_console_log("[file watcher] rebuild for %s complete", fw->filename.c_str());
                        fw->hotload_callback(fw->changes);
                        fw->changes.clear();
                        fw->invalidated = false;
                    }
                }
            }
            else
            {
                pen::json files = fw->dependencies["files"];
                s32       num_files = files.size();
                for (s32 i = 0; i < num_files; ++i)
                {
                    pen::json   outputs = files[i];
                    s32         num_inputs = outputs.size();
                    u32         current_ts = 0;
                    
                    if(pen::filesystem_getmtime(fw->filename.c_str(), current_ts) == PEN_ERR_OK)
                    {
                        for (s32 j = 0; j < num_inputs; ++j)
                        {
                            Str ifn = outputs[j]["name"].as_str();
                            u32 input_ts = 0;
                            if(pen::filesystem_getmtime(ifn.c_str(), input_ts) == PEN_ERR_OK)
                            {
                                if (!fw->invalidated)
                                {
                                    if (input_ts > current_ts)
                                    {
                                        dev_console_log("[file watcher] input file %s has changed", ifn.c_str());

                                        Str data_file = outputs[j]["data_file"].as_str();
                                        fw->changes.push_back(PEN_HASH(data_file.c_str()));
                                        fw->rebuild_ts = input_ts;
                                        
                                        fw->build_callback();
                                        fw->invalidated = true;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
} // namespace put
