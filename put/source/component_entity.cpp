#include <functional>
#include <fstream>

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
	namespace ces
	{
        //id hashes
        const hash_id ID_JOINT = PEN_HASH("joint");
        const hash_id ID_TRAJECTORY = PEN_HASH("trajectoryshjnt");
        
        enum pmm_transform_types
        {
            PMM_TRANSLATE = 0,
            PMM_ROTATE = 1,
            PMM_MATRIX
        };
        
        enum e_debug_draw_flags
        {
            DD_MATRIX = 1<<0,
            DD_BONES = 1<<1,
            DD_AABB = 1<<2,
            DD_GRID = 1<<3,
            DD_HIDE = 1<<4,
            DD_NODE = 1<<5,
            
            DD_NUM_FLAGS = 6
        };
        
        const c8* dd_names[]
        {
            "Matrices",
            "Bones",
            "AABB",
            "Grid",
            "Hide Main Render",
            "Selected Node"
        };
        static_assert(sizeof(dd_names)/sizeof(dd_names[0]) == DD_NUM_FLAGS, "mismatched");

#define ALLOC_COMPONENT_ARRAY( SCENE, COMPONENT, TYPE )											\
		if( !SCENE->COMPONENT )																	\
			SCENE->COMPONENT = (TYPE*)pen::memory_alloc(sizeof(TYPE)*SCENE->nodes_size );		\
		else																					\
			SCENE->COMPONENT = (TYPE*)pen::memory_realloc(SCENE->COMPONENT,sizeof(TYPE)*SCENE->nodes_size); \
		pen::memory_zero( SCENE->COMPONENT, sizeof(TYPE)*SCENE->nodes_size)

#define FREE_COMPONENT_ARRAY( SCENE, COMPONENT ) pen::memory_free( SCENE->COMPONENT ); SCENE->COMPONENT = nullptr

#ifdef CES_DEBUG
#define ASSIGN_DEBUG_NAME( D, S ) D = S
#else
#define ASSIGN_DEBUG_NAME( D, S )
#endif
        
		struct component_entity_scene_instance
		{
			u32 id_name;
			const c8* name;
			entity_scene* scene;
		};

		std::vector<component_entity_scene_instance> k_scenes;
        
        struct per_model_cbuffer
        {
            mat4 world_matrix;
        };

		void resize_scene_buffers(entity_scene* scene)
		{
			scene->nodes_size += 1024;
			
			//ids
			ALLOC_COMPONENT_ARRAY(scene, id_name, hash_id);
			ALLOC_COMPONENT_ARRAY(scene, id_geometry, hash_id);
			ALLOC_COMPONENT_ARRAY(scene, id_material, hash_id);
            ALLOC_COMPONENT_ARRAY(scene, id_resource, hash_id);

            //flags
            ALLOC_COMPONENT_ARRAY(scene, entities, a_u64);
            
			//components
			ALLOC_COMPONENT_ARRAY(scene, parents, u32);
			ALLOC_COMPONENT_ARRAY(scene, local_matrices, mat4);
			ALLOC_COMPONENT_ARRAY(scene, world_matrices, mat4);
			ALLOC_COMPONENT_ARRAY(scene, offset_matrices, mat4);
			ALLOC_COMPONENT_ARRAY(scene, physics_matrices, mat4);
			ALLOC_COMPONENT_ARRAY(scene, physics_handles, u32);
			ALLOC_COMPONENT_ARRAY(scene, multibody_handles, u32);
			ALLOC_COMPONENT_ARRAY(scene, multibody_link, s32);
            ALLOC_COMPONENT_ARRAY(scene, cbuffer, u32);
            
            ALLOC_COMPONENT_ARRAY(scene, geometries, scene_node_geometry);
            ALLOC_COMPONENT_ARRAY(scene, materials, scene_node_material);
            ALLOC_COMPONENT_ARRAY(scene, physics_data, scene_node_physics);
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

		void free_scene_buffers( entity_scene* scene )
		{
            FREE_COMPONENT_ARRAY(scene, entities);
            
			FREE_COMPONENT_ARRAY(scene, id_name);
			FREE_COMPONENT_ARRAY(scene, id_geometry);
			FREE_COMPONENT_ARRAY(scene, id_material);
            FREE_COMPONENT_ARRAY(scene, id_resource);
            
			FREE_COMPONENT_ARRAY(scene, parents);
			FREE_COMPONENT_ARRAY(scene, local_matrices);
			FREE_COMPONENT_ARRAY(scene, world_matrices);
			FREE_COMPONENT_ARRAY(scene, offset_matrices);
			FREE_COMPONENT_ARRAY(scene, physics_matrices);
			FREE_COMPONENT_ARRAY(scene, physics_handles);
			FREE_COMPONENT_ARRAY(scene, multibody_handles);
			FREE_COMPONENT_ARRAY(scene, multibody_link);
            FREE_COMPONENT_ARRAY(scene, cbuffer);
            
            FREE_COMPONENT_ARRAY(scene, geometries);
            FREE_COMPONENT_ARRAY(scene, materials);
            FREE_COMPONENT_ARRAY(scene, physics_data);
            FREE_COMPONENT_ARRAY(scene, anim_controller);

#ifdef CES_DEBUG
			FREE_COMPONENT_ARRAY(scene, names);
			FREE_COMPONENT_ARRAY(scene, geometry_names);
			FREE_COMPONENT_ARRAY(scene, material_names);
#endif
		}
        
        void clone_node( entity_scene* scene, u32 src, u32 dst, s32 parent, vec3f offset, const c8* suffix)
        {
            entity_scene* p_sn = scene;
            
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
            
            //ids
            p_sn->id_name[dst] = p_sn->id_name[src];
            p_sn->id_geometry[dst] = p_sn->id_geometry[src];
            p_sn->id_material[dst] = p_sn->id_material[src];
            p_sn->id_resource[dst] = p_sn->id_resource[src];
            
            //componenets
            p_sn->local_matrices[dst] = p_sn->local_matrices[src];
            p_sn->world_matrices[dst] = p_sn->world_matrices[src];
            p_sn->offset_matrices[dst] = p_sn->offset_matrices[src];
            p_sn->physics_matrices[dst] = p_sn->physics_matrices[src];
            p_sn->physics_handles[dst] = p_sn->physics_handles[src];
            p_sn->multibody_handles[dst] = p_sn->multibody_handles[src];
            p_sn->multibody_link[dst] = p_sn->multibody_link[src];
            p_sn->cbuffer[dst] = p_sn->cbuffer[src];
            
            p_sn->physics_data[dst] = p_sn->physics_data[src];
            p_sn->anim_controller[dst] = p_sn->anim_controller[src];
            p_sn->geometries[dst] = p_sn->geometries[src];
            p_sn->materials[dst] = p_sn->materials[src];
            
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

		entity_scene*	create_scene( const c8* name )
		{
			component_entity_scene_instance new_instance;
			new_instance.name = name;
			new_instance.scene = new entity_scene();

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
        
        Str read_parsable_string( std::ifstream& ifs )
        {
            Str name;
            u32 len = 0;
            
            ifs.read((c8*)&len, sizeof(u32));
            
            for(s32 i = 0; i < len; ++i)
            {
                c8 c;
                ifs.read((c8*)&c, 1);
                name.append(c);
            }
            
            return name;
        }
        
        void write_parsable_string( const Str& str, std::ofstream& ofs )
        {
            if(str.c_str())
            {
                u32 len = str.length();
                ofs.write( (const c8*)&len, sizeof(u32) );
                ofs.write( (const c8*)str.c_str(), len);
            }
            else
            {
                u32 zero = 0;
                ofs.write( (const c8*)&zero, sizeof(u32) );
            }
        }
        
        static std::vector<geometry_resource*> k_geometry_resources;
        
        geometry_resource* get_geometry_resource( hash_id hash )
        {
            for( auto* g : k_geometry_resources )
            {
                if( hash == g->hash )
                {
                    return g;
                }
            }
        
            return nullptr;
        }
        
        void instantiate_geometry( geometry_resource* gr, scene_node_geometry* instance )
        {
            instance->position_buffer = gr->position_buffer;
            instance->vertex_buffer = gr->vertex_buffer;
            instance->index_buffer = gr->index_buffer;
            instance->num_indices = gr->num_indices;
            instance->num_vertices = gr->num_vertices;
            instance->index_type = gr->index_type;
            instance->vertex_size = gr->vertex_size;
            instance->p_skin = gr->p_skin;
        }
        
        void load_geometry_resource( const c8* filename, const c8* geometry_name, const c8* data )
        {
            //generate hash
            pen::hash_murmur hm;
            hm.begin(0);
            hm.add(filename, pen::string_length(filename));
            hm.add(geometry_name, pen::string_length(geometry_name));
            hash_id file_hash = hm.end();
            
            //check for existing
            for( s32 g = 0; g < k_geometry_resources.size(); ++g )
            {
                if( file_hash == k_geometry_resources[g]->file_hash )
                {
                    return;
                }
            }
            
            u32* p_reader = (u32*)data;
            u32 version = *p_reader++;
            u32 num_meshes = *p_reader++;
            
            if( version < 1 )
                return;
            
            std::vector<Str> mat_names;
            for (u32 submesh = 0; submesh < num_meshes; ++submesh)
            {
                mat_names.push_back( read_parsable_string((const u32**)&p_reader) );
            }
            
            for (u32 submesh = 0; submesh < num_meshes; ++submesh)
            {
                hm.begin(0);
                hm.add(filename, pen::string_length(filename));
                hm.add(geometry_name, pen::string_length(geometry_name));
                hm.add(submesh);
                hash_id sub_hash = hm.end();
                
                geometry_resource* p_geometry = new geometry_resource;
                
                p_geometry->file_hash = file_hash;
                p_geometry->hash = sub_hash;
                p_geometry->geometry_name = geometry_name;
                p_geometry->filename = filename;
                p_geometry->material_name = mat_names[submesh];
                p_geometry->submesh_index = submesh;
                
                //skip physics
                p_reader++;
                p_reader++;
                
                vec3f min_extents;
                vec3f max_extents;
                
                pen::memory_cpy(&min_extents, p_reader, sizeof(vec3f));
                p_reader += 3;
                
                pen::memory_cpy(&max_extents, p_reader, sizeof(vec3f));
                p_reader += 3;
                
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
                    
                    p_geometry->p_skin = (scene_node_skin*)pen::memory_alloc(sizeof(scene_node_skin));
                    
                    pen::memory_cpy(&p_geometry->p_skin->bind_shape_matirx, p_reader, sizeof(mat4));
                    p_reader += 16;
                    
                    mat4 max_swap;
                    max_swap.create_axis_swap(vec3f(1.0f, 0.0f, 0.0f), vec3f(0.0f, 0.0f, -1.0f), vec3f(0.0f, 1.0f, 0.0f));
                    mat4 max_swap_inv = max_swap.inverse4x4();
                    
                    mat4 final_bind = max_swap * p_geometry->p_skin->bind_shape_matirx * max_swap_inv;
                    
                    p_geometry->p_skin->bind_shape_matirx = final_bind;
                    
                    u32 num_ijb_floats = *p_reader++;
                    pen::memory_cpy(&p_geometry->p_skin->joint_bind_matrices[0], p_reader, sizeof(f32) * num_ijb_floats);
                    p_reader += num_ijb_floats;
                    
                    p_geometry->p_skin->num_joints = num_ijb_floats / 16;
                    
                    for (u32 joint = 0; joint < p_geometry->p_skin->num_joints; ++joint)
                    {
                        p_geometry->p_skin->joint_bind_matrices[joint] = max_swap * p_geometry->p_skin->joint_bind_matrices[joint] * max_swap_inv;
                        p_geometry->p_skin->joint_matrices[joint].create_identity();
                    }
                }
                
                p_geometry->vertex_size = vertex_size;
                
                //all vertex data is written out as 4 byte ints
                u32 num_verts = num_floats / (vertex_size / sizeof(u32));
                u32 num_pos_verts = num_pos_floats / (sizeof(vertex_position) / sizeof(u32));
                
                p_geometry->num_vertices = num_verts;
                
                pen::buffer_creation_params bcp;
                bcp.usage_flags = PEN_USAGE_DEFAULT;
                bcp.bind_flags = PEN_BIND_VERTEX_BUFFER;
                bcp.cpu_access_flags = 0;
                bcp.buffer_size = sizeof(vertex_position) * num_pos_verts;
                bcp.data = (void*)p_reader;
                
                p_geometry->position_buffer = pen::renderer_create_buffer(bcp);
                
                p_reader += bcp.buffer_size / sizeof(f32);
                
                bcp.buffer_size = vertex_size * num_verts;
                bcp.data = (void*)p_reader;
                
                p_geometry->vertex_buffer = pen::renderer_create_buffer(bcp);
                
                p_reader += bcp.buffer_size / sizeof(u32);
                
                bcp.usage_flags = PEN_USAGE_DEFAULT;
                bcp.bind_flags = PEN_BIND_INDEX_BUFFER;
                bcp.cpu_access_flags = 0;
                bcp.buffer_size = index_size * num_indices;
                bcp.data = (void*)p_reader;
                
                p_geometry->num_indices = num_indices;
                p_geometry->index_type = index_size == 2 ? PEN_FORMAT_R16_UINT : PEN_FORMAT_R32_UINT;
                p_geometry->index_buffer = pen::renderer_create_buffer(bcp);
                
                p_reader = (u32*)((c8*)p_reader + bcp.buffer_size);
                
                p_reader += num_collision_floats;
                
                k_geometry_resources.push_back(p_geometry);
            }
        }

        void load_geometry
        (
            const c8* data,
            scene_node_geometry* p_geometries,
            scene_node_physics* p_physics,
            entity_scene* scene,
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
                
                //p_geometries[i].submesh_material_index = -1;
                
                u32 num_symbols = material_symbols.size();
                for( u32 j = 0; j < num_symbols; ++j)
                {
                    if ( mesh_material == material_symbols[j] )
                    {
                        //p_geometries[i].submesh_material_index = j;
                        break;
                    }
                }
                
                //PEN_ASSERT(p_geometries[i].submesh_material_index != -1);
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
        
        static std::vector<material_resource*> k_material_resources;
        
        material_resource* get_material_resource( hash_id hash )
        {
            for( auto* m : k_material_resources )
            {
                if( m->hash == hash )
                {
                    return m;
                }
            }
            
            return nullptr;
        }
        
        void instantiate_material( material_resource* mr, scene_node_material* instance )
        {
            instance->diffuse_rgb_shininess = mr->diffuse_rgb_shininess;
            instance->specular_rgb_reflect = mr->specular_rgb_reflect;
            
            pen::memory_cpy(instance->texture_id, mr->texture_id, sizeof(u32)*SN_NUM_TEXTURES);
        }
        
        void load_material_resource( const c8* filename, const c8* material_name, const c8* data )
        {
            pen::hash_murmur hm;
            hm.begin();
            hm.add(filename, pen::string_length(filename));
            hm.add(material_name, pen::string_length(material_name));
            hash_id hash = hm.end();
            
            for( s32 m = 0; m < k_material_resources.size(); ++m )
            {
                if( k_material_resources[ m ]->hash == hash )
                {
                    return;
                }
            }
            
            const u32* p_reader = ( u32* ) data;
            
            u32 version = *p_reader++;
            
            if( version < 1 )
                return;
            
            material_resource* p_mat = new material_resource;
            
            p_mat->filename = filename;
            p_mat->material_name = material_name;
            p_mat->hash = hash;
            
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
            
            //todo variable number of maps
            
            for (u32 map = 0; map < put::ces::SN_NUM_TEXTURES; ++map)
            {
                Str texture_name = read_parsable_string(&p_reader);
                
                p_mat->texture_id[map] = -1;
                
                if (!texture_name.empty())
                    p_mat->texture_id[map] = put::load_texture(texture_name.c_str());
            }
            
            k_material_resources.push_back(p_mat);
            
            return;
        }
        
        static std::vector<animation> k_animations;
        
        anim_handle load_pma(const c8* filename)
        {
            hash_id filename_hash = PEN_HASH( filename );
            
            //search for existing
            s32 num_anims = k_animations.size();
            for( s32 i = 0; i < num_anims; ++i )
            {
                if( k_animations[i].id_name == filename_hash )
                {
                    return (anim_handle)i;
                }
            }
            
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
            
            new_animation.length = 0.0f;
            new_animation.step = FLT_MAX;
            
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
                            new_animation.channels[i].num_frames = num_floats;
                            new_animation.channels[i].times = data;
                            break;
                        case A_TRANSFORM:
                            new_animation.channels[i].matrices = (mat4*)data;
                            break;
                        default:
                            break;
                    };
                }
                
                for( s32 t = 0; t < new_animation.channels[i].num_frames; ++t)
                {
                    f32* times = new_animation.channels[i].times;
                    if( t > 0 )
                    {
                        f32 interval = times[t] - times[t-1];
                        new_animation.step = fmin( new_animation.step, interval );
                    }
                    
                    new_animation.length = fmax( times[t], new_animation.length );
                }
            }
            
            pen::memory_free(anim_file);
            return (anim_handle)k_animations.size()-1;
        }
        
        void load_pmm(const c8* filename, entity_scene* scene, u32 load_flags )
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
            std::vector<Str> geometry_names;
            
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
                geometry_names.push_back(name);
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
            
            //load resources
            if(load_flags & PMM_MATERIAL)
            {
                for (u32 m = 0; m < num_materials; ++m)
                {
                    u32* p_mat_data = (u32*)(p_data_start + material_offsets[m]);
                    load_material_resource( filename, material_names[m].c_str(), (const c8*)p_mat_data );
                }
            }
            
            if(load_flags & PMM_GEOMETRY)
            {
                for (u32 g = 0; g < num_geom; ++g)
                {
                    u32* p_geom_data = (u32*)(p_data_start + geom_offsets[g]);
                    load_geometry_resource( filename, geometry_names[g].c_str(), (const c8*)p_geom_data );
                }
            }
            
            if(!(load_flags&PMM_NODES))
            {
                pen::memory_free(model_file);
                return;
            }
            
            //scene nodes
            u32 node_zero_offset = scene->num_nodes;
            u32 current_node = node_zero_offset;
            u32 inserted_nodes = 0;
            
            //load scene nodes
            for (u32 n = 0; n < num_import_nodes; ++n)
            {
                u32 node_type = *p_u32reader++;
                
                Str node_name = read_parsable_string(&p_u32reader);
                Str geometry_name = read_parsable_string(&p_u32reader);
                
                scene->id_name[current_node] = PEN_HASH( node_name.c_str() );
                scene->id_geometry[current_node] = PEN_HASH( geometry_name.c_str() );
                
                ASSIGN_DEBUG_NAME( scene->names[current_node], node_name );
                ASSIGN_DEBUG_NAME( scene->geometry_names[current_node], geometry_name );
                
                if( scene->id_geometry[current_node] == ID_JOINT )
                    scene->entities[current_node] |= CMP_BONE;
                
                if( scene->id_name[current_node] == ID_TRAJECTORY )
                    scene->entities[current_node] |= CMP_ANIM_TRAJECTORY;
                
                u32 num_meshes = *p_u32reader++;
                
                std::vector<Str> mesh_material_names;
                std::vector<Str> mesh_material_symbols;
                
                //material pre load
                if (num_meshes > 0)
                {
                    //read in material names
                    for (u32 mat = 0; mat < num_meshes; ++mat)
                        mesh_material_names.push_back(read_parsable_string(&p_u32reader));
                    
                    //read material symbol names
                    for (u32 mat = 0; mat < num_meshes; ++mat)
                        mesh_material_symbols.push_back(read_parsable_string(&p_u32reader));
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
                
                static f32 zero_rotation_epsilon = 0.000001f;
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
                        
                        //generate geometry hash
                        pen::hash_murmur hm;
                        hm.begin(0);
                        hm.add(filename, pen::string_length(filename));
                        hm.add(geometry_name.c_str(), geometry_name.length());
                        hm.add(submesh);
                        hash_id geom_hash = hm.end();
                        
                        scene->id_geometry[dest] = geom_hash;
                        
                        geometry_resource* gr = get_geometry_resource(geom_hash);
                        
                        if( gr )
                        {
                            instantiate_geometry(gr, &scene->geometries[dest]);
                            
                            hm.begin();
                            hm.add(filename, pen::string_length(filename));
                            hm.add( gr->material_name.c_str(), gr->material_name.length());
                            hash_id material_hash = hm.end();
                            
                            material_resource* mr = get_material_resource(material_hash);
                            
                            if( mr )
                            {
                                ASSIGN_DEBUG_NAME(scene->material_names[dest], gr->material_name);
                                
                                instantiate_material(mr, &scene->materials[dest]);
                                
                                scene->id_material[dest] = material_hash;
                            }
                        }
                    }
                }
                
                current_node = dest + 1;
                scene->num_nodes = current_node;
            }
            
            pen::memory_free(model_file);
            return;
        }

		void render_scene_view( const scene_view& view )
		{
            entity_scene* scene = view.scene;
            
            if( scene->debug_flags & DD_HIDE )
                return;
            
            static pmfx::pmfx_handle model_pmfx = pmfx::load("fx_test");
            
            pen::renderer_set_constant_buffer(view.cb_view, 0, PEN_SHADER_TYPE_VS);
            
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
					pen::renderer_set_vertex_buffer(p_geom->vertex_buffer, 0, p_geom->vertex_size, 0 );
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
		}

		void update_scene( entity_scene* scene, f32 dt )
		{
            for (u32 n = 0; n < scene->num_nodes; ++n)
            {
                if( scene->entities[n] & CMP_ANIM_CONTROLLER )
                {
                    auto& controller = scene->anim_controller[n];
                    
                    bool apply_trajectory = false;
                    mat4 trajectory;
                    
                    if( is_valid( controller.current_animation ) )
                    {
                        auto& anim = k_animations[controller.current_animation];
                        
                        if(controller.play_flags == 1)
                            controller.current_time += dt*0.1f;
                        
                        for( s32 c = 0; c < anim.num_channels; ++c )
                        {
                            s32 num_frames = anim.channels[c].num_frames;
                            
                            if( num_frames <= 0 )
                                continue;
                            
                            s32 t = 0;
                            for( t = 0; t < num_frames; ++t )
                                if( controller.current_time < anim.channels[c].times[t] )
                                    break;
                            
                            if( t >= num_frames )
                                t = num_frames-1;

                            mat4& mat = anim.channels[c].matrices[t];
                            
                            //todo bake
                            for( s32 b = 1; b < anim.num_channels+1; ++b )
                            {
                                if( scene->id_name[n+b] == anim.channels[c].target && scene->entities[n+b] & CMP_BONE )
                                {
                                    if( scene->entities[n+b] & CMP_ANIM_TRAJECTORY )
                                    {
                                        trajectory = mat;
                                    }
                                    
                                    scene->local_matrices[n+b] = mat;
                                    break;
                                }
                            }
                            
                            if( controller.current_time > anim.length )
                            {
                                apply_trajectory = true;
                                controller.current_time = 0.0f;
                            }
                        }
                    }
                    
                    if( apply_trajectory )
                    {
                        vec3f r = scene->local_matrices[n].get_right();
                        vec3f u = scene->local_matrices[n].get_up();
                        vec3f f = scene->local_matrices[n].get_fwd();
                        vec3f p = scene->local_matrices[n].get_translation();
                        
                        scene->local_matrices[n].set_vectors(r, u, f, p + trajectory.get_translation() );
                    }
                }
            }
            
			for (u32 n = 0; n < scene->num_nodes; ++n)
			{
				u32 parent = scene->parents[n];

				if (parent == n)
					scene->world_matrices[n] = scene->local_matrices[n];
				else
					scene->world_matrices[n] = scene->world_matrices[parent] * scene->local_matrices[n];
			}
		}
        
        void save_scene( const c8* filename, entity_scene* scene )
        {
            //fix down
            //-take subsets of scenes and make into a new scene, to save unity style prefabs
            
            std::ofstream ofs(filename, std::ofstream::binary);

            static s32 version = 1;
            ofs.write( (const c8*)&version, sizeof(s32));
            ofs.write( (const c8*)&scene->num_nodes, sizeof(u32));
            
            //user prefs
            ofs.write( (const c8*)&scene->debug_flags, sizeof(u32));
            ofs.write( (const c8*)&scene->selected_index, sizeof(s32));
            
            //names
            for( s32 n = 0; n < scene->num_nodes; ++n )
            {
                write_parsable_string(scene->names[n], ofs);
                write_parsable_string(scene->geometry_names[n], ofs);
                write_parsable_string(scene->material_names[n], ofs);
            }
            
            //simple parts of the scene are just homogeonous chucks of data
            ofs.write( (const c8*)scene->entities,          sizeof( a_u64 ) * scene->num_nodes );
            ofs.write( (const c8*)scene->parents,           sizeof( u32 )   * scene->num_nodes );
            ofs.write( (const c8*)scene->local_matrices,    sizeof( mat4 )  * scene->num_nodes );
            ofs.write( (const c8*)scene->world_matrices,    sizeof( mat4 )  * scene->num_nodes );
            ofs.write( (const c8*)scene->offset_matrices,   sizeof( mat4 )  * scene->num_nodes );
            ofs.write( (const c8*)scene->physics_matrices,  sizeof( mat4 )  * scene->num_nodes );
            
            //animations need reloading from files
            for( s32 n = 0; n < scene->num_nodes; ++n )
            {
                s32 size = scene->anim_controller[n].handles.size();
                
                ofs.write( (const c8*)&size, sizeof( s32 ) );
                
                for( auto& h : scene->anim_controller[n].handles )
                {
                    write_parsable_string( k_animations[h].name, ofs);
                }
            }
            
            //geometry
            for( s32 n = 0; n < scene->num_nodes; ++n )
            {
                if( scene->entities[n] & CMP_GEOMETRY )
                {
                    geometry_resource* gr = get_geometry_resource(scene->id_geometry[n]);
                    
                    //has geometry
                    u32 one = 1;
                    ofs.write( (const c8*)&one, sizeof( u32 ) );
                    ofs.write( (const c8*)&gr->submesh_index, sizeof( u32 ) );
                    
                    write_parsable_string(gr->filename, ofs);
                    write_parsable_string(gr->geometry_name, ofs);
                }
                else
                {
                    u32 zero = 0;
                    ofs.write( (const c8*)&zero, sizeof( u32 ) );
                }
            }
            
            //material
            for( s32 n = 0; n < scene->num_nodes; ++n )
            {
                material_resource* mr = get_material_resource(scene->id_material[n]);
                
                if( scene->entities[n] & CMP_MATERIAL && mr )
                {
                    //has geometry
                    u32 one = 1;
                    ofs.write( (const c8*)&one, sizeof( u32 ) );
                    
                    write_parsable_string(mr->filename, ofs);
                    write_parsable_string(mr->material_name, ofs);
                }
                else
                {
                    u32 zero = 0;
                    ofs.write( (const c8*)&zero, sizeof( u32 ) );
                }
            }
            
            ofs.close();
        }
        
        static bool* k_dd_bools = nullptr;
        
        void load_scene( const c8* filename, entity_scene* scene )
        {
            std::ifstream ifs(filename, std::ofstream::binary);
            
            s32 version;
            ifs.read((c8*)&version, sizeof(s32));
            ifs.read((c8*)&scene->num_nodes, sizeof(u32));
            
            //user prefs
            ifs.read((c8*)&scene->debug_flags, sizeof(u32));
            ifs.read((c8*)&scene->selected_index, sizeof(s32));
            
            if(!k_dd_bools)
            {
                k_dd_bools = new bool[DD_NUM_FLAGS];
            }
            
            for( s32 i = 0; i < DD_NUM_FLAGS; ++i )
            {
                if( scene->debug_flags & (1<<i) )
                    k_dd_bools[i] = true;
            }
            
            //names
            for( s32 n = 0; n < scene->num_nodes; ++n )
            {
                scene->names[n] = read_parsable_string(ifs);
                scene->geometry_names[n] = read_parsable_string(ifs);
                scene->material_names[n] = read_parsable_string(ifs);
                
                //generate hashes
                scene->id_name[n] = PEN_HASH(scene->names[n].c_str());
                scene->id_geometry[n] = PEN_HASH(scene->geometry_names[n].c_str());
                scene->id_material[n] = PEN_HASH(scene->material_names[n].c_str());
            }
            
            //data
            ifs.read( (c8*)scene->entities, sizeof( a_u64 ) * scene->num_nodes );
            ifs.read( (c8*)scene->parents, sizeof( u32 ) * scene->num_nodes );
            ifs.read( (c8*)scene->local_matrices, sizeof( mat4 ) * scene->num_nodes );
            ifs.read( (c8*)scene->world_matrices, sizeof( mat4 ) * scene->num_nodes );
            ifs.read( (c8*)scene->offset_matrices, sizeof( mat4 ) * scene->num_nodes );
            ifs.read( (c8*)scene->physics_matrices, sizeof( mat4 ) * scene->num_nodes );
            
            //animations
            for( s32 n = 0; n < scene->num_nodes; ++n )
            {
                s32 size;
                ifs.read( (c8*)&size, sizeof(s32) );
                
                for( s32 i = 0; i < size; ++i )
                {
                    Str anim_name = read_parsable_string(ifs);
                    
                    anim_handle h = load_pma(anim_name.c_str());
                    
                    scene->anim_controller[n].handles.push_back( h );
                }
            }
            
            //geometry
            for( s32 n = 0; n < scene->num_nodes; ++n )
            {
                u32 has = 0;
                ifs.read( (c8*)&has, sizeof( u32 ) );
                
                if( scene->entities[n] & CMP_GEOMETRY && has )
                {
                    u32 submesh;
                    ifs.read((c8*)&submesh, sizeof(u32));
                    
                    Str filename = read_parsable_string(ifs);
                    Str geometry_name = read_parsable_string(ifs);
                    
                    load_pmm(filename.c_str(), nullptr, PMM_GEOMETRY);
                    
                    pen::hash_murmur hm;
                    hm.begin(0);
                    hm.add(filename.c_str(), filename.length());
                    hm.add(geometry_name.c_str(), geometry_name.length());
                    hm.add(submesh);
                    hash_id geom_hash = hm.end();
                    
                    geometry_resource* gr = get_geometry_resource(geom_hash);
                    
                    scene->id_geometry[n] = geom_hash;
                    
                    if( gr )
                    {
                        instantiate_geometry(gr, &scene->geometries[n]);
                    }
                }
            }
            
            //materials
            for( s32 n = 0; n < scene->num_nodes; ++n )
            {
                u32 has = 0;
                ifs.read( (c8*)&has, sizeof( u32 ) );
                
                if( scene->entities[n] & CMP_MATERIAL && has )
                {
                    Str filename = read_parsable_string(ifs);
                    Str material_name = read_parsable_string(ifs);
                    
                    load_pmm(filename.c_str(), nullptr, PMM_MATERIAL);
                    
                    pen::hash_murmur hm;
                    hm.begin();
                    hm.add(filename.c_str(), filename.length() );
                    hm.add( material_name.c_str(), material_name.length());
                    hash_id material_hash = hm.end();
                    
                    material_resource* mr = get_material_resource(material_hash);
                    
                    if( mr )
                    {
                        ASSIGN_DEBUG_NAME(scene->material_names[n], material_name);
                        scene->id_material[n] = material_hash;
                        
                        instantiate_material(mr, &scene->materials[n]);
                    }
                }
            }
            
            ifs.close();
        }
        
        struct scene_tree
        {
            s32 node_index;
            const c8* node_name;
            
            std::vector<scene_tree> children;
        };
        
        void scene_tree_add_node( scene_tree& tree, scene_tree& node, std::vector<s32>& heirarchy )
        {
            if( heirarchy.empty() )
                return;
            
            if( heirarchy[0] == tree.node_index )
            {
                heirarchy.erase(heirarchy.begin());
                
                if( heirarchy.empty() )
                {
                    tree.children.push_back(node);
                    return;
                }
                
                for( auto& child : tree.children )
                {
                    scene_tree_add_node(child, node, heirarchy );
                }
            }
        }
        
        void scene_tree_enumerate( const scene_tree& tree, s32& selected )
        {
            for( auto& child : tree.children )
            {
                if( child.children.empty() )
                {
                    ImGui::Selectable(child.node_name);
                    if (ImGui::IsItemClicked())
                        selected = child.node_index;
                }
                else
                {
                    if(tree.node_index == -1)
                        ImGui::Unindent(ImGui::GetTreeNodeToLabelSpacing());
                    
                    ImGuiTreeNodeFlags node_flags = selected == child.node_index ? ImGuiTreeNodeFlags_Selected : 0;
                    bool node_open = ImGui::TreeNodeEx((void*)(intptr_t)child.node_index, node_flags, child.node_name, child.node_index);
                    if (ImGui::IsItemClicked())
                        selected = child.node_index;
                    
                    if( node_open )
                    {
                        scene_tree_enumerate(child, selected);
                        ImGui::TreePop();
                    }
                    
                    if(tree.node_index == -1)
                        ImGui::Indent(ImGui::GetTreeNodeToLabelSpacing());
                }
            }
        }
        
        void enumerate_resources( bool* open )
        {
            ImGui::Begin("Resource Browser", open );
            
            if( ImGui::CollapsingHeader("Geometry") )
            {
                for( auto* g : k_geometry_resources )
                {
                    ImGui::Text("Source: %s", g->filename.c_str());
                    ImGui::Text("Geometry: %s", g->geometry_name.c_str());
                    ImGui::Text("Material: %s", g->material_name.c_str());
                    ImGui::Text("File Hash: %i", g->file_hash);
                    ImGui::Text("Hash: %i", g->hash);
                    ImGui::Text("Vertices: %i", g->num_vertices);
                    ImGui::Text("Indices: %i", g->num_indices);
                    
                    ImGui::Separator();
                }
            }
            
            if( ImGui::CollapsingHeader("Materials") )
            {
                for( auto* m : k_material_resources )
                {
                    for (u32 t = 0; t < put::ces::SN_NUM_TEXTURES; ++t)
                    {
                        if (m->texture_id[t] > 0)
                        {
                            if (t > 0)
                                ImGui::SameLine();
                            
                            ImGui::Image(&m->texture_id[t], ImVec2(128, 128));
                        }
                    }
                    
                    ImGui::Separator();
                }
            }
            
            ImGui::End();
        }
        
        void enumerate_scene_ui( entity_scene* scene, bool* open )
        {
#ifdef CES_DEBUG
            ImGui::Begin("Scene Browser", open );
            
            if (ImGui::CollapsingHeader("Debug Draw"))
            {
                if(!k_dd_bools)
                {
                    k_dd_bools = new bool[DD_NUM_FLAGS];
                    pen::memory_set(k_dd_bools, 0x0, sizeof(bool)*DD_NUM_FLAGS);
                }
                
                for( s32 i = 0; i < DD_NUM_FLAGS; ++i )
                {
                    ImGui::Checkbox(dd_names[i], &k_dd_bools[i]);
                    
                    u32 mask = 1<<i;
                    
                    if(k_dd_bools[i])
                        scene->debug_flags |= mask;
                    else
                        scene->debug_flags &= ~(mask);
                    
                    if( i != DD_NUM_FLAGS-1 )
                    {
                        ImGui::SameLine();
                    }
                }
            }
            
            static bool list_view = false;
            if( ImGui::Button(ICON_FA_LIST) )
                list_view = true;
            
            ImGui::SameLine();
            if( ImGui::Button(ICON_FA_USB) )
                list_view = false;
            
            ImGui::Columns( 2 );
            
            ImGui::BeginChild("Entities", ImVec2(0, 0), true );
            
            s32& selected_index = scene->selected_index;
            
            if( list_view )
            {
                for (u32 i = 0; i < scene->num_nodes; ++i)
                {
                    bool selected = false;
                    ImGui::Selectable(scene->names[i].c_str(), &selected);
                    
                    if (selected)
                    {
                        selected_index = i;
                    }
                }
            }
            else
            {
                //tree view
                scene_tree tree;
                tree.node_index = -1;
                
                //todo this could be cached
                for( s32 n = 0; n < scene->num_nodes; ++n )
                {
                    scene_tree node;
                    node.node_name = scene->names[n].c_str();
                    node.node_index = n;
                    
                    if( scene->parents[n] == n )
                    {
                        tree.children.push_back(node);
                    }
                    else
                    {
                        std::vector<s32> heirarchy;
                        
                        u32 p = n;
                        while( scene->parents[p] != p )
                        {
                            p = scene->parents[p];
                            heirarchy.insert(heirarchy.begin(), p);
                        }
                        
                        heirarchy.insert(heirarchy.begin(),-1);
                        
                        scene_tree_add_node( tree, node, heirarchy );
                    }
                }
                
                scene_tree_enumerate(tree, selected_index);
            }
            
            ImGui::EndChild();
            //ImGui::SameLine();
            
            ImGui::NextColumn();
            
            ImGui::BeginChild("Selected", ImVec2(0, 0), true );
            
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
                    
                    if( is_valid(controller.current_animation) )
                    {
                        if( ImGui::InputInt("Frame", &controller.current_frame ) )
                            controller.play_flags = 0;
                        
                        ImGui::SameLine();
                        
                        if( controller.play_flags == 0 )
                        {
                            if( ImGui::Button(ICON_FA_PLAY) )
                                controller.play_flags = 1;
                        }
                        else
                        {
                            if( ImGui::Button(ICON_FA_STOP) )
                                controller.play_flags = 0;
                        }
                    }
                    
                    ImGui::Separator();
                }
            }
            
            ImGui::EndChild();

            ImGui::Columns(1);
            
            ImGui::End();
