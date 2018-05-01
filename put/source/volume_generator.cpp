#include "volume_generator.h"

#include "ces/ces_scene.h"
#include "ces/ces_resources.h"
#include "ces/ces_utilities.h"
#include "camera.h"
#include "pmfx.h"
#include "dev_ui.h"
#include "debug_render.h"

#include "memory.h"
#include "hash.h"
#include "pen.h"
#include "data_struct.h"

#include "sdf/makelevelset3.h"
extern mls_progress g_mls_progress;

namespace put
{
	using namespace ces;

	namespace vgt
	{
		struct triangle
		{
			vec3f v[3];
			vec3f n;
		};

		struct triangle_octree
		{
			triangle*			triangles = nullptr;
			triangle*			failed_triangles = nullptr;
			triangle_octree*	children = nullptr;
			u32					id = 0;
			extents				e;
		};

		static const int k_num_axes = 6;

		enum volume_types
		{
			VOLUME_RASTERISED_TEXELS = 0,
			VOLUME_SIGNED_DISTANCE_FIELD
		};

		enum volume_raster_axis
		{
			ZAXIS_POS,
			YAXIS_POS,
			XAXIS_POS,
			ZAXIS_NEG,
			YAXIS_NEG,
			XAXIS_NEG
		};

		enum volume_raster_axis_flags
		{
			Z_POS_MASK = 1<<0,
			Y_POS_MASK = 1<<1,
			X_POS_MASK = 1<<2,
			Z_NEG_MASK = 1<<3,
			Y_NEG_MASK = 1<<4,
			X_NEG_MASK = 1<<5,

			AXIS_ALL_MASK = (1<<6)-1
		};

		enum rasterise_capture_data
		{
			CAPTURE_ALBEDO = 0,
			CAPTURE_NORMALS,
			CAPTURE_BAKED_LIGHTING,
			CAPTURE_OCCUPANCY,
			CAPTURE_CUSTOM
		};

		struct vgt_options
		{
			s32		volume_dimension = 7;
			u32		rasterise_axes = AXIS_ALL_MASK;
			s32		volume_type = VOLUME_RASTERISED_TEXELS;
			s32		capture_data = 0;
		};

		struct vgt_rasteriser_job
		{
			vgt_options options;
			void**		volume_slices[k_num_axes] = { 0 };
			s32			current_slice = 0;
			s32			current_requested_slice = -1;
			s32			current_axis = 0;
			u32			dimension;
			extents		scene_extents;
			extents		current_slice_aabb;
			bool		rasterise_in_progress = false;
			a_u32		combine_in_progress;
			a_u32		combine_position;
			u32			block_size;
			u32			data_size;
			u8*			volume_data;
		};
		static vgt_rasteriser_job	k_rasteriser_job;

		struct dd
		{
			vec3f pos;
			vec4f closest;
		};

		struct vgt_sdf_job
		{
			vgt_options		options;
			entity_scene*	scene;
			triangle_octree tree;
			u32				volume_dim;
			u32				texture_format;
			u32				block_size;
			u32				data_size;
			u8*				volume_data;
			extents			scene_extents;
            bool            trust_sign = true;
            f32             padding;
			u32				generate_in_progress = 0;
		};
		static vgt_sdf_job			k_sdf_job;

		static put::camera			k_volume_raster_ortho;

		inline u8* get_texel(u32 axis, u32 x, u32 y, u32 z)
		{
			u32& volume_dim = k_rasteriser_job.dimension;
			void*** volume_slices = k_rasteriser_job.volume_slices;

			u32 block_size = 4;
			u32 row_pitch = volume_dim * 4;

			u32 invx = volume_dim - x - 1;
			u32 invy = volume_dim - y - 1;
			u32 invz = volume_dim - z - 1;

			u8* slice = nullptr;

			u32 mask = k_rasteriser_job.options.rasterise_axes;

			if (!(mask & 1 << axis))
				return nullptr;

			swap(y, invy);

			switch (axis)
			{
				case ZAXIS_POS:
				{
					u32 offset_zpos = y * row_pitch + invx * block_size;
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
					u32 offset_ypos = invz * row_pitch + x * block_size;
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
					slice = (u8*)volume_slices[2][x];
					return &slice[offset_xpos];
				}
				case XAXIS_NEG:
				{
					u32 offset_xneg = y * row_pitch + invz * block_size;
					slice = (u8*)volume_slices[5][invx];
					return &slice[offset_xneg];
				}
				default:
					return nullptr;
			}

			return nullptr;
		}

