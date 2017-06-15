#include <functional>
#include <string>

#include "component_entity.h"
#include "file_system.h"
#include "dev_ui.h"
#include "debug_render.h"
#include "layer_controller.h"
#include "pmfx.h"

using namespace put;

namespace put
{
	namespace ces
	{

#define ALLOC_COMPONENT_ARRAY( SCENE, COMPONENT, TYPE )											\
		if( !SCENE->COMPONENT )																	\
			SCENE->COMPONENT = (TYPE*)pen::memory_alloc(sizeof(TYPE)*SCENE->nodes_size );		\
		else																					\
			SCENE->COMPONENT = (TYPE*)pen::memory_realloc(SCENE->COMPONENT,sizeof(TYPE)*SCENE->nodes_size); \
		pen::memory_zero( SCENE->COMPONENT, sizeof(TYPE)*SCENE->nodes_size)

#define FREE_COMPONENT_ARRAY( SCENE, COMPONENT ) pen::memory_free( SCENE->COMPONENT ); SCENE->COMPONENT = nullptr

#define SCENE_DIR_MACRO "data/models/%s/scene.pms"
#define GEOM_DIR_MACRO "data/models/%s/%s.pmg"
#define MATERIAL_DIR_MACRO "data/models/%s/materials/%s.pmm"

		struct component_entity_scene_instance
		{
			u32 id_name;
			const c8* name;
			component_entity_scene* scene;
		};

		std::vector<component_entity_scene_instance> k_scenes;
        
        struct per_model_cbuffer
        {
            mat4 world_matrix;
        };

		void resize_scene_buffers(component_entity_scene* scene)
		{
			scene->nodes_size += 1024;
			
			//ids
			ALLOC_COMPONENT_ARRAY(scene, id_name, u32);
			ALLOC_COMPONENT_ARRAY(scene, id_geometry, u32);
			ALLOC_COMPONENT_ARRAY(scene, id_material, u32);

			//components
			ALLOC_COMPONENT_ARRAY(scene, entities, a_u64);
			ALLOC_COMPONENT_ARRAY(scene, parents, u32);
			ALLOC_COMPONENT_ARRAY(scene, geometries, scene_node_geometry);
			ALLOC_COMPONENT_ARRAY(scene, materials, scene_node_material);
			ALLOC_COMPONENT_ARRAY(scene, local_matrices, mat4);
			ALLOC_COMPONENT_ARRAY(scene, world_matrices, mat4);
			ALLOC_COMPONENT_ARRAY(scene, offset_matrices, mat4);
			ALLOC_COMPONENT_ARRAY(scene, physics_matrices, mat4);
			ALLOC_COMPONENT_ARRAY(scene, physics_handles, u32);
			ALLOC_COMPONENT_ARRAY(scene, multibody_handles, u32);
			ALLOC_COMPONENT_ARRAY(scene, multibody_link, s32);
			ALLOC_COMPONENT_ARRAY(scene, physics_data, scene_node_physics);
            ALLOC_COMPONENT_ARRAY(scene, cbuffer, u32);
            
            for( u32 i = 0; i < scene->nodes_size; ++i )
            {
                scene->cbuffer[i] = PEN_INVALID_HANDLE;
            }

			//display info - could be disabled in shipable builds
			ALLOC_COMPONENT_ARRAY(scene, names, c8*);
			ALLOC_COMPONENT_ARRAY(scene, geometry_names, c8*);
			ALLOC_COMPONENT_ARRAY(scene, material_names, c8*);
		}

		void free_scene_buffers( component_entity_scene* scene )
		{
			FREE_COMPONENT_ARRAY(scene, id_name);
			FREE_COMPONENT_ARRAY(scene, id_geometry);
			FREE_COMPONENT_ARRAY(scene, id_material);

			FREE_COMPONENT_ARRAY(scene, entities);
			FREE_COMPONENT_ARRAY(scene, parents);
			FREE_COMPONENT_ARRAY(scene, geometries);
			FREE_COMPONENT_ARRAY(scene, materials);
			FREE_COMPONENT_ARRAY(scene, local_matrices);
			FREE_COMPONENT_ARRAY(scene, world_matrices);
			FREE_COMPONENT_ARRAY(scene, offset_matrices);
			FREE_COMPONENT_ARRAY(scene, physics_matrices);
			FREE_COMPONENT_ARRAY(scene, physics_handles);
			FREE_COMPONENT_ARRAY(scene, multibody_handles);
			FREE_COMPONENT_ARRAY(scene, multibody_link);
			FREE_COMPONENT_ARRAY(scene, physics_data);

			//free name memory
			for (u32 i = 0; i < scene->num_nodes; ++i)
			{
				pen::memory_free(scene->names[i]);
				pen::memory_free(scene->geometry_names[i]);
				pen::memory_free(scene->material_names[i]);
			}

			FREE_COMPONENT_ARRAY(scene, names);
			FREE_COMPONENT_ARRAY(scene, geometry_names);
			FREE_COMPONENT_ARRAY(scene, material_names);
		}

		component_entity_scene*	create_scene( const c8* name )
		{
			component_entity_scene_instance new_instance;
			new_instance.name = name;
			new_instance.scene = new component_entity_scene();

			k_scenes.push_back(new_instance);

			resize_scene_buffers(new_instance.scene);

			return new_instance.scene;
		}

		u32 read_parsable_char(char** buf, u32* data_start)
		{
			u32 u32s_read = 0;
			u32 num_chars = *data_start++;
			u32s_read++;

			c8 _buf[MAX_SCENE_NODE_CHARS];
			for (u32 c = 0; c < num_chars; ++c)
			{
				if (c < MAX_SCENE_NODE_CHARS)
				{
					_buf[c] = (c8)*data_start;
				}

				PEN_ASSERT(c < MAX_SCENE_NODE_CHARS);

				data_start++;
				u32s_read++;
			}

			if (num_chars >= MAX_SCENE_NODE_CHARS)
			{
				num_chars = MAX_SCENE_NODE_CHARS - 1;
			}

			_buf[num_chars] = '\0';


			*buf = (c8*)pen::memory_alloc(num_chars + 1);

			pen::memory_cpy(*buf, &_buf[0], num_chars + 1);

			return u32s_read;
		}

