#include "loader.h"
#include "memory.h"
#include "file_system.h"
#include "renderer.h"
#include "pen_string.h"

namespace put
{
#define DDS_RGBA 0x01
#define DDS_BC	 0x04

#define DXT1	 0x31545844	 
#define DXT2	 0x32545844	 
#define DXT3	 0x33545844	 
#define DXT4	 0x34545844	 
#define DXT5	 0x35545844	 
#define DX10	 0x30315844	 

	typedef struct dds_pixel_format
	{
		ulong size;
		ulong flags;
		ulong four_cc;
		ulong rgb_bit_count;
		ulong r_mask;
		ulong g_mask;
		ulong b_mask;
		ulong a_mask;

	} dds_pixel_format;

	typedef struct dds_header
	{
		ulong				magic;
		ulong				size;
		ulong				flags;
		ulong				height;
		ulong				width;
		ulong				pitch_or_linear_size;
		ulong				depth;
		ulong				mip_map_count;
		ulong				reserved[11];
		dds_pixel_format	pixel_format;
		ulong				caps;
		ulong				caps2;
		ulong				caps3;
		ulong				caps4;
		ulong				reserved2;
	} dds_header;

	u32 calc_level_size( u32 width, u32 height, bool compressed, u32 block_size )
	{
		if( compressed )
		{
			return max( 1, ( ( width + 3 ) / 4 ) ) * max( 1, ( ( height + 3 ) / 4 ) ) * block_size;
		}

		return	width * height * block_size; 		
	}

	u32 dds_pixel_format_to_texture_format( const dds_pixel_format &ddspf, bool &compressed, u32 &block_size, bool &dx10_header_present )
	{
		dx10_header_present = FALSE;
		compressed = FALSE;

		if( ddspf.four_cc )
		{
			compressed = TRUE;
			block_size = 16;

			switch( ddspf.four_cc )
			{
			case DXT1:
				block_size = 8;
				return PEN_FORMAT_BC1_UNORM;
			case DXT2:
				return PEN_FORMAT_BC2_UNORM;
			case DXT3:
				return PEN_FORMAT_BC3_UNORM;
			case DXT4:
				return PEN_FORMAT_BC4_UNORM;
			case DXT5:
				return PEN_FORMAT_BC5_UNORM;
			case DX10:
				dx10_header_present = TRUE;
				return 0;
			}
		}
		else
		{
			u32 rgba = (ddspf.r_mask | ddspf.g_mask | ddspf.b_mask | ddspf.a_mask);
			if ( rgba == 0xffffffff)
			{
				block_size = 4;
				return PEN_FORMAT_B8G8R8A8_UNORM;
			}
		}

		//only supported formats are RGBA 
		PEN_ASSERT( 0 );

		return 0;
	}

	pen::texture_creation_params* loader_load_texture( const c8* filename )
	{
		pen::texture_creation_params* tcp = (pen::texture_creation_params*)pen::memory_alloc( sizeof( pen::texture_creation_params ) );

		//load a texture file from disk.
		void* file_data = NULL;
		u32	  file_data_size = 0;
		pen::filesystem_read_file_to_buffer( filename, &file_data, file_data_size );

		//parse dds header
		dds_header* ddsh = (dds_header*)file_data;

		bool dx10_header_present;
		bool compressed;
		u32	 block_size;

		u32 format = dds_pixel_format_to_texture_format( ddsh->pixel_format, compressed, block_size, dx10_header_present );

		//fill out texture_creation_params
		tcp->width = ddsh->width;
		tcp->height = ddsh->height;
		tcp->format = format;
		tcp->num_mips = ddsh->mip_map_count;
		tcp->num_arrays = 1;
		tcp->sample_count = 1;
		tcp->sample_quality = 0;
		tcp->usage = PEN_USAGE_DEFAULT;
		tcp->bind_flags = PEN_BIND_SHADER_RESOURCE;
		tcp->cpu_access_flags = 0;
		tcp->flags = 0;
		tcp->block_size = block_size;
		tcp->pixels_per_block =  compressed ? 4 : 1;

		//allocate and copy texture data

		//top level
		u32 data_size = calc_level_size( tcp->width, tcp->height, compressed, block_size );
		
		//mips / faces / slices / depths
		u32 ext_data_size = 0;

		u32 mip_width = tcp->width >> 1;
		u32 mip_height = tcp->height >> 1;

		for( u32 i = 0; i < tcp->num_mips; ++i )
		{
			ext_data_size += calc_level_size( mip_width, mip_height, compressed, block_size );

			mip_width = mip_width > 1 ? mip_width >> 1 : 1;
			mip_height = mip_height > 1 ? mip_height >> 1 : 1;
		}

		tcp->data_size = data_size + ext_data_size;
		tcp->data = pen::memory_alloc( tcp->data_size  );

		//copy texture data s32o the tcp storage
		u8* top_image_start = (u8*)file_data + sizeof( dds_header );
		u8* ext_image_start = top_image_start + data_size;

		pen::memory_cpy( tcp->data, top_image_start, data_size );
		pen::memory_cpy( (u8*)tcp->data + data_size, ext_image_start, ext_data_size );

		//free the files contents
		pen::memory_free( file_data );

		return tcp;
	}

