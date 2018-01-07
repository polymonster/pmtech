#include "dev_ui.h"
#include "hash.h"
#include "pen_string.h"
#include "str_utilities.h"
#include "file_system.h"
#include "debug_render.h"

#include "ces/ces_utilities.h"
#include "ces/ces_resources.h"

namespace put
{    
    enum pmm_transform_types
    {
        PMM_TRANSLATE = 0,
        PMM_ROTATE = 1,
        PMM_MATRIX
    };
    
    //id hashes
	const hash_id ID_CONTROL_RIG = PEN_HASH("controlrig");
    const hash_id ID_JOINT = PEN_HASH("joint");
    const hash_id ID_TRAJECTORY = PEN_HASH("trajectoryshjnt");
    
    namespace ces
    {
        static std::vector<geometry_resource*> k_geometry_resources;
        static std::vector<material_resource*> k_material_resources;
        
        void create_cube_primitive( )
        {
            static const u32 num_verts = 24;
            vertex_model v[num_verts];
            geometry_resource* p_geometry = new geometry_resource;
            
            //3 ------ 2
            //|        |
            //|        |
            //0 ------ 1
            
            //7 ------ 6
            //|        |
            //|        |
            //4 ------ 5
            
            vec3f corners[] =
            {
                vec3f(-1.0f, -1.0f, -1.0f),
                vec3f( 1.0f, -1.0f, -1.0f),
                vec3f( 1.0f, -1.0f,  1.0f),
                vec3f(-1.0f, -1.0f,  1.0f),
                
                vec3f(-1.0f,  1.0f, -1.0f),
                vec3f( 1.0f,  1.0f, -1.0f),
                vec3f( 1.0f,  1.0f,  1.0f),
                vec3f(-1.0f,  1.0f,  1.0f)
            };
            
            vec3f face_normals[] =
            {
                vec3f( 0.0f, -1.0f, 0.0f ),
                vec3f( 0.0f, 0.0f, -1.0f ),
                vec3f( 0.0f, 0.0f,  1.0f ),
                
                vec3f( 0.0f, 1.0f, 0.0f ),
                vec3f( -1.0f, 0.0f, 0.0f ),
                vec3f( 1.0f, 0.0f, 0.0f )
            };
            
            vec3f face_tangents[] =
            {
                vec3f( -1.0f, 0.0f, 0.0f ),
                vec3f( -1.0f, 0.0f, -1.0f ),
                vec3f( 1.0f, 0.0f, 0.0f ),
                
                vec3f( 1.0f, 0.0f, 0.0f ),
                vec3f( 0.0f, 0.0f, -1.0f ),
                vec3f( 0.0f, 0.0f, 1.0f )
            };
            
            s32 c[] =
            {
                0, 3, 2, 1,
                0, 1, 5, 4,
                3, 7, 6, 2,
                
                4, 5, 6, 7,
                3, 0, 4, 7,
                1, 2, 6, 5
            };
            
            const u32 num_indices = 36;
            u16 indices[num_indices];
            
            for( s32 i = 0; i < 6; ++i )
            {
                s32 offset = i*4;
                s32 index_offset = i*6;
                
                vec3f bt = maths::cross(face_normals[i], face_tangents[i]);
                
                for( s32 j = 0; j < 4; ++j )
                {
                    s32 cc = c[offset+j];
                    
                    v[offset+j].x = corners[cc].x;
                    v[offset+j].y = corners[cc].y;
                    v[offset+j].z = corners[cc].z;
                    v[offset+j].w = 1.0f;
                    
                    v[offset+j].nx = face_normals[i].x;
                    v[offset+j].ny = face_normals[i].y;
                    v[offset+j].nz = face_normals[i].z;
                    v[offset+j].nw = 1.0f;
                    
                    v[offset+j].tx = face_tangents[i].x;
                    v[offset+j].ty = face_tangents[i].y;
                    v[offset+j].tz = face_tangents[i].z;
                    v[offset+j].tw = 1.0f;
                    
                    v[offset+j].bx = bt.x;
                    v[offset+j].by = bt.y;
                    v[offset+j].bz = bt.z;
                    v[offset+j].bw = 1.0f;
                }
                
                indices[index_offset+0] = offset+0;
                indices[index_offset+1] = offset+1;
                indices[index_offset+2] = offset+2;
                
                indices[index_offset+3] = offset+2;
                indices[index_offset+4] = offset+3;
                indices[index_offset+5] = offset+0;
            }
            
            //VB
            pen::buffer_creation_params bcp;
            bcp.usage_flags = PEN_USAGE_DEFAULT;
            bcp.bind_flags = PEN_BIND_VERTEX_BUFFER;
            bcp.cpu_access_flags = 0;
            bcp.buffer_size = sizeof(vertex_model) * num_verts;
            bcp.data = (void*)v;
            
            p_geometry->vertex_buffer = pen::renderer_create_buffer(bcp);

            //IB
            bcp.usage_flags = PEN_USAGE_DEFAULT;
            bcp.bind_flags = PEN_BIND_INDEX_BUFFER;
            bcp.cpu_access_flags = 0;
            bcp.buffer_size = 2 * num_indices;
            bcp.data = (void*)indices;
            
            p_geometry->index_buffer = pen::renderer_create_buffer(bcp);
            
            p_geometry->num_indices = 36;
            p_geometry->num_vertices = num_verts;
            p_geometry->vertex_size = sizeof(vertex_model);
            p_geometry->index_type = PEN_FORMAT_R16_UINT;
            
            p_geometry->physics_info.min_extents = -vec3f::one();
            p_geometry->physics_info.max_extents =  vec3f::one();
            
            //hash / ids
            p_geometry->geometry_name = "cube";
            p_geometry->hash = PEN_HASH("cube");
            p_geometry->file_hash = PEN_HASH("primitive");
            p_geometry->filename = "primitive";
            p_geometry->p_skin = nullptr;
        
            k_geometry_resources.push_back(p_geometry);
        }
        