		void load_mesh(const c8* filename, scene_node_geometry* p_geometries, scene_node_physics* p_physics, component_entity_scene* scene, u32 node_index, const c8** material_names)
		{
			//mesh data test
			void* mesh_file;
			u32   mesh_file_size;
			pen::filesystem_read_file_to_buffer(filename, &mesh_file, mesh_file_size);

			u32* p_reader = (u32*)mesh_file;
			u32 version = *p_reader++;
			u32 num_meshes = *p_reader++;

			u32 collision_mesh = 1;

			physics::collision_mesh_data* temp_collision_data;
			if (collision_mesh)
			{
				temp_collision_data = (physics::collision_mesh_data*)pen::memory_alloc(sizeof(physics::collision_mesh_data) * num_meshes);
			}

			//map mesh material id's to material file id's
			c8* mat_name_buf;
			for (u32 i = 0; i < num_meshes; ++i)
			{
				u32 u32s_read = read_parsable_char(&mat_name_buf, p_reader);
				p_reader += u32s_read;

				p_geometries[i].submesh_material_index = -1;

				for (u32 j = 0; j < num_meshes; ++j)
				{
					if (pen::string_compare(mat_name_buf, material_names[j]) == 0)
					{
						p_geometries[i].submesh_material_index = j;
						break;
					}
				}

				PEN_ASSERT(p_geometries[i].submesh_material_index != -1);
			}
			pen::memory_free(mat_name_buf);

			for (u32 submesh = 0; submesh < num_meshes; ++submesh)
			{
				//physics
				u32 collision_shape = *p_reader++;
				u32 collision_dynamic = *p_reader++;

				vec3f min_extents;
				vec3f max_extents;

				pen::memory_cpy(&min_extents, p_reader, sizeof(vec3f));
				p_reader += 3;

				pen::memory_cpy(&max_extents, p_reader, sizeof(vec3f));
				p_reader += 3;

				p_physics[submesh].min_extents = min_extents;
				p_physics[submesh].max_extents = max_extents;
				p_physics[submesh].centre = min_extents + ((max_extents - min_extents) / 2.0f);
				p_physics[submesh].collision_shape = collision_shape;
				p_physics[submesh].collision_dynamic = collision_dynamic;

				//vb and ib
				u32 num_pos_floats = *p_reader++;
				u32 num_floats = *p_reader++;
				u32 num_indices = *p_reader++;
				u32 num_collision_floats = *p_reader++;
				u32 skinned = *p_reader++;

				u32 index_size = num_indices < 65535 ? 2 : 4;

				u32 vertex_size = sizeof(vertex_model);

				if (skinned)
				{
					vertex_size = sizeof(vertex_model_skinned);

					p_geometries[submesh].p_skin = (scene_node_skin*)pen::memory_alloc(sizeof(scene_node_skin));

					pen::memory_cpy(&p_geometries[submesh].p_skin->bind_shape_matirx, p_reader, sizeof(mat4));
					p_reader += 16;

					mat4 max_swap;
					max_swap.create_axis_swap(vec3f(1.0f, 0.0f, 0.0f), vec3f(0.0f, 0.0f, -1.0f), vec3f(0.0f, 1.0f, 0.0f));
					mat4 max_swap_inv = max_swap.inverse4x4();

					mat4 final_bind = max_swap * p_geometries[submesh].p_skin->bind_shape_matirx * max_swap_inv;

					p_geometries[submesh].p_skin->bind_shape_matirx = final_bind;

					u32 num_ijb_floats = *p_reader++;
					pen::memory_cpy(&p_geometries[submesh].p_skin->joint_bind_matrices[0], p_reader, sizeof(f32) * num_ijb_floats);
					p_reader += num_ijb_floats;

					p_geometries[submesh].p_skin->num_joints = num_ijb_floats / 16;

					for (u32 joint = 0; joint < p_geometries[submesh].p_skin->num_joints; ++joint)
					{
						p_geometries[submesh].p_skin->joint_bind_matrices[joint] = max_swap * p_geometries[submesh].p_skin->joint_bind_matrices[joint] * max_swap_inv;
						p_geometries[submesh].p_skin->joint_matrices[joint].create_identity();
					}

					scene->entities[node_index] |= CMP_SKINNED;
				}

				//all vertex data is written out as 4 byte ints
				u32 num_verts = num_floats / (vertex_size / sizeof(u32));

				p_geometries[submesh].num_vertices = num_verts;

				pen::buffer_creation_params bcp;
				bcp.usage_flags = PEN_USAGE_DEFAULT;
				bcp.bind_flags = PEN_BIND_VERTEX_BUFFER;
				bcp.cpu_access_flags = 0;
				bcp.buffer_size = sizeof(vertex_position) * num_verts;
				bcp.data = (void*)p_reader;

				p_geometries[submesh].position_buffer = pen::renderer_create_buffer(bcp);

				p_reader += bcp.buffer_size / sizeof(f32);

				bcp.buffer_size = vertex_size * num_verts;
				bcp.data = (void*)p_reader;

				p_geometries[submesh].vertex_buffer = pen::renderer_create_buffer(bcp);

				p_reader += bcp.buffer_size / sizeof(u32);

				if (skinned)
				{
					//create an empty buffer for stream out
					bcp.buffer_size = sizeof(vertex_model) * num_verts;
					bcp.bind_flags |= PEN_BIND_STREAM_OUTPUT;

					void* p_data = pen::memory_alloc(sizeof(vertex_model) * num_verts);
					pen::memory_zero(p_data, sizeof(vertex_model) * num_verts);

					bcp.data = p_data;

					u32 vb = pen::renderer_create_buffer(bcp);

					bcp.buffer_size = sizeof(vertex_position) * num_verts;

					u32 pb = pen::renderer_create_buffer(bcp);

					p_geometries[submesh].skinned_position = p_geometries[submesh].position_buffer;
					p_geometries[submesh].pre_skin_vertex_buffer = p_geometries[submesh].vertex_buffer;

					p_geometries[submesh].vertex_buffer = vb;
					p_geometries[submesh].position_buffer = pb;

					pen::memory_free(p_data);
				}

				bcp.usage_flags = PEN_USAGE_DEFAULT;
				bcp.bind_flags = PEN_BIND_INDEX_BUFFER;
				bcp.cpu_access_flags = 0;
				bcp.buffer_size = index_size * num_indices;
				bcp.data = (void*)p_reader;

				p_geometries[submesh].num_indices = num_indices;
				p_geometries[submesh].index_type = index_size == 2 ? PEN_FORMAT_R16_UINT : PEN_FORMAT_R32_UINT;
				p_geometries[submesh].index_buffer = pen::renderer_create_buffer(bcp);

				p_reader += bcp.buffer_size / sizeof(u32);

				//collision mesh
				if (collision_mesh)
				{
					temp_collision_data[submesh].vertices = (f32*)pen::memory_alloc(sizeof(f32) * num_collision_floats);
					temp_collision_data[submesh].num_floats = num_collision_floats;

					pen::memory_cpy(temp_collision_data[submesh].vertices, p_reader, sizeof(f32) * num_collision_floats);
				}

				p_reader += num_collision_floats;
			}

			if (collision_mesh)
			{
				u32 total_num_floats = 0;
				for (u32 s = 0; s < num_meshes; ++s)
				{
					total_num_floats += temp_collision_data[s].num_floats;
				}

				p_physics->mesh_data.vertices = (f32*)pen::memory_alloc(sizeof(f32) * total_num_floats);
				p_physics->mesh_data.indices = (u32*)pen::memory_alloc(sizeof(u32) * total_num_floats);

				p_physics->mesh_data.num_indices = total_num_floats;
				p_physics->mesh_data.num_floats = total_num_floats;

				f32* p_vertices = p_physics->mesh_data.vertices;

				for (u32 s = 0; s < num_meshes; ++s)
				{
					pen::memory_cpy(p_vertices, temp_collision_data[s].vertices, sizeof(f32) * temp_collision_data[s].num_floats);

					p_vertices += temp_collision_data[s].num_floats;
				}

				for (u32 v = 0; v < total_num_floats; ++v)
				{
					p_physics->mesh_data.indices[v] = v;
				}

				//centralise mesh
				vec3f avg = vec3f::zero();
				for (u32 v = 0; v < total_num_floats; v += 3)
				{
					avg.x += p_physics->mesh_data.vertices[v + 0];
					avg.y += p_physics->mesh_data.vertices[v + 1];
					avg.z += p_physics->mesh_data.vertices[v + 2];
				}

				avg /= (f32)(total_num_floats / 3);
				for (u32 v = 0; v < total_num_floats; v += 3)
				{
					p_physics->mesh_data.vertices[v + 0] -= avg.x;
					p_physics->mesh_data.vertices[v + 1] -= avg.y;
					p_physics->mesh_data.vertices[v + 2] -= avg.z;
				}

				for (u32 sm = 0; sm < num_meshes; ++sm)
				{
					pen::memory_free(temp_collision_data[sm].vertices);
				}

				pen::memory_free(temp_collision_data);
			}

			pen::memory_free(mesh_file);
		}

		void load_material(const c8* filename, scene_node_material* p_mat)
		{
			void* material_file;
			u32   material_file_size;
			pen::filesystem_read_file_to_buffer(filename, &material_file, material_file_size);

			u32* p_reader = (u32*)material_file;

			u32 version = *p_reader++;

			//diffuse
			pen::memory_cpy(&p_mat->diffuse_rgb_shininess, p_reader, sizeof(vec4f));
			p_reader += 4;

			//specular
			pen::memory_cpy(&p_mat->specular_rgb_reflect, p_reader, sizeof(vec4f));
			p_reader += 4;

			//shininess
			pen::memory_cpy(&p_mat->diffuse_rgb_shininess.w, p_reader, sizeof(f32));
			p_reader++;

			//reflectivity
			pen::memory_cpy(&p_mat->specular_rgb_reflect.w, p_reader, sizeof(f32));
			p_reader++;

			u32 u32s_read = 0;
			for (u32 map = 0; map < put::ces::SN_NUM_TEXTURES; ++map)
			{
				c8* map_buffer;
				u32s_read = read_parsable_char(&map_buffer, p_reader);
				p_reader += u32s_read;

				if (u32s_read > 1)
				{
					p_mat->texture_id[map] = put::load_texture(map_buffer);
				}
				else
				{
					p_mat->texture_id[map] = -1;
				}

				pen::memory_free(map_buffer);
			}

			pen::memory_free(material_file);

			return;
		}

