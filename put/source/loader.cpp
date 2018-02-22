//#include <fstream>

#include "definitions.h"
#include "loader.h"
#include "memory.h"
#include "file_system.h"
#include "renderer.h"
#include "pen_string.h"
#include "str/Str.h"
#include "hash.h"
#include "dev_ui.h"
#include "str_utilities.h"
#include "pen_json.h"

#include <vector>

namespace put
{
    enum texture_type
    {
        DDS_RGBA = 0x01,
        DDS_BC = 0x04,
    };
    
    enum compression_format
    {
        DXT1 = 0x31545844,
        DXT2 = 0x32545844,
        DXT3 = 0x33545844,
        DXT4 = 0x34545844,
        DXT5 = 0x35545844,
        DX10 = 0x30315844,
    };

    //dds documentation os MSDN defines all these data types as ulongs.. but they are only 4 bytes in actual data..
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
        u32     magic;
        u32		size;
        u32		flags;
        u32		height;
        u32		width;
        u32		pitch_or_linear_size;
        u32		depth;
        u32		mip_map_count;
        u32		reserved[11];
        ddspf	pixel_format;
        u32		caps;
        u32		caps2;
        u32		caps3;
        u32		caps4;
        u32		reserved2;
    };

	struct texture_reference
	{
		hash_id id_name;
		Str filename;
		u32 handle;
		pen::texture_creation_params tcp;
	};
	static std::vector<texture_reference> k_texture_references;

    struct file_watch
	{
		hash_id		id_name;
		Str			filename;
		pen::json	dependencies;
		bool		invalidated = false;
		std::vector<hash_id> changes;

		void(*build_callback)();
		void(*hotload_callback)(std::vector<hash_id>& dirty);
	};

	std::vector<file_watch*> k_file_watches;

	u32 calc_level_size( u32 width, u32 height, bool compressed, u32 block_size )
	{
		if( compressed )
		{
			return PEN_UMAX( 1, ( ( width + 3 ) / 4 ) ) * PEN_UMAX( 1, ( ( height + 3 ) / 4 ) ) * block_size;
		}

		return	width * height * block_size; 		
	}

	u32 dds_pixel_format_to_texture_format( const ddspf &pixel_format, bool &compressed, u32 &block_size, bool &dx10_header_present )
	{
		dx10_header_present = false;
		compressed = false;

		if( pixel_format.four_cc )
		{
			compressed = true;
			block_size = 16;

			switch( pixel_format.four_cc )
			{
			case DXT1:
				block_size = 8;
				return PEN_TEX_FORMAT_BC1_UNORM;
			case DXT2:
				return PEN_TEX_FORMAT_BC2_UNORM;
			case DXT3:
				return PEN_TEX_FORMAT_BC3_UNORM;
			case DXT4:
				return PEN_TEX_FORMAT_BC4_UNORM;
			case DXT5:
				return PEN_TEX_FORMAT_BC5_UNORM;
			case DX10:
				dx10_header_present = true;
				return 0;
			}
		}
		else
		{
			u32 rgba = (pixel_format.r_mask | pixel_format.g_mask | pixel_format.b_mask | pixel_format.a_mask);
			if ( rgba == 0xffffffff)
			{
				block_size = 4;
				return PEN_TEX_FORMAT_BGRA8_UNORM;
			}
		}

		//only supported formats are RGBA 
		PEN_ASSERT( 0 );

		return 0;
	}

	u32 load_texture_internal(const c8* filename, hash_id hh, pen::texture_creation_params& tcp )
	{
		//load a texture file from disk.
		void* file_data = nullptr;
		u32	  file_data_size = 0;

		u32 pen_err = pen::filesystem_read_file_to_buffer(filename, &file_data, file_data_size);

		if (pen_err != PEN_ERR_OK)
		{
			dev_console_log_level(dev_ui::CONSOLE_ERROR, "[error] texture - unabled to find file: %s", filename);
			pen::memory_free(file_data);
			return 0;
		}

		//parse dds header
		dds_header* ddsh = (dds_header*)file_data;

		bool dx10_header_present;
		bool compressed;
		u32	 block_size;

		u32 format = dds_pixel_format_to_texture_format(ddsh->pixel_format, compressed, block_size, dx10_header_present);

		//fill out texture_creation_params
		tcp.width = ddsh->width;
		tcp.height = ddsh->height;
		tcp.format = format;
		tcp.num_mips = ddsh->mip_map_count;
		tcp.num_arrays = 1;
		tcp.sample_count = 1;
		tcp.sample_quality = 0;
		tcp.usage = PEN_USAGE_DEFAULT;
		tcp.bind_flags = PEN_BIND_SHADER_RESOURCE;
		tcp.cpu_access_flags = 0;
		tcp.flags = 0;
		tcp.block_size = block_size;
		tcp.pixels_per_block = compressed ? 4 : 1;

		//allocate and copy texture data

		//top level
		u32 data_size = calc_level_size(tcp.width, tcp.height, compressed, block_size);

		//mips / faces / slices / depths
		u32 ext_data_size = 0;

		u32 mip_width = tcp.width >> 1;
		u32 mip_height = tcp.height >> 1;

		for (u32 i = 0; i < tcp.num_mips - 1; ++i)
		{
			ext_data_size += calc_level_size(mip_width, mip_height, compressed, block_size);

			mip_width = mip_width > 1 ? mip_width >> 1 : 1;
			mip_height = mip_height > 1 ? mip_height >> 1 : 1;
		}

		tcp.data_size = data_size + ext_data_size;
		tcp.data = pen::memory_alloc(tcp.data_size);

		//copy texture data into the tcp storage
		u8* top_image_start = (u8*)file_data + sizeof(dds_header);
		u8* ext_image_start = top_image_start + data_size;

		pen::memory_cpy(tcp.data, top_image_start, data_size);
		pen::memory_cpy((u8*)tcp.data + data_size, ext_image_start, ext_data_size);

		//free the files contents
		pen::memory_free(file_data);

		u32 texture_index = pen::renderer_create_texture(tcp);

		pen::memory_free(tcp.data);

		return texture_index;
	}

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

				put::str_replace_chars(pmtech_dir, '/', PEN_DIR);

				build_tool_str.append(PEN_SHADER_COMPILE_PRE_CMD);
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

	void texture_build()
	{
		Str build_cmd = get_build_cmd();

		build_cmd.append(" -actions textures");

		system(build_cmd.c_str());
	}

	void texture_hotload( std::vector<hash_id>& dirty )
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

	void add_file_watcher( const c8* filename, void(*build_callback)(), void(*hotload_callback)(std::vector<hash_id>& dirty) )
	{
		Str fn = filename;

		//replace filename with dependencies.json
		u32 loc = put::str_find_reverse(fn, "/");

		fn.appendf_from(loc+2, "%s", "dependencies.json");

		hash_id id_name = PEN_HASH(fn.c_str());

		//search for existing
		for (auto* fw : k_file_watches)
		{
			if (id_name == fw->id_name)
			{
				return;
			}
		}

		//add new
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
		//print build cmd to console first time init
		get_build_cmd();

		for (auto* fw : k_file_watches)
		{
			if (fw->invalidated)
			{
				fw->dependencies = pen::json::load_from_file(fw->filename.c_str());
			}

			bool invalidated = false;
			pen::json files = fw->dependencies["files"];
			s32 num_files = files.size();
			for (s32 i = 0; i < num_files; ++i)
			{
				pen::json outputs = files[i];
				s32 num_outputs = outputs.size();
				for (s32 j = 0; j < num_outputs; ++j)
				{
					pen::json inputs = outputs[j];
					s32 num_inputs = outputs.size();
					for (s32 k = 0; k < num_inputs; ++k)
					{
						pen::json input = inputs[k];

						u32 built_ts = input["timestamp"].as_u32();
						Str fn = input["name"].as_str();
						str_replace_chars(fn, '@', ':');

						u32 current_ts;
						pen_error err = pen::filesystem_getmtime(fn.c_str(), current_ts);

						if (current_ts > built_ts && err == PEN_ERR_OK )
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
					//trigger rebuild
					fw->build_callback();
				}

				fw->invalidated = true;
			}
			else
			{
				if (fw->invalidated)
				{
					//rebuild has succeeded
					dev_console_log("[file watcher] rebuild for %s complete", fw->filename.c_str());

					fw->hotload_callback(fw->changes);
					fw->changes.clear();

					fw->invalidated = false;
				}
			}
		}
	}
    
	u32 load_texture( const c8* filename )
	{
        //check for existing
        hash_id hh = PEN_HASH(filename);
        for( auto& t : k_texture_references )
            if( t.id_name == hh )
                return t.handle;

		add_file_watcher(filename, texture_build, texture_hotload );

		pen::texture_creation_params tcp;
		u32 texture_index = load_texture_internal(filename, hh, tcp);

		k_texture_references.push_back({ hh, filename, texture_index, tcp });

		return texture_index;
	}

	void get_texture_info( u32 handle, texture_info& info )
	{
		for (auto& t : k_texture_references)
		{
			if (t.handle == handle)
				info = t.tcp;
		}
	}
}