        void create_cylinder_primitive()
        {
            static const u32 num_verts = 66;
            vertex_model v[num_verts];
            geometry_resource* p_geometry = new geometry_resource;
            
            vec3f axis = vec3f::unit_y();
            vec3f right = vec3f::unit_x();
            
            vec3f up = maths::cross( axis, right );
            right = maths::cross( axis, up );
            
            static const s32 segments = 16;
            
            vec3f points[segments];
            
            f32 angle = 0.0;
            f32 angle_step = PI_2/segments;
            for( s32 i = 0; i < segments; ++i )
            {
                f32 x = cos(angle);
                f32 y = -sin(angle);
                
                vec3f v1 = maths::normalise(vec3f(x, y, 0.0 ));
                
                v1 = right * x + up * y;
                
                angle += angle_step;
                
                points[i] = v1;
            }
            
            vec3f bottom_points[segments];
            for( s32 i = 0; i < segments; ++i )
                bottom_points[i] = points[i] - vec3f(0.0f, 1.0f, 0.0f );
            
            vec3f top_points[segments];
            for( s32 i = 0; i < segments; ++i )
                top_points[i] = points[i] + vec3f(0.0f, 1.0f, 0.0f );
            
            //bottom ring
            for( s32 i = 0; i < segments; ++i )
            {
                v[i].x = bottom_points[i].x;
                v[i].y = bottom_points[i].y;
                v[i].z = bottom_points[i].z;
                v[i].w = 1.0f;
                
                v[i].nx = bottom_points[i].x;
                v[i].ny = bottom_points[i].y;
                v[i].nz = bottom_points[i].z;
                v[i].nw = 1.0f;
                
                put::dbg::add_point(bottom_points[i], 0.1f);
            }
            
            //top ring
            for( s32 i = 0; i < segments; ++i )
            {
                s32 vi = i + segments;
                
                v[vi].x = top_points[i].x;
                v[vi].y = top_points[i].y;
                v[vi].z = top_points[i].z;
                v[vi].w = 1.0f;
                
                v[vi].nx = top_points[i].x;
                v[vi].ny = top_points[i].y;
                v[vi].nz = top_points[i].z;
                v[vi].nw = 1.0f;
                
                put::dbg::add_point(top_points[i], 0.1f);
            }
            
            //bottom face
            for( s32 i = 0; i < segments; ++i )
            {
                s32 vi = (segments * 2) + i;
                
                v[vi].x = bottom_points[i].x;
                v[vi].y = bottom_points[i].y;
                v[vi].z = bottom_points[i].z;
                v[vi].w = 1.0f;
                
                v[vi].nx = 0.0f;
                v[vi].ny = -1.0f;
                v[vi].nz = 0.0f;
                v[vi].nw = 1.0f;
            }
            
            //top face
            for( s32 i = 0; i < segments; ++i )
            {
                s32 vi = (segments * 3) + i;
                
                v[vi].x = top_points[i].x;
                v[vi].y = top_points[i].y;
                v[vi].z = top_points[i].z;
                v[vi].w = 1.0f;
                
                v[vi].nx = 0.0f;
                v[vi].ny = 1.0f;
                v[vi].nz = 0.0f;
                v[vi].nw = 1.0f;
            }
            
            //centre points
            v[64].x = 0.0f;
            v[64].y = -1.0f;
            v[64].z = 0.0f;
            v[64].w = 1.0f;
            
            v[64].nx = 0.0f;
            v[64].ny = -1.0f;
            v[64].nz = 0.0f;
            v[64].nw = 1.0f;
            
            v[65].x = 0.0f;
            v[65].y = 1.0f;
            v[65].z = 0.0f;
            v[65].w = 1.0f;
            
            v[65].nx = 0.0f;
            v[65].ny = 1.0f;
            v[65].nz = 0.0f;
            v[65].nw = 1.0f;
            
            //sides
            const u32 num_indices = segments*6 + segments*3*2;
            u16 indices[num_indices] = { 0 };
            
            for( s32 i = 0; i < segments; ++i )
            {
                s32 bottom = i;
                s32 top = i + segments;
                s32 next = (i + 1) % segments;
                s32 top_next = ((i + 1) % segments) + segments;
                
                s32 index_offset = i * 6;
                
                indices[index_offset+0] = bottom;
                indices[index_offset+1] = top;
                indices[index_offset+2] = next;
                
                indices[index_offset+3] = top;
                indices[index_offset+4] = top_next;
                indices[index_offset+5] = next;
            }
            
            //bottom face
            for( s32 i = 0; i < segments; ++i )
            {
                s32 face_offset = (segments*2);
                
                s32 face_current = face_offset + i;
                s32 face_next = face_offset + (i+1)%segments;
                
                s32 index_offset = i*3 + (segments*6);
                
                indices[index_offset+0] = 64;
                indices[index_offset+1] = face_current;
                indices[index_offset+2] = face_next;
            }
            
            //top face
            for( s32 i = 0; i < segments; ++i )
            {
                s32 face_offset = (segments*3);

                s32 face_current = face_offset + i;
                s32 face_next = face_offset + (i+1)%segments;
                
                s32 index_offset = i*3 + (segments*6) + (segments*3);
                
                indices[index_offset+0] = 65;
                indices[index_offset+1] = face_next;
                indices[index_offset+2] = face_current;
            }
            
            static bool once = false;
            if(once)
                return;
            
            once = true;
            
            //VB
            pen::buffer_creation_params bcp;
            bcp.usage_flags = PEN_USAGE_DEFAULT;
            bcp.bind_flags = PEN_BIND_VERTEX_BUFFER;
            bcp.cpu_access_flags = 0;
            bcp.buffer_size = sizeof(vertex_model) * num_verts;
            bcp.data = (void*)v;
            
            p_geometry->vertex_buffer = pen::renderer_create_buffer(bcp);
            
            //IB
            bcp.usage_flags = PEN_USAGE_DEFAULT;
            bcp.bind_flags = PEN_BIND_INDEX_BUFFER;
            bcp.cpu_access_flags = 0;
            bcp.buffer_size = 2 * num_indices;
            bcp.data = (void*)indices;
            
            p_geometry->index_buffer = pen::renderer_create_buffer(bcp);
            
            //info
            p_geometry->num_indices = num_indices;
            p_geometry->num_vertices = num_verts;
            p_geometry->vertex_size = sizeof(vertex_model);
            p_geometry->index_type = PEN_FORMAT_R16_UINT;
            p_geometry->physics_info.min_extents = -vec3f::one();
            p_geometry->physics_info.max_extents =  vec3f::one();
            p_geometry->geometry_name = "cylinder";
            p_geometry->hash = PEN_HASH("cylinder");
            p_geometry->file_hash = PEN_HASH("primitive");
            p_geometry->filename = "primitive";
            p_geometry->p_skin = nullptr;
            
            k_geometry_resources.push_back(p_geometry);
        }
        