		void import_model_scene(const c8* model_scene_name, component_entity_scene* scene)
		{
			void* scene_file;
			u32   scene_file_size;

			c8 scene_filename[128];
			pen::string_format(&scene_filename[0], 128, SCENE_DIR_MACRO, model_scene_name);

			pen_error err = pen::filesystem_read_file_to_buffer(scene_filename, &scene_file, scene_file_size);

            if( err != PEN_ERR_OK )
            {
				//failed load file
				PEN_ASSERT(0);
            }
            
			u32* p_u32reader = (u32*)scene_file;
			u32 version = *p_u32reader++;
			u32 num_import_nodes = *p_u32reader++;

			u32 node_zero_offset = scene->num_nodes;
			u32 current_node = node_zero_offset;
			u32 inserted_nodes = 0;

			for (u32 n = 0; n < num_import_nodes; ++n)
			{
				u32 node_type = *p_u32reader++;

				p_u32reader += read_parsable_char( &scene->names[current_node], p_u32reader);
				p_u32reader += read_parsable_char( &scene->geometry_names[current_node], p_u32reader);

				u32 num_meshes = *p_u32reader++;

				scene_node_material* p_materials = NULL;
				c8** p_material_names;
				c8** p_material_symbols;

				//material pre load
				if (num_meshes > 0)
				{
					p_materials = (scene_node_material*)pen::memory_alloc(sizeof(scene_node_material) * num_meshes);

					p_material_names = (c8**)pen::memory_alloc(sizeof(c8*) * num_meshes);
					p_material_symbols = (c8**)pen::memory_alloc(sizeof(c8*) * num_meshes);

					//read in material filenames
					for (u32 mat = 0; mat < num_meshes; ++mat)
					{
						p_u32reader += read_parsable_char(&p_material_names[mat], p_u32reader);
					}

					//material symbol names
					for (u32 mat = 0; mat < num_meshes; ++mat)
					{
						p_u32reader += read_parsable_char(&p_material_symbols[mat], p_u32reader);
					}

					//load materials
					for (u32 mat = 0; mat < num_meshes; ++mat)
					{
						c8 material_filename[128];
						pen::string_format(&material_filename[0], 128, MATERIAL_DIR_MACRO, model_scene_name, p_material_names[mat]);

						load_material(material_filename, &p_materials[mat]);
					}
				}

				//transformation load
				u32 parent = *p_u32reader++ + node_zero_offset + inserted_nodes;
				scene->parents[current_node] = parent;
				u32 transforms = *p_u32reader++;

				vec3f translation;
				vec4f rotations[3];
				u32	  num_rotations = 0;

				for (u32 t = 0; t < transforms; ++t)
				{
					u32 type = *p_u32reader++;

					if (type == 0)
					{
						//translate
						pen::memory_cpy(&translation, p_u32reader, 12);
						p_u32reader += 3;
					}
					else if (type == 1)
					{
						//rotate
						pen::memory_cpy(&rotations[num_rotations], p_u32reader, 16);

						//convert to radians
						rotations[num_rotations].w = put::maths::deg_to_rad(rotations[num_rotations].w);

						static f32 zero_rotation_epsilon = 0.000001f;
						if (rotations[num_rotations].w < zero_rotation_epsilon && rotations[num_rotations].w > zero_rotation_epsilon)
						{
							rotations[num_rotations].w = 0.0f;
						}

						num_rotations++;

						p_u32reader += 4;
					}
					else
					{
						//unsupported transform type
						PEN_ASSERT(0);
					}
				}

				quat final_rotation;
				if (num_rotations == 0)
				{
					//no rotation
					final_rotation.euler_angles(0.0f, 0.0f, 0.0f);
				}
				else if (num_rotations == 1)
				{
					//axis angle
					final_rotation.axis_angle(rotations[0]);
				}
				else if (num_rotations == 3)
				{
					//euler angles
					f32 z_theta = 0;
					f32 y_theta = 0;
					f32 x_theta = 0;

					for (u32 r = 0; r < 3; ++r)
					{
						if (rotations[r].z == 1.0f)
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

						final_rotation.euler_angles(z_theta, y_theta, x_theta);
						vec3f corrected_euler = vec3f(x_theta, y_theta, z_theta);
					}
				}

				scene->physics_data[current_node].collision_shape = 0;

				scene_node_geometry* p_geometries = NULL;
				scene_node_physics* p_physics = NULL;

				//mesh load
				if (num_meshes > 0)
				{
					p_geometries = (scene_node_geometry*)pen::memory_alloc(sizeof(scene_node_geometry) * num_meshes);
					p_physics = (scene_node_physics*)pen::memory_alloc(sizeof(scene_node_physics) * num_meshes);

					if (node_type == NODE_TYPE_GEOM)
					{
						//load geom file
						c8 filename[128];
						pen::string_format(&filename[0], 128, GEOM_DIR_MACRO, model_scene_name, scene->geometry_names[current_node]);

						load_mesh(filename, p_geometries, p_physics, scene, current_node, (const c8**)p_material_symbols);
					}
				}

				//make a transform matrix for geometry
				mat4 rot_mat;
				final_rotation.get_matrix(rot_mat);

				mat4 translation_mat;
				translation_mat.create_translation(translation);

				scene->local_matrices[current_node] = (translation_mat * rot_mat);

				//store intial position for physics to hook into later
				scene->physics_data[current_node].start_position = translation;
				scene->physics_data[current_node].start_rotation = final_rotation;

				//clone sub meshes to own scene node
				if (node_type == NODE_TYPE_GEOM)
				{
					scene->entities[current_node] |= CMP_GEOMETRY;
					scene->entities[current_node] |= CMP_MATERIAL;
				}

				//assign geometry, materials and physics
				u32 dest = current_node;
				if (num_meshes > 0)
				{
					for (u32 submesh = 0; submesh < num_meshes; ++submesh)
					{
						dest = current_node + submesh;

						if (submesh > 0)
						{
							inserted_nodes++;
							clone_node( scene, current_node, dest, current_node);
							scene->local_matrices[dest].create_identity();
						}

						scene->geometries[dest] = p_geometries[submesh];
						scene->materials[dest] = p_materials[p_geometries[submesh].submesh_material_index];

						scene->material_names[dest] = p_material_names[p_geometries[submesh].submesh_material_index];

						p_physics[submesh].start_position = translation;
						p_physics[submesh].start_rotation = final_rotation;

						scene->physics_data[dest] = p_physics[submesh];
					}

					for (u32 m = 0; m < num_meshes; ++m)
					{
						pen::memory_free(p_material_symbols[m]);
					}

					//delete temp data
					pen::memory_free(p_material_symbols);
					pen::memory_free(p_material_names);
					pen::memory_free(p_geometries);
					pen::memory_free(p_materials);
					pen::memory_free(p_physics);
				}

				current_node = dest + 1;
				scene->num_nodes = current_node;
			}
		}

		void clone_node( component_entity_scene* scene, u32 src, u32 dst, s32 parent, vec3f offset, const c8* suffix)
		{
			component_entity_scene* p_sn = scene;

			u32 buffer_size = pen::string_length(p_sn->names[src]) + pen::string_length(suffix) + 1;

			p_sn->names[dst] = (c8*)pen::memory_alloc(buffer_size);

			pen::memory_cpy(&p_sn->names[dst][0], &p_sn->names[src][0], buffer_size);
			pen::string_concatonate(&p_sn->names[dst][0], suffix, buffer_size);

			p_sn->geometry_names[dst] = p_sn->geometry_names[src];
			p_sn->material_names[dst] = p_sn->material_names[src];

			u32 parent_offset = p_sn->parents[src] - src;
			if (parent == -1)
			{
				p_sn->parents[dst] = dst - parent_offset;
			}
			else
			{
				p_sn->parents[dst] = parent;
			}

			pen::memory_cpy(&p_sn->entities[dst], &p_sn->entities[src], sizeof(a_u64));

			p_sn->geometries[dst] = p_sn->geometries[src];
			p_sn->materials[dst] = p_sn->materials[src];
			p_sn->local_matrices[dst] = p_sn->local_matrices[src];
			p_sn->world_matrices[dst] = p_sn->world_matrices[src];
			p_sn->offset_matrices[dst] = p_sn->offset_matrices[src];
			p_sn->physics_matrices[dst] = p_sn->physics_matrices[src];
			p_sn->physics_handles[dst] = p_sn->physics_handles[src];
			p_sn->multibody_handles[dst] = p_sn->multibody_handles[src];
			p_sn->multibody_link[dst] = p_sn->multibody_link[src];
			p_sn->physics_data[dst] = p_sn->physics_data[src];

			if (dst >= p_sn->num_nodes)
			{
				p_sn->num_nodes = dst + 1;
			}

			p_sn->physics_data[dst].start_position += offset;

			vec3f right = p_sn->local_matrices[dst].get_right();
			vec3f up = p_sn->local_matrices[dst].get_up();
			vec3f fwd = p_sn->local_matrices[dst].get_fwd();
			vec3f translation = p_sn->local_matrices[dst].get_translation();

			p_sn->local_matrices[dst].set_vectors(right, up, fwd, translation + offset);
		}