		void image_read_back(void* p_data, u32 row_pitch, u32 depth_pitch, u32 block_size)
		{
			s32& current_slice = k_rasteriser_job.current_slice;
			s32& current_axis = k_rasteriser_job.current_axis;
			void*** volume_slices = k_rasteriser_job.volume_slices;

			u32 w = row_pitch / block_size;
			u32 h = depth_pitch / row_pitch;

			if (w != k_rasteriser_job.dimension)
			{
				u32 dest_row_pitch = k_rasteriser_job.dimension * block_size;
				u8* src_iter = (u8*)p_data;
				u8* dest_iter = (u8*)volume_slices[current_axis][current_slice];
				for (u32 y = 0; y < h; ++y)
				{
					pen::memory_cpy(dest_iter, src_iter, dest_row_pitch);
					src_iter += row_pitch;
					dest_iter += dest_row_pitch;
				}
			}
			else
			{
				pen::memory_cpy(volume_slices[current_axis][current_slice], p_data, depth_pitch);
			}

			current_slice++;
		}

		PEN_TRV raster_voxel_combine(void* params)
		{
			pen::job_thread_params* job_params = (pen::job_thread_params*)params;
			vgt_rasteriser_job*		rasteriser_job = (vgt_rasteriser_job*)job_params->user_data;
			pen::job*		p_thread_info = job_params->job_info;
			pen::thread_semaphore_signal(p_thread_info->p_sem_continue, 1);

			u32& volume_dim = rasteriser_job->dimension;
			void*** volume_slices = rasteriser_job->volume_slices;

			//create a simple 3d texture
			rasteriser_job->block_size = 4;
			rasteriser_job->data_size = volume_dim * volume_dim * volume_dim * rasteriser_job->block_size;

			u8* volume_data = (u8*)pen::memory_alloc(rasteriser_job->data_size);
			u32 row_pitch = volume_dim * rasteriser_job->block_size;
			u32 slice_pitch = volume_dim  * row_pitch;

			rasteriser_job->combine_position = 0;

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
						rasteriser_job->combine_position++;

						u32 offset = z * slice_pitch + y * row_pitch + x * rasteriser_job->block_size;

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

			rasteriser_job->volume_data = volume_data;

			rasteriser_job->combine_in_progress = 2;

			return PEN_THREAD_OK;
		}

		u32 create_volume_from_data( u32 volume_dim, u32 block_size, u32 data_size, u32 tex_format, u8* volume_data )
		{
			pen::texture_creation_params tcp;
			tcp.collection_type = pen::TEXTURE_COLLECTION_VOLUME;

			tcp.width = volume_dim;
			tcp.height = volume_dim;
			tcp.format = tex_format;
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

			return pen::renderer_create_texture(tcp);
		}