        void create_geometry_primitives()
        {
            //default material
            material_resource* mr = new material_resource;
            
            mr->diffuse_rgb_shininess = vec4f( 0.5f, 0.5f, 0.5f, 0.5f );
            mr->specular_rgb_reflect = vec4f( 0.5f, 0.5f, 0.5f, 0.5f );
            mr->filename = "default_material";
            mr->material_name = "default_material";
            mr->hash = PEN_HASH("default_material");
            
            static const u32 default_maps[] =
            {
                put::load_texture("data/textures/defaults/albedo.dds"),
                put::load_texture("data/textures/defaults/normal.dds"),
                put::load_texture("data/textures/defaults/spec.dds"),
                put::load_texture("data/textures/defaults/black.dds")
            };
            
            for( s32 i = 0; i < 4; ++i)
                mr->texture_id[i] = default_maps[i];
            
            k_material_resources.push_back(mr);
            
            //geom primitives
            create_cube_primitive();
            create_cylinder_primitive();
        }
        
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

		void instantiate_physics(entity_scene* scene, u32 node_index )
		{
			u32 s = node_index;
			scene_node_physics& snp = scene->physics_data[s];

			vec3f min = scene->bounding_volumes[s].min_extents;
			vec3f max = scene->bounding_volumes[s].max_extents;
			vec3f centre = min + (max - min);

			vec3f pos = scene->transforms[s].translation;
			vec3f scale = scene->transforms[s].scale;
			quat rotation = scene->transforms[s].rotation;

			scene->offset_matrices[s] = mat4::create_scale(scale);

			physics::rigid_body_params rb = { 0 };
			rb.dimensions = (max - min) * scale * 0.5;
			rb.mass = snp.mass;
			rb.group = 1;
			rb.position = pos;
			rb.rotation = rotation;
			rb.shape = snp.collision_shape;
			rb.shape_up_axis = physics::UP_Y;
			rb.mask = 0xffffffff;

			scene->physics_handles[s] = physics::add_rb(rb);
			scene->entities[s] |= CMP_PHYSICS;
		}
        
