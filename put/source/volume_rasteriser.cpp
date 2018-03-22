#include "volume_rasteriser.h"

#include "ces/ces_scene.h"
#include "ces/ces_resources.h"
#include "ces/ces_utilities.h"
#include "camera.h"

#include "pen.h"
#include "memory.h"
#include "hash.h"
#include "pmfx.h"

namespace put
{
	using namespace ces;

	namespace vrt
	{
		static bool enable_volume_raster = false;

		const u32	volume_dim = 256;
		put::camera volume_raster_ortho;
		u32			current_requestd_slice = -1;
		s32			current_slice = 0;
		s32			current_axis = 0;
		bool		added_to_scene = false;

		void*		volume_slices[6][volume_dim] = { 0 };

		enum volume_raster_axis
		{
			ZAXIS_POS,
			YAXIS_POS,
			XAXIS_POS,
			ZAXIS_NEG,
			YAXIS_NEG,
			XAXIS_NEG
		};

		inline u8* get_texel(u32 axis, u32 x, u32 y, u32 z)
		{
			static u32 block_size = 4;
			static u32 row_pitch = volume_dim * 4;

			u32 invx = volume_dim - x - 1;
			u32 invy = volume_dim - y - 1;
			u32 invz = volume_dim - z - 1;
			u8* slice = nullptr;

			u32 mask = 0xff;

			if (!(mask & 1 << axis))
				return nullptr;

			PEN_SWAP(y, invy);

			switch (axis)
			{
				case ZAXIS_POS:
				{
					u32 offset_zpos = y * row_pitch + x * block_size;
					slice = (u8*)volume_slices[0][z];
					return &slice[offset_zpos];
				}
				case ZAXIS_NEG:
				{
					u32 offset_zneg = y * row_pitch + x * block_size;
					slice = (u8*)volume_slices[3][invz];
					return &slice[offset_zneg];
				}
				case YAXIS_POS:
				{
					u32 offset_ypos = z * row_pitch + x * block_size;
					slice = (u8*)volume_slices[1][invy];
					return &slice[offset_ypos];
				}
				case YAXIS_NEG:
				{
					u32 offset_yneg = z * row_pitch + x * block_size;
					slice = (u8*)volume_slices[4][y];
					return &slice[offset_yneg];
				}
				case XAXIS_POS:
				{
					u32 offset_xpos = y * row_pitch + z * block_size;
					slice = (u8*)volume_slices[2][invx];
					return &slice[offset_xpos];
				}
				case XAXIS_NEG:
				{
					u32 offset_xneg = y * row_pitch + z * block_size;
					slice = (u8*)volume_slices[5][x];
					return &slice[offset_xneg];
				}
				default:
					return nullptr;
			}

			return nullptr;
		}

		void image_read_back(void* p_data, u32 row_pitch, u32 depth_pitch, u32 block_size)
		{
			u32 w = row_pitch / block_size;
			u32 h = depth_pitch / row_pitch;

			if (!volume_slices[current_axis][current_slice])
			{
				volume_slices[current_axis][current_slice] = pen::memory_alloc(depth_pitch);
				pen::memory_cpy(volume_slices[current_axis][current_slice], p_data, depth_pitch);
			}

			current_slice++;
		}

		void volume_raster_completed(ces::entity_scene* scene)
		{
			if (added_to_scene)
				return;

			added_to_scene = true;

			material_resource* default_material = get_material_resource(PEN_HASH("default_material"));
			geometry_resource* cube = get_geometry_resource(PEN_HASH("cube"));

			u32 new_prim = get_new_node(scene);
			scene->names[new_prim] = "sphere";
			scene->names[new_prim].appendf("%i", new_prim);
			scene->transforms[new_prim].rotation = quat();
			scene->transforms[new_prim].scale = vec3f(10.0f);
			scene->transforms[new_prim].translation = vec3f::zero();
			scene->entities[new_prim] |= CMP_TRANSFORM;
			scene->parents[new_prim] = new_prim;
			instantiate_geometry(cube, scene, new_prim);
			instantiate_material(default_material, scene, new_prim);
			instantiate_model_cbuffer(scene, new_prim);

			//create a simple 3d texture
			u32 block_size = 4;
			u32 data_size = volume_dim * volume_dim * volume_dim * block_size;

			u8* volume_data = (u8*)pen::memory_alloc(data_size);
			u32 row_pitch = volume_dim * block_size;
			u32 slice_pitch = volume_dim  * row_pitch;

			for (u32 z = 0; z < volume_dim; ++z)
			{
				u8* slice_mem[6] = { 0 };
				for (u32 a = 0; a < 6; ++a)
				{
					slice_mem[a] = (u8*)volume_slices[a][z];
				}

				for (u32 y = 0; y < volume_dim; ++y)
				{
					for (u32 x = 0; x < volume_dim; ++x)
					{
						u32 offset = z * slice_pitch + y * row_pitch + x * block_size;

						u8 rgba[4] = { 0 };

						for (u32 a = 0; a < 6; ++a)
						{
							u8* tex = get_texel(a, x, y, z);

							if (!tex)
								continue;

							if (tex[3] > 8)
								for (u32 p = 0; p < 4; ++p)
									rgba[p] = tex[p];
						}



						volume_data[offset + 0] = rgba[2];
						volume_data[offset + 1] = rgba[1];
						volume_data[offset + 2] = rgba[0];
						volume_data[offset + 3] = rgba[3];
					}
				}
			}

			pen::texture_creation_params tcp;
			tcp.collection_type = pen::TEXTURE_COLLECTION_VOLUME;

			tcp.width = volume_dim;
			tcp.height = volume_dim;
			tcp.format = PEN_TEX_FORMAT_BGRA8_UNORM;
			tcp.num_mips = 1;
			tcp.num_arrays = volume_dim;
			tcp.sample_count = 1;
			tcp.sample_quality = 0;
			tcp.usage = PEN_USAGE_DEFAULT;
			tcp.bind_flags = PEN_BIND_SHADER_RESOURCE;
			tcp.cpu_access_flags = 0;
			tcp.flags = 0;
			tcp.block_size = block_size;
			tcp.pixels_per_block = 1;
			tcp.data = volume_data;
			tcp.data_size = data_size;

			u32 volume_texture = pen::renderer_create_texture(tcp);

			//set material for basic volume texture
			scene_node_material& mat = scene->materials[new_prim];
			mat.texture_id[4] = volume_texture;
			mat.default_pmfx_shader = pmfx::load_shader("pmfx_utility");
			mat.id_default_shader = PEN_HASH("pmfx_utility");
			mat.id_default_technique = PEN_HASH("volume_texture");
		}