	void loader_free_texture( pen::texture_creation_params** tcp )
	{
		pen::memory_free( (*tcp)->data );
		pen::memory_free( *tcp );

		*tcp = NULL;
	}

	c8 semantic_names[3][16] =
	{
		"POSITION",
		"TEXCOORD",
		"COLOR",
	};

	shader_program loader_load_shader_program( const c8* vs_filename, const c8* ps_filename, const c8* input_layout_filename )
	{
		shader_program prog;

		//shaders
		pen::shader_load_params vs_slp;
		vs_slp.type = PEN_SHADER_TYPE_VS;

		pen::shader_load_params ps_slp;
		ps_slp.type = PEN_SHADER_TYPE_PS;

		//textured
		pen::filesystem_read_file_to_buffer( vs_filename, &vs_slp.byte_code, vs_slp.byte_code_size );

		if (ps_filename != NULL)
		{
			pen::filesystem_read_file_to_buffer(ps_filename, &ps_slp.byte_code, ps_slp.byte_code_size);
		}
		else
		{
			ps_slp.byte_code = NULL;
			ps_slp.byte_code_size = 0;
		}

		prog.vertex_shader = pen::defer::renderer_load_shader( vs_slp );
		prog.pixel_shader = pen::defer::renderer_load_shader( ps_slp );

		void* il_data;
		u32 il_data_size;
		pen::filesystem_read_file_to_buffer( input_layout_filename, &il_data, il_data_size );
		int* input_parser = (int*)il_data;

		pen::input_layout_creation_params ilp;
		ilp.vs_byte_code = vs_slp.byte_code;
		ilp.vs_byte_code_size = vs_slp.byte_code_size;

		ilp.num_elements = *input_parser;
		input_parser++;

		ilp.input_layout = (pen::input_layout_desc*)pen::memory_alloc( sizeof(pen::input_layout_desc) * ilp.num_elements );
	
		for( u32 i = 0; i < ilp.num_elements; i++ )
		{
			u32 size = *input_parser;
			input_parser++;

			u32 offset = *input_parser;
			input_parser++;

			u32 semantic_lookup_index = *input_parser;
			input_parser++;

			u32 semantic_index = *input_parser;
			input_parser++;

			ilp.input_layout[i].semantic_index = semantic_index;

			switch( size )
			{
			case 4:
				ilp.input_layout[i].format = PEN_FORMAT_R32_FLOAT;
				break;
			case 8:
				ilp.input_layout[i].format = PEN_FORMAT_R32G32_FLOAT;
				break;
			case 12:
				ilp.input_layout[i].format = PEN_FORMAT_R32G32B32_FLOAT;
				break;
			case 16:
				ilp.input_layout[i].format = PEN_FORMAT_R32G32B32A32_FLOAT;
				break;
			}

			ilp.input_layout[i].semantic_name = &semantic_names[semantic_lookup_index][0];
			
			ilp.input_layout[i].input_slot = 0;
			ilp.input_layout[i].aligned_byte_offset = offset;
			ilp.input_layout[i].input_slot_class = D3D11_INPUT_PER_VERTEX_DATA;
			ilp.input_layout[i].instance_data_step_rate = 0;
		}

		prog.input_layout = pen::defer::renderer_create_input_layout( ilp );

		pen::memory_free( vs_slp.byte_code );
		pen::memory_free( ps_slp.byte_code );
		pen::memory_free( ilp.input_layout );
		pen::memory_free( il_data );

		return prog;
	}

