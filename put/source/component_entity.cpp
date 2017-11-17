#include <functional>

#include "component_entity.h"
#include "file_system.h"
#include "dev_ui.h"
#include "debug_render.h"
#include "layer_controller.h"
#include "pmfx.h"
#include "str/Str.h"
#include "hash.h"

using namespace put;

namespace put
{
    enum pmm_transform_types
    {
        PMM_TRANSLATE = 0,
        PMM_ROTATE = 1,
        PMM_MATRIX
    };
    
    const hash_id ID_JOINT = PEN_HASH("joint");
    
	namespace ces
	{
#define ALLOC_COMPONENT_ARRAY( SCENE, COMPONENT, TYPE )											\
		if( !SCENE->COMPONENT )																	\
			SCENE->COMPONENT = (TYPE*)pen::memory_alloc(sizeof(TYPE)*SCENE->nodes_size );		\
		else																					\
			SCENE->COMPONENT = (TYPE*)pen::memory_realloc(SCENE->COMPONENT,sizeof(TYPE)*SCENE->nodes_size); \
		pen::memory_zero( SCENE->COMPONENT, sizeof(TYPE)*SCENE->nodes_size)

#define FREE_COMPONENT_ARRAY( SCENE, COMPONENT ) pen::memory_free( SCENE->COMPONENT ); SCENE->COMPONENT = nullptr

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
			ALLOC_COMPONENT_ARRAY(scene, id_name, hash_id);
			ALLOC_COMPONENT_ARRAY(scene, id_geometry, hash_id);
			ALLOC_COMPONENT_ARRAY(scene, id_material, hash_id);

            //flags
            ALLOC_COMPONENT_ARRAY(scene, entities, a_u64);
            
			//components
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
            ALLOC_COMPONENT_ARRAY(scene, anim_controller, animation_controller);

            for( u32 i = 0; i < scene->nodes_size; ++i )
                scene->cbuffer[i] = PEN_INVALID_HANDLE;
            
#ifdef CES_DEBUG
            //debug components
			ALLOC_COMPONENT_ARRAY(scene, names, Str);
			ALLOC_COMPONENT_ARRAY(scene, geometry_names, Str);
			ALLOC_COMPONENT_ARRAY(scene, material_names, Str);
#endif
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
            FREE_COMPONENT_ARRAY(scene, cbuffer);
            FREE_COMPONENT_ARRAY(scene, anim_controller);

#ifdef CES_DEBUG
			FREE_COMPONENT_ARRAY(scene, names);
			FREE_COMPONENT_ARRAY(scene, geometry_names);
			FREE_COMPONENT_ARRAY(scene, material_names);
#endif
		}
        
        void clone_node( component_entity_scene* scene, u32 src, u32 dst, s32 parent, vec3f offset, const c8* suffix)
        {
            component_entity_scene* p_sn = scene;
            
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
            
            p_sn->id_name[dst] = p_sn->id_name[src];
            p_sn->id_geometry[dst] = p_sn->id_geometry[src];
            p_sn->id_material[dst] = p_sn->id_material[src];
            
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
            p_sn->cbuffer[dst] = p_sn->cbuffer[src];
            p_sn->anim_controller[dst] = p_sn->anim_controller[src];
            
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
            
#ifdef CES_DEBUG
            p_sn->names[dst] = p_sn->names[src].c_str();
            p_sn->names[dst].append(suffix);
            p_sn->geometry_names[dst] = p_sn->geometry_names[src].c_str();
            p_sn->material_names[dst] = p_sn->material_names[src].c_str();
#endif
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
        
        Str read_parsable_string(const u32** data)
        {
            Str name;
            
            const u32* p_len = *data;
            u32 name_len = *p_len++;
            c8* char_reader = (c8*)p_len;
            for(s32 j = 0; j < name_len; ++j)
            {
                name.append((c8)*char_reader);
                char_reader+=4;
            }
            
            *data += name_len + 1;
            
            return name;
        }
        
        void load_animations( const c8* data )
        {
#if 0
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
                        
                        p_skeleton->animations[a].rotations[t].euler_angles( put::maths::deg_to_rad( euler_angs.z ), put::maths::deg_to_rad( euler_angs.y ), put::maths::deg_to_rad( euler_angs.x ) );
                        p_int += 3;
                    }
                }
            }