		void enumerate_scene_ui(component_entity_scene* scene, bool* open )
		{
			ImGui::Begin("Scene Browser", open );

			ImGui::BeginChild("Entities", ImVec2(400, 400), true );

			static s32 selected_index = -1;
			for (u32 i = 0; i < scene->num_nodes; ++i)
			{
				bool selected = false;
				ImGui::Selectable(scene->names[i], &selected);

				if (selected)
				{
					selected_index = i;
				}
			}

			ImGui::EndChild();

			ImGui::SameLine();

			ImGui::BeginChild("Selected", ImVec2(400, 400), true );

			if (selected_index != -1)
			{
				//header
				ImGui::Text("%s", scene->names[selected_index]);
				ImGui::Separator();

				//geom
				ImGui::Text("Geometry: %s", scene->geometry_names[selected_index]);
				ImGui::Separator();

				//material
				ImGui::Text("Materal: %s", scene->material_names[selected_index]);

				if (scene->material_names[selected_index])
				{
					for (u32 t = 0; t < put::ces::SN_NUM_TEXTURES; ++t)
					{
						if (scene->materials[selected_index].texture_id[t] > 0)
						{
							if (t > 0)
								ImGui::SameLine();

							ImGui::Image(&scene->materials[selected_index].texture_id[t], ImVec2(128, 128));
						}
					}
				}
				ImGui::Separator();

				ImGui::Text("Physics: %s", scene->material_names[selected_index]);
			}

			ImGui::EndChild();

			ImGui::End();
		}
        
		void render_scene_view( const scene_view& view, scene_render_type render_type )
		{            
			component_entity_scene* scene = view.scene;
            
            static pmfx::shader_program* shp_debug = pmfx::load_shader_program("model_debug");
            
            pen::renderer_set_constant_buffer(view.cb_view, 0, PEN_SHADER_TYPE_VS);
            
            static bool first = true;
            
			for (u32 n = 0; n < scene->num_nodes; ++n)
			{
				if (scene->entities[n] & CMP_GEOMETRY && scene->entities[n] & CMP_MATERIAL && (!(scene->entities[n] & CMP_PHYSICS)) )
				{
					scene_node_geometry* p_geom = &scene->geometries[n];
					scene_node_material* p_mat = &scene->materials[n];

                    //move this to update / bake static
                    if( first )
                    {
                        per_model_cbuffer cb =
                        {
                            scene->world_matrices[n]
                        };
                        
                        if (scene->cbuffer[n] == PEN_INVALID_HANDLE)
                        {
                            pen::buffer_creation_params bcp;
                            bcp.usage_flags = PEN_USAGE_DYNAMIC;
                            bcp.bind_flags = PEN_BIND_CONSTANT_BUFFER;
                            bcp.cpu_access_flags = PEN_CPU_ACCESS_WRITE;
                            bcp.buffer_size = sizeof(per_model_cbuffer);
                            bcp.data = nullptr;
                            
                            scene->cbuffer[n] = pen::renderer_create_buffer(bcp);
                        }
                        
                        //per object world matrix
                        pen::renderer_update_buffer(scene->cbuffer[n], &cb, sizeof(per_model_cbuffer));
                    }
                    
                    //set shader
                    pen::renderer_set_shader(shp_debug->vertex_shader, PEN_SHADER_TYPE_VS);
                    pen::renderer_set_shader(shp_debug->pixel_shader, PEN_SHADER_TYPE_PS);
                    pen::renderer_set_input_layout(shp_debug->input_layout);
                    
					pen::renderer_set_constant_buffer(scene->cbuffer[n], 1, PEN_SHADER_TYPE_VS);

					//set ib / vb
					pen::renderer_set_vertex_buffer(p_geom->vertex_buffer, 0, sizeof(vertex_model), 0 );
					pen::renderer_set_index_buffer(p_geom->index_buffer, p_geom->index_type, 0);

					//set textures
					if (p_mat)
					{
						for (u32 t = 0; t < put::ces::SN_NUM_TEXTURES; ++t)
						{
							if (p_mat->texture_id[t])
							{
								pen::renderer_set_texture(p_mat->texture_id[t], put::layer_controller_built_in_handles().sampler_linear_wrap, t, PEN_SHADER_TYPE_PS );
							}
						}
					}

					//draw
					pen::renderer_draw_indexed(scene->geometries[n].num_indices, 0, 0, PEN_PT_TRIANGLELIST);
                    
				}
			}

            first = false;
            
			//pen::renderer_consume_cmd_buffer();

			/*
			u32 num_modes = get_num_nodes();

			scene_node_geometry* p_geometries = get_sn_geometry(0);
			scene_node_material* p_materials = get_sn_material(0);
			mat4* p_world_matrices = get_sn_worldmat(0);
			a_u64* p_entities = get_sn_entityflags(0);

			for (u32 i = 0; i < num_modes; ++i)
			{
				if (p_entities[i] & CMP_GEOMETRY && p_entities[i] & CMP_MATERIAL)
				{
					scene_node_geometry* p_geom = &p_geometries[i];
					scene_node_material* p_material = &p_materials[i];

					pen::renderer_set_shader(g_djscene_data.sh_point_light.vertex_shader, PEN_SHADER_TYPE_VS);
					pen::renderer_set_input_layout(g_djscene_data.sh_point_light.input_layout);
					stride = sizeof(vertex_model);

					//bind shaders and textures
					if (p_material->texture_id[0] != -1)
					{
						pen::renderer_set_shader(g_djscene_data.sh_point_light_textured.pixel_shader, PEN_SHADER_TYPE_PS);
						pen::renderer_set_texture(p_material->texture_id[0], g_djscene_data.sampler_trilinear_wrapped, 0, PEN_SHADER_TYPE_PS);
						pen::renderer_set_texture(g_djscene_data.rt2_cube_test_depth, g_djscene_data.sampler_trilinear_clamped, 1, PEN_SHADER_TYPE_PS);
					}
					else
					{
						//dummy cubemap
						pen::renderer_set_texture(g_djscene_data.rt2_cube_test_depth, g_djscene_data.sampler_trilinear_comparison, 0, PEN_SHADER_TYPE_PS);
						pen::renderer_set_shader(g_djscene_data.sh_point_light.pixel_shader, PEN_SHADER_TYPE_PS);
					}

					c8* name = get_sn_name(i);

					pen::renderer_set_vertex_buffer(p_geom->vertex_buffer, 0, 1, &stride, &offset);

					pen::renderer_set_index_buffer(p_geom->index_buffer, p_geom->index_type, 0);

					//shader constants and textures
					cbuffer_coloured cb;
					cb.world_matrix = p_world_matrices[i];
					cb.camera_pos = vec4f(g_djscene_data.cam.pos, 1.0f);
					cb.diffuse = p_material->diffuse_rgb_shininess;

					if (g_djscene_tweakables.lighting_mode == 0.0f)
					{
						cb.light_params = vec4f(
							g_djscene_tweakables.cook_torrence_roughness * g_djscene_tweakables.cook_torrence_roughness,
							g_djscene_tweakables.cook_torrence_reflection,
							1.0f - g_djscene_tweakables.cook_torrence_reflection,
							0.0f);
					}
					else
					{
						cb.light_params = vec4f(g_djscene_tweakables.phong_specular_power, g_djscene_tweakables.phong_specular_strength, 0.0f, 0.0f);
					}

					pen::renderer_update_buffer(g_djscene_data.cb_coloured, &cb, sizeof(cbuffer_coloured));

					pen::renderer_set_constant_buffer(cb_view, 0, PEN_SHADER_TYPE_VS);
					pen::renderer_set_constant_buffer(g_djscene_data.cb_coloured, 1, PEN_SHADER_TYPE_VS);
					pen::renderer_set_constant_buffer(g_djscene_data.cb_point_light, 2, PEN_SHADER_TYPE_VS);

					pen::renderer_set_constant_buffer(g_djscene_data.cb_tweaker, 3, PEN_SHADER_TYPE_VS);

					pen::renderer_set_constant_buffer(g_djscene_data.cb_coloured, 0, PEN_SHADER_TYPE_PS);
					pen::renderer_set_constant_buffer(g_djscene_data.cb_point_light, 1, PEN_SHADER_TYPE_PS);

					//draw
					pen::renderer_draw_indexed(p_geom->num_indices, 0, 0, PEN_PT_TRIANGLELIST);
				}
			}

			pen::renderer_consume_cmd_buffer();
			*/
		}

