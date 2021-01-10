#include "cr/cr.h"
#include "ecs/ecs_scene.h"
#include "ecs/ecs_utilities.h"
#include "ecs/ecs_resources.h"
#include "ecs/ecs_cull.h"

#include "imgui/imgui.h"

#define DLL 1
#include "ecs/ecs_live.h"
#include "str/Str.cpp"

#include "renderer.h"
#include "data_struct.h"
#include "timer.h"

#include "maths/maths.h"
#include "../../shader_structs/forward_render.h"

#include <stdio.h>

#define JC_VORONOI_IMPLEMENTATION
#include "jc_voronoi/jc_voronoi.h"

namespace
{
    void draw_edges(const jcv_diagram* diagram);
    void draw_cells(const jcv_diagram* diagram);
    
    struct voronoi_map
    {
        jcv_diagram  diagram;
        u32          num_points;
        vec3f*       points = nullptr;
        jcv_point*   vpoints = nullptr;;
    };
    
    vec3f jcv_to_vec(const jcv_point& p)
    {
        // could transform in plane
        return vec3f(p.x, 0.0f, p.y);
    }
    
    voronoi_map* voronoi_map_generate()
    {
        voronoi_map* voronoi = new voronoi_map();
        
        // generates random points
        srand(0);
        voronoi->num_points = 100;
        for(u32 i = 0; i < voronoi->num_points; ++i)
        {
            // 3d point for rendering
            vec3f p = vec3f((rand()%200)-100, 0.0f, (rand()%200)-100);
            sb_push(voronoi->points, p);
            
            // 2d point for the diagram
            jcv_point vp;
            vp.x = p.x;
            vp.y = p.z;
            sb_push(voronoi->vpoints, vp);
        }
        
        memset(&voronoi->diagram, 0, sizeof(jcv_diagram));
        jcv_diagram_generate(voronoi->num_points, voronoi->vpoints, 0, 0, &voronoi->diagram);
        
        return voronoi;
    }
    
    void voronoi_map_draw_edges(const voronoi_map* voronoi)
    {
        const jcv_edge* edge = jcv_diagram_get_edges( &voronoi->diagram );
        u32 count = 0;
        u32 bb = 43;
        while( edge )
        {
            if(count < bb)
                add_line(vec3f(edge->pos[0].x, 0.0f, edge->pos[0].y), vec3f(edge->pos[1].x, 0.0f, edge->pos[1].y));
            
            edge = jcv_diagram_get_next_edge(edge);
            
            ++count;
        }
    }

    void voronoi_map_draw_points(const voronoi_map* voronoi)
    {
        for(u32 i = 0; i < voronoi->num_points; ++i)
        {
            add_point(voronoi->points[i], 1.0f, vec4f::yellow());
        }
    }

    void voronoi_map_draw_cells(const voronoi_map* voronoi)
    {
        // If you want to draw triangles, or relax the diagram,
        // you can iterate over the sites and get all edges easily
        const jcv_site* sites = jcv_diagram_get_sites( &voronoi->diagram );
        
        u32 s = 8;
        
        for( int i = 0; i < voronoi->diagram.numsites; ++i )
        {
            if(i != s)
                continue;
                
            const jcv_site* site = &sites[i];

            const jcv_graphedge* e = site->edges;
            
            u32 cc = 0;
            while( e )
            {
                vec3f p1 = jcv_to_vec(site->p);
                vec3f p2 = jcv_to_vec(e->pos[0]);
                vec3f p3 = jcv_to_vec(e->pos[1]);
                e = e->next;
                
                //add_triangle(p1, p2, p3);
                
                if(cc < 5)
                    add_line(p2, p3);
                //break;
                
                ++cc;
            }
        }
    }
}

struct live_lib
{
    u32                 box_start = 0;
    u32                 box_end;
    camera              cull_cam;
    ecs_scene*          scene;
    voronoi_map*        voronoi;
    
