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
    namespace ces
    {
        void create_cone_primitive()
        {
            static const s32 segments = 16;

            static const u32 num_verts = (segments + 1) + (segments*2);
            vertex_model v[num_verts];
            geometry_resource* p_geometry = new geometry_resource;

            vec3f axis = vec3f::unit_y();
            vec3f right = vec3f::unit_x();

            vec3f up = maths::cross( axis, right );
            right = maths::cross( axis, up );

            vec3f points[segments];
            vec3f tangents[segments];

            f32 angle = 0.0;
            f32 angle_step = PI_2 / segments;
            for (s32 i = 0; i < segments; ++i)
            {
                f32 x = cos( angle );
                f32 y = -sin( angle );

                vec3f v1 = right * x + up * y;

                angle += angle_step;

                x = cos( angle );
                y = -sin( angle );

                vec3f v2 = right * x + up * y;

                points[i] = v1;

                tangents[i] = v2 - v1;
            }

            vec3f bottom_points[segments];
            for (s32 i = 0; i < segments; ++i)
                bottom_points[i] = points[i] - vec3f( 0.0f, 0.5f, 0.0f );

            vec3f top_point = vec3f( 0.0, 0.5f, 0.0f );

            //bottom face
            for (s32 i = 0; i < segments; ++i)
            {
                s32 vi = i;

                v[vi].x = bottom_points[i].x;
                v[vi].y = bottom_points[i].y;
                v[vi].z = bottom_points[i].z;
                v[vi].w = 1.0f;

                v[vi].nx = 0.0f;
                v[vi].ny = -1.0f;
                v[vi].nz = 0.0f;
                v[vi].nw = 1.0f;

                v[vi].tx = 1.0f;
                v[vi].ty = 0.0f;
                v[vi].tz = 0.0f;
                v[vi].tw = 1.0f;

                v[vi].bx = 0.0f;
                v[vi].by = 0.0f;
                v[vi].bz = 1.0f;
                v[vi].bw = 1.0f;
            }

            s32 bm = segments;

            //bottom middle
            v[bm].x = 0.0f;
            v[bm].y = -0.5f;
            v[bm].z = 0.0f;
            v[bm].w = 1.0f;

            v[bm].nx = 0.0f;
            v[bm].ny = -1.0f;
            v[bm].nz = 0.0f;
            v[bm].nw = 1.0f;

            v[bm].tx = 1.0f;
            v[bm].ty = 0.0f;
            v[bm].tz = 0.0f;
            v[bm].tw = 1.0f;

            v[bm].bx = 0.0f;
            v[bm].by = 0.0f;
            v[bm].bz = 1.0f;
            v[bm].bw = 1.0f;

            //sides
            for (s32 i = 0; i < segments; ++i)
            {
                s32 start_offset = segments + 1;
                s32 vi = start_offset + (i * 2);

                s32 next = (i + 1) % segments;

                vec3f p1 = bottom_points[i];
                vec3f p2 = top_point;
                vec3f p3 = bottom_points[next];

                vec3f t = p3 - p1;
                vec3f b = p2 - p1;
                vec3f n = maths::cross( t, b );

                v[vi].x = bottom_points[i].x;
                v[vi].y = bottom_points[i].y;
                v[vi].z = bottom_points[i].z;
                v[vi].w = 1.0f;

                v[vi + 1].x = top_point.x;
                v[vi + 1].y = top_point.y;
                v[vi + 1].z = top_point.z;
                v[vi + 1].w = 1.0f;

                for (s32 x = 0; x < 2; ++x)
                {
                    v[vi + x].nx = n.x; v[vi + x].ny = n.y; v[vi + x].nz = n.z;
                    v[vi + x].tx = t.x; v[vi + x].ty = t.y; v[vi + x].tz = t.z;
                    v[vi + x].bx = b.x; v[vi + x].by = b.y; v[vi + x].bz = b.z;
                }
            }

            const u32 num_indices = (segments * 3) + (segments * 3);
            u16 indices[num_indices] = { 0 };

            //bottom face
            for (s32 i = 0; i < segments; ++i)
            {
                s32 face_current = i;
                s32 face_next = (i + 1) % segments;

                s32 index_offset = i * 3;

                indices[index_offset + 0] = bm;
                indices[index_offset + 1] = face_current;
                indices[index_offset + 2] = face_next;
            }

            //sides
            for (s32 i = 0; i < segments; ++i)
            {
                s32 sides_offset = segments + 1;

                s32 face_current = sides_offset + (i * 2);
                s32 index_offset = segments * 3 + (i * 3);

                indices[index_offset + 0] = face_current;
                indices[index_offset + 1] = face_current+1;

                s32 face_next = sides_offset + ((i + 1) % segments) * 2;
                indices[index_offset + 2] = face_next;
            }

            //VB
            pen::buffer_creation_params bcp;
            bcp.usage_flags = PEN_USAGE_DEFAULT;
            bcp.bind_flags = PEN_BIND_VERTEX_BUFFER;
            bcp.cpu_access_flags = 0;
            bcp.buffer_size = sizeof( vertex_model ) * num_verts;
            bcp.data = ( void* )v;

            p_geometry->vertex_buffer = pen::renderer_create_buffer( bcp );

            //IB
            bcp.usage_flags = PEN_USAGE_DEFAULT;
            bcp.bind_flags = PEN_BIND_INDEX_BUFFER;
            bcp.cpu_access_flags = 0;
            bcp.buffer_size = 2 * num_indices;
            bcp.data = ( void* )indices;

            p_geometry->index_buffer = pen::renderer_create_buffer( bcp );

            //info
            p_geometry->num_indices = num_indices;
            p_geometry->num_vertices = num_verts;
            p_geometry->vertex_size = sizeof( vertex_model );
            p_geometry->index_type = PEN_FORMAT_R16_UINT;
            p_geometry->min_extents = -vec3f( 1.0f, 0.5f, 1.0f );
            p_geometry->max_extents = vec3f( 1.0f, 0.5f, 1.0f );
            p_geometry->geometry_name = "cone";
            p_geometry->hash = PEN_HASH( "cone" );
            p_geometry->file_hash = PEN_HASH( "primitive" );
            p_geometry->filename = "primitive";
            p_geometry->p_skin = nullptr;

            add_geometry_resource( p_geometry );
        }

        void create_capsule_primitive()
        {
            static const s32 segments = 16;
            static const s32 rows = 16;

            static const u32 num_verts = segments*rows;
            vertex_model v[num_verts];
            geometry_resource* p_geometry = new geometry_resource;

            f32 angle = 0.0;
            f32 angle_step = PI_2 / segments;
            f32 height_step = 2.0f / (rows - 1);

            s32 v_index = 0;

            f32 y = -1.0f;
            for (s32 r = 0; r < rows; ++r)
            {
                for (s32 i = 0; i < segments; ++i)
                {
                    f32 x = cos( angle );
                    f32 z = -sin( angle );

                    angle += angle_step;

                    f32 radius = 1.0f - fabs( y );

                    vec3f xz = vec3f( x, 0.0f, z ) * radius;
                    vec3f p = vec3f( xz.x, y, xz.z );
                    
                    //tangent
                    x = cos( angle );
                    z = -sin( angle );

                    xz = vec3f( x, 0.0f, z ) * radius;

                    vec3f p_next = vec3f( xz.x, y + height_step, xz.z );
                    
                    p = maths::normalise( p );
                    p_next = maths::normalise( p_next );
                    
                    vec3f n = p;
                    
                    if( r < segments / 2.0f )
                    {
                        p.y -= 0.5f;
                        p_next -= 0.5f;
                    }
                    else if( r > segments / 2.0f )
                    {
                        p.y += 0.5f;
                        p_next += 0.5f;
                    }
                    
                    if( fabs(r - (segments / 2.0f)) < 2.0f )
                    {
                        n = put::maths::normalise(xz);
                    }

                    v[v_index].x = p.x;
                    v[v_index].y = p.y;
                    v[v_index].z = p.z;
                    v[v_index].w = 1.0f;

                    v[v_index].nx = n.x;
                    v[v_index].ny = n.y;
                    v[v_index].nz = n.z;
                    v[v_index].nw = 1.0f;

                    vec3f t = maths::normalise( p_next - p );

                    v[v_index].tx = t.x;
                    v[v_index].ty = t.y;
                    v[v_index].tz = t.z;
                    v[v_index].tw = 1.0f;

                    vec3f bt = maths::cross( p, t );

                    v[v_index].bx = bt.x;
                    v[v_index].by = bt.y;
                    v[v_index].bz = bt.z;
                    v[v_index].bw = 1.0f;

                    v_index++;
                }

                y += height_step;
            }

            static const u32 num_indices = 6 * (rows - 1)*segments;
            u16 indices[num_indices] = { 0 };
            s32 index_offset = 0;

            for (s32 r = 0; r < rows - 1; ++r)
            {
                for (s32 i = 0; i < segments; ++i)
                {
                    s32 i_next = ((i + 1) % segments);

                    s32 v_index = r * segments;
                    s32 v_next_index = (r + 1) * segments + i;
                    s32 v_next_next_index = (r + 1) * segments + i_next;

                    indices[index_offset + 0] = v_index + i;
                    indices[index_offset + 1] = v_next_index;
                    indices[index_offset + 2] = v_index + i_next;

                    indices[index_offset + 3] = v_next_index;
                    indices[index_offset + 4] = v_next_next_index;
                    indices[index_offset + 5] = v_index + i_next;

                    index_offset += 6;
                }
            }

            //VB
            pen::buffer_creation_params bcp;
            bcp.usage_flags = PEN_USAGE_DEFAULT;
            bcp.bind_flags = PEN_BIND_VERTEX_BUFFER;
            bcp.cpu_access_flags = 0;
            bcp.buffer_size = sizeof( vertex_model ) * num_verts;
            bcp.data = ( void* )v;

            p_geometry->vertex_buffer = pen::renderer_create_buffer( bcp );

            //IB
            bcp.usage_flags = PEN_USAGE_DEFAULT;
            bcp.bind_flags = PEN_BIND_INDEX_BUFFER;
            bcp.cpu_access_flags = 0;
            bcp.buffer_size = 2 * num_indices;
            bcp.data = ( void* )indices;

            p_geometry->index_buffer = pen::renderer_create_buffer( bcp );

            p_geometry->num_indices = num_indices;
            p_geometry->num_vertices = num_verts;
            p_geometry->vertex_size = sizeof( vertex_model );
            p_geometry->index_type = PEN_FORMAT_R16_UINT;

            p_geometry->min_extents = vec3f(-1.0f, -1.5f, -1.0f);
            p_geometry->max_extents = vec3f(1.0f, 1.5f, 1.0f);

            //hash / ids
            p_geometry->geometry_name = "capsule";
            p_geometry->hash = PEN_HASH( "capsule" );
            p_geometry->file_hash = PEN_HASH( "primitive" );
            p_geometry->filename = "primitive";
            p_geometry->p_skin = nullptr;

            add_geometry_resource( p_geometry );
        }

        void create_sphere_primitive()
        {
            static const s32 segments = 16;
            static const s32 rows = 16;

            static const u32 num_verts = segments*rows;
            vertex_model v[num_verts];
            geometry_resource* p_geometry = new geometry_resource;

            f32 angle = 0.0;
            f32 angle_step = PI_2 / segments;
            f32 height_step = 2.0f / (segments - 1);

            s32 v_index = 0;

            f32 y = -1.0f;
            for (s32 r = 0; r < rows; ++r)
            {
                for (s32 i = 0; i < segments; ++i)
                {
                    f32 x = cos( angle );
                    f32 z = -sin( angle );

                    angle += angle_step;

                    f32 radius = 1.0f - fabs( y );

                    vec3f xz = vec3f( x, 0.0f, z ) * radius;
                    vec3f p = vec3f( xz.x, y, xz.z );

                    //tangent
                    x = cos( angle );
                    z = -sin( angle );

                    xz = vec3f( x, 0.0f, z ) * radius;

                    vec3f p_next = vec3f( xz.x, y, xz.z );

                    p = maths::normalise( p );
                    p_next = maths::normalise( p_next );

                    v[v_index].x = p.x;
                    v[v_index].y = p.y;
                    v[v_index].z = p.z;
                    v[v_index].w = 1.0f;

                    v[v_index].nx = p.x;
                    v[v_index].ny = p.y;
                    v[v_index].nz = p.z;
                    v[v_index].nw = 1.0f;

                    vec3f t = p_next - p;

                    v[v_index].tx = t.x;
                    v[v_index].ty = t.y;
                    v[v_index].tz = t.z;
                    v[v_index].tw = 1.0f;

                    vec3f bt = maths::cross( p, t );

                    v[v_index].bx = bt.x;
                    v[v_index].by = bt.y;
                    v[v_index].bz = bt.z;
                    v[v_index].bw = 1.0f;

                    v_index++;
                }

                y += height_step;
            }

            static const u32 num_indices = 6 * (rows - 1)*segments;
            u16 indices[num_indices] = { 0 };
            s32 index_offset = 0;

            for (s32 r = 0; r < rows - 1; ++r)
            {
                for (s32 i = 0; i < segments; ++i)
                {
                    s32 i_next = ((i + 1) % segments);

                    s32 v_index = r * segments;
                    s32 v_next_index = (r + 1) * segments + i;
                    s32 v_next_next_index = (r + 1) * segments + i_next;

                    indices[index_offset + 0] = v_index + i;
                    indices[index_offset + 1] = v_next_index;
                    indices[index_offset + 2] = v_index + i_next;

                    indices[index_offset + 3] = v_next_index;
                    indices[index_offset + 4] = v_next_next_index;
                    indices[index_offset + 5] = v_index + i_next;

                    index_offset += 6;
                }
            }

            //VB
            pen::buffer_creation_params bcp;
            bcp.usage_flags = PEN_USAGE_DEFAULT;
            bcp.bind_flags = PEN_BIND_VERTEX_BUFFER;
            bcp.cpu_access_flags = 0;
            bcp.buffer_size = sizeof( vertex_model ) * num_verts;
            bcp.data = ( void* )v;

            p_geometry->vertex_buffer = pen::renderer_create_buffer( bcp );

            //IB
            bcp.usage_flags = PEN_USAGE_DEFAULT;
            bcp.bind_flags = PEN_BIND_INDEX_BUFFER;
            bcp.cpu_access_flags = 0;
            bcp.buffer_size = 2 * num_indices;
            bcp.data = ( void* )indices;

            p_geometry->index_buffer = pen::renderer_create_buffer( bcp );

            p_geometry->num_indices = num_indices;
            p_geometry->num_vertices = num_verts;
            p_geometry->vertex_size = sizeof( vertex_model );
            p_geometry->index_type = PEN_FORMAT_R16_UINT;

            p_geometry->min_extents = -vec3f::one();
            p_geometry->max_extents = vec3f::one();

            //hash / ids
            p_geometry->geometry_name = "sphere";
            p_geometry->hash = PEN_HASH( "sphere" );
            p_geometry->file_hash = PEN_HASH( "primitive" );
            p_geometry->filename = "primitive";
            p_geometry->p_skin = nullptr;

            add_geometry_resource( p_geometry );
        }

        void create_cube_primitive()
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
                vec3f( -1.0f, -1.0f, -1.0f ),
                vec3f( 1.0f, -1.0f, -1.0f ),
                vec3f( 1.0f, -1.0f,  1.0f ),
                vec3f( -1.0f, -1.0f,  1.0f ),

                vec3f( -1.0f,  1.0f, -1.0f ),
                vec3f( 1.0f,  1.0f, -1.0f ),
                vec3f( 1.0f,  1.0f,  1.0f ),
                vec3f( -1.0f,  1.0f,  1.0f )
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

            for (s32 i = 0; i < 6; ++i)
            {
                s32 offset = i * 4;
                s32 index_offset = i * 6;

                vec3f bt = maths::cross( face_normals[i], face_tangents[i] );

                for (s32 j = 0; j < 4; ++j)
                {
                    s32 cc = c[offset + j];

                    v[offset + j].x = corners[cc].x;
                    v[offset + j].y = corners[cc].y;
                    v[offset + j].z = corners[cc].z;
                    v[offset + j].w = 1.0f;

                    v[offset + j].nx = face_normals[i].x;
                    v[offset + j].ny = face_normals[i].y;
                    v[offset + j].nz = face_normals[i].z;
                    v[offset + j].nw = 1.0f;

                    v[offset + j].tx = face_tangents[i].x;
                    v[offset + j].ty = face_tangents[i].y;
                    v[offset + j].tz = face_tangents[i].z;
                    v[offset + j].tw = 1.0f;

                    v[offset + j].bx = bt.x;
                    v[offset + j].by = bt.y;
                    v[offset + j].bz = bt.z;
                    v[offset + j].bw = 1.0f;
                }

                indices[index_offset + 0] = offset + 0;
                indices[index_offset + 1] = offset + 1;
                indices[index_offset + 2] = offset + 2;

                indices[index_offset + 3] = offset + 2;
                indices[index_offset + 4] = offset + 3;
                indices[index_offset + 5] = offset + 0;
            }

            //VB
            pen::buffer_creation_params bcp;
            bcp.usage_flags = PEN_USAGE_DEFAULT;
            bcp.bind_flags = PEN_BIND_VERTEX_BUFFER;
            bcp.cpu_access_flags = 0;
            bcp.buffer_size = sizeof( vertex_model ) * num_verts;
            bcp.data = ( void* )v;

            p_geometry->vertex_buffer = pen::renderer_create_buffer( bcp );

            //IB
            bcp.usage_flags = PEN_USAGE_DEFAULT;
            bcp.bind_flags = PEN_BIND_INDEX_BUFFER;
            bcp.cpu_access_flags = 0;
            bcp.buffer_size = 2 * num_indices;
            bcp.data = ( void* )indices;

            p_geometry->index_buffer = pen::renderer_create_buffer( bcp );

            p_geometry->num_indices = 36;
            p_geometry->num_vertices = num_verts;
            p_geometry->vertex_size = sizeof( vertex_model );
            p_geometry->index_type = PEN_FORMAT_R16_UINT;

            p_geometry->min_extents = -vec3f::one();
            p_geometry->max_extents = vec3f::one();

            //hash / ids
            p_geometry->geometry_name = "cube";
            p_geometry->hash = PEN_HASH( "cube" );
            p_geometry->file_hash = PEN_HASH( "primitive" );
            p_geometry->filename = "primitive";
            p_geometry->p_skin = nullptr;

            add_geometry_resource( p_geometry );
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
            vec3f tangents[segments];

            f32 angle = 0.0;
            f32 angle_step = PI_2 / segments;
            for (s32 i = 0; i < segments; ++i)
            {
                f32 x = cos( angle );
                f32 y = -sin( angle );

                vec3f v1 = right * x + up * y;

                angle += angle_step;

                x = cos( angle );
                y = -sin( angle );

                vec3f v2 = right * x + up * y;

                points[i] = v1;

                tangents[i] = v2 - v1;
            }

            vec3f bottom_points[segments];
            for (s32 i = 0; i < segments; ++i)
                bottom_points[i] = points[i] - vec3f( 0.0f, 1.0f, 0.0f );

            vec3f top_points[segments];
            for (s32 i = 0; i < segments; ++i)
                top_points[i] = points[i] + vec3f( 0.0f, 1.0f, 0.0f );

            //bottom ring
            for (s32 i = 0; i < segments; ++i)
            {
                v[i].x = bottom_points[i].x;
                v[i].y = bottom_points[i].y;
                v[i].z = bottom_points[i].z;
                v[i].w = 1.0f;

                v[i].nx = points[i].x;
                v[i].ny = points[i].y;
                v[i].nz = points[i].z;
                v[i].nw = 1.0f;

                v[i].tx = tangents[i].x;
                v[i].ty = tangents[i].y;
                v[i].tz = tangents[i].z;
                v[i].tw = 1.0f;

                vec3f bt = maths::cross( tangents[i], points[i] );

                v[i].bx = bt.x;
                v[i].by = bt.y;
                v[i].bz = bt.z;
                v[i].bw = 1.0f;
            }

            //top ring
            for (s32 i = 0; i < segments; ++i)
            {
                s32 vi = i + segments;

                v[vi].x = top_points[i].x;
                v[vi].y = top_points[i].y;
                v[vi].z = top_points[i].z;
                v[vi].w = 1.0f;

                v[vi].nx = points[i].x;
                v[vi].ny = points[i].y;
                v[vi].nz = points[i].z;
                v[vi].nw = 1.0f;

                v[vi].tx = tangents[i].x;
                v[vi].ty = tangents[i].y;
                v[vi].tz = tangents[i].z;
                v[vi].tw = 1.0f;

                vec3f bt = maths::cross( tangents[i], points[i] );

                v[vi].bx = bt.x;
                v[vi].by = bt.y;
                v[vi].bz = bt.z;
                v[vi].bw = 1.0f;
            }

            //bottom face
            for (s32 i = 0; i < segments; ++i)
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

                v[vi].tx = 1.0f;
                v[vi].ty = 0.0f;
                v[vi].tz = 0.0f;
                v[vi].tw = 1.0f;

                v[vi].bx = 0.0f;
                v[vi].by = 0.0f;
                v[vi].bz = 1.0f;
                v[vi].bw = 1.0f;
            }

            //top face
            for (s32 i = 0; i < segments; ++i)
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

                v[vi].tx = 1.0f;
                v[vi].ty = 0.0f;
                v[vi].tz = 0.0f;
                v[vi].tw = 1.0f;

                v[vi].bx = 0.0f;
                v[vi].by = 0.0f;
                v[vi].bz = 1.0f;
                v[vi].bw = 1.0f;
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
            const u32 num_indices = segments * 6 + segments * 3 * 2;
            u16 indices[num_indices] = { 0 };

            for (s32 i = 0; i < segments; ++i)
            {
                s32 bottom = i;
                s32 top = i + segments;
                s32 next = (i + 1) % segments;
                s32 top_next = ((i + 1) % segments) + segments;

                s32 index_offset = i * 6;

                indices[index_offset + 0] = bottom;
                indices[index_offset + 1] = top;
                indices[index_offset + 2] = next;

                indices[index_offset + 3] = top;
                indices[index_offset + 4] = top_next;
                indices[index_offset + 5] = next;
            }

            //bottom face
            for (s32 i = 0; i < segments; ++i)
            {
                s32 face_offset = (segments * 2);

                s32 face_current = face_offset + i;
                s32 face_next = face_offset + (i + 1) % segments;

                s32 index_offset = i * 3 + (segments * 6);

                indices[index_offset + 0] = 64;
                indices[index_offset + 1] = face_current;
                indices[index_offset + 2] = face_next;
            }

            //top face
            for (s32 i = 0; i < segments; ++i)
            {
                s32 face_offset = (segments * 3);

                s32 face_current = face_offset + i;
                s32 face_next = face_offset + (i + 1) % segments;

                s32 index_offset = i * 3 + (segments * 6) + (segments * 3);

                indices[index_offset + 0] = 65;
                indices[index_offset + 1] = face_next;
                indices[index_offset + 2] = face_current;
            }

            //VB
            pen::buffer_creation_params bcp;
            bcp.usage_flags = PEN_USAGE_DEFAULT;
            bcp.bind_flags = PEN_BIND_VERTEX_BUFFER;
            bcp.cpu_access_flags = 0;
            bcp.buffer_size = sizeof( vertex_model ) * num_verts;
            bcp.data = ( void* )v;

            p_geometry->vertex_buffer = pen::renderer_create_buffer( bcp );

            //IB
            bcp.usage_flags = PEN_USAGE_DEFAULT;
            bcp.bind_flags = PEN_BIND_INDEX_BUFFER;
            bcp.cpu_access_flags = 0;
            bcp.buffer_size = 2 * num_indices;
            bcp.data = ( void* )indices;

            p_geometry->index_buffer = pen::renderer_create_buffer( bcp );

            //info
            p_geometry->num_indices = num_indices;
            p_geometry->num_vertices = num_verts;
            p_geometry->vertex_size = sizeof( vertex_model );
            p_geometry->index_type = PEN_FORMAT_R16_UINT;
            p_geometry->min_extents = -vec3f::one();
            p_geometry->max_extents = vec3f::one();
            p_geometry->geometry_name = "cylinder";
            p_geometry->hash = PEN_HASH( "cylinder" );
            p_geometry->file_hash = PEN_HASH( "primitive" );
            p_geometry->filename = "primitive";
            p_geometry->p_skin = nullptr;

            add_geometry_resource( p_geometry );
        }

        void create_geometry_primitives()
        {
            //default material
            material_resource* mr = new material_resource;

            mr->diffuse_rgb_shininess = vec4f( 0.5f, 0.5f, 0.5f, 0.5f );
            mr->specular_rgb_reflect = vec4f( 0.5f, 0.5f, 0.5f, 0.5f );
            mr->filename = "default_material";
            mr->material_name = "default_material";
            mr->hash = PEN_HASH( "default_material" );

            static const u32 default_maps[] =
            {
                put::load_texture( "data/textures/defaults/albedo.dds" ),
                put::load_texture( "data/textures/defaults/normal.dds" ),
                put::load_texture( "data/textures/defaults/spec.dds" ),
                put::load_texture( "data/textures/defaults/black.dds" )
            };

            for (s32 i = 0; i < 4; ++i)
                mr->texture_id[i] = default_maps[i];

            add_material_resource( mr );

            //geom primitives
            create_cube_primitive();
            create_cylinder_primitive();
            create_sphere_primitive();
            create_capsule_primitive();
            create_cone_primitive();
        }
    }
}