		void update_scene_matrices(component_entity_scene* scene)
		{
			for (u32 n = 0; n < scene->num_nodes; ++n)
			{
				u32 parent = scene->parents[n];

				if (parent == n)
					scene->world_matrices[n] = scene->local_matrices[n];
				else
					scene->world_matrices[n] = scene->world_matrices[parent] * scene->local_matrices[n];
			}
		}

		void render_scene_debug( component_entity_scene* scene, const scene_view& view )
		{
			for (u32 n = 0; n < scene->num_nodes; ++n)
			{
				put::dbg::add_coord_space(scene->world_matrices[n], 0.5f);
			}

			put::dbg::render_3d(view.cb_view);
		}
	}
}

#if 0
#include "dj_scene.h"
#include "dj_scene_ces.h"
#include "filesystem.h"
#include "memory.h"
#include "pen_string.h"

namespace djscene
{
	scene_nodes g_scene_nodes;

	u32 read_parsable_char( char* buf, u32* data_start )
	{
		u32 u32s_read = 0;
		u32 num_chars = *data_start++;
		u32s_read++;

		c8 _buf[MAX_SCENE_NODE_CHARS];
		for (u32 c = 0; c < num_chars; ++c)
		{
			if (c < MAX_SCENE_NODE_CHARS)
			{
				_buf[c] = ( c8 ) *data_start;
			}

			if( c >= MAX_SCENE_NODE_CHARS )
			{
				int i = 0;
			}
	
			PEN_ASSERT( c < MAX_SCENE_NODE_CHARS );

			data_start++;
			u32s_read++;
		}

		if (num_chars >= MAX_SCENE_NODE_CHARS)
		{
			num_chars = MAX_SCENE_NODE_CHARS - 1;
		}

		_buf[num_chars] = '\0';

		pen::memory_cpy( buf, &_buf[0], num_chars + 1 );

		return u32s_read;
	}

	void load_mesh( const c8* filename, scene_node_geometry* p_geometries, scene_node_physics* p_physics, u32 node_index, const c8** material_names )
	{
		//mesh data test
		void* mesh_file;
		u32   mesh_file_size;
		pen::filesystem_read_file_to_buffer( filename, &mesh_file, mesh_file_size );

		u32* p_reader = ( u32* ) mesh_file;
		u32 version = *p_reader++;
		u32 num_meshes = *p_reader++;

		scene_node_geometry* p_parent_geom = &g_scene_nodes.geometries[node_index];
		scene_node_geometry* p_geom = p_parent_geom;

		u32 collision_mesh = 1; 

		physics::collision_mesh_data* temp_collision_data;
		if( collision_mesh )
		{
			temp_collision_data = (physics::collision_mesh_data*)pen::memory_alloc( sizeof( physics::collision_mesh_data ) * num_meshes );
		}

		//map mesh material id's to material file id's
		c8 mat_name_buf[MAX_SCENE_NODE_CHARS];
		for( u32 i = 0; i < num_meshes; ++i )
		{
			u32 u32s_read = read_parsable_char( mat_name_buf, p_reader );
			p_reader += u32s_read;

			p_geometries[i].submesh_material_index = -1;

			for( u32 j = 0; j < num_meshes; ++j )
			{
				if( pen::string_compare( mat_name_buf, material_names[j] ) == 0 )
				{
					p_geometries[i].submesh_material_index  = j;
					break;
				}
			}

			PEN_ASSERT( p_geometries[i].submesh_material_index != -1 );
		}

		for (u32 submesh = 0; submesh < num_meshes; ++submesh)
		{
			//physics
			u32 collision_shape = *p_reader++;
			u32 collision_dynamic = *p_reader++;

			vec3f min_extents;
			vec3f max_extents;

			pen::memory_cpy( &min_extents, p_reader, sizeof(vec3f) );
			p_reader += 3;

			pen::memory_cpy( &max_extents, p_reader, sizeof(vec3f) );
			p_reader += 3;

			p_physics[submesh].min_extents = min_extents;
			p_physics[submesh].max_extents = max_extents;
			p_physics[submesh].centre = min_extents + ((max_extents - min_extents) / 2.0f);
			p_physics[submesh].collision_shape = collision_shape;
			p_physics[submesh].collision_dynamic = collision_dynamic;

			//vb and ib
			u32 num_pos_floats = *p_reader++;
			u32 num_floats = *p_reader++;
			u32 num_indices = *p_reader++;
			u32 num_collision_floats = *p_reader++;
			u32 skinned = *p_reader++;

			u32 index_size = num_indices < 65535 ? 2 : 4;

			if( index_size == 4 )
			{
				int i = 0;
			}

			u32 vertex_size = sizeof( vertex_model );

			if( skinned )
			{
				vertex_size = sizeof( vertex_model_skinned );

				p_geometries[ submesh ].p_skin = (scene_node_skin*)pen::memory_alloc( sizeof(scene_node_skin) );

				pen::memory_cpy( &p_geometries[ submesh ].p_skin->bind_shape_matirx, p_reader, sizeof(mat4) );
				p_reader += 16;

				mat4 max_swap;
				max_swap.create_axis_swap( vec3f( 1.0f, 0.0f, 0.0f ), vec3f( 0.0f, 0.0f, -1.0f ), vec3f( 0.0f, 1.0f, 0.0f ) );
				mat4 max_swap_inv = max_swap.inverse4x4();

				mat4 final_bind = max_swap * p_geometries[ submesh ].p_skin->bind_shape_matirx * max_swap_inv;

				p_geometries[ submesh ].p_skin->bind_shape_matirx = final_bind;
			
				u32 num_ijb_floats = *p_reader++;
				pen::memory_cpy( &p_geometries[ submesh ].p_skin->joint_bind_matrices[0], p_reader, sizeof(f32) * num_ijb_floats );
				p_reader += num_ijb_floats;

				p_geometries[ submesh ].p_skin->num_joints = num_ijb_floats / 16;

				for( u32 joint = 0; joint < p_geometries[ submesh ].p_skin->num_joints; ++joint )
				{
					p_geometries[ submesh ].p_skin->joint_bind_matrices[joint] = max_swap * p_geometries[ submesh ].p_skin->joint_bind_matrices[joint] * max_swap_inv;
					p_geometries[ submesh ].p_skin->joint_matrices[joint].create_identity( );
 				}
			
				g_scene_nodes.entities[node_index] |= CMP_SKINNED;
			}

			//all vertex data is written out as 4 byte ints
			u32 num_verts = num_floats / ( vertex_size / sizeof( u32 ) );

			p_geometries[ submesh ].num_vertices = num_verts; 

			u32 num_tris = num_verts / 3;

			pen::buffer_creation_params bcp;
			bcp.usage_flags = PEN_USAGE_DEFAULT;
			bcp.bind_flags = PEN_BIND_VERTEX_BUFFER | PEN_BIND_STREAM_OUTPUT;
			bcp.cpu_access_flags = 0;
			bcp.buffer_size = sizeof( vertex_position ) * num_verts;
			bcp.data = ( void* ) p_reader;

			p_geometries[ submesh ].position_buffer = pen::renderer_create_buffer( bcp );

			pen::test_function();

			p_reader += bcp.buffer_size / sizeof(f32);

			bcp.buffer_size = vertex_size * num_verts;
			bcp.data = ( void* ) p_reader;

			p_geometries[submesh].vertex_buffer = pen::renderer_create_buffer( bcp );

			p_reader += bcp.buffer_size / sizeof(u32);

			if( skinned )
			{
				//create an empty buffer for stream out
				bcp.buffer_size = sizeof( vertex_model ) * num_verts;
				bcp.bind_flags |= PEN_BIND_STREAM_OUTPUT;

				void* p_data = pen::memory_alloc(sizeof( vertex_model ) * num_verts);
				pen::memory_zero(p_data,sizeof( vertex_model ) * num_verts);

				bcp.data = p_data;

				u32 vb = pen::renderer_create_buffer( bcp );

				bcp.buffer_size = sizeof( vertex_position ) * num_verts;

				u32 pb = pen::renderer_create_buffer( bcp );

				p_geometries[submesh].skinned_position = p_geometries[submesh].position_buffer; 
				p_geometries[submesh].pre_skin_vertex_buffer = p_geometries[submesh].vertex_buffer;
				
				p_geometries[submesh].vertex_buffer = vb;
				p_geometries[submesh].position_buffer = pb;

				pen::memory_free(p_data);
			}

			bcp.usage_flags = PEN_USAGE_DEFAULT;
			bcp.bind_flags = PEN_BIND_INDEX_BUFFER;
			bcp.cpu_access_flags = 0;
			bcp.buffer_size = index_size * num_indices;
			bcp.data = ( void* ) p_reader;

			p_geometries[submesh].num_indices = num_indices;
			p_geometries[submesh].index_type = index_size == 2 ? PEN_FORMAT_R16_UINT : PEN_FORMAT_R32_UINT;
			p_geometries[submesh].index_buffer = pen::renderer_create_buffer( bcp );

			p_reader += bcp.buffer_size / sizeof(u32);

			//collision mesh
			if (collision_mesh)
			{
				temp_collision_data[ submesh ].vertices = (f32*)pen::memory_alloc( sizeof(f32) * num_collision_floats );
				temp_collision_data[ submesh ].num_floats = num_collision_floats;

				pen::memory_cpy( temp_collision_data[submesh].vertices, p_reader, sizeof(f32) * num_collision_floats );
			}

			p_reader += num_collision_floats;
		}

		if( collision_mesh )
		{
			u32 total_num_floats = 0;
			for( u32 s = 0; s < num_meshes; ++s )
			{
				total_num_floats += temp_collision_data[ s ].num_floats;
			}

			p_physics->mesh_data.vertices = (f32*)pen::memory_alloc( sizeof(f32) * total_num_floats );
			p_physics->mesh_data.indices = (u32*)pen::memory_alloc( sizeof(u32) * total_num_floats );

			p_physics->mesh_data.num_indices = total_num_floats;
			p_physics->mesh_data.num_floats = total_num_floats;

			f32* p_vertices = p_physics->mesh_data.vertices;

			for( u32 s = 0; s < num_meshes; ++s )
			{
				pen::memory_cpy( p_vertices, temp_collision_data[ s ].vertices, sizeof(f32) * temp_collision_data[ s ].num_floats );

				p_vertices += temp_collision_data[ s ].num_floats;
			}

			for( u32 v = 0; v < total_num_floats; ++v )
			{
				p_physics->mesh_data.indices[v] = v;
			}

			//centralise mesh
			vec3f avg = vec3f::zero();
			for( u32 v = 0; v < total_num_floats; v+=3 )
			{
				avg.x += p_physics->mesh_data.vertices[ v + 0 ];
				avg.y += p_physics->mesh_data.vertices[ v + 1 ];
				avg.z += p_physics->mesh_data.vertices[ v + 2 ];
			}

			avg /= (f32)(total_num_floats/3);
			for (u32 v = 0; v < total_num_floats; v += 3)
			{
				p_physics->mesh_data.vertices[v + 0] -= avg.x;
				p_physics->mesh_data.vertices[v + 1] -= avg.y;
				p_physics->mesh_data.vertices[v + 2] -= avg.z;
			}

			for( u32 sm = 0; sm < num_meshes; ++sm )
			{
				pen::memory_free( temp_collision_data[ sm ].vertices );
			}

			pen::memory_free( temp_collision_data );
		}

		pen::memory_free( mesh_file );
	}