        void instantiate_geometry( geometry_resource* gr, entity_scene* scene, s32 node_index )
        {
            scene_node_geometry* instance = &scene->geometries[node_index];
            
            instance->position_buffer = gr->position_buffer;
            instance->vertex_buffer = gr->vertex_buffer;
            instance->index_buffer = gr->index_buffer;
            instance->num_indices = gr->num_indices;
            instance->num_vertices = gr->num_vertices;
            instance->index_type = gr->index_type;
            instance->vertex_size = gr->vertex_size;
            instance->p_skin = gr->p_skin;
            
            bounding_volume* bv = &scene->bounding_volumes[node_index];
            
            bv->min_extents = gr->physics_info.min_extents;
            bv->max_extents = gr->physics_info.max_extents;
			bv->radius = maths::magnitude(bv->max_extents - bv->min_extents) * 0.5f;

            scene->entities[node_index] |= CMP_GEOMETRY;
        }
        
        void instantiate_model_cbuffer( entity_scene* scene, s32 node_index )
        {
            pen::buffer_creation_params bcp;
            bcp.usage_flags = PEN_USAGE_DYNAMIC;
            bcp.bind_flags = PEN_BIND_CONSTANT_BUFFER;
            bcp.cpu_access_flags = PEN_CPU_ACCESS_WRITE;
            bcp.buffer_size = sizeof(per_model_cbuffer);
            bcp.data = nullptr;
            
            scene->cbuffer[node_index] = pen::renderer_create_buffer(bcp);
        }
        