		void volume_rasteriser_update(put::camera_controller* cc)
		{
			if (!enable_volume_raster)
				return;

			if (current_requestd_slice == current_slice)
				return;

			if (current_slice >= volume_dim)
			{
				current_axis++;
				current_slice = 0;
			}

			if (current_axis > 5)
			{
				volume_raster_completed( cc->scene );
				return;
			}

			static mat4 axis_swaps[] =
			{
				mat4::create_axis_swap(vec3f::unit_x(), vec3f::unit_y(), vec3f::unit_z()),
				mat4::create_axis_swap(vec3f::unit_x(), -vec3f::unit_z(), vec3f::unit_y()),
				mat4::create_axis_swap(vec3f::unit_z(), vec3f::unit_y(), vec3f::unit_x()),

				mat4::create_axis_swap(vec3f::unit_x(), vec3f::unit_y(), -vec3f::unit_z()),
				mat4::create_axis_swap(vec3f::unit_x(), -vec3f::unit_z(), -vec3f::unit_y()),
				mat4::create_axis_swap(vec3f::unit_z(), vec3f::unit_y(), -vec3f::unit_x())
			};

			vec3f min = cc->scene->renderable_extents.min;
			vec3f max = cc->scene->renderable_extents.max;

			vec3f dim = max - min;
			f32 texel_boarder = dim.max_component() / volume_dim;
			min -= texel_boarder;
			max += texel_boarder;

			vec3f smin[] =
			{
				vec3f(min.x, min.y, min.z),
				vec3f(min.x, min.z, min.y),
				vec3f(min.z, min.y, min.x),

				vec3f(min.x, min.y, max.z),
				vec3f(min.x, min.z, max.y),
				vec3f(min.z, min.y, max.x)
			};

			vec3f smax[] =
			{
				vec3f(max.x, max.y, max.z),
				vec3f(max.x, max.z, max.y),
				vec3f(max.z, max.y, max.x),

				vec3f(max.x, max.y, min.z),
				vec3f(max.x, max.z, min.y),
				vec3f(max.z, max.y, min.x)
			};

			vec3f mmin = smin[current_axis];
			vec3f mmax = smax[current_axis];

			f32 slice_thickness = (mmax.z - mmin.z) / volume_dim;
			f32 near_slice = mmin.z + slice_thickness * current_slice;

			put::camera_create_orthographic(&volume_raster_ortho, mmin.x, mmax.x, mmin.y, mmax.y, near_slice, near_slice + slice_thickness);
			volume_raster_ortho.view = axis_swaps[current_axis];

			static hash_id id_volume_raster = PEN_HASH("volume_raster");
			const pmfx::render_target* rt = pmfx::get_render_target(id_volume_raster);

			pen::resource_read_back_params rrbp;
			rrbp.block_size = 4;
			rrbp.row_pitch = volume_dim * rrbp.block_size;
			rrbp.depth_pitch = volume_dim * rrbp.row_pitch;
			rrbp.data_size = rrbp.depth_pitch;
			rrbp.resource_index = rt->handle;
			rrbp.format = PEN_TEX_FORMAT_BGRA8_UNORM;
			rrbp.call_back_function = image_read_back;

			pen::renderer_read_back_resource(rrbp);
			current_requestd_slice = current_slice;
		}

		void init(ces::entity_scene* scene)
		{
			put::camera_controller cc;
			cc.camera = &volume_raster_ortho;
			cc.update_function = &volume_rasteriser_update;
			cc.name = "volume_rasteriser_camera";
			cc.id_name = PEN_HASH(cc.name.c_str());
			cc.scene = scene;

			pmfx::register_camera(cc);
		}
	}
}