	void load_material( const c8* filename, scene_node_material* p_mat )
	{
		void* material_file;
		u32   material_file_size;
		pen::filesystem_read_file_to_buffer( filename, &material_file, material_file_size );

		u32* p_reader = ( u32* ) material_file;

		u32 version = *p_reader++;

		//diffuse
		pen::memory_cpy( &p_mat->diffuse_rgb_shininess, p_reader, sizeof(vec4f) );
		p_reader += 4;

		//specular
		pen::memory_cpy( &p_mat->specular_rgb_reflect, p_reader, sizeof(vec4f) );
		p_reader += 4;

		//shininess
		pen::memory_cpy( &p_mat->diffuse_rgb_shininess.w, p_reader, sizeof(f32) );
		p_reader++;

		//reflectivity
		pen::memory_cpy( &p_mat->specular_rgb_reflect.w, p_reader, sizeof(f32) );
		p_reader++;

		u32 u32s_read = 0;
		c8 map_buffer[128];
		pen::texture_creation_params* tcp;

		for (u32 map = 0; map < MAX_MAPS; ++map)
		{
			u32s_read = read_parsable_char( map_buffer, p_reader );
			p_reader += u32s_read;

			if (u32s_read > 1)
			{
				PEN_PRINTF( "%s\n", map_buffer );
				tcp = put::loader_load_texture( map_buffer );
				p_mat->texture_id[map] = pen::renderer_create_texture2d( *tcp );
				put::loader_free_texture( &tcp );
			}
			else
			{
				p_mat->texture_id[map] = -1;
			}
		}

		//clean up
		pen::memory_free( material_file );

		return;
	}

	s32 get_scene_node_by_name( const c8* name )
	{
		for (u32 i = 0; i < g_scene_nodes.num_nodes; ++i)
		{
			const c8* nn = g_scene_nodes.names[i];

			if (pen::string_compare( nn, name ) == 0)
			{
				return i;
			}
		}

		return -1;
	}

	s32 get_cloned_scene_node_by_name( const c8* name, const c8* suffix, vec3f offset, u32 disble_src )
	{
		s32 index = -1;

		c8 node_cloned_name[128];
		pen::string_format( node_cloned_name, 64, "%s_cloned", name );

		for (u32 i = 0; i < g_scene_nodes.num_nodes; ++i)
		{
			const c8* nn = g_scene_nodes.names[i];

			if (pen::string_compare( nn, name ) == 0)
			{
				index = i;
				break;
			}
		}

		if( index == -1 )
		{
			return index;
		}

		u32 clone_index = get_num_nodes();
		
		clone_node( index, clone_index, -1, offset, suffix );

		if( disble_src )
		{
			*get_sn_entityflags( index ) &= ~CMP_GEOMETRY;
			*get_sn_entityflags( clone_index ) |= CMP_GEOMETRY;
		}

		u32 cloned_sub_node = index + 1;
		while( 1 )
		{
			const c8* nn = g_scene_nodes.names[ cloned_sub_node ];

			if( pen::string_compare( nn, node_cloned_name ) != 0 )
			{
				break;
			}

			u32 sub_clone_dest = get_num_nodes();
			clone_node( cloned_sub_node, sub_clone_dest, clone_index, vec3f::zero(), suffix );

			*get_sn_entityflags( clone_index ) |= CMP_GEOMETRY;

			cloned_sub_node++;
		}

		return clone_index;
	}

	scene_nodes* get_scene_nodes( )
	{
		return &g_scene_nodes;
	}

	scene_node_physics* get_sn_physics( u32 index )
	{
		return &g_scene_nodes.physics_data[ index ];
	}

	u32 get_num_nodes( )
	{
		return g_scene_nodes.num_nodes;
	}

	c8* get_sn_name( u32 index )
	{
		return &g_scene_nodes.names[index][0];
	}

	scene_node_geometry* get_sn_geometry( u32 index )
	{
		return &g_scene_nodes.geometries[ index ];
	}

	scene_node_material* get_sn_material( u32 index )
	{
		return &g_scene_nodes.materials[ index ];
	}

	u32* get_sn_physicshandle( u32 index )
	{
		return &g_scene_nodes.physics_handles[ index ];
	}