		void volume_raster_completed(ces::entity_scene* scene)
		{
			if (k_rasteriser_job.combine_in_progress == 0)
			{
				k_rasteriser_job.combine_in_progress = 1;
				pen::thread_create_job(raster_voxel_combine, 1024 * 1024 * 1024, &k_rasteriser_job,
                                        pen::THREAD_START_DETACHED);
				return;
			}
			else
			{
				if (k_rasteriser_job.combine_in_progress < 2)
					return;
			}

			u32& volume_dim = k_rasteriser_job.dimension;

			//create texture
			u32 volume_texture = create_volume_from_data(volume_dim,
                                                         k_rasteriser_job.block_size,
                                                         k_rasteriser_job.data_size, PEN_TEX_FORMAT_BGRA8_UNORM, k_rasteriser_job.volume_data);

			//create material for volume ray trace
			material_resource* volume_material = new material_resource;
			volume_material->material_name = "volume_material";
			volume_material->shader_name = "pmfx_utility";
			volume_material->id_shader = PEN_HASH("pmfx_utility");
			volume_material->id_technique = PEN_HASH("volume_texture");
			volume_material->id_sampler_state[SN_VOLUME_TEXTURE] = PEN_HASH("clamp_point_sampler_state");
			volume_material->texture_handles[SN_VOLUME_TEXTURE] = volume_texture;
			add_material_resource(volume_material);

			geometry_resource* cube = get_geometry_resource(PEN_HASH("cube"));

			vec3f scale = (k_rasteriser_job.scene_extents.max - k_rasteriser_job.scene_extents.min) / 2.0f;
			vec3f pos = k_rasteriser_job.scene_extents.min + scale;

			u32 new_prim = get_new_node(scene);
			scene->names[new_prim] = "volume";
			scene->names[new_prim].appendf("%i", new_prim);
			scene->transforms[new_prim].rotation = quat();
			scene->transforms[new_prim].scale = scale;
			scene->transforms[new_prim].translation = pos;
			scene->entities[new_prim] |= CMP_TRANSFORM;
			scene->parents[new_prim] = new_prim;
			instantiate_geometry(cube, scene, new_prim);
			instantiate_material(volume_material, scene, new_prim);
			instantiate_model_cbuffer(scene, new_prim);

			//clean up
			for (u32 a = 0; a < 6; ++a)
			{
				for (u32 s = 0; s < k_rasteriser_job.dimension; ++s)
					pen::memory_free(k_rasteriser_job.volume_slices[a][s]);

				pen::memory_free(k_rasteriser_job.volume_slices[a]);
			}

			//save to disk?
			pen::memory_free(k_rasteriser_job.volume_data);

			//completed
			k_rasteriser_job.rasterise_in_progress = false;
			k_rasteriser_job.combine_in_progress = 0;
		}

		void volume_rasteriser_update(put::scene_controller* sc)
		{
			//update incremental job
			if (!k_rasteriser_job.rasterise_in_progress)
				return;

			if (k_rasteriser_job.current_requested_slice == k_rasteriser_job.current_slice)
				return;

			if (k_rasteriser_job.current_slice >= k_rasteriser_job.dimension)
			{
				while (!(k_rasteriser_job.options.rasterise_axes & 1<<(++k_rasteriser_job.current_axis)))
					if (k_rasteriser_job.current_axis > 5)
						break;

				k_rasteriser_job.current_slice = 0;
			}

			if (k_rasteriser_job.current_axis > 5)
			{
				volume_raster_completed( sc->scene );
				return;
			}

			if (!(k_rasteriser_job.options.rasterise_axes & 1 << k_rasteriser_job.current_axis))
			{
				k_rasteriser_job.current_axis++;
				return;
			}

			u32& volume_dim = k_rasteriser_job.dimension;
			s32& current_slice = k_rasteriser_job.current_slice;
			s32& current_axis = k_rasteriser_job.current_axis;
			s32& current_requested_slice = k_rasteriser_job.current_requested_slice;
			k_rasteriser_job.scene_extents = sc->scene->renderable_extents;

			vec3f min = sc->scene->renderable_extents.min;
			vec3f max = sc->scene->renderable_extents.max;

			vec3f dim = max - min;
			//f32 texel_boarder = dim.max_component() / volume_dim;

			f32 texel_boarder = component_wise_max(dim) / volume_dim;

			min -= texel_boarder;
			max += texel_boarder;

			static mat4 axis_swaps[] =
			{
				mat::create_axis_swap(vec3f::unit_x(), vec3f::unit_y(), -vec3f::unit_z()),
				mat::create_axis_swap(vec3f::unit_x(), -vec3f::unit_z(), vec3f::unit_y()),
				mat::create_axis_swap(-vec3f::unit_z(), vec3f::unit_y(), vec3f::unit_x()),

				mat::create_axis_swap(vec3f::unit_x(), vec3f::unit_y(), -vec3f::unit_z()),
				mat::create_axis_swap(vec3f::unit_x(), -vec3f::unit_z(), vec3f::unit_y()),
				mat::create_axis_swap(-vec3f::unit_z(), vec3f::unit_y(), vec3f::unit_x())
			};

			vec3f smin[] =
			{
				vec3f(max.x, min.y, min.z),
				vec3f(min.x, min.z, min.y),
				vec3f(min.z, min.y, min.x),

				vec3f(min.x, min.y, max.z),
				vec3f(min.x, max.z, max.y),
				vec3f(max.z, min.y, max.x)
			};

			vec3f smax[] =
			{
				vec3f(min.x, max.y, max.z),
				vec3f(max.x, max.z, max.y),
				vec3f(max.z, max.y, max.x),

				vec3f(max.x, max.y, min.z),
				vec3f(max.x, min.z, min.y),
				vec3f(min.z, max.y, min.x)
			};

			vec3f mmin = smin[current_axis];
			vec3f mmax = smax[current_axis];

			f32 slice_thickness = (mmax.z - mmin.z) / volume_dim;
			f32 near_slice = mmin.z + slice_thickness * current_slice;

			mmin.z = near_slice;
			mmax.z = near_slice + slice_thickness;

			put::camera_create_orthographic(&k_volume_raster_ortho, mmin.x, mmax.x, mmin.y, mmax.y, mmin.z, mmax.z);
			k_volume_raster_ortho.view = axis_swaps[current_axis];

			k_rasteriser_job.current_slice_aabb.min = k_volume_raster_ortho.view.transform_vector(mmin);
			k_rasteriser_job.current_slice_aabb.max = k_volume_raster_ortho.view.transform_vector(mmax);

			k_rasteriser_job.current_slice_aabb.min.z *= -1;
			k_rasteriser_job.current_slice_aabb.max.z *= -1;

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
			current_requested_slice = current_slice;
		}

