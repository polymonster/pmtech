#include <fstream>

#include "definitions.h"
#include "loader.h"
#include "memory.h"
#include "file_system.h"
#include "renderer.h"
#include "pen_string.h"
#include "json.hpp"

using json = nlohmann::json;

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

	u32 loader_load_texture( const c8* filename )
	{
		//load a texture file from disk.
		void* file_data = NULL;
		u32	  file_data_size = 0;
        
		u32 pen_err = pen::filesystem_read_file_to_buffer( filename, &file_data, file_data_size );
        
        if( pen_err != PEN_ERR_OK )
        {
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

		for( u32 i = 0; i < tcp.num_mips; ++i )
		{
			ext_data_size += calc_level_size( mip_width, mip_height, compressed, block_size );

			mip_width = mip_width > 1 ? mip_width >> 1 : 1;
			mip_height = mip_height > 1 ? mip_height >> 1 : 1;
		}

		tcp.data_size = data_size + ext_data_size;
		tcp.data = pen::memory_alloc( tcp.data_size  );

		//copy texture data s32o the tcp storage
		u8* top_image_start = (u8*)file_data + sizeof( dds_header );
		u8* ext_image_start = top_image_start + data_size;

		pen::memory_cpy( tcp.data, top_image_start, data_size );
		pen::memory_cpy( (u8*)tcp.data + data_size, ext_image_start, ext_data_size );

		//free the files contents
		pen::memory_free( file_data );
        
        u32 texture_index = pen::defer::renderer_create_texture2d( tcp );
        
        pen::memory_free( tcp.data );
        
		return texture_index;
	}

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

	shader_program& loader_load_shader_program( const c8* shader_name, managed_shader* ms )
	{
        c8 vs_file_buf[ 256 ];
        c8 ps_file_buf[ 256 ];
        c8 info_file_buf[ 256 ];

        pen::string_format( vs_file_buf, 256, "data/shaders/%s/%s.vsc", pen::renderer_get_shader_platform(), shader_name );
        pen::string_format( ps_file_buf, 256, "data/shaders/%s/%s.psc", pen::renderer_get_shader_platform(), shader_name );
        pen::string_format( info_file_buf, 256, "data/shaders/%s/%s.json", pen::renderer_get_shader_platform(), shader_name);

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
            return null_shader;
        }

		if (!ms)
		{
			//add a new managed shader
			s_managed_shaders.push_back(managed_shader{});
			ms = &s_managed_shaders.back();

			u32 name_len = pen::string_length(shader_name);
			pen::memory_cpy(ms->shader_name, shader_name, name_len);
		}
		else
		{
			//otherwise we are hot loading
			ms->invalidated_program = ms->program;

			//compare timestamps of the .info files to see if we have actually updated
            u32 current_info_ts;
            pen_error err = pen::filesystem_getmtime( info_file_buf, current_info_ts );
			
            if ( err == PEN_ERR_OK && (u32)current_info_ts <= ms->info_timestamp)
			{
				//return ourselves, so we remain invalid until the newly compiled shader is ready
				return ms->program;
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

		ms->program.input_layout = pen::defer::renderer_create_input_layout(ilp);

		if ( err != PEN_ERR_OK  )
		{
            //this shader set does not have a pixel shader which is valid, to allow fast z-only writes.
            ps_slp.byte_code = NULL;
            ps_slp.byte_code_size = 0;
		}

        err = pen::filesystem_read_file_to_buffer(ps_file_buf, &ps_slp.byte_code, ps_slp.byte_code_size);

		ms->program.vertex_shader = pen::defer::renderer_load_shader( vs_slp );
		ms->program.pixel_shader = pen::defer::renderer_load_shader( ps_slp );
        
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
        
		ms->program.program_index = pen::defer::renderer_link_shader_program(link_params);
        
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

		return ms->program;
	}
    
    void loader_release_shader_program( put::shader_program& shader_program )
    {
        pen::defer::renderer_release_shader( shader_program.vertex_shader, PEN_SHADER_TYPE_VS );
        pen::defer::renderer_release_shader( shader_program.pixel_shader, PEN_SHADER_TYPE_PS );
        pen::defer::renderer_release_input_layout( shader_program.input_layout );
        pen::defer::renderer_release_program( shader_program.program_index );
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
    
    void loader_poll_for_changes()
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
					put::loader_load_shader_program( s_managed_shaders[i].shader_name, &s_managed_shaders[i] );
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