	u32* get_sn_parents( u32 index )
	{
		return &g_scene_nodes.parents[ index ];
	}

	u32* get_sn_multibodyhandle( u32 index )
	{
		return &g_scene_nodes.multibody_handles[ index ];
	}

	s32* get_sn_multibodylink( u32 index )
	{
		return &g_scene_nodes.multibody_link[ index ];
	}

	mat4* get_sn_offsetmat( u32 index )
	{
		return &g_scene_nodes.offset_matrices[ index ];
	}

	mat4* get_sn_worldmat( u32 index )
	{
		return &g_scene_nodes.world_matrices[ index ];
	}

	mat4* get_sn_localmat( u32 index )
	{
		return &g_scene_nodes.local_matrices[ index ];
	}

	a_u64* get_sn_entityflags( u32 index )
	{
		return &g_scene_nodes.entities[ index ];
	}

	void clone_node( u32 src, u32 dst, s32 parent, vec3f offset, const c8* suffix )
	{
		scene_nodes* p_sn = get_scene_nodes();

		pen::memory_cpy( &p_sn->names[ dst ][ 0 ], &p_sn->names[ src ][ 0 ], MAX_SCENE_NODE_CHARS );
		pen::string_concatonate( &p_sn->names[ dst ][ 0 ], suffix, MAX_SCENE_NODE_CHARS );

		pen::memory_cpy( &p_sn->geometry_names[ dst ][ 0 ], &p_sn->geometry_names[ src ][ 0 ], MAX_SCENE_NODE_CHARS );
		pen::memory_cpy( &p_sn->material_names[ dst ][ 0 ], &p_sn->material_names[ src ][ 0 ], MAX_SCENE_NODE_CHARS );

		u32 parent_offset = p_sn->parents[ src ] - src;

		if( parent == -1 )
		{
			p_sn->parents[dst] = dst - parent_offset;
		}
		else
		{
			p_sn->parents[dst]			= parent;
		}

		//p_sn->entities[ dst ]			= p_sn->entities[ src ];

		pen::memory_cpy( &p_sn->entities[ dst ], &p_sn->entities[ src ], sizeof( a_u64 ) );

		p_sn->geometries[ dst ]			= p_sn->geometries[ src ];
		p_sn->materials[ dst ]			= p_sn->materials[ src ];
		p_sn->local_matrices[ dst ]		= p_sn->local_matrices[ src ];
		p_sn->world_matrices[ dst ]		= p_sn->world_matrices[ src ];
		p_sn->offset_matrices[ dst ]	= p_sn->offset_matrices[ src ];
		p_sn->physics_matrices[ dst ]	= p_sn->physics_matrices[ src ];
		p_sn->physics_handles[ dst ]	= p_sn->physics_handles[ src ];
		p_sn->multibody_handles[ dst ]	= p_sn->multibody_handles[ src ];
		p_sn->multibody_link[ dst ]		= p_sn->multibody_link[ src ];
		p_sn->physics_data[ dst ]		= p_sn->physics_data[ src ];

		if( dst >= p_sn->num_nodes )
		{
			p_sn->num_nodes = dst + 1;
		}

		p_sn->physics_data[ dst ].start_position += offset;

		vec3f right = p_sn->local_matrices[ dst ].get_right();
		vec3f up = p_sn->local_matrices[ dst ].get_up();
		vec3f fwd = p_sn->local_matrices[ dst ].get_fwd();
		vec3f translation = p_sn->local_matrices[ dst ].get_translation();

		p_sn->local_matrices[ dst ].set_vectors( right, up, fwd, translation + offset );
	}

	void get_rb_params_from_node( u32 node_index, f32 mass, physics::rigid_body_params &rbp )
	{
		scene_node_physics* p_phys = get_sn_physics( node_index );

		//initialise as y up
		rbp.shape_up_axis = 0;

		//set position
		physics::set_lw_vec3f( &rbp.position, p_phys->start_position + p_phys->centre );

		//set dimensions
		if( rbp.shape == physics::CAPSULE )
		{
			physics::set_lw_vec3f( &rbp.dimensions, (p_phys->max_extents - p_phys->min_extents) / 2.0f );

			rbp.dimensions.y -= rbp.dimensions.x;
			rbp.dimensions.y *= 2.0f;
			
		}
		else
		{
			physics::set_lw_vec3f( &rbp.dimensions, (p_phys->max_extents - p_phys->min_extents) / 2.0f );
		}

		//set rotation
		rbp.rotation = p_phys->start_rotation;

		//set mass
		rbp.mass = p_phys->collision_dynamic == 1 ? mass : 0.0f;

		//set shape
		rbp.shape = p_phys->collision_shape;

		if( rbp.shape == physics::HULL || rbp.shape == physics::MESH )
		{
			rbp.mesh_data = p_phys->mesh_data;
		}
	}

	void create_compound_from_nodes( physics::compound_rb_params &crbp, const c8* compound_names, u32 clone, c8* suffix, vec3f offset )
	{
		static c8 physics_types[][8] =
		{
			"pxbox_d",
			"pxbox_s",
			"pxcyl_s",
			"pxcyl_d"
		};

		c8	node_name[MAX_SCENE_NODE_CHARS];
		u32 sub_indices[MAX_SUBS_COMPOUND];

		u32 num_sub_shapes = 0;
		s32 sub_index = -1;

		//count number of sub shapes
		do 
		{
			for (u32 j = 0; j < 4; ++j)
			{
				pen::string_format( node_name, MAX_SCENE_NODE_CHARS, "%s_%i_%s", compound_names, num_sub_shapes, physics_types[j] );

				sub_index = get_cloned_scene_node_by_name( node_name, suffix, offset, 1 );

				if( sub_index != -1 )
				{
					sub_indices[ num_sub_shapes ] = sub_index;
					num_sub_shapes++;
					break;
				}
			}

		} while ( sub_index != -1 );

		//allocate sub shapes
		crbp.num_shapes = num_sub_shapes;
		crbp.rb = (physics::rigid_body_params*)pen::memory_alloc( sizeof(physics::rigid_body_params) * num_sub_shapes );

		//get rb params
		for( u32 s = 0; s < num_sub_shapes; ++s )
		{
			if ( sub_indices[s] != -1)
			{
				//hide its geometric representation
				a_u64* entity = get_sn_entityflags( sub_indices[s] );
				*entity &= ~CMP_GEOMETRY;

				scene_node_physics* p_phys = get_sn_physics( sub_indices[s] );

				get_rb_params_from_node( sub_indices[s], 1.0f, crbp.rb[s] );
			}
		}
	}
 
	void create_compound_rb( const c8* compound_name, const c8* base_name, f32 mass, u32 group, u32 mask, u32 &rb_index )
	{
		physics::rigid_body_params rbp;
		physics::compound_rb_params crb;

		u32 centre_index = get_scene_node_by_name( base_name );
		get_rb_params_from_node( centre_index, mass, rbp );

		//zero pos and rot
		crb.base.position = rbp.position;
		crb.base.rotation = rbp.rotation;

		crb.base.group = group;
		crb.base.mask = mask;
		crb.base.mass = mass;
		crb.base.shape = physics::COMPOUND;

		create_compound_from_nodes( crb, compound_name );

		rb_index = physics::add_compound_rb( crb );

		mat4*	p_offset_mat = get_sn_offsetmat( centre_index );
		p_offset_mat->create_translation( vec3f::zero( ) );

		*get_sn_physicshandle( centre_index ) = rb_index;
		*get_sn_entityflags( centre_index ) |= CMP_PHYSICS;
		*get_sn_entityflags( centre_index ) |= CMP_GEOMETRY;
	}