		PEN_TRV sdf_generate(void* params)
		{
			pen::job_thread_params* job_params = (pen::job_thread_params*)params;
			vgt_sdf_job*			sdf_job = (vgt_sdf_job*)job_params->user_data;

			pen::job*		p_thread_info = job_params->job_info;
			pen::thread_semaphore_signal(p_thread_info->p_sem_continue, 1);

			u32 volume_dim = 1<<sdf_job->options.volume_dimension;

			//create a simple 3d texture
			u32 block_size = sdf_job->block_size;
			u32 data_size = volume_dim * volume_dim * volume_dim * block_size;

			u8* volume_data = (u8*)pen::memory_alloc(data_size);
			u32 row_pitch = volume_dim * block_size;
			u32 slice_pitch = volume_dim  * row_pitch;

			k_sdf_job.scene_extents = sdf_job->scene->renderable_extents;
			
            vec3f sd = k_sdf_job.scene_extents.max - k_sdf_job.scene_extents.min;
			k_sdf_job.scene_extents.min -= sd * k_sdf_job.padding;
			k_sdf_job.scene_extents.max += sd * k_sdf_job.padding;

			extents scene_extents = k_sdf_job.scene_extents;
			vec3f scene_dimension = scene_extents.max - scene_extents.min;

			sdf_job->volume_data = volume_data;
			sdf_job->volume_dim = volume_dim;
			sdf_job->data_size = data_size;
            
            std::vector<vec3f> vertices;
            std::vector<vec3ui> triangles;
            
            for (u32 n = 0; n < sdf_job->scene->nodes_size; ++n)
            {
                if (sdf_job->scene->entities[n] & CMP_GEOMETRY)
                {
                    geometry_resource* gr = get_geometry_resource(sdf_job->scene->id_geometry[n]);
                    
                    vec4f* vertex_positions = (vec4f*)gr->cpu_position_buffer;
                    
                    if(!gr->cpu_index_buffer || !vertex_positions)
                    {
                        dev_console_log_level(dev_ui::CONSOLE_ERROR,
                                              "[error] mesh %s does not have cpu vertex / triangle data",
                                              sdf_job->scene->names[n].c_str());
                        
                        continue;
                    }
                    
                    for(u32 i = 0; i < gr->num_vertices; ++i)
                    {
                        vec3f tv = sdf_job->scene->world_matrices[n].transform_vector(vertex_positions[i].xyz);
                        vertices.push_back(tv);
                    }
                    
                    for (u32 i = 0; i < gr->num_indices; i += 3)
                    {
                        if(gr->index_type == PEN_FORMAT_R32_UINT)
                        {
                            u32* indices = (u32*)gr->cpu_index_buffer;
                            u32 i0, i1, i2;
                            i0 = indices[i + 0];
                            i1 = indices[i + 1];
                            i2 = indices[i + 2];
                            triangles.push_back(vec3ui(i0, i1, i2));
                        }
                        else
                        {
                            u16* indices = (u16*)gr->cpu_index_buffer;
                            u16 i0, i1, i2;
                            i0 = indices[i + 0];
                            i1 = indices[i + 1];
                            i2 = indices[i + 2];
                            triangles.push_back(vec3ui(i0, i1, i2));
                        }
                    }
                }
            }
            
            pen::renderer_set_rasterizer_state(0);
            
            if(triangles.size() > 0)
            {
                f32 dx = component_wise_max(scene_dimension) / (f32)volume_dim;
                
                vec3f centre = scene_extents.min + ((scene_extents.max - scene_extents.min) / 2.0f);
                
                vec3f grid_origin = centre - vec3f(component_wise_max(scene_dimension)/2.0f);
                
                Array3f phi_grid;
                make_level_set3(triangles, vertices, grid_origin, dx,
                                volume_dim, volume_dim, volume_dim, phi_grid );
                
                for (u32 z = 0; z < volume_dim; ++z)
                {
                    for (u32 y = 0; y < volume_dim; ++y)
                    {
                        for (u32 x = 0; x < volume_dim; ++x)
                        {
                            u32 offset = z * slice_pitch + y * row_pitch + x * block_size;
                            
                            f32* f = (f32*)(&volume_data[offset + 0]);
                            
                            //non water tight meshes signs cannot be trusted
                            f32 tsf = phi_grid(x, y, z);
                            if( !sdf_job->trust_sign )
                                tsf = fabs(tsf);
                            
                            *f = tsf;
                        }
                    }
                }
            }
            else
            {
                dev_console_log_level(dev_ui::CONSOLE_ERROR,
                                      "%s", "[error] no triangles in scene to generate sdf");
            }

			if (p_thread_info->p_completion_callback)
				p_thread_info->p_completion_callback(nullptr);

			sdf_job->generate_in_progress = 2;
			return PEN_THREAD_OK;
		}