    void init(live_context* ctx)
    {
        scene = ctx->scene;
        
        ecs::clear_scene(scene);
        voronoi = voronoi_map_generate();
        return;
        
        // primitive resources
        material_resource* default_material = get_material_resource(PEN_HASH("default_material"));
        
        test_mesh();
        test_mesh2();
        test_mesh3();
    
        const c8* primitive_names[] = {
            "subway",
            "lamp_post",
            "building"
        };
        
        geometry_resource** primitives = nullptr;
        
        for(u32 i = 0; i < PEN_ARRAY_SIZE(primitive_names); ++i)
        {
            sb_push(primitives, get_geometry_resource(PEN_HASH(primitive_names[i])));
        }
        
        // add lights
        u32 light = get_new_entity(scene);
        scene->names[light] = "front_light";
        scene->id_name[light] = PEN_HASH("front_light");
        scene->lights[light].colour = vec3f::one();
        scene->lights[light].direction = vec3f::one();
        scene->lights[light].type = e_light_type::dir;
        //scene->lights[light].flags = e_light_flags::shadow_map;
        scene->transforms[light].translation = vec3f::zero();
        scene->transforms[light].rotation = quat();
        scene->transforms[light].scale = vec3f::one();
        scene->entities[light] |= e_cmp::light;
        scene->entities[light] |= e_cmp::transform;
        
        light = get_new_entity(scene);
        scene->names[light] = "front_light";
        scene->id_name[light] = PEN_HASH("front_light");
        scene->lights[light].colour = vec3f::one();
        scene->lights[light].direction = vec3f(-1.0f, 0.0f, 1.0f);
        scene->lights[light].type = e_light_type::dir;
        scene->transforms[light].translation = vec3f::zero();
        scene->transforms[light].rotation = quat();
        scene->transforms[light].scale = vec3f::one();
        scene->entities[light] |= e_cmp::light;
        scene->entities[light] |= e_cmp::transform;
        
        // add primitve instances
        vec3f pos[] = {
            vec3f::unit_x() * - 6.0f,
            vec3f::unit_x() * - 3.0f,
            vec3f::unit_x() * 0.0f,
            vec3f::unit_x() * 3.0f,
            vec3f::unit_x() * 6.0f,
            
            vec3f::unit_x() * - 6.0f + vec3f::unit_z() * 3.0f,
            vec3f::unit_x() * - 3.0f + vec3f::unit_z() * 3.0f,
            vec3f::unit_x() * 0.0f + vec3f::unit_z() * 3.0f,
            vec3f::unit_x() * 3.0f + vec3f::unit_z() * 3.0f,
            vec3f::unit_x() * 6.0f + vec3f::unit_z() * 3.0f,
        };
        
        vec4f col[] = {
            vec4f::orange(),
            vec4f::yellow(),
            vec4f::green(),
            vec4f::cyan(),
            vec4f::magenta(),
            vec4f::white(),
            vec4f::red(),
            vec4f::blue(),
            vec4f::magenta(),
            vec4f::cyan()
        };
        
        for(u32 i = 0; i < 70; ++i)
        {
            test_road(voronoi, i);
            
            Str f;
            f.appendf("cell_%i", i);
            auto geom = get_geometry_resource(PEN_HASH(f.c_str()));
            
            u32 new_prim = get_new_entity(scene);
            scene->names[new_prim] = f;
            scene->names[new_prim].appendf("%i", new_prim);
            scene->transforms[new_prim].rotation = quat();
            scene->transforms[new_prim].scale = vec3f::one();
            scene->transforms[new_prim].translation = vec3f::zero();
            scene->entities[new_prim] |= e_cmp::transform;
            scene->parents[new_prim] = new_prim;
            instantiate_geometry(geom, scene, new_prim);
            instantiate_material(default_material, scene, new_prim);
            instantiate_model_cbuffer(scene, new_prim);
        }

        /*
        for (s32 p = 0; p < PEN_ARRAY_SIZE(primitive_names); ++p)
        {
            u32 new_prim = get_new_entity(scene);
            scene->names[new_prim] = primitive_names[p];
            scene->names[new_prim].appendf("%i", new_prim);
            scene->transforms[new_prim].rotation = quat();
            scene->transforms[new_prim].scale = vec3f::one();
            scene->transforms[new_prim].translation = pos[p] + vec3f::unit_y() * 2.0f;
            scene->entities[new_prim] |= e_cmp::transform;
            scene->parents[new_prim] = new_prim;
            instantiate_geometry(primitives[p], scene, new_prim);
            instantiate_material(default_material, scene, new_prim);
            instantiate_model_cbuffer(scene, new_prim);
            
            forward_render::forward_lit* mat = (forward_render::forward_lit*)&scene->material_data[new_prim].data[0];
            mat->m_albedo = col[p];
        }
        */
    }
    
    struct edge
    {
        vec3f start;
        vec3f end;
        mat4  mat;
    };
    edge** s_edge_strips = nullptr;
    
    void extrude(edge*& strip, vec3f dir)
    {
        struct edge ne;
        
        u32 i = sb_count(strip);
        
        ne.start = strip[i-1].start + dir;
        ne.end = strip[i-1].end + dir;
        ne.mat = mat4::create_identity();
        
        sb_push(strip, ne);
    }
    
    vec3f get_rot_origin(edge* strip, mat4 mr, vec3f rr)
    {
        mr = strip[0].mat;
        
        vec3f r = normalised(strip[0].end - strip[0].start);
        vec3f a = normalised(strip[0].start - strip[1].start);
        
        mr = mat::create_axis_swap(r, cross(a, r), a);
        mat4 inv = mat::inverse4x4(mr);
        
        vec3f min = FLT_MAX;
        vec3f max = -FLT_MAX;
        
        u32 ec = sb_count(strip);
        for(u32 i = 0; i < ec; ++i)
        {
            vec3f v = inv.transform_vector(strip[i].end);
            vec3f v2 = inv.transform_vector(strip[i].start);
            
            min = min_union(min, v);
            max = max_union(max, v);
            
            min = min_union(min, v2);
            max = max_union(max, v2);
        }
        
        vec3f corners[] = {
            min,
            {min.x, min.y, max.z},
            {min.x, max.y, max.z},
            {min.x, max.y, min.z},
            {max.x, min.y, min.z},
            {max.x, min.y, max.z},
            max,
            {max.x, max.y, min.z},
        };
        
        vec3f mid = mr.transform_vector(min + (max - min) * 0.5f);
        vec3f vdir = mid + rr;
        
        // find closest corner to the new direction
        vec3f cc;
        f32 cd = FLT_MAX;
        for(u32 i = 4; i < 8; ++i)
        {
            vec3f tc = mr.transform_vector(corners[i]);
            f32 d = dist(tc, vdir);
            if(d < cd)
            {
                cd = d;
                cc = tc;
            }
        }

        return cc;
    }
    