        void instantiate_anim_controller( entity_scene* scene, s32 node_index )
        {
            scene_node_geometry* geom = &scene->geometries[node_index];
            
            if( geom->p_skin )
            {
                animation_controller& controller = scene->anim_controller[node_index];
                
                std::vector<s32> joint_indices;
				build_heirarchy_node_list( scene, node_index, joint_indices );
                
                controller.joints_offset = -1; //scene tree has a -1 node to begin
                for( s32 jj = 0; jj < joint_indices.size(); ++jj )
                {
                    s32 jnode = joint_indices[jj];
                    
                    if( jnode > -1 && scene->entities[jnode] & CMP_BONE )
                        break;
                    else
                        controller.joints_offset++;

					//parent stray nodes to the top level anim / geom node
					if (jnode > -1)
						if (scene->parents[jnode] == jnode)
							scene->parents[jnode] = node_index;
                }

				controller.current_time = 0.0f;
            }
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
                
                p_geometry->p_skin = nullptr;
                p_geometry->file_hash = file_hash;
                p_geometry->hash = sub_hash;
                p_geometry->geometry_name = geometry_name;
                p_geometry->filename = filename;
                p_geometry->material_name = mat_names[submesh];
                p_geometry->material_id_name = PEN_HASH(mat_names[submesh].c_str());
                p_geometry->submesh_index = submesh;
                
                //skip physics
                p_reader++;
                p_reader++;
                
                vec3f min_extents;
                vec3f max_extents;
                
                pen::memory_cpy(&p_geometry->physics_info.min_extents , p_reader, sizeof(vec3f));
                p_reader += 3;
                
                pen::memory_cpy(&p_geometry->physics_info.max_extents , p_reader, sizeof(vec3f));
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
                    
                    //max_swap.create_axis_swap(vec3f(1.0f, 0.0f, 0.0f), vec3f(0.0f, 0.0f, -1.0f), vec3f(0.0f, 1.0f, 0.0f));
                    mat4 max_swap = mat4::create_axis_swap(vec3f(1.0f, 0.0f, 0.0f), vec3f(0.0f, 1.0f, 0.0f), vec3f(0.0f, 0.0f, 1.0f));
        
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
                    }
                    
                    p_geometry->p_skin->bone_cbuffer = PEN_INVALID_HANDLE;
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
        
        void instantiate_material( material_resource* mr, entity_scene* scene, u32 node_index )
        {
            scene_node_material* instance = &scene->materials[node_index];
            
            instance->diffuse_rgb_shininess = mr->diffuse_rgb_shininess;
            instance->specular_rgb_reflect = mr->specular_rgb_reflect;
            
            pen::memory_cpy(instance->texture_id, mr->texture_id, sizeof(u32)*SN_NUM_TEXTURES);
            
            scene->entities[node_index] |= CMP_MATERIAL;
        }
        