		static ces::entity_scene* k_main_scene;
		void init(ces::entity_scene* scene)
		{
			k_main_scene = scene;
			put::scene_controller cc;
			cc.camera = &k_volume_raster_ortho;
			cc.update_function = &volume_rasteriser_update;
			cc.name = "volume_rasteriser_camera";
			cc.id_name = PEN_HASH(cc.name.c_str());
			cc.scene = scene;

			pmfx::register_scene_controller(cc);
		}

		static vgt_options k_options;

		void rasterise_ui()
		{
			static const c8* axis_names[] =
			{
				"z+", "y+", "x+",
				"z-", "y-", "x-"
			};

			ImGui::Text("Rasterise Axes");

			for (u32 a = 0; a < k_num_axes; a++)
			{
				ImGui::CheckboxFlags(axis_names[a], &k_options.rasterise_axes, 1 << a);

				if (a < k_num_axes - 1)
					ImGui::SameLine();
			}

			static const c8* capture_data_names[] =
			{
				"Albedo",
				"Normals",
				"Baked Lighting",
				"Occupancy",
				"Custom"
			};

			ImGui::Combo("Capture", &k_options.capture_data, capture_data_names, PEN_ARRAY_SIZE(capture_data_names));

			if (!k_rasteriser_job.rasterise_in_progress)
			{
				if (ImGui::Button("Go"))
				{
					//setup new job
					k_rasteriser_job.options = k_options;
					u32 dim = 1 << k_rasteriser_job.options.volume_dimension;

					k_rasteriser_job.dimension = dim;
					k_rasteriser_job.current_axis = 0;
					k_rasteriser_job.current_slice = 0;

					//allocate cpu mem for rasterised slices
					for (u32 a = 0; a < 6; ++a)
					{
						//alloc slices array
						k_rasteriser_job.volume_slices[a] = (void**)pen::memory_alloc(k_rasteriser_job.dimension * sizeof(void**));

						//alloc slices mem
						for (u32 s = 0; s < k_rasteriser_job.dimension; ++s)
							k_rasteriser_job.volume_slices[a][s] = pen::memory_alloc(pow(k_rasteriser_job.dimension, 2) * 4);
					}

					//flag to start reasterising
					k_rasteriser_job.rasterise_in_progress = true;
				}
			}
			else
			{
				ImGui::Separator();

				ImGui::Text("Progress");

				if (k_rasteriser_job.combine_in_progress > 0)
				{
                    f32 progress = (f32)k_rasteriser_job.combine_position / (f32)pow(k_rasteriser_job.dimension, 3);
					ImGui::ProgressBar(progress);
				}
				else
				{
					ImGui::Text("Rasterising");

					ImGui::Text("Axis %i", k_rasteriser_job.current_axis);
					ImGui::Text("Slice %i", k_rasteriser_job.current_slice);

					static hash_id id_volume_raster_rt = PEN_HASH("volume_raster");
					const pmfx::render_target* volume_rt = pmfx::get_render_target(id_volume_raster_rt);
					ImGui::Image((void*)&volume_rt->handle, ImVec2(256, 256));

					put::dbg::add_aabb(k_rasteriser_job.current_slice_aabb.min,
                                       k_rasteriser_job.current_slice_aabb.max, vec4f::cyan());
				}
			}
		}