    void rotate_strip(edge*& strip, const mat4& rot)
    {
        u32 ec = sb_count(strip);
        for(u32 i = 0; i < ec; ++i)
        {
            auto& e = strip[i];
            e.start = rot.transform_vector(e.start);
            e.end = rot.transform_vector(e.end);
        }
    }
    
    edge* bend(edge**& strips, f32 length, vec2f v)
    {
        edge* strip = strips[sb_count(strips)-1];
        
        edge* ee = nullptr;
        edge* ej = nullptr;
                        
        vec3f right = normalised(strip[0].end - strip[0].start);
        vec3f va = normalised(strip[1].start - strip[0].start);
        vec3f vu = cross(va, right);
        
        mat4 mb = mat::create_rotation(vu, v.y);
        mb *= mat::create_rotation(va, v.x);
                
        vec3f new_right = mb.transform_vector(right) * sgn(length);
                
        vec3f rot_origin = get_rot_origin(strip, mb, new_right);
        
        mat4 mr;
        mr = mat::create_translation(rot_origin);
        mr *= mb;
        mr *= mat::create_translation(-rot_origin);
        
        vec3f* prev_pos = nullptr;
        
        u32 ec = sb_count(strip);
        for(u32 i = 0; i < ec; ++i)
        {
            auto e = strip[i];
            
            e.start = e.end;
            e.end += right * length;
            e.mat = mb;
            
            e.start = mr.transform_vector(e.start);
            e.end = mr.transform_vector(e.end);
            
            sb_push(ee, e);
            sb_push(prev_pos, strip[i].end);
        }
        
        u32 res = 16;
        for(u32 j = 1; j < res+1; ++j)
        {
            u32 ec = sb_count(strip);
            for(u32 i = 0; i < ec; ++i)
            {
                mat4 mj;
                mj = mat::create_translation(rot_origin);
                
                mat4 mb2 = mat::create_rotation(vu, (v.y/(f32)res) * j);
                mb2 *= mat::create_rotation(va, (v.x/(f32)res) * j);
                
                mj *= mb2;
                mj *= mat::create_translation(-rot_origin);
                                
                vec3f ip = mj.transform_vector(strip[i].end);
                
                auto e = strip[i];
                e.start = prev_pos[i];
                e.end = ip;
                sb_push(ej, e);
                
                prev_pos[i] = ip;
            }
            
            sb_push(strips, ej);
            ej = nullptr;
        }
    
        sb_push(strips, ee);
        return ej;
    }
    
    int on_load(live_context* ctx)
    {
        init(ctx);
                
        return 0;
    }
    
    void mesh_from_strips(Str name, edge** strips, bool flip = false)
    {
        vertex_model* verts = nullptr;
        
        u32 num_strips = sb_count(strips);
        for(u32 s = 0; s < num_strips; ++s)
        {
            u32 ec = sb_count(strips[s]);
            for(u32 i = 0; i < ec; ++i)
            {
                auto& e1 = strips[s][i];
                
                if(i < ec-1)
                {
                    auto& e2 = strips[s][i+1];
                                        
                    vec3f vv[] = {
                        e1.end,
                        e1.start,
                        e2.start,
                        e2.start,
                        e2.end,
                        e1.end
                    };
                                        
                    vec3f t = normalised(e1.end - e1.start);
                    if(mag2(e1.end - e1.start) < 0.0001)
                    {
                        t = normalised(e2.end - e2.start);
                    }
                    
                    vec3f b = normalised(e2.start - e1.start);
                    if(mag2(e2.start - e1.start) < 0.0001)
                    {
                        b = normalised(e2.end - e1.end);
                    }

                    vec3f n = cross(t, b);
                    
                    if(!flip)
                    {
                        std::swap(vv[0], vv[1]);
                        std::swap(vv[3], vv[4]);
                        
                        n *= -1.0f;
                        t *= -1.0f;
                        b *= -1.0f;
                    }
                    
                    for(u32 k = 0; k < 6; ++k)
                    {
                        vertex_model v;
                        v.pos = vec4f(vv[k], 1.0f);
                        v.normal = vec4f(n, 0.0f);
                        v.tangent = vec4f(t, 0.0f);
                        v.bitangent = vec4f(b, 0.0f);
                    
                        sb_push(verts, v);
                    }
                }
            }
        }
        
        create_primitive_resource_faceted(name, verts, sb_count(verts));
    }
    