        void load_material_resource( const c8* filename, const c8* material_name, const c8* data )
        {
            pen::hash_murmur hm;
            hm.begin();
            hm.add(filename, pen::string_length(filename));
            hm.add(material_name, pen::string_length(material_name));
            hash_id hash = hm.end();
            
            for( s32 m = 0; m < k_material_resources.size(); ++m )
                if( k_material_resources[ m ]->hash == hash )
                    return;
            
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
            
            u32 num_maps = *p_reader++;
            
            //clear all maps to invalid
            static const u32 default_maps[] =
            {
                put::load_texture("data/textures/defaults/albedo.dds"),
                put::load_texture("data/textures/defaults/normal.dds"),
                put::load_texture("data/textures/defaults/spec.dds"),
                put::load_texture("data/textures/defaults/black.dds")
            };
            
            for( u32 map = 0; map < SN_NUM_TEXTURES; ++map )
                p_mat->texture_id[map] = default_maps[map];
            
            for (u32 map = 0; map < num_maps; ++map)
            {
                u32 map_type = *p_reader++;
                Str texture_name = read_parsable_string(&p_reader);
                p_mat->texture_id[map_type] = put::load_texture(texture_name.c_str());
            }
            
            
            k_material_resources.push_back(p_mat);
            
            return;
        }
        
        static std::vector<animation_resource> k_animations;
        
        animation_resource* get_animation_resource( anim_handle h )
        {
			if (h >= k_animations.size())
				return nullptr;

            return &k_animations[h];
        }
        
        anim_handle load_pma(const c8* filename)
        {
            Str pd = put::dev_ui::get_program_preference_filename("project_dir");
            
            Str stipped_filename = put::str_replace_string( filename, pd.c_str(), "" );
            
            hash_id filename_hash = PEN_HASH( stipped_filename.c_str() );
            
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
                //TODO error dialog
                return INVALID_HANDLE;
            }
            
            const u32* p_u32reader = (u32*)anim_file;
            
            u32 version = *p_u32reader++;
            
            if( version < 1 )
            {
                pen::memory_free(anim_file);
                return INVALID_HANDLE;
            }
            
            k_animations.push_back(animation_resource());
            animation_resource& new_animation = k_animations.back();
            
            new_animation.name = stipped_filename;
            new_animation.id_name = filename_hash;
            
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
        
        struct mat_symbol_name
        {
            hash_id symbol;
            Str name;
        };
        