#endif
        }

        void load_geometry
        (
            const c8* data,
            scene_node_geometry* p_geometries,
            scene_node_physics* p_physics,
            component_entity_scene* scene,
            u32 node_index,
            std::vector<Str>& material_symbols
        )
        {
            u32* p_reader = (u32*)data;
            u32 version = *p_reader++;
            u32 num_meshes = *p_reader++;
            
            if( version < 1 )
                return;
            
            u32 collision_mesh = 1;
            
            physics::collision_mesh_data* temp_collision_data;
            if (collision_mesh)
                temp_collision_data = (physics::collision_mesh_data*)pen::memory_alloc(sizeof(physics::collision_mesh_data) * num_meshes);
            
            //map mesh material id's to material file id's
            for (u32 i = 0; i < num_meshes; ++i)
            {
                Str mesh_material = read_parsable_string((const u32**)&p_reader);
                
                p_geometries[i].submesh_material_index = -1;
                
                u32 num_symbols = material_symbols.size();
                for( u32 j = 0; j < num_symbols; ++j)
                {
                    if ( mesh_material == material_symbols[j] )
                    {
                        p_geometries[i].submesh_material_index = j;
                        break;
                    }
                }
                
                PEN_ASSERT(p_geometries[i].submesh_material_index != -1);
            }
            
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
                u32 num_pos_verts = num_pos_floats / (sizeof(vertex_position) / sizeof(u32));
                
                p_geometries[submesh].num_vertices = num_verts;
                
                pen::buffer_creation_params bcp;
                bcp.usage_flags = PEN_USAGE_DEFAULT;
                bcp.bind_flags = PEN_BIND_VERTEX_BUFFER;
                bcp.cpu_access_flags = 0;
                bcp.buffer_size = sizeof(vertex_position) * num_pos_verts;
                bcp.data = (void*)p_reader;
                
                p_geometries[submesh].position_buffer = pen::renderer_create_buffer(bcp);
                
                p_reader += bcp.buffer_size / sizeof(f32);
                
                bcp.buffer_size = vertex_size * num_verts;
                bcp.data = (void*)p_reader;
                
                p_geometries[submesh].vertex_buffer = pen::renderer_create_buffer(bcp);
                
                p_reader += bcp.buffer_size / sizeof(u32);
                
                //stream out / transform feedback
                if (0 /*skinned*/)
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
                
                p_reader = (u32*)((c8*)p_reader + bcp.buffer_size);
                
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
        }

        void load_material( const u32* data, scene_node_material* p_mat )
        {
            const u32* p_reader = ( u32* ) data;
            
            u32 version = *p_reader++;
            
            if( version < 1 )
                return;
            
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
            
            for (u32 map = 0; map < put::ces::SN_NUM_TEXTURES; ++map)
            {
                Str texture_name = read_parsable_string(&p_reader);
                
                p_mat->texture_id[map] = -1;
                
                if (!texture_name.empty())
                    p_mat->texture_id[map] = put::load_texture(texture_name.c_str());
            }
            
            return;
        }
        
        static std::vector<animation> k_animations;
        
        anim_handle load_pma(const c8* filename)
        {
            void* anim_file;
            u32   anim_file_size;
            
            pen_error err = pen::filesystem_read_file_to_buffer(filename, &anim_file, anim_file_size);
            
            if( err != PEN_ERR_OK || anim_file_size == 0 )
            {
                //failed load file
                PEN_ASSERT(0);
            }
            
            const u32* p_u32reader = (u32*)anim_file;
            
            u32 version = *p_u32reader++;
            
            if( version < 1 )
            {
                pen::memory_free(anim_file);
                return INVALID_HANDLE;
            }
            
            k_animations.push_back(animation());
            animation& new_animation = k_animations.back();
            
            new_animation.name = filename;
            new_animation.id_name = PEN_HASH(filename);
 
            u32 num_channels = *p_u32reader++;
            
            new_animation.num_channels = num_channels;
            new_animation.channels = new node_animation_channel[num_channels];
            
            for( s32 i = 0; i < num_channels; ++i )
            {
                Str bone_name = read_parsable_string(&p_u32reader);
                new_animation.channels[i].target = PEN_HASH(bone_name.c_str());
                
                u32 num_sources = *p_u32reader++;
                
                for( s32 j = 0; j < num_sources; ++j )
                {
                    u32 sematic = *p_u32reader++;
                    u32 type = *p_u32reader++;
                    u32 num_floats = *p_u32reader++;
                    
                    if( type > 1 )
                        continue;
                    
                    f32* data = new f32[num_floats];
                    pen::memory_cpy(data, p_u32reader, sizeof(f32)*num_floats);
                    
                    p_u32reader += num_floats;
                    
                    switch (sematic)
                    {
                        case A_TIME:
                            new_animation.channels[i].times = data;
                            break;
                        case A_TRANSFORM:
                            new_animation.channels[i].matrices = (mat4*)data;
                            break;
                        default:
                            break;
                    };
                }
            }
            
            pen::memory_free(anim_file);
            return (anim_handle)k_animations.size()-1;
        }
        
        void load_pmm(const c8* filename, component_entity_scene* scene)
        {
            void* model_file;
            u32   model_file_size;
            
            pen_error err = pen::filesystem_read_file_to_buffer(filename, &model_file, model_file_size);
            
            if( err != PEN_ERR_OK || model_file_size == 0 )
            {
                //failed load file
                PEN_ASSERT(0);
            }
            
            const u32* p_u32reader = (u32*)model_file;
            
            //
            u32 num_scene = *p_u32reader++;
            u32 num_geom = *p_u32reader++;
            u32 num_materials = *p_u32reader++;
            
            std::vector<u32> scene_offsets;
            std::vector<u32> geom_offsets;
            std::vector<u32> material_offsets;
            
            std::vector<Str> material_names;
            std::vector<hash_id> id_geometry;
            
            for(s32 i = 0; i < num_scene; ++i)
                scene_offsets.push_back(*p_u32reader++);
    
            for(s32 i = 0; i < num_materials; ++i)
            {
                Str name = read_parsable_string(&p_u32reader);
                material_offsets.push_back(*p_u32reader++);
                material_names.push_back(name);
            }
            
            for(s32 i = 0; i < num_geom; ++i)
            {
                Str name = read_parsable_string(&p_u32reader);
                geom_offsets.push_back(*p_u32reader++);
                id_geometry.push_back(PEN_HASH(name.c_str()));
            }
            
            c8* p_data_start = (c8*)p_u32reader;
            
            p_u32reader = (u32*)p_data_start+scene_offsets[0];
            u32 version = *p_u32reader++;
            u32 num_import_nodes = *p_u32reader++;
            
            if( version < 1 )
            {
                pen::memory_free(model_file);
                return;
            }
  
            u32 node_zero_offset = scene->num_nodes;
            u32 current_node = node_zero_offset;
            u32 inserted_nodes = 0;
            
            for (u32 n = 0; n < num_import_nodes; ++n)
            {
                u32 node_type = *p_u32reader++;
                
                Str node_name = read_parsable_string(&p_u32reader);
                Str geometry_name = read_parsable_string(&p_u32reader);
                
                scene->id_name[current_node] = PEN_HASH( node_name.c_str() );
                scene->id_geometry[current_node] = PEN_HASH( geometry_name.c_str() );
                
#ifdef CES_DEBUG
                scene->names[current_node] = node_name;
                scene->geometry_names[current_node] = geometry_name;
#endif
                
                if( scene->id_geometry[current_node] == ID_JOINT )
                    scene->entities[current_node] |= CMP_BONE;
                
                u32 num_meshes = *p_u32reader++;
                
                scene_node_material* p_materials = NULL;
                std::vector<Str> mesh_material_names;
                std::vector<Str> mesh_material_symbols;
                
                //material pre load
                if (num_meshes > 0)
                {
                    p_materials = (scene_node_material*)pen::memory_alloc(sizeof(scene_node_material) * num_meshes);
                    
                    //read in material filenames
                    for (u32 mat = 0; mat < num_meshes; ++mat)
                        mesh_material_names.push_back(read_parsable_string(&p_u32reader));
                    
                    //read material symbol names
                    for (u32 mat = 0; mat < num_meshes; ++mat)
                        mesh_material_symbols.push_back(read_parsable_string(&p_u32reader));
                    
                    //load materials
                    for (u32 mi = 0; mi < num_meshes; ++mi)
                    {
                        for( s32 mati = 0; mati < num_materials; ++mati )
                        {
                            if( material_names[mati] == mesh_material_names[mi] )
                            {
                                u32* p_mat_data = (u32*)(p_data_start + material_offsets[mati]);
                                load_material(p_mat_data, &p_materials[mi]);
                            }
                        }
                    }
                }
                
                //transformation load
                u32 parent = *p_u32reader++ + node_zero_offset + inserted_nodes;
                scene->parents[current_node] = parent;
                u32 transforms = *p_u32reader++;
                
                vec3f translation;
                vec4f rotations[3];
                mat4  matrix;
                bool  has_matrix_transform = false;
                u32   num_rotations = 0;
                
                for (u32 t = 0; t < transforms; ++t)
                {
                    u32 type = *p_u32reader++;
                    
                    switch( type )
                    {
                        case PMM_TRANSLATE:
                            pen::memory_cpy(&translation, p_u32reader, 12);
                            p_u32reader += 3;
                            break;
                        case PMM_ROTATE:
                            pen::memory_cpy(&rotations[num_rotations], p_u32reader, 16);
                            rotations[num_rotations].w = put::maths::deg_to_rad(rotations[num_rotations].w);
                            static f32 zero_rotation_epsilon = 0.000001f;
                            if (rotations[num_rotations].w < zero_rotation_epsilon && rotations[num_rotations].w > zero_rotation_epsilon)
                                rotations[num_rotations].w = 0.0f;
                            num_rotations++;
                            p_u32reader += 4;
                            break;
                        case PMM_MATRIX:
                            has_matrix_transform = true;
                            pen::memory_cpy(&matrix, p_u32reader, 16 * 4);
                            p_u32reader += 16;
                            break;
                        default:
                            //unsupported transform type
                            PEN_ASSERT(0);
                            break;
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
                        hash_id id = scene->id_geometry[current_node];
                        
                        for (u32 g = 0; g < num_geom; ++g)
                        {
                            if( id == id_geometry[g] )
                            {
                                u32* p_geom_data = (u32*)(p_data_start + geom_offsets[g]);
                                load_geometry((const c8*)p_geom_data, p_geometries, p_physics, scene, current_node, mesh_material_symbols);
                            }
                        }
                    }
                }
                
                //make a transform matrix for geometry
                mat4 rot_mat;
                final_rotation.get_matrix(rot_mat);
                
                mat4 translation_mat;
                translation_mat.create_translation(translation);
                
                if(!has_matrix_transform)
                    matrix = translation_mat * rot_mat;
                
                scene->local_matrices[current_node] = (matrix);
                
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
                        
                        Str node_suffix;
                        node_suffix.appendf("_%i", submesh);
                        
                        if (submesh > 0)
                        {
                            inserted_nodes++;
                            clone_node( scene, current_node, dest, current_node, vec3f::zero(), (const c8*)node_suffix.c_str() );
                            scene->local_matrices[dest].create_identity();
                        }
                        
                        scene->geometries[dest] = p_geometries[submesh];
                        
                        scene->materials[dest] = p_materials[p_geometries[submesh].submesh_material_index];
                        
                        u32 mat_index = p_geometries[submesh].submesh_material_index;
                        scene->id_material[dest] = PEN_HASH(mesh_material_names[mat_index]);
                        
                        p_physics[submesh].start_position = translation;
                        p_physics[submesh].start_rotation = final_rotation;
                        
                        scene->physics_data[dest] = p_physics[submesh];
                        
#ifdef CES_DEBUG
                        scene->material_names[dest] = mesh_material_names[p_geometries[submesh].submesh_material_index];
#endif
                    }
                    
                    //delete temp data
                    pen::memory_free(p_geometries);
                    pen::memory_free(p_materials);
                    pen::memory_free(p_physics);
                }
                
                current_node = dest + 1;
                scene->num_nodes = current_node;
            }
            
            pen::memory_free(model_file);
            return;
        }

		void render_scene_view( const scene_view& view, scene_render_type render_type )
		{
            component_entity_scene* scene = view.scene;
            
            static pmfx::pmfx_handle model_pmfx = pmfx::load("fx_test");
            
            pen::renderer_set_constant_buffer(view.cb_view, 0, PEN_SHADER_TYPE_VS);
            
            static bool first = true;
            
			for (u32 n = 0; n < scene->num_nodes; ++n)
			{
				if (scene->entities[n] & CMP_GEOMETRY && scene->entities[n] & CMP_MATERIAL && (!(scene->entities[n] & CMP_PHYSICS)) )
				{
					scene_node_geometry* p_geom = &scene->geometries[n];
					scene_node_material* p_mat = &scene->materials[n];

                    //move this to update / bake static
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
                    
                    pmfx::set_technique( model_pmfx, 0 );
                    
					pen::renderer_set_constant_buffer(scene->cbuffer[n], 1, PEN_SHADER_TYPE_VS);

					//set ib / vb
                    s32 stride = scene->entities[n] & CMP_SKINNED ? sizeof(vertex_model_skinned) : sizeof(vertex_model);
                    
					pen::renderer_set_vertex_buffer(p_geom->vertex_buffer, 0, stride, 0 );
					pen::renderer_set_index_buffer(p_geom->index_buffer, p_geom->index_type, 0);

					//set textures
					if (p_mat)
					{
						for (u32 t = 0; t < put::ces::SN_NUM_TEXTURES; ++t)
						{
							if ( is_valid(p_mat->texture_id[t]) )
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
        
        enum e_debug_draw_flags
        {
            DD_MATRIX = 1<<0,
            DD_BONES = 1<<1,
            DD_AABB = 1<<2,
            DD_GRID = 1<<3,
            
            DD_NUM_FLAGS = 4
        };
        
        const c8* dd_names[]
        {
            "Matrices",
            "Bones",
            "AABB",
            "Grid"
        };
        static_assert(sizeof(dd_names)/sizeof(dd_names[0]) == DD_NUM_FLAGS, "mismatched");
        
        void enumerate_scene_ui(component_entity_scene* scene, bool* open )
        {
#ifdef CES_DEBUG
            ImGui::Begin("Scene Browser", open );
            
            if (ImGui::CollapsingHeader("Debug Draw"))
            {
                static bool* dd_bools = new bool[DD_NUM_FLAGS];
                
                for( s32 i = 0; i < DD_NUM_FLAGS; ++i )
                {
                    ImGui::Checkbox(dd_names[i], &dd_bools[i]);
                    
                    u32 mask = 1<<i;
                    
                    if(dd_bools[i])
                        scene->debug_flags |= mask;
                    else
                        scene->debug_flags &= ~(mask);
                    
                    if( i != DD_NUM_FLAGS-1 )
                    {
                        ImGui::SameLine();
                    }
                }
            }
            
            ImGui::BeginChild("Entities", ImVec2(400, 400), true );
            
            s32& selected_index = scene->selected_index;
            
            for (u32 i = 0; i < scene->num_nodes; ++i)
            {
                bool selected = false;
                ImGui::Selectable(scene->names[i].c_str(), &selected);
                
                if (selected)
                {
                    selected_index = i;
                }
            }
            
            ImGui::EndChild();
            
            ImGui::SameLine();
            
            ImGui::BeginChild("Selected", ImVec2(416, 400), true );
            
            if (selected_index != -1)
            {
                //header
                ImGui::Text("%s", scene->names[selected_index].c_str());
                
                s32 parent_index = scene->parents[selected_index];
                if( parent_index != selected_index)
                    ImGui::Text("Parent: %s", scene->names[parent_index].c_str());
                
                ImGui::Separator();
                
                //geom
                ImGui::Text("Geometry: %s", scene->geometry_names[selected_index].c_str());
                ImGui::Separator();
                
                //material
                ImGui::Text("Material: %s", scene->material_names[selected_index].c_str());
                
                if (scene->material_names[selected_index].c_str())
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
                
                static bool open_anim_import = false;
                
                if( ImGui::CollapsingHeader("Animations") )
                {
                    if( ImGui::Button("Add Animation") )
                        open_anim_import = true;
                    
                    auto& controller = scene->anim_controller[selected_index];
                    
                    if( open_anim_import )
                    {
                        const c8* anim_import = put::dev_ui::file_browser(open_anim_import, 1, "**.pma" );
                        
                        if(anim_import)
                        {
                            anim_handle ah = load_pma(anim_import);
                            
                            if( is_valid(ah) )
                            {
                                scene->entities[selected_index] |= CMP_ANIM_CONTROLLER;
                                
                                bool exists = false;
                                
                                for( auto& h : controller.handles )
                                    if( h == ah )
                                        exists = true;
                                
                                if(!exists)
                                    scene->anim_controller[selected_index].handles.push_back(ah);
                            }
                        }
                    }
                    
                    for( auto& h : controller.handles )
                    {
                        auto& anim = k_animations[h];
                        
                        bool selected = false;
                        ImGui::Selectable(anim.name.c_str(), &selected);
                        
                        if( selected )
                            controller.current_animation = h;
                    }
                    ImGui::Separator();
                }
            }
            
            ImGui::EndChild();
            
            ImGui::End();
#endif
        }
        

		void render_scene_debug( component_entity_scene* scene, const scene_view& view )
		{
#ifdef CES_DEBUG
            if( scene->debug_flags & DD_MATRIX )
            {
                for (u32 n = 0; n < scene->num_nodes; ++n)
                {
                    put::dbg::add_coord_space(scene->world_matrices[n], 0.5f);
                }
            }

            if( scene->debug_flags & DD_BONES )
            {
                for (u32 n = 0; n < scene->num_nodes; ++n)
                {
                    if( !(scene->entities[n] & CMP_BONE)  )
                        continue;
                    
                    u32 p = scene->parents[n];
                    if( p != n )
                    {
                        vec3f p1 = scene->world_matrices[n].get_translation();
                        vec3f p2 = scene->world_matrices[p].get_translation();
                        
                        put::dbg::add_line(p1, p2, vec4f::magenta() );
                    }
                }
            }
            
            if( scene->debug_flags & DD_GRID )
            {
                put::dbg::add_grid(vec3f::zero(), vec3f(100.0f), 100);
            }

			put::dbg::render_3d(view.cb_view);
#endif
		}
	}
}