    void test_mesh()
    {
        if(s_edge_strips)
        {
            sb_free(s_edge_strips);
            s_edge_strips = nullptr;
        }
        
        sb_push(s_edge_strips, nullptr);
        
        edge e;
        e.start = vec3f(0.5f);
        e.end = vec3f(0.5f) + vec3f::unit_x() * 5.0f;
        e.mat = mat4::create_identity();
        sb_push(s_edge_strips[0], e);

        extrude(s_edge_strips[0], vec3f::unit_z() * -10.0f);
        extrude(s_edge_strips[0], vec3f::unit_y() * 10.0f);
        extrude(s_edge_strips[0], vec3f::unit_z() * -10.0f);
        extrude(s_edge_strips[0], vec3f::unit_y() * 10.0f);
        extrude(s_edge_strips[0], vec3f::unit_z() * 1.0f);
        extrude(s_edge_strips[0], vec3f::unit_y() * 1.0f);
        extrude(s_edge_strips[0], vec3f::unit_z() * -2.0f);
        extrude(s_edge_strips[0], vec3f::unit_y() * -2.0f);
        extrude(s_edge_strips[0], vec3f::unit_z() * -5.0f);
        
        // ...
        
        static const f32 theta = -M_PI/8.0;
        
        bend(s_edge_strips, 10.0f, vec2f(theta, 0.0));
        bend(s_edge_strips, 10.0f, vec2f(-theta, 0.0));
        bend(s_edge_strips, 10.0f, vec2f(0.0f, theta*4.0f));
        bend(s_edge_strips, 10.0f, vec2f(0.0f, theta*4.0f));
        bend(s_edge_strips, 30.0f, vec2f(theta, 0.0f));
        bend(s_edge_strips, 30.0f, vec2f(-theta, 0.0f));
        
        bend(s_edge_strips, 30.0f, vec2f(0.0f, theta));
        bend(s_edge_strips, 30.0f, vec2f(-theta, 0.0f));
        bend(s_edge_strips, 30.0f, vec2f(-theta, 0.0f));
        bend(s_edge_strips, 30.0f, vec2f(theta*2.0f, 0.0f));
        bend(s_edge_strips, 30.0f, vec2f(0.0f, theta*4.0f));
        
        bend(s_edge_strips, 30.0f, vec2f(theta, 0.0f));
        bend(s_edge_strips, 30.0f, vec2f(theta, 0.0f));
        bend(s_edge_strips, 30.0f, vec2f(theta, 0.0f));
        bend(s_edge_strips, 30.0f, vec2f(theta, 0.0f));
        
        bend(s_edge_strips, 30.0f, vec2f(0.0, theta));
        bend(s_edge_strips, 30.0f, vec2f(0.0, theta));
        bend(s_edge_strips, 30.0f, vec2f(0.0, theta));
        bend(s_edge_strips, 30.0f, vec2f(0.0, theta));
        
        for(u32 j = 0; j < 64; ++j)
        {
            bend(s_edge_strips, 5.0f, vec2f(theta));
        }
        
        mesh_from_strips("subway", s_edge_strips, true);
    }
    
    void test_mesh2()
    {
        if(s_edge_strips)
        {
            sb_free(s_edge_strips);
            s_edge_strips = nullptr;
        }
        
        sb_push(s_edge_strips, nullptr);
        
        edge e;
        e.start = vec3f(0.5f);
        e.end = vec3f(0.5f) + vec3f::unit_y();
        e.mat = mat4::create_identity();
        sb_push(s_edge_strips[0], e);

        extrude(s_edge_strips[0], vec3f::unit_x() * 0.5f);
        extrude(s_edge_strips[0], vec3f::unit_z() * -0.5f);
        extrude(s_edge_strips[0], vec3f::unit_x() * -0.5f);
        extrude(s_edge_strips[0], vec3f::unit_z() * 0.5f);
        
        static const f32 theta = -M_PI/8.0;
        
        bend(s_edge_strips, 20.0f, vec2f(0.0, 0.0));
        bend(s_edge_strips, 10.0f, vec2f(-theta*4.0f, 0.0));
        
        mesh_from_strips("lamp_post", s_edge_strips);
    }
    