	skeleton* loader_load_skeleton( const c8* filename )
	{
		void*  skeleton_data;
		u32	   file_size;

		pen::filesystem_read_file_to_buffer( filename, &skeleton_data, file_size );

		u32*	header = (u32*)skeleton_data;

		//alloc a new skeleton
		skeleton* p_skeleton = (skeleton*)pen::memory_alloc(sizeof(skeleton));

		//with number of joints allocate space for parents and transforms
		p_skeleton->num_joints = (u32)header[ 1 ];

		p_skeleton->parents = (u32*)pen::memory_alloc(sizeof(u32)*p_skeleton->num_joints);
		p_skeleton->offsets = (vec3f*)pen::memory_alloc(sizeof(vec3f)*p_skeleton->num_joints);
		p_skeleton->rotations = (Quaternion*)pen::memory_alloc(sizeof(Quaternion)*p_skeleton->num_joints);
		p_skeleton->names = (c8**)pen::memory_alloc(sizeof(c8*)*p_skeleton->num_joints);

		u32* p_int = &header[ 2 ];
		for( u32 i = 0; i < p_skeleton->num_joints; ++i )
		{
			//name
			u32 num_chars = *p_int++;
			p_skeleton->names[ i ] = (c8*)pen::memory_alloc(sizeof(c8)*(num_chars+1));
			
			c8 buf[ 32 ];
			for( u32 c = 0; c < num_chars; ++c)
			{
				if( c >= 32 )
				{
					break;
				}

				buf[ c ] = (c8)*p_int++;
			}

			pen::memory_cpy( &p_skeleton->names[ i ][ 0 ], &buf[ 0 ], num_chars );
			p_skeleton->names[ i ][ num_chars ] = '\0';

			//parent
			p_skeleton->parents[ i ] = *p_int++;

			//num transforms
			u32 num_transforms = *p_int++;

			u32 num_rotations = 0;
			vec4f rotations[ 3 ];

			for( u32 t = 0; t < num_transforms; ++t )
			{
				u32 type = *p_int++;

				if( type == 0 )
				{
					//translate
					pen::memory_set( &p_skeleton->offsets[i], 0xff, 12 );
					pen::memory_cpy( &p_skeleton->offsets[i], p_int, 12 );
					p_int += 3;
				}
				else if( type == 1 )
				{
					//rotate
					pen::memory_cpy( &rotations[ num_rotations ], p_int, 16 );

					//convert to radians
					rotations[ num_rotations ].w = psmath::deg_to_rad( rotations[ num_rotations ].w );

					static f32 zero_rotation_epsilon = 0.000001f;
					if( rotations[ num_rotations ].w < zero_rotation_epsilon && rotations[ num_rotations ].w > zero_rotation_epsilon )
					{
						rotations[ num_rotations ].w = 0.0f;
					}

					num_rotations++;

					p_int += 4;
				}
				else
				{
					//unsupported transform type
					PEN_ASSERT( 0 );
				}
			}

			PEN_ASSERT( num_rotations <= 3 );

			if( num_rotations == 0 )
			{
				//no rotation
				p_skeleton->rotations[ i ].euler_angles( 0.0f, 0.0f, 0.0f );
			}
			else if( num_rotations == 1 )
			{
				//axis angle
				p_skeleton->rotations[ i ].axis_angle( rotations[ 0 ] );
			}
			else if( num_rotations == 3 )
			{
				//euler angles
				f32 z_theta = 0; 
				f32 y_theta = 0; 
				f32 x_theta = 0; 

				for( u32 r = 0; r < 3; ++r )
				{
					if( rotations[r].z == 1.0f )
					{
						z_theta = rotations[r].w;
					}
					else if (rotations[r].y == 1.0f)
					{
						y_theta = rotations[r].w;
					}
					else if (rotations[r].x == 1.0f)
					{
						x_theta = rotations[r].w;
					}

					p_skeleton->rotations[i].euler_angles( z_theta, y_theta, x_theta );
				}
			}
		}

		//animations
		p_skeleton->num_anims = *p_int++;

		p_skeleton->animations = (animation*)pen::memory_alloc( sizeof(animation)*p_skeleton->num_anims  );

		vec3f anim_val;
		for( u32 a = 0; a < p_skeleton->num_anims ; a++ )
		{
			p_skeleton->animations[a].bone_index = *p_int++;
			p_skeleton->animations[a].num_times = *p_int++;

			p_skeleton->animations[a].rotations = NULL;
			p_skeleton->animations[a].translations = NULL;

			u32 num_translations = *p_int++;
			u32 num_rotations = *p_int++;

			p_skeleton->animations[a].timeline = (f32*)pen::memory_alloc( sizeof(f32)*p_skeleton->animations[a].num_times );

			if (num_translations == p_skeleton->animations[a].num_times)
			{
				p_skeleton->animations[a].translations = (vec3f*)pen::memory_alloc( sizeof(vec3f)*num_translations );
			}

			if (num_rotations == p_skeleton->animations[a].num_times)
			{
				p_skeleton->animations[a].rotations = (Quaternion*)pen::memory_alloc( sizeof(Quaternion)*num_rotations );
				p_skeleton->animations[a].euler_angles = (vec3f*)pen::memory_alloc( sizeof(vec3f)*num_rotations );
			}

			for (u32 t = 0; t < p_skeleton->animations[a].num_times; ++t)
			{
				pen::memory_cpy( &p_skeleton->animations[a].timeline[t], p_int, 4 );
				p_int++;

				if (p_skeleton->animations[a].translations)
				{
					pen::memory_cpy( &p_skeleton->animations[a].translations[t], p_int, 12 );
					p_int += 3;
				}

				if (p_skeleton->animations[a].rotations)
				{
					vec3f euler_angs;
					pen::memory_cpy( &euler_angs, p_int, 12 );

					if( vec3f::almost_equal( euler_angs, vec3f::zero() ) )
					{
						euler_angs = vec3f::zero();
					}

					pen::memory_cpy( &p_skeleton->animations[a].euler_angles[t], &euler_angs, 12 );
					
					p_skeleton->animations[a].rotations[t].euler_angles( psmath::deg_to_rad( euler_angs.z ), psmath::deg_to_rad( euler_angs.y ), psmath::deg_to_rad( euler_angs.x ) );
					p_int += 3;
				}
			}
		}

		pen::memory_free( skeleton_data );

		return p_skeleton;
	}
}