#endif
        }
        

		void render_scene_debug( const scene_view& view )
		{
#ifdef CES_DEBUG
            entity_scene* scene = view.scene;
            
            if( scene->debug_flags & DD_MATRIX )
            {
                for (u32 n = 0; n < scene->num_nodes; ++n)
                {
                    put::dbg::add_coord_space(scene->world_matrices[n], 0.5f);
                }
            }
            
            if( scene->debug_flags & DD_AABB )
            {
                for (u32 n = 0; n < scene->num_nodes; ++n)
                {
                    put::dbg::add_aabb(scene->physics_data[n].min_extents, scene->physics_data[n].max_extents);
                }
            }

            if( scene->debug_flags & DD_BONES )
            {
                for (u32 n = 0; n < scene->num_nodes; ++n)
                {
                    if( !(scene->entities[n] & CMP_BONE)  )
                        continue;
                    
                    if( scene->entities[n] & CMP_ANIM_TRAJECTORY )
                    {
                        vec3f p = scene->world_matrices[n].get_translation();
                        
                        put::dbg::add_aabb( p - vec3f(0.1f, 0.1f, 0.1f), p + vec3f(0.1f, 0.1f, 0.1f), vec4f::green() );
                    }
                    
                    u32 p = scene->parents[n];
                    if( p != n )
                    {
                        if( !(scene->entities[p] & CMP_BONE) || (scene->entities[p] & CMP_ANIM_TRAJECTORY) )
                            continue;
                        
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
            
            if( scene->debug_flags & DD_NODE )
            {
                vec3f p = scene->world_matrices[scene->selected_index].get_translation();
                
                put::dbg::add_aabb( p - vec3f(0.1f, 0.1f, 0.1f), p + vec3f(0.1f, 0.1f, 0.1f));
            }

			put::dbg::render_3d(view.cb_view);
#endif
		}
	}
}