    void test_mesh3()
    {
        if(s_edge_strips)
        {
            sb_free(s_edge_strips);
            s_edge_strips = nullptr;
        }
        
        sb_push(s_edge_strips, nullptr);
        
        edge e;
        e.start = vec3f(0.0f, 20.0f, -25.0f);
        e.end = e.start - vec3f::unit_x() * 20.0f;
        e.mat = mat4::create_identity();
        sb_push(s_edge_strips[0], e);

        extrude(s_edge_strips[0], vec3f::unit_y() * 5.0f);
        extrude(s_edge_strips[0], vec3f::unit_z() * -2.0f);
        extrude(s_edge_strips[0], vec3f::unit_y() * 20.0f);
        extrude(s_edge_strips[0], vec3f::unit_z() * 2.0f);
        extrude(s_edge_strips[0], vec3f::unit_y() * 2.0f);
        extrude(s_edge_strips[0], vec3f::unit_z() * -2.0f);
        
        static const f32 theta = -M_PI/2.0;
        
        bend(s_edge_strips, 20.0f, vec2f(-theta, 0.0));
        bend(s_edge_strips, 20.0f, vec2f(-theta, 0.0));
        bend(s_edge_strips, 20.0f, vec2f(-theta, 0.0));
        bend(s_edge_strips, 20.0f, vec2f(-theta, 0.0));
        
        mesh_from_strips("building", s_edge_strips);
        
        /*
        u32 num_strips = sb_count(s_edge_strips);
        for(u32 s = 0; s < num_strips; ++s)
        {
            u32 ec = sb_count(s_edge_strips[s]);
            for(u32 i = 0; i < ec; ++i)
            {
                auto& e1 = s_edge_strips[s][i];
                
                if(i < ec-1)
                {
                    auto& e2 = s_edge_strips[s][i+1];
                    
                    add_triangle(e1.start, e1.end, e2.start);
                    add_triangle(e2.start, e2.end, e1.end);
                }
            }
        }
        */
    }
    
    void draw_edge_strip_triangles(edge** edge_strips)
    {
        u32 num_strips = sb_count(edge_strips);
        for(u32 s = 0; s < num_strips; ++s)
        {
            u32 ec = sb_count(edge_strips[s]);
            for(u32 i = 0; i < ec; ++i)
            {
                auto& e1 = edge_strips[s][i];
                
                if(i < ec-1)
                {
                    auto& e2 = edge_strips[s][i+1];
                    
                    add_triangle(e1.start, e1.end, e2.start);
                    add_triangle(e2.start, e2.end, e1.end);
                }
            }
        }
    }
    
    void voronoi_map_draw_cells(const voronoi_map* voronoi)
    {
        // If you want to draw triangles, or relax the diagram,
        // you can iterate over the sites and get all edges easily
        const jcv_site* sites = jcv_diagram_get_sites( &voronoi->diagram );
        
        u32 s = 8;
        
        for( int i = 0; i < voronoi->diagram.numsites; ++i )
        {
            if(i != s)
                continue;
                
            const jcv_site* site = &sites[i];

            const jcv_graphedge* e = site->edges;
            
            u32 cc = 0;
            while( e )
            {
                vec3f p1 = jcv_to_vec(site->p);
                vec3f p2 = jcv_to_vec(e->pos[0]);
                vec3f p3 = jcv_to_vec(e->pos[1]);
                e = e->next;
                
                //add_triangle(p1, p2, p3);
                
                if(cc < 5)
                    add_line(p2, p3);
                //break;
                
                ++cc;
            }
        }
    }
    
    void subdivide_hull(const vec3f& start_pos, const vec3f& axis, const vec3f& right, vec3f* inset_edge_points)
    {
        u32 nep = sb_count(inset_edge_points);
        u32 sub = 50;
        f32 spacing = 5.0f;
        for(u32 s = 0; s < sub; ++s)
        {
            vec3f p = start_pos + right * (f32)s * spacing;
            vec3f ray = p - axis * 10000.0f;
            p += axis * 10000.0f;
            
            vec3f* ips = nullptr;
            for(s32 i = 0; i < nep; i++)
            {
                s32 n = (i+1)%nep;
                
                vec3f p2 = inset_edge_points[i];
                vec3f p3 = inset_edge_points[n];
                
                vec3f ip;
                if(maths::line_vs_line(p, ray, p2, p3, ip))
                {
                    sb_push(ips, ip);
                }
            }
            
            if(sb_count(ips) == 2)
                add_line(ips[0], ips[1], vec4f::magenta());
                
            //add_line(p, ray, vec4f::green());
        }
    }
    
