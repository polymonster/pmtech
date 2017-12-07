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
    
    struct texture_reference
    {
        hash_id id_name;
        Str filename;
        u32 handle;
    };
    static std::vector<texture_reference> k_texture_references;

	u32 load_texture( const c8* filename )
	{
        //check for existing
        hash_id hh = PEN_HASH(filename);
        for( auto& t : k_texture_references )
            if( t.id_name == hh )
                return t.handle;
        
		//load a texture file from disk.
		void* file_data = NULL;
		u32	  file_data_size = 0;
        
		u32 pen_err = pen::filesystem_read_file_to_buffer( filename, &file_data, file_data_size );
        
        if( pen_err != PEN_ERR_OK )
        {
            put::dev_ui::log_level(dev_ui::CONSOLE_ERROR, "[error] texture - unabled to find file: %s", filename );
            pen::memory_free( file_data );
            return 0;
        }
        
        pen::texture_creation_params tcp;

		//parse dds header
		dds_header* ddsh = (dds_header*)file_data;

		bool dx10_header_present;
		bool compressed;
		u32	 block_size;

		u32 format = dds_pixel_format_to_texture_format( ddsh->pixel_format, compressed, block_size, dx10_header_present );

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
		tcp.pixels_per_block =  compressed ? 4 : 1;

		//allocate and copy texture data

		//top level
		u32 data_size = calc_level_size( tcp.width, tcp.height, compressed, block_size );
		
		//mips / faces / slices / depths
		u32 ext_data_size = 0;

		u32 mip_width = tcp.width >> 1;
		u32 mip_height = tcp.height >> 1;

		for( u32 i = 0; i < tcp.num_mips-1; ++i )
		{
			ext_data_size += calc_level_size( mip_width, mip_height, compressed, block_size );

			mip_width = mip_width > 1 ? mip_width >> 1 : 1;
			mip_height = mip_height > 1 ? mip_height >> 1 : 1;
		}

		tcp.data_size = data_size + ext_data_size;
		tcp.data = pen::memory_alloc( tcp.data_size  );

		//copy texture data into the tcp storage
		u8* top_image_start = (u8*)file_data + sizeof( dds_header );
		u8* ext_image_start = top_image_start + data_size;

		pen::memory_cpy( tcp.data, top_image_start, data_size );
		pen::memory_cpy( (u8*)tcp.data + data_size, ext_image_start, ext_data_size );

		//free the files contents
		pen::memory_free( file_data );
        
        u32 texture_index = pen::renderer_create_texture( tcp );
        
        pen::memory_free( tcp.data );
        
        //add to list to track
        k_texture_references.push_back({hh, filename, texture_index});
        
		return texture_index;
	}
}