        void load_pmm(const c8* filename, entity_scene* scene, u32 load_flags )
        {
			if(scene)
				scene->flags |= INVALIDATE_SCENE_TREE;

            void* model_file;
            u32   model_file_size;
            
            pen_error err = pen::filesystem_read_file_to_buffer(filename, &model_file, model_file_size);
            
            if( err != PEN_ERR_OK || model_file_size == 0 )
            {
                //TODO error dialog
                dev_ui::log_level(dev_ui::CONSOLE_ERROR, "[error] load pmm - failed to find file: %s", filename );
                return;
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
			s32 nodes_start, nodes_end;
			get_new_nodes_contiguous(scene, num_import_nodes, nodes_start, nodes_end);

            u32 node_zero_offset = nodes_start;
            u32 current_node = node_zero_offset;
            u32 inserted_nodes = 0;
            
            //load scene nodes
            for (u32 n = 0; n < num_import_nodes; ++n)
            {
                p_u32reader++; //e_node type
                
                Str node_name = read_parsable_string(&p_u32reader);
                Str geometry_name = read_parsable_string(&p_u32reader);
                
                scene->id_name[current_node] = PEN_HASH( node_name.c_str() );
                scene->id_geometry[current_node] = PEN_HASH( geometry_name.c_str() );
                
                ASSIGN_DEBUG_NAME( scene->names[current_node], node_name );
                ASSIGN_DEBUG_NAME( scene->geometry_names[current_node], geometry_name );
                
                scene->entities[current_node] |= CMP_ALLOCATED;
                
                if( scene->id_geometry[current_node] == ID_JOINT )
                    scene->entities[current_node] |= CMP_BONE;
                
                if( scene->id_name[current_node] == ID_TRAJECTORY )
                    scene->entities[current_node] |= CMP_ANIM_TRAJECTORY;
                
                u32 num_meshes = *p_u32reader++;
                
                std::vector<mat_symbol_name> mesh_material_names;
                
                //material pre load
                for (u32 mat = 0; mat < num_meshes; ++mat)
                {
                    Str name = read_parsable_string(&p_u32reader);
                    Str symbol = read_parsable_string(&p_u32reader);
                    
                    mesh_material_names.push_back({PEN_HASH(symbol.c_str()), name});
                }
                
                //transformation load
                u32 parent = *p_u32reader++ + node_zero_offset + inserted_nodes;
                scene->parents[current_node] = parent;
                u32 transforms = *p_u32reader++;
                
				//parent fix up
				if (scene->id_name[current_node] == ID_CONTROL_RIG)
					scene->parents[current_node] = node_zero_offset;

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

				if (!has_matrix_transform)
				{
					//create matrix from transform
					scene->transforms[current_node].translation = translation;
					scene->transforms[current_node].rotation = final_rotation;
					scene->transforms[current_node].scale = vec3f::one();

					//make a transform matrix for geometry
					mat4 rot_mat;
					final_rotation.get_matrix(rot_mat);

					mat4 translation_mat = mat4::create_translation(translation);

					matrix = translation_mat * rot_mat;
				}
				else
				{
					//decompose matrix into transform
					scene->transforms[current_node].translation = matrix.get_translation();
					scene->transforms[current_node].rotation.from_matrix(matrix);
					
					f32 sx = put::maths::magnitude(matrix.get_right());
					f32 sy = put::maths::magnitude(matrix.get_up());
					f32 sz = put::maths::magnitude(matrix.get_fwd());

                    scene->transforms[current_node].scale = vec3f( sx, sy, sz );
				}

                
                scene->local_matrices[current_node] = (matrix);
                
                //store intial position for physics to hook into later
                scene->physics_data[current_node].start_position = translation;
                scene->physics_data[current_node].start_rotation = final_rotation;

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
                            scene->local_matrices[dest] = mat4::create_identity();
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
                            instantiate_geometry( gr, scene, dest );
                            
                            instantiate_model_cbuffer( scene, dest );
                            
                            //find mat name from symbol
                            Str mat_name = "";
                            for( auto& ms : mesh_material_names )
                                if( ms.symbol == gr->material_id_name)
                                    mat_name = ms.name;
                            
                            hm.begin();
                            hm.add(filename, pen::string_length(filename));
                            hm.add(mat_name.c_str(), mat_name.length());
                            hash_id material_hash = hm.end();
                            
                            material_resource* mr = get_material_resource(material_hash);
                            
                            if( mr )
                            {
                                ASSIGN_DEBUG_NAME(scene->material_names[dest], mat_name);
                                
                                instantiate_material(mr, scene, dest);
                                
                                scene->id_material[dest] = material_hash;
                            }
                            else
                            {
                                scene->entities[n] |= CMP_MATERIAL;
                            }
                        }
                        else
                        {
                            put::dev_ui::log_level(dev_ui::CONSOLE_ERROR, "[error] geometry - missing file : %s", geometry_name.c_str() );
                        }
                    }
                }
                
                current_node = dest + 1;
                //scene->num_nodes = current_node;
            }
            
            //now we have loaded the whole scene fix up any anim controllers
            for( s32 i = node_zero_offset; i < node_zero_offset + num_import_nodes; ++i )
            {
                if( (scene->entities[i] & CMP_GEOMETRY) && scene->geometries[i].p_skin )
                {
                    instantiate_anim_controller(scene, i);
                }
            }
            
            pen::memory_free(model_file);
            return;
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
    }
}

#if 0
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
#endif