    void test_road(const voronoi_map* voronoi, u32 cell)
    {
        edge** edge_strips = nullptr;
        sb_push(edge_strips, nullptr);
        
        // build from cell
        const jcv_site* sites = jcv_diagram_get_sites( &voronoi->diagram );
        const jcv_site* site = &sites[cell];
        const jcv_graphedge* jvce = site->edges;
                
        vec3f* edge_points = nullptr;
        while( jvce )
        {
            vec3f p2 = jcv_to_vec(jvce->pos[1]);
            jvce = jvce->next;

            sb_push(edge_points, p2);
        }
        u32 nep = sb_count(edge_points);
        
        vec3f* inset_edge_points = nullptr;
        
        vec3f prev_line[2];
        vec3f first_line[2];
        vec3f prev_perp;
        
        f32 inset = 1.0f;
        
        // inset edges
        vec3f* inset_overlapped = nullptr;
        for(s32 i = nep-1; i >= 0; i--)
        {
            s32 n = i-1 < 0 ? nep-1 : i-1;
            
            vec3f p1 = edge_points[i];
            vec3f p2 = edge_points[n];
            
            add_line(p1, p2, vec4f::yellow());
            
            vec3f vl = normalised(vec3f(p2-p1));
            
            vec3f perp = cross(vl, vec3f::unit_y());
            
            perp *= inset;
            
            vec3f inset_edge0 = p1 - perp;
            vec3f inset_edge1 = p2 - perp;
            
            if(mag(inset_edge1 - inset_edge0) > 0.1)
            {
                sb_push(inset_overlapped, inset_edge0);
                sb_push(inset_overlapped, inset_edge1);
            }
        }
        
        // inset points with intersections
        u32 no = sb_count(inset_overlapped);
        for(s32 i = 0; i < no; i+=2)
        {
            s32 n = (i + 2) % no;
            
            vec3f p0 = inset_overlapped[i];
            vec3f p1 = inset_overlapped[i+1];
            
            vec3f pn0 = inset_overlapped[n];
            vec3f pn1 = inset_overlapped[n+1];

            vec3f ip;
            if(maths::line_vs_line(p0, p1, pn0, pn1, ip))
            {
                sb_push(inset_edge_points, ip);
            }
        }
        
        // weld points that are less than inset apart
        nep = sb_count(inset_edge_points);
        for(s32 i = 0; i < nep; i++)
        {
            for(s32 j = i + 1; j < nep; j++)
            {
                vec3f v = inset_edge_points[i] - inset_edge_points[j];
                if(mag2(v))
                {
                    if(mag(v) < inset)
                    {
                        inset_edge_points[j] = inset_edge_points[i];
                    }
                }
            }
        }
        
        // strip out dead edges
        vec3f* inset_edge_points2 = nullptr;
        for(s32 i = 0; i < nep; i++)
        {
            s32 n = (i+1)%nep;
            
            vec3f p2 = inset_edge_points[i];
            vec3f p3 = inset_edge_points[n];
            
            if(mag2(p3-p2))
            {
                sb_push(inset_edge_points2, p2);
            }
        }
        
        inset_edge_points = inset_edge_points2;
        
        vec3f right = normalised(inset_edge_points[1] - inset_edge_points[0]);
        vec3f up = vec3f::unit_y();
        vec3f at = normalised(cross(right, up));
                
        edge e;
        e.start = inset_edge_points[0];
        e.end = inset_edge_points[1];
        e.mat = mat4::create_identity();
        
        sb_push(edge_strips[0], e);
        extrude(edge_strips[0], at * 0.5f);
        extrude(edge_strips[0], vec3f::unit_y() * -0.5f);
        extrude(edge_strips[0], at * 0.5f);
                
        vec3f prev_vl = right;
                
        nep = sb_count(inset_edge_points);
        
        vec3f bl = vec3f::flt_max();
        for(s32 i = 0; i < nep; i++)
        {
            s32 n = (i+1)%nep;
            
            vec3f p2 = inset_edge_points[i];
            vec3f p3 = inset_edge_points[n];
            
            add_line(p2, p3, vec4f::blue());
            
            bl.x = std::min<f32>(p2.x, bl.x);
            bl.z = std::min<f32>(p2.z, bl.z);
            bl.y = std::min<f32>(p2.y, bl.y);
            
            if(i == 0)
                continue;
            
            vec3f vl = normalised(vec3f(p3-p2));
            
            f32 yaw = acos(dot(vl, prev_vl));
            
            bend(edge_strips, mag(p3-p2), vec2f(0.0, yaw));
            
            prev_vl = vl;
        }
        
        // subdivide cell
        subdivide_hull(e.start, right, -at, inset_edge_points);
        subdivide_hull(e.start - right * 50.0f, at, right, inset_edge_points);
                
        // draw_edge_strip_triangles(edge_strips);
        
        Str f;
        f.appendf("cell_%i", cell);
        
        // mesh_from_strips(f.c_str(), edge_strips);
                
        u32 ns = sb_count(edge_strips);
        for(u32 s = 0; s < ns; ++s)
        {
            sb_free(edge_strips[s]);
        }
        
        sb_free(edge_strips);
        sb_free(edge_points);
    }
    
    int on_update(f32 dt)
    {
        u32 test_single = -1;
        if(test_single != -1)
        {
            test_road(voronoi, test_single);
        }
        else
        {
            for(u32 i = 0; i < voronoi->diagram.numsites; ++i)
                test_road(voronoi, i);
        }

        return 0;
    }
    
    int on_unload()
    {
        return 0;
    }
};

CR_EXPORT int cr_main(struct cr_plugin *ctx, enum cr_op operation)
{
    live_context* live_ctx = (live_context*)ctx->userdata;
    static live_lib ll;
    
    switch (operation)
    {
        case CR_LOAD:
            return ll.on_load(live_ctx);
        case CR_UNLOAD:
            return ll.on_unload();
        case CR_CLOSE:
            return 0;
        default:
            break;
    }
    
    return ll.on_update(live_ctx->dt);
}

