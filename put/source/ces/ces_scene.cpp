#include <functional>
#include <fstream>

#include "file_system.h"
#include "dev_ui.h"
#include "debug_render.h"
#include "pmfx_controller.h"
#include "pmfx.h"
#include "str/Str.h"
#include "str_utilities.h"
#include "hash.h"

#include "ces/ces_utilities.h"
#include "ces/ces_resources.h"
#include "ces/ces_scene.h"

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
        
		struct entity_scene_instance
		{
			u32 id_name;
			const c8* name;
			entity_scene* scene;
		};

		std::vector<entity_scene_instance> k_scenes;
        
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

			ALLOC_COMPONENT_ARRAY(scene, transforms, transform);
			ALLOC_COMPONENT_ARRAY(scene, local_matrices, mat4);
			ALLOC_COMPONENT_ARRAY(scene, world_matrices, mat4);
			ALLOC_COMPONENT_ARRAY(scene, offset_matrices, mat4);
			ALLOC_COMPONENT_ARRAY(scene, physics_matrices, mat4);
            ALLOC_COMPONENT_ARRAY(scene, bounding_volumes, bounding_volume);
        
			ALLOC_COMPONENT_ARRAY(scene, physics_handles, u32);
			ALLOC_COMPONENT_ARRAY(scene, multibody_handles, u32);
			ALLOC_COMPONENT_ARRAY(scene, multibody_link, s32);
            ALLOC_COMPONENT_ARRAY(scene, cbuffer, u32);
            
            ALLOC_COMPONENT_ARRAY(scene, geometries, scene_node_geometry);
            ALLOC_COMPONENT_ARRAY(scene, materials, scene_node_material);
            ALLOC_COMPONENT_ARRAY(scene, physics_data, scene_node_physics);
            ALLOC_COMPONENT_ARRAY(scene, anim_controller, animation_controller);
            ALLOC_COMPONENT_ARRAY(scene, lights, scene_node_light);

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
			FREE_COMPONENT_ARRAY(scene, transforms);
			FREE_COMPONENT_ARRAY(scene, local_matrices);
			FREE_COMPONENT_ARRAY(scene, world_matrices);
			FREE_COMPONENT_ARRAY(scene, offset_matrices);
			FREE_COMPONENT_ARRAY(scene, physics_matrices);
            FREE_COMPONENT_ARRAY(scene, bounding_volumes);
            
			FREE_COMPONENT_ARRAY(scene, physics_handles);
			FREE_COMPONENT_ARRAY(scene, multibody_handles);
			FREE_COMPONENT_ARRAY(scene, multibody_link);
            FREE_COMPONENT_ARRAY(scene, cbuffer);
            
            FREE_COMPONENT_ARRAY(scene, geometries);
            FREE_COMPONENT_ARRAY(scene, materials);
            FREE_COMPONENT_ARRAY(scene, physics_data);
            FREE_COMPONENT_ARRAY(scene, anim_controller);
            FREE_COMPONENT_ARRAY(scene, lights);

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
			p_sn->transforms[dst] = p_sn->transforms[src];
            p_sn->local_matrices[dst] = p_sn->local_matrices[src];
            p_sn->world_matrices[dst] = p_sn->world_matrices[src];
            p_sn->offset_matrices[dst] = p_sn->offset_matrices[src];
            p_sn->physics_matrices[dst] = p_sn->physics_matrices[src];
            p_sn->bounding_volumes[dst] = p_sn->bounding_volumes[src];
            
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
			entity_scene_instance new_instance;
			new_instance.name = name;
			new_instance.scene = new entity_scene();

			k_scenes.push_back(new_instance);

			resize_scene_buffers(new_instance.scene);

			//create buffers
			pen::buffer_creation_params bcp;
			bcp.usage_flags = PEN_USAGE_DYNAMIC;
			bcp.bind_flags = PEN_BIND_CONSTANT_BUFFER;
			bcp.cpu_access_flags = PEN_CPU_ACCESS_WRITE;
			bcp.buffer_size = sizeof(forward_light) * 8;
			bcp.data = nullptr;

			new_instance.scene->forward_light_buffer = pen::renderer_create_buffer(bcp);

			return new_instance.scene;
		}
        
		void render_scene_view( const scene_view& view )
		{
            entity_scene* scene = view.scene;
            
            if( scene->view_flags & SV_HIDE )
                return;
            
            pen::renderer_set_constant_buffer(view.cb_view, 0, PEN_SHADER_TYPE_VS);
            
			for (u32 n = 0; n < scene->num_nodes; ++n)
			{
				if (scene->entities[n] & CMP_GEOMETRY && scene->entities[n] & CMP_MATERIAL && (!(scene->entities[n] & CMP_PHYSICS)) )
                {
					scene_node_geometry* p_geom = &scene->geometries[n];
					scene_node_material* p_mat = &scene->materials[n];

                    if( p_geom->p_skin )
                    {
                        if( p_geom->p_skin->bone_cbuffer == PEN_INVALID_HANDLE )
                        {
                            pen::buffer_creation_params bcp;
                            bcp.usage_flags = PEN_USAGE_DYNAMIC;
                            bcp.bind_flags = PEN_BIND_CONSTANT_BUFFER;
                            bcp.cpu_access_flags = PEN_CPU_ACCESS_WRITE;
                            bcp.buffer_size = sizeof(mat4) * 85;
                            bcp.data = nullptr;
                            
                            p_geom->p_skin->bone_cbuffer = pen::renderer_create_buffer(bcp);
                        }
                        
                        static mat4 bb[85];
                        
                        s32 joints_offset = scene->anim_controller[n].joints_offset;
                        for( s32 i = 0; i < p_geom->p_skin->num_joints; ++i )
                        {
                            bb[i] = scene->world_matrices[n + joints_offset + i] * p_geom->p_skin->joint_bind_matrices[i];
                        }
                        
                        pen::renderer_update_buffer(p_geom->p_skin->bone_cbuffer, bb, sizeof(bb));
                        
                        pen::renderer_set_constant_buffer(p_geom->p_skin->bone_cbuffer, 2, PEN_SHADER_TYPE_VS);
                    }
                    
                    static hash_id ID_SUB_TYPE_SKINNED = PEN_HASH("_skinned");
                    static hash_id ID_SUB_TYPE_NON_SKINNED = PEN_HASH("");
                    
                    hash_id mh = p_geom->p_skin ? ID_SUB_TYPE_SKINNED : ID_SUB_TYPE_NON_SKINNED;

                    if( !pmfx::set_technique( view.pmfx_shader, view.technique, mh ) )
                        continue;
                    
                    //set cbs
					pen::renderer_set_constant_buffer(scene->cbuffer[n], 1, PEN_SHADER_TYPE_VS);

					//forward lights
                    if( view.render_flags & RENDER_FORWARD_LIT )
                        pen::renderer_set_constant_buffer(scene->forward_light_buffer, 3, PEN_SHADER_TYPE_PS);

					//set ib / vb
					pen::renderer_set_vertex_buffer(p_geom->vertex_buffer, 0, p_geom->vertex_size, 0 );
					pen::renderer_set_index_buffer(p_geom->index_buffer, p_geom->index_type, 0);

					//set textures
					if (p_mat)
					{
                        //todo - set sampler states from material
                        static u32 ss_wrap = put::pmfx::get_render_state_by_name( PEN_HASH("wrap_linear_sampler_state") );
                        
						for (u32 t = 0; t < put::ces::SN_NUM_TEXTURES; ++t)
						{
							if ( is_valid(p_mat->texture_id[t]) && ss_wrap )
							{
								pen::renderer_set_texture(p_mat->texture_id[t], ss_wrap, t, PEN_SHADER_TYPE_PS );
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
            //animations
            for (u32 n = 0; n < scene->num_nodes; ++n)
            {
                if( scene->entities[n] & CMP_ANIM_CONTROLLER )
                {
                    auto& controller = scene->anim_controller[n];
                    
                    bool apply_trajectory = false;
                    mat4 trajectory;
                    
                    if( is_valid( controller.current_animation ) )
                    {
                        auto* anim = get_animation_resource( controller.current_animation );
                        
                        if(!anim)
                            continue;
                        
                        if(controller.play_flags == 1)
                            controller.current_time += dt*0.1f;
                        
                        s32 joints_offset = scene->anim_controller[n].joints_offset;
                        
                        for( s32 c = 0; c < anim->num_channels; ++c )
                        {
                            s32 num_frames = anim->channels[c].num_frames;
                            
                            if( num_frames <= 0 )
                                continue;
                            
                            s32 t = 0;
                            for( t = 0; t < num_frames; ++t )
                                if( controller.current_time < anim->channels[c].times[t] )
                                    break;
                            
                            //loop
                            if( t >= num_frames )
                                t = 0;

                            mat4& mat = anim->channels[c].matrices[t];
                            
                            scene->local_matrices[n+c+joints_offset] = mat;
                            
                            if( controller.current_time > anim->length )
                            {
                                apply_trajectory = true;
                                controller.current_time = +0.0f;
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
            
            //scene node transform
			for (u32 n = 0; n < scene->num_nodes; ++n)
			{
				u32 parent = scene->parents[n];

				if (scene->entities[n] & CMP_TRANSFORM)
				{
					transform& t = scene->transforms[n];

					//generate matrix from transform
					mat4 rot_mat;
					t.rotation.get_matrix(rot_mat);

					mat4 translation_mat = mat4::create_translation(t.translation);

					mat4 scale_mat = mat4::create_scale(t.scale);

					scene->local_matrices[n] = translation_mat * rot_mat * scale_mat;

					//
					scene->entities[n] &= ~CMP_TRANSFORM;
				}

				if (parent == n)
					scene->world_matrices[n] = scene->local_matrices[n];
				else
					scene->world_matrices[n] = scene->world_matrices[parent] * scene->local_matrices[n];
			}
            
            //bounding volume transform
            static vec3f corners[] =
            {
                vec3f(0.0f, 0.0f, 0.0f),
                
                vec3f(1.0f, 0.0f, 0.0f),
                vec3f(0.0f, 1.0f, 0.0f),
                vec3f(0.0f, 1.0f, 1.0f),
                
                vec3f(1.0f, 1.0f, 1.0f),
                vec3f(0.0f, 1.0f, 1.0f),
                vec3f(0.0f, 1.0f, 1.0f),
                
                vec3f(0.0f, 1.0f, 1.0f)
            };
            
            for( s32 n = 0; n < scene->num_nodes; ++n )
            {
                vec3f min = scene->bounding_volumes[n].min_extents;
                vec3f max = scene->bounding_volumes[n].max_extents - min;
                
                vec3f& tmin = scene->bounding_volumes[n].transformed_min_extents;
                vec3f& tmax = scene->bounding_volumes[n].transformed_max_extents;
            
                tmax = vec3f::flt_min();
                tmin = vec3f::flt_max();
                
                for( s32 c = 0; c < 8; ++c )
                {
                    vec3f p = scene->world_matrices[n].transform_vector(min + max * corners[c]);
                    
                    tmax = vec3f::vmax( tmax, p );
                    tmin = vec3f::vmin( tmin, p );
                }
            }
            
            //update c buffers
            for( s32 n = 0; n < scene->num_nodes; ++n )
            {
                if( scene->cbuffer[n] == INVALID_HANDLE )
                    continue;
                
                per_model_cbuffer cb =
                {
                    scene->world_matrices[n],
                    vec4f((f32)n, 0.0f, 0.0f, 0.0f)
                };
                
                //per object world matrix
                pen::renderer_update_buffer(scene->cbuffer[n], &cb, sizeof(per_model_cbuffer));
            }

			static forward_light light_buffer[8];
			s32 pos = 0;
			for (s32 n = 0; n < scene->num_nodes; ++n)
			{
				if (!(scene->entities[n] & CMP_LIGHT))
					continue;
				
				transform& t = scene->transforms[n];
				scene_node_light& l = scene->lights[n];

				light_buffer[pos].pos_radius = vec4f(t.translation, 1.0 );
				light_buffer[pos].colour = vec4f(l.colour, 1.0);

				++pos;
			}

			pen::renderer_update_buffer(scene->forward_light_buffer, &light_buffer[0], sizeof(light_buffer));
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
            ofs.write( (const c8*)&scene->view_flags, sizeof(u32));
            ofs.write( (const c8*)&scene->selected_index, sizeof(s32));
            
            //names
            for( s32 n = 0; n < scene->num_nodes; ++n )
            {
                write_parsable_string(scene->names[n], ofs);
                write_parsable_string(scene->geometry_names[n], ofs);
                write_parsable_string(scene->material_names[n], ofs);
            }
            
            //simple parts of the scene are just homogeonous chucks of data
            ofs.write( (const c8*)scene->entities,          sizeof( a_u64 )				* scene->num_nodes );
            ofs.write( (const c8*)scene->parents,           sizeof( u32 )				* scene->num_nodes );
			ofs.write( (const c8*)scene->transforms,		sizeof(transform)			* scene->num_nodes );
            ofs.write( (const c8*)scene->local_matrices,    sizeof( mat4 )				* scene->num_nodes );
            ofs.write( (const c8*)scene->world_matrices,    sizeof( mat4 )				* scene->num_nodes );
            ofs.write( (const c8*)scene->offset_matrices,   sizeof( mat4 )				* scene->num_nodes );
            ofs.write( (const c8*)scene->physics_matrices,  sizeof( mat4 )				* scene->num_nodes );
			ofs.write( (const c8*)scene->bounding_volumes,	sizeof( bounding_volume )	* scene->num_nodes );
			ofs.write( (const c8*)scene->lights,			sizeof( scene_node_light )	* scene->num_nodes );
            
            //animations need reloading from files
            for( s32 n = 0; n < scene->num_nodes; ++n )
            {
                s32 size = scene->anim_controller[n].handles.size();
                
                ofs.write( (const c8*)&size, sizeof( s32 ) );
                
                for( auto& h : scene->anim_controller[n].handles )
                {
                    auto* anim = get_animation_resource(h);
                    
                    write_parsable_string( anim->name, ofs);
                }

				ofs.write((const c8*)&scene->anim_controller[n].joints_offset, sizeof(animation_controller) - sizeof(std::vector<anim_handle>));
            }
            
            Str project_dir = dev_ui::get_program_preference("project_dir").as_str();
            
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
                    
                    Str stripped_filename = gr->filename;
                    stripped_filename = put::str_replace_string(stripped_filename, project_dir.c_str(), "");
                    
                    write_parsable_string(stripped_filename.c_str(), ofs);
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
                    
                    Str stripped_filename = mr->filename;
                    stripped_filename = put::str_replace_string(stripped_filename, project_dir.c_str(), "");
                    
                    write_parsable_string(stripped_filename.c_str(), ofs);
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
        
        void load_scene( const c8* filename, entity_scene* scene )
        {
            bool error = false;
            
            std::ifstream ifs(filename, std::ofstream::binary);
            
            s32 version;
            ifs.read((c8*)&version, sizeof(s32));
            ifs.read((c8*)&scene->num_nodes, sizeof(u32));
            
            //user prefs
            ifs.read((c8*)&scene->view_flags, sizeof(u32));
            ifs.read((c8*)&scene->selected_index, sizeof(s32));
            
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
			ifs.read( (c8*)scene->transforms, sizeof(transform) * scene->num_nodes);
            ifs.read( (c8*)scene->local_matrices, sizeof( mat4 ) * scene->num_nodes );
            ifs.read( (c8*)scene->world_matrices, sizeof( mat4 ) * scene->num_nodes );
            ifs.read( (c8*)scene->offset_matrices, sizeof( mat4 ) * scene->num_nodes );
            ifs.read( (c8*)scene->physics_matrices, sizeof( mat4 ) * scene->num_nodes );
			ifs.read((c8*)scene->bounding_volumes, sizeof(bounding_volume) * scene->num_nodes);
			ifs.read((c8*)scene->lights, sizeof(scene_node_light) * scene->num_nodes);

            //animations
            for( s32 n = 0; n < scene->num_nodes; ++n )
            {
                s32 size;
                ifs.read( (c8*)&size, sizeof(s32) );
                
                for( s32 i = 0; i < size; ++i )
                {
                    Str anim_name = dev_ui::get_program_preference("project_dir").as_str();
                    anim_name.append( read_parsable_string(ifs).c_str() );
                    
                    anim_handle h = load_pma(anim_name.c_str());
                    
                    if( !is_valid(h) )
                    {
                        dev_ui::log_level(dev_ui::CONSOLE_ERROR, "[error] animation - cannot find pma file: %s", anim_name.c_str() );
                        error = true;
                    }
                    
                    scene->anim_controller[n].handles.push_back( h );
                }

				ifs.read((c8*)&scene->anim_controller[n].joints_offset, sizeof(animation_controller) - sizeof(std::vector<anim_handle>));
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

                    Str filename = dev_ui::get_program_preference("project_dir").as_str();
                    filename.append( read_parsable_string(ifs).c_str() );
                    
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
                        instantiate_geometry(gr, scene, n );
                        
                        instantiate_model_cbuffer(scene, n);
                        
                        if( gr->p_skin)
                            instantiate_anim_controller( scene, n );
                    }
                    else
                    {
                        dev_ui::log_level(dev_ui::CONSOLE_ERROR, "[error] geometry - cannot find pmm file: %s", filename.c_str() );
                        scene->entities[n] &= ~CMP_GEOMETRY;
                        error = true;
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
                    Str filename = dev_ui::get_program_preference("project_dir").as_str();
                    filename.append( read_parsable_string(ifs).c_str() );
                    
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
                        
                        instantiate_material(mr, scene, n);
                    }
                    else
                    {
                        dev_ui::log_level(dev_ui::CONSOLE_ERROR, "[error] material - cannot find pmm file: %s", filename.c_str() );
                        scene->entities[n] &= ~CMP_MATERIAL;
                        error = true;
                    }
                }
            }
            
            update_view_flags( scene, error );
            
            ifs.close();
        }
	}
}