	void create_physics_object( u32 node_index, physics::constraint_params* p_constraint, f32 mass, u32 group, u32 mask, vec3f scale_dimensions )
	{
		physics::rigid_body_params rbp;
		get_rb_params_from_node( node_index, mass, rbp );

		rbp.group = group;
		rbp.mask = mask;

		rbp.dimensions.x *= scale_dimensions.x;
		rbp.dimensions.y *= scale_dimensions.y;
		rbp.dimensions.z *= scale_dimensions.z;

		u32* p_rbhandle = get_sn_physicshandle( node_index );

		scene_node_physics* p_phys = get_sn_physics( node_index );

		if (rbp.shape == physics::HULL)
		{
			rbp.position.x -= p_phys->centre.x;
			rbp.position.y -= p_phys->centre.y;
			rbp.position.z -= p_phys->centre.z;
		}

		if (p_constraint)
		{
			p_constraint->rb = rbp;
			*p_rbhandle = physics::add_constrained_rb( *p_constraint );
		}
		else
		{
			*p_rbhandle = physics::add_rb( rbp );
		}

		mat4*	p_offset_mat = get_sn_offsetmat( node_index );
		a_u64*  p_entity_flags = get_sn_entityflags( node_index );

		if( rbp.shape == physics::HULL )
		{
			p_offset_mat->create_translation( vec3f::zero() );
		}
		else
		{
			p_offset_mat->create_translation( p_phys->centre * -1.0f );
		}
		

		*p_entity_flags |= CMP_PHYSICS;
	}

	u32 create_slider_widget( u32 node_index, u32 start_index, u32 end_index, f32 mass, f32 invert_max, vec3f &slider_start, vec3f &slider_end, u32 &rb_handle )
	{
		slider_start = get_sn_physics( start_index )->start_position;
		slider_end = get_sn_physics( end_index )->start_position;

		physics::constraint_params cp;
		cp.type = physics::DOF6;

		//cannot rotate
		physics::set_lw_vec3f( &cp.lower_limit_rotation, vec3f::zero( ) );
		physics::set_lw_vec3f( &cp.upper_limit_rotation, vec3f::zero( ) );

		//moves in the slider axis
		physics::set_lw_vec3f( &cp.lower_limit_translation, vec3f::zero( ) );
		physics::set_lw_vec3f( &cp.upper_limit_translation, (slider_end - slider_start) * invert_max );

		cp.linear_damping = 1.0f;
		cp.angular_damping = 1.0f;

		create_physics_object( node_index, &cp, mass, COL_WIDGET, COL_PLAYER );

		rb_handle = *get_sn_physicshandle( node_index );

		physics::set_v3( rb_handle, put::maths::normalise(slider_end - slider_start) * invert_max, physics::CMD_SET_LINEAR_FACTOR );
		physics::set_v3( rb_handle, vec3f::zero( ), physics::CMD_SET_ANGULAR_FACTOR );
		physics::set_v3( rb_handle, vec3f::zero( ), physics::CMD_SET_GRAVITY );


		return 1;
	}

	u32 create_button_widget( u32 node_index, u32 up_index, u32 down_index, f32 mass, f32 spring, button &button_data )
	{
		button_data.limit_up = get_sn_physics( up_index )->start_position;
		button_data.limit_down = get_sn_physics( down_index )->start_position;

		physics::constraint_params cp;
		cp.type = physics::DOF6;

		//cannot rotate
		physics::set_lw_vec3f( &cp.lower_limit_rotation, vec3f::zero( ) );
		physics::set_lw_vec3f( &cp.upper_limit_rotation, vec3f::zero( ) );

		//moves in the button axis
		vec3f lower = button_data.limit_down - button_data.limit_up;
		physics::set_lw_vec3f( &cp.lower_limit_translation, lower - vec3f( 0.0f, 0.0f, 0.0f ) );
		physics::set_lw_vec3f( &cp.upper_limit_translation, vec3f::zero( ) );

		cp.linear_damping = 1.0f;
		cp.angular_damping = 1.0f;

		create_physics_object( node_index, &cp, mass, COL_WIDGET, COL_PLAYER );

		button_data.entity_index = *get_sn_physicshandle( node_index );

		button_data.spring = spring;

		button_data.offset = get_sn_offsetmat( node_index )->get_translation( );

		physics::set_v3( button_data.entity_index, vec3f( 0.0f, 0.0f, 0.0f ), physics::CMD_SET_GRAVITY );

		physics::set_v3( button_data.entity_index, vec3f::unit_y( ), physics::CMD_SET_LINEAR_FACTOR );
		physics::set_v3( button_data.entity_index, vec3f::zero( ), physics::CMD_SET_ANGULAR_FACTOR );

		physics::set_v3( button_data.entity_index, vec3f( 1.0f, -spring, 10.0f ), physics::CMD_SET_BUTTON_MOTOR );

		return 1;
	}

	u32 create_rotary_widget( u32 node_index, f32 mass, vec3f axis, vec3f pivot, u32 &rb_handle, vec2f limits )
	{
		physics::constraint_params cp;
		cp.type = physics::HINGE;

		cp.lower_limit_rotation.x = limits.x;
		cp.upper_limit_rotation.x = limits.y;

		//rotates about its origin in axis
		physics::set_lw_vec3f( &cp.axis, axis );
		physics::set_lw_vec3f( &cp.pivot, pivot );

		create_physics_object( node_index, &cp, mass, COL_WIDGET, COL_PLAYER | COL_WIDGET_BASE | COL_RECORDS );

		rb_handle = *get_sn_physicshandle( node_index );

		physics::set_v3( rb_handle, axis, physics::CMD_SET_ANGULAR_FACTOR );
		physics::set_v3( rb_handle, vec3f::zero(), physics::CMD_SET_LINEAR_FACTOR );

		return 1;
	}

	u32 check_button_state( button &button_data, u32 button_lock, f32 down_epsilon, f32 up_epsilon )
	{
		mat4 light_mat = physics::get_rb_matrix( button_data.entity_index );

		f32  y_pos = button_data.offset.y + light_mat.get_translation( ).y;

		u32 state = BUTTON_TRANS;

		//dbg::add_point( button_data.limit_down, 0.5f );
		f32 button_axis_length = ( button_data.limit_up.y - button_data.limit_down.y );

		if( !button_data.debounce )
		{
			if (y_pos <= button_data.limit_up.y - button_axis_length * down_epsilon)
			{
				if (button_lock & BUTTON_LOCK_DOWN && button_data.lock_state != BUTTON_DOWN)
				{
					physics::set_v3( button_data.entity_index, vec3f::zero( ), physics::CMD_SET_BUTTON_MOTOR );
					button_data.lock_state = BUTTON_DOWN;
				}

				physics::set_v3( button_data.entity_index, vec3f( 1.0f, 1.0f, 1.0f ), physics::CMD_SET_GRAVITY );

				button_data.debounce = 1;
				state = BUTTON_DOWN;
				return state;
			}
		}

		if (y_pos >= button_data.limit_up.y - button_axis_length * up_epsilon )
		{
			physics::set_v3( button_data.entity_index, vec3f( 1.0f, -10.0f, 1.0f ), physics::CMD_SET_BUTTON_MOTOR );

			button_data.lock_state = BUTTON_UP;
			button_data.debounce = 0;
			state = BUTTON_UP;
		}

		return state;
	}

	void release_button( button &button_data )
	{
		physics::set_v3( button_data.entity_index, vec3f( 1.0f, -button_data.spring, 10.0f ), physics::CMD_SET_BUTTON_MOTOR );
		physics::set_v3( button_data.entity_index, vec3f( 1.0f, 1.0f, 1.0f ), physics::CMD_SET_GRAVITY );
	}

	f32 get_slider_ratio( slider slider_node )
	{
		u32 rb = slider_node.entity_index;
		vec3f slider_min = slider_node.limit_min;
		vec3f slider_max = slider_node.limit_max;

		mat4 slider_mat = physics::get_rb_matrix( rb );
		vec3f slider_pos = slider_mat.get_translation( );

		f32 offset = put::maths::magnitude( slider_pos - slider_min );

		f32 max_vol = put::maths::magnitude( slider_max - slider_min );

		f32 vol = offset / max_vol;

		vol = vol < 0.1f ? 0.0f : vol;

		return vol;
	}

	void hide_node( u32 node_index, u32 remove_physics )
	{
		a_u64* flags = get_sn_entityflags( node_index );
		*flags &= ~CMP_GEOMETRY;

		if (remove_physics)
		{
			*flags &= ~CMP_PHYSICS;

			u32 physics_entity = *get_sn_physicshandle( node_index );

			physics::remove_from_world( physics_entity );
		}
	}

	void show_node( u32 node_index, u32 add_physics )
	{
		a_u64* flags = get_sn_entityflags( node_index );

		*flags |= CMP_GEOMETRY;

		if (add_physics)
		{
			*flags |= CMP_PHYSICS;

			u32 physics_entity = *get_sn_physicshandle( node_index );

			physics::add_to_world( physics_entity );
		}
	}
}
#endif