namespace pen
{
    pen_creation_params pen_entry(int argc, char** argv)
    {
        return {};
    }
    
    void* user_entry(void* params)
    {
        return nullptr;
    }
}

void basis_from_axis(const vec3d axis, vec3d& right, vec3d& up, vec3d& at)
{
    right = cross(axis, vec3d::unit_y());
    
    if (mag(right) < 0.1)
        right = cross(axis, vec3d::unit_z());
    
    if (mag(right) < 0.1)
        right = cross(axis, vec3d::unit_x());
        
    normalise(right);
    up = normalised(cross(axis, right));
    right = normalised(cross(axis, up));
    at = cross(right, up);
}

void pentagon_in_axis(const vec3d axis, const vec3d pos, f64 start_angle, bool recurse)
{
    vec3d right, up, at;
    basis_from_axis(axis, right, up, at);
    
    f64 half_gr = 1.61803398875l/2.0;
        
    f64 internal_angle = 0.309017 * 1.5;
    f64 angle_step = M_PI / 2.5;
    f64 a = start_angle;
    for(u32 i = 0; i < 5; ++i)
    {
        f64 x = sin(a) * M_INV_PHI;
        f64 y = cos(a) * M_INV_PHI;
        
        vec3d p = pos + right * x + up * y;
        
        a += angle_step;
        f64 x2 = sin(a) * M_INV_PHI;
        f64 y2 = cos(a) * M_INV_PHI;
        
        vec3d np = pos + right * x2 + up * y2;
        add_line((vec3f)p, (vec3f)np, vec4f::green());
                    
        vec3d ev = normalised(np - p);
        vec3d cp = normalised(cross(ev, axis));

        vec3d mid = p + (np - p) * 0.5;
        
        f64 rx = sin((M_PI*2.0)+internal_angle) * M_INV_PHI;
        f64 ry = cos((M_PI*2.0)+internal_angle) * M_INV_PHI;
        vec3d xp = mid + cp * rx + axis * ry;
        
        vec3d xv = normalised(xp - mid);

        if(recurse)
        {
            vec3d next_axis = normalised(cross(xv, ev));
            pentagon_in_axis(next_axis, mid + xv * half_gr * M_INV_PHI, M_PI + start_angle, false);
        }
    }
}

void penatgon_icosa(const vec3d axis, const vec3d pos, f64 start_angle)
{
    vec3d right, up, at;
    basis_from_axis(axis, right, up, at);
    
    vec3d tip = pos - at * M_INV_PHI;
    vec3d dip = pos + at * 0.5 * 2.0;
    
    f64 angle_step = M_PI / 2.5;
    f64 a = start_angle;
    for(u32 i = 0; i < 5; ++i)
    {
        f64 x = sin(a);
        f64 y = cos(a);
        
        vec3d p = pos + right * x + up * y;
        
        a += angle_step;
        f64 x2 = sin(a);
        f64 y2 = cos(a);
        
        vec3d np = pos + right * x2 + up * y2;
        add_line((vec3f)p, (vec3f)np, vec4f::green());
        add_line((vec3f)p, (vec3f)tip, vec4f::yellow());
        add_line((vec3f)np, (vec3f)tip, vec4f::cyan());
        
        vec3d side_dip = dip + cross(normalized(p-np), at);
        add_line((vec3f)np, (vec3f)side_dip, vec4f::magenta());
        add_line((vec3f)p, (vec3f)side_dip, vec4f::magenta());
    }
}

void icosahedron(vec3f axis, vec3f pos)
{
    penatgon_icosa((vec3d)axis, (vec3d)(pos + axis * 0.5f), 0.0);
    penatgon_icosa((vec3d)-axis, (vec3d)(pos - axis * 0.5f), M_PI);
}

void dodecahedron(vec3f axis, vec3f pos)
{
    f32 h = M_PI*0.83333333333f * 0.5f * M_INV_PHI;
    pentagon_in_axis((vec3d)axis, (vec3d)pos + vec3d(0.0, -h, 0.0), 0.0f, true);
    pentagon_in_axis((vec3d)-axis, (vec3d)pos + vec3d(0.0, h, 0.0), M_PI, true);
}