		void sdf_ui()
		{
			static const c8* texture_fromat[] =
			{
				"8bit",
				"32bit Floating Point",
			};

			static s32 sdf_texture_format = 1;
			ImGui::Combo("Capture", &sdf_texture_format, texture_fromat, PEN_ARRAY_SIZE(texture_fromat));
            ImGui::Checkbox("Signed", &k_sdf_job.trust_sign);
            ImGui::InputFloat("Padding", &k_sdf_job.padding);
            
			if (!k_sdf_job.generate_in_progress)
			{
				if (ImGui::Button("Go"))
				{
					if (sdf_texture_format == 0)
					{
						k_sdf_job.block_size = 1;
						k_sdf_job.texture_format = PEN_TEX_FORMAT_R8_UNORM;
					}
					else
					{
						k_sdf_job.block_size = 4;
						k_sdf_job.texture_format = PEN_TEX_FORMAT_R32_FLOAT;
					}

					k_sdf_job.generate_in_progress = 1;
					k_sdf_job.scene = k_main_scene;
					k_sdf_job.options = k_options;

					pen::thread_create_job(sdf_generate, 1024 * 1024 * 1024, &k_sdf_job, pen::THREAD_START_DETACHED);
					return;
				}
			}
			else
			{
				ImGui::ProgressBar((g_mls_progress.triangles + g_mls_progress.sweeps) * 0.5f, ImVec2(-1,0));

				if (k_sdf_job.generate_in_progress == 2)
				{
					//create texture
					u32 volume_texture = create_volume_from_data(	k_sdf_job.volume_dim,
																	k_sdf_job.block_size,
																	k_sdf_job.data_size,
																	k_sdf_job.texture_format,
																	k_sdf_job.volume_data);

					//create material for volume sdf sphere trace
					material_resource* sdf_material = new material_resource;
					sdf_material->material_name = "volume_sdf_material";
					sdf_material->shader_name = "pmfx_utility";
					sdf_material->id_shader = PEN_HASH("pmfx_utility");
					sdf_material->id_technique = PEN_HASH("volume_sdf");
					sdf_material->id_sampler_state[SN_VOLUME_TEXTURE] = PEN_HASH("clamp_linear_sampler_state");
					sdf_material->texture_handles[SN_VOLUME_TEXTURE] = volume_texture;
					add_material_resource(sdf_material);

					geometry_resource* cube = get_geometry_resource(PEN_HASH("cube"));

                    f32 single_scale = component_wise_max((k_sdf_job.scene_extents.max - k_sdf_job.scene_extents.min) / 2.0f);
					vec3f scale = vec3f(single_scale);
					vec3f pos = k_sdf_job.scene_extents.min + scale;

					u32 new_prim = get_new_node(k_main_scene);
					k_main_scene->names[new_prim] = "volume";
					k_main_scene->names[new_prim].appendf("%i", new_prim);
					k_main_scene->transforms[new_prim].rotation = quat();
					k_main_scene->transforms[new_prim].scale = scale;
					k_main_scene->transforms[new_prim].translation = pos;
					k_main_scene->entities[new_prim] |= CMP_TRANSFORM | CMP_SDF_SHADOW;
					k_main_scene->parents[new_prim] = new_prim;
					instantiate_geometry(cube, k_main_scene, new_prim);
					instantiate_material(sdf_material, k_main_scene, new_prim);
					instantiate_model_cbuffer(k_main_scene, new_prim);

					//add shadow receiver
					material_resource* sdf_shadow_material = new material_resource;
					sdf_shadow_material->material_name = "shadow_sdf_material";
					sdf_shadow_material->shader_name = "pmfx_utility";
					sdf_shadow_material->id_shader = PEN_HASH("pmfx_utility");
					sdf_shadow_material->id_technique = PEN_HASH("shadow_sdf");
					sdf_shadow_material->id_sampler_state[SN_VOLUME_TEXTURE] = PEN_HASH("clamp_linear_sampler_state");
					sdf_shadow_material->texture_handles[SN_VOLUME_TEXTURE] = volume_texture;
					add_material_resource(sdf_shadow_material);

					new_prim = get_new_node(k_main_scene);
					k_main_scene->names[new_prim] = "volume_receiever";
					k_main_scene->names[new_prim].appendf("%i", new_prim);
					k_main_scene->transforms[new_prim].rotation = quat();
					k_main_scene->transforms[new_prim].scale = vec3f(10, 1, 10);
					k_main_scene->transforms[new_prim].translation = vec3f(0, -1, 0);
					k_main_scene->entities[new_prim] |= CMP_TRANSFORM;
					k_main_scene->parents[new_prim] = new_prim;
					instantiate_geometry(cube, k_main_scene, new_prim);
					instantiate_material(sdf_shadow_material, k_main_scene, new_prim);
					instantiate_model_cbuffer(k_main_scene, new_prim);

					k_sdf_job.generate_in_progress = 0;
				}
			}
		}