void terahedron(vec3d axis, vec3d pos)
{
    vertex_model* vertices = nullptr;
    
    vec3d right, up, at;
    basis_from_axis(axis, right, up, at);
        
    vec3d tip = pos - at * sqrt(2.0); // sqrt 2 is pythagoras constant
    
    f64 angle_step = (M_PI*2.0) / 3.0;
    f64 a = 0.0f;
    for(u32 i = 0; i < 3; ++i)
    {
        f64 x = sin(a);
        f64 y = cos(a);
                    
        vec3d p = pos + right * x + up * y;
        
        a += angle_step;
        f64 x2 = sin(a);
        f64 y2 = cos(a);
        
        vec3d np = pos + right * x2 + up * y2;
        
        vec3f n = maths::get_normal((vec3f)p, (vec3f)np, (vec3f)tip);
        vec3f b = (vec3f)normalised(p - np);
        vec3f t = cross(n, b);
        
        vertex_model v[3];
        
        v[0].pos.xyz = (vec3f)p;
        v[1].pos.xyz = (vec3f)np;
        v[2].pos.xyz = (vec3f)tip;
        
        for(u32 j = 0; j < 3; ++j)
        {
            v[j].pos.w = 1.0;
            v[j].normal = vec4f(n, 1.0f);
            v[j].bitangent = vec4f(b, 1.0f);
            v[j].tangent = vec4f(t, 1.0f);
        }
    }
    
    u16* indices = nullptr;
    
    vec3f min_extents = vec3f::flt_max();
    vec3f max_extents = -vec3f::flt_max();
    
    u32 nv = sb_count(vertices);
    for(u32 i = 0; i < nv; i++)
    {
        sb_push(indices, i);
        
        min_extents = min_union(vertices[i].pos.xyz, min_extents);
        max_extents = max_union(vertices[i].pos.xyz, max_extents);
    }

    sb_free(vertices);
}

void octahedron()
{
    vertex_model* vertices = nullptr;
    
    vec3f corner[] = {
        vec3f(-1.0, 0.0, -1.0),
        vec3f(-1.0, 0.0, 1.0),
        vec3f(1.0, 0.0, 1.0),
        vec3f(1.0, 0.0, -1.0)
    };
    
    f32 pc = sqrt(2.0);
    vec3f tip = vec3f(0.0f, pc, 0.0f);
    vec3f dip = vec3f(0.0f, -pc, 0.0f);
    
    for(u32 i = 0; i < 4; ++i)
    {
        u32 n = (i + 1) % 4;
        
        vec3f y[] = {
            tip,
            dip
        };
        
        // 2 tris per edg
        for(u32 j = 0; j < 2; ++j)
        {
            vertex_model v[3];
            v[0].pos.xyz = corner[i];
            v[1].pos.xyz = corner[n];
            v[2].pos.xyz = y[j];
            
            vec3f n = maths::get_normal(v[0].pos.xyz, v[1].pos.xyz, v[2].pos.xyz);
            vec3f b = normalised(v[0].pos.xyz - v[1].pos.xyz);
            vec3f t = cross(n, b);
            
            for(u32 k = 0; k < 3; ++k)
            {
                v[k].pos.w = 1.0f;
                v[k].normal = vec4f(n, 1.0f);
                v[k].tangent = vec4f(t, 1.0f);
                v[k].bitangent = vec4f(b, 1.0f);
                
                sb_push(vertices, v[k]);
            }
        }
    }
    
    u16* indices = nullptr;
    
    vec3f min_extents = vec3f::flt_max();
    vec3f max_extents = -vec3f::flt_max();
    
    u32 nv = sb_count(vertices);
    for(u32 i = 0; i < nv; i++)
    {
        sb_push(indices, i);
        
        min_extents = min_union(vertices[i].pos.xyz, min_extents);
        max_extents = max_union(vertices[i].pos.xyz, max_extents);
    }
    
    for(u32 i = 0; i < nv; i+=3)
    {
        dbg::add_triangle(vertices[i].pos.xyz, vertices[i+1].pos.xyz, vertices[i+2].pos.xyz);
    }
}

void create_torus_primitive(f32 radius)
{
    f64 angle_step = (M_PI*2.0)/64.0;
    f64 aa = 0.0f;
    for(u32 i = 0; i < 64; ++i)
    {
        f64 x = sin(aa);
        f64 y = cos(aa);
        
        aa += angle_step;
        f64 x2 = sin(aa);
        f64 y2 = cos(aa);
        
        f64 x3 = sin(aa + angle_step);
        f64 y3 = cos(aa + angle_step);
        
        vec3f p = vec3f(x, 0.0, y);
        vec3f np = vec3f(x2, 0.0, y2);
        vec3f nnp = vec3f(x3, 0.0, y3);
        
        vec3f at = normalized(np - p);
        vec3f up = vec3f::unit_y();
        vec3f right = cross(up, at);
        
        vec3f nat = normalized(nnp - np);
        vec3f nright = cross(up, nat);
        
        f64 ab = 0.0f;
        for(u32 j = 0; j < 64; ++j)
        {
            f32 vx = sin(ab) * radius;
            f32 vy = cos(ab) * radius;
            
            vec3f vv = p + vx * up + vy * right;
            
            ab += angle_step;
            
            f32 vx2 = sin(ab) * radius;
            f32 vy2 = cos(ab) * radius;
            
            vec3f vv2 = p + vx2 * up + vy2 * right;
            
            add_line(vv, vv2);
            
            vec3f vv3 = np + vx * up + vy * nright;
            vec3f vv4 = np + vx2 * up + vy2 * nright;
            
            //add_line(vv, vv2);
            
            //add_line(vv, vv3, vec4f::yellow());
            
            add_line(vv, vv3, vec4f::yellow());
        }
    }
}