		void show_dev_ui()
		{
			//main menu option -------------------------------------------------
			ImGui::BeginMainMenuBar();

			static bool open_vgt = false;
			if (ImGui::Button(ICON_FA_CUBE))
			{
				open_vgt = true;
			}
			put::dev_ui::set_tooltip("Volume Generator");

			ImGui::EndMainMenuBar();

			//volume generator ui -----------------------------------------------
			if (open_vgt)
			{
				ImGui::Begin("Volume Generator", &open_vgt, ImGuiWindowFlags_AlwaysAutoResize);

				//choose resolution
				static const c8* dimensions[] =
				{
					"1", "2", "4", "8", "16", "32", "64", "128", "256", "512"
				};

				ImGui::Combo("Resolution", &k_options.volume_dimension, dimensions, PEN_ARRAY_SIZE(dimensions));

				ImGui::SameLine();

				float size_mb = (pow(1 << k_options.volume_dimension, 3) * 4) / 1024 / 1024;

				ImGui::LabelText("Size", "%.2f(mb)", size_mb);

				//choose volume data type
				static const c8* volume_type[] =
				{
					"Rasterised Texels", 
					"Signed Distance Field"
				};

				ImGui::Combo("Type", &k_options.volume_type, volume_type, PEN_ARRAY_SIZE(volume_type));

				ImGui::Separator();

				if (k_options.volume_type == VOLUME_RASTERISED_TEXELS)
				{
					rasterise_ui();
				}
				else if (k_options.volume_type == VOLUME_SIGNED_DISTANCE_FIELD)
				{
					sdf_ui();
				}

				ImGui::End();
			}
		}

		void post_update()
		{
			static u32 dim = 128;
			static hash_id id_volume_raster_rt = PEN_HASH("volume_raster");
			static hash_id id_volume_raster_ds = PEN_HASH("volume_raster_ds");

			u32 cur_dim = 1 << k_options.volume_dimension;

			//resize targets
			if(cur_dim != dim)
			{ 
				dim = cur_dim;

				pmfx::resize_render_target(id_volume_raster_rt, dim, dim, "rgba8");
				pmfx::resize_render_target(id_volume_raster_ds, dim, dim, "d24s8");
				pmfx::resize_viewports();

				pen::renderer_consume_cmd_buffer();
			}
		}
	}
}


