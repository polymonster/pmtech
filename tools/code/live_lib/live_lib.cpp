#include "cr/cr.h"
#include "ecs/ecs_scene.h"
#include "ecs/ecs_utilities.h"
#include "ecs/ecs_resources.h"
#include "ecs/ecs_cull.h"
#include "debug_render.h"

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

using namespace put;
using namespace dbg;
using namespace ecs;

bool g_mesh = false;
bool g_debug = true;

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
        voronoi->num_points = 30;
        for(u32 i = 0; i < voronoi->num_points; ++i)
        {
            // 3d point for rendering
            vec3f p = vec3f((rand() % 60) - 30, 0.0f, (rand() % 60) - 30);
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
                                
                if(cc < 5)
                    add_line(p2, p3);
                
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
        
        // primitive resources
        material_resource* default_material = get_material_resource(PEN_HASH("default_material"));
        
        //test_mesh();
        //test_mesh2();
        //test_mesh3();
    
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
        generate();
                
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
        
        // cap
        vec3f* inner_loop = nullptr;
        for(u32 s = 0; s < num_strips; ++s)
        {
            sb_push(inner_loop, strips[s][0].end);
        }
        
        u32 nl = sb_count(inner_loop);
        vec3f* cap_hull = nullptr;
        convex_hull_from_points(cap_hull, inner_loop, nl);
        
        u32 cl = sb_count(cap_hull);
        vec3f mid = get_convex_hull_centre(cap_hull, cl);
        for(u32 s = 0; s < cl; ++s)
        {
            u32 next = (s + 1) % cl;
            
            // one tri per hull side
            vec3f vv[3] = {
                cap_hull[s],
                mid,
                cap_hull[next]
            };
            
            vec3f t = vec3f::unit_x();
            vec3f b = vec3f::unit_z();
            vec3f n = vec3f::unit_y();
            
            for(u32 k = 0; k < 3; ++k)
            {
                vertex_model v;
                v.pos = vec4f(vv[k], 1.0f);
                v.normal = vec4f(n, 0.0f);
                v.tangent = vec4f(t, 0.0f);
                v.bitangent = vec4f(b, 0.0f);
            
                sb_push(verts, v);
            }
            
            //add_line(cap_hull[s], cap_hull[t], vec4f::magenta());
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
                
                if(i < ec-2)
                {
                    auto& e2 = edge_strips[s][i+1];
                    
                    //add_triangle(e1.start, e1.end, e2.start);
                    //add_triangle(e2.start, e2.end, e1.end);
                }
                else
                {
                    if(i < ec-1)
                        add_line(e1.start, e1.end, vec4f::red());
                }
            }
        }
        
        // cap
        /*
        vec3f* inner_loop = nullptr;
        for(u32 s = 0; s < num_strips; ++s)
        {
            sb_push(inner_loop, edge_strips[s][0].end);
        }
        
        u32 nl = sb_count(inner_loop);
        vec3f* cap_hull = nullptr;
        convex_hull_from_points(cap_hull, inner_loop, nl);
        
        u32 cl = sb_count(cap_hull);
        for(u32 s = 0; s < cl; ++s)
        {
            u32 t = (s + 1) % cl;
            add_line(cap_hull[s], cap_hull[t], vec4f::magenta());
        }
        */
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
        }
    }
    
    vec3f get_convex_hull_centre(vec3f* hull, u32 count)
    {
        vec3f mid = vec3f::zero();
        for(u32 i = 0; i < count; ++i)
        {
            mid += hull[i];
        }
        
        mid /= (f32)count;
        return mid;
    }
    
    bool point_inside_convex_hull(vec3f* hull, size_t ncp, const vec3f& up, const vec3f& p0)
    {
        for(size_t i = 0; i < ncp; ++i)
        {
            size_t i2 = (i+1)%ncp;
            
            vec3f p1 = hull[i];
            vec3f p2 = hull[i2];
            
            vec3f v1 = p2 - p1;
            vec3f v2 = p0 - p1;
            
            if(dot(cross(v2,v1), up) > 0.0f)
                return false;
        }
        
        return true;
    }

    vec3f closest_point_on_convex_hull(vec3f* hull, size_t ncp, vec3f p)
    {
        f32 cd = FLT_MAX;
        vec3f cp = vec3f::zero();
        for(u32 i = 0; i < ncp; ++i)
        {
            u32 n = (i+1)%ncp;
            vec3f lp = maths::closest_point_on_line(hull[i], hull[n], p);
            f32 d = mag2(p-lp);
            if(d < cd)
            {
                cp = lp;
                cd = d;
            }
        }
        return cp;
    }

    f32 convex_hull_area(vec3f* points, size_t num_points)
    {
        f32 sum = 0.0f;
        for (u32 i = 0; i < num_points; ++i)
        {
            u32 n = (i + 1) % num_points;
            vec3f v1 = points[i];
            vec3f v2 = points[n];

            sum += v1.x * v2.z - v2.x * v1.z;
        }

        return 0.5f * abs(sum);
    }
    
    void convex_hull_from_points(vec3f*& hull, vec3f* points, size_t num_points)
    {
        vec3f* to_sort = nullptr;
        bool*  visited = nullptr;
        for (u32 i = 0; i < num_points; ++i)
        {
            u32 count = sb_count(to_sort);
            bool dupe = false;
            for (u32 j = 0; j < count; ++j)
            {
                if(almost_equal(to_sort[j], points[i], 0.001f))
                    dupe = true;
            }
            
            if (!dupe)
            {
                sb_push(to_sort, points[i]);
                sb_push(visited, false);
            }
        }
        num_points = sb_count(to_sort);

        //find right most
        num_points = sb_count(to_sort);
        vec3f cur = to_sort[0];
        size_t curi = 0;
        for (size_t i = 1; i < num_points; ++i)
        {
            if(to_sort[i].z >= cur.z)
                //if (to_sort[i].x >= cur.x)
                {
                    cur = to_sort[i];
                    curi = i;
                }
        }
        
        // wind
        sb_push(hull, cur);
        u32 iters = 0;
        for(;;)
        {
            size_t rm = (curi+1)%num_points;
            vec3f x1 = to_sort[rm];

            for (size_t i = 0; i < num_points; ++i)
            {
                if(i == curi)
                    continue;

                if (visited[i])
                    continue;
                
                vec3f x2 = to_sort[i];
                vec3f v1 = x1 - cur;
                vec3f v2 = x2 - cur;

                vec3f cp = cross(v2, v1);
                if (cp.y > 0.0f)
                {
                    x1 = to_sort[i];
                    rm = i;
                }
            }

            f32 diff = mag2(x1 - hull[0]);
            if(almost_equal(x1, hull[0], 0.01f))
                break;
            
            cur = x1;
            curi = rm;
            visited[rm] = true;
            sb_push(hull, x1);
            ++iters;

            // saftey break, but we shouldnt hit this
            if (iters > num_points)
                break;
        }
        
        sb_free(to_sort);
    }

    bool line_vs_convex_hull(vec3f l1, vec3f l2, vec3f* hull, u32 num_points, vec3f& ip)
    {
        for (u32 i = 0; i < num_points; ++i)
        {
            u32 n = (i + 1) % num_points;

            l1.y = 0.0f;
            l2.y = 0.0f;
            hull[i].y = 0.0f;
            hull[n].y = 0.0f;

            if(maths::line_vs_line(hull[i], hull[n], l1, l2, ip))
            {
                return true;
            }
        }

        return false;
    }

    bool line_vs_convex_hull_ex(vec3f l1, vec3f l2, vec3f* hull, u32 num_points, vec3f& ip)
    {
        for (u32 i = 0; i < num_points; ++i)
        {
            u32 n = (i + 1) % num_points;

            l1.y = 0.0f;
            l2.y = 0.0f;
            hull[i].y = 0.0f;
            hull[n].y = 0.0f;

            if(maths::line_vs_line(hull[i], hull[n], l1, l2, ip))
            {
                f32 d = abs(dot(normalised(hull[n] - hull[i]), normalised(l2 - l1)));

                return d < 0.01f;
            }
        }

        return false;
    }

    void draw_convex_hull(const vec3f* points, size_t num_points, vec4f col = vec4f::white())
    {
        for (u32 i = 0; i < num_points; ++i)
        {
            u32 n = (i + 1) % num_points;
            add_line(points[i], points[n], col);
            add_point(points[i], 0.1f, col/(f32)i);
        }
    }

    f32 convex_hull_distance(const vec3f* hull, u32 num_points, const vec3f& p)
    {
        f32 cd = FLT_MAX;
        for (u32 i = 0; i < num_points; ++i)
        {
            u32 n = (i+1) % num_points;
            f32 d = maths::point_segment_distance(hull[i], hull[n], p);
            cd = std::min<f32>(d, cd);
        }
        return cd;
    }

    vec3f* convex_hull_tidy(vec3f* hull, u32 num_points, f32 weld_distance)
    {
        // weld points that are less than weld_distance apart
        for(s32 i = 0; i < num_points; i++)
        {
            for(s32 j = i + 1; j < num_points; j++)
            {
                vec3f v = hull[i] - hull[j];
                if(mag2(v))
                {
                    if(mag(v) < weld_distance)
                    {
                        hull[j] = hull[i];
                    }
                }
            }
        }
        
        // strip out dead edges
        vec3f* tidy = nullptr;
        for(s32 i = 0; i < num_points; i++)
        {
            s32 n = (i+1)%num_points;
            
            vec3f p2 = hull[i];
            vec3f p3 = hull[n];
            
            if(mag2(p3-p2))
            {
                sb_push(tidy, p2);
            }
        }

        sb_free(hull);
        return tidy;
    }
    
    void curb(edge**& edge_strips, vec3f* hull_points)
    {
        vec3f right = normalised(hull_points[1] - hull_points[0]);
        vec3f up = vec3f::unit_y();
        vec3f at = normalised(cross(right, up));
        
        f32 height = ((f32)(rand()%RAND_MAX) / (f32)RAND_MAX) * 5.0f;
        height = 0.1f;
        vec3f ystart = vec3f(0.0f, height, 0.0f);
                
        edge e;
        e.start = hull_points[0] + ystart;
        e.end = hull_points[1] + ystart;
        e.mat = mat4::create_identity();
        
        f32 scale = 0.1f;
        
        sb_push(edge_strips[0], e);
        extrude(edge_strips[0], at * scale * 2.0f);
        extrude(edge_strips[0], vec3f::unit_y() * -scale * 10.0f * height);
                
        vec3f prev_vl = right;
                
        u32 nep = sb_count(hull_points);
        
        vec3f bl = vec3f::flt_max();
        for(s32 i = 0; i < nep+1; i++)
        {
            s32 n = (i+1)%nep;
            s32 ii = (i)%nep;
            
            vec3f p2 = hull_points[ii];
            vec3f p3 = hull_points[n];
     
            bl.x = std::min<f32>(p2.x, bl.x);
            bl.z = std::min<f32>(p2.z, bl.z);
            bl.y = std::min<f32>(p2.y, bl.y);
            
            if(i == 0)
                continue;
            
            vec3f vl = normalised(vec3f(p3-p2));
            
            f32 yaw = acos(dot(vl, prev_vl));
            if (std::isnan(yaw))
            {
                yaw = 0.0f;
            }
      
            bend(edge_strips, mag(p3-p2), vec2f(0.0, yaw));
                        
            prev_vl = vl;
        }
    }

    struct segment
    {
        vec3f p1, p2;
    };

    struct road_network_params
    {
        f32 inset = 0.5f;
        f32 major_inset = 0.75f;
        f32 subdiv_size = 0.4f;
    };

    struct road_section
    {
        segment entry;
        segment exit;
        vec3f   dir;
        u32*    destinations = nullptr;
    };

    struct road_network
    {
        road_network_params params = {};
        vec3f*              outer_hull = nullptr;
        vec3f*              inset_hull = nullptr;
        segment             extent_axis_points[2] = {};
        f32                 extent_axis_length[2] = {};
        vec3f               extent_axis[2] = {};
        vec3f               up = vec3f::zero();
        vec3f               right = vec3f::zero();
        vec3f               at = vec3f::zero();
        vec3f**             valid_sub_hulls = nullptr;
        road_section*       outer_road_sections = nullptr;
        road_section*       inner_road_sections = nullptr;
    };

    void get_hull_subdivision_extents(road_network& net)
    {
        // basis for hull
        net.right = normalised(net.inset_hull[1] - net.inset_hull[0]);
        net.up = vec3f::unit_y();
        net.at = normalised(cross(net.right, net.up));
                
        edge e;
        e.start = net.inset_hull[0];
        e.end = net.inset_hull[1];
        e.mat = mat4::create_identity();
                
        // get extents in right axis
        vec3f side_start = e.start - net.right * 1000.0f;
        vec3f side_end = e.start + net.right * 1000.0f;
        vec3f side_v = normalised(side_end - side_start);

        f32 mind = FLT_MAX;
        f32 maxd = -FLT_MAX;

        vec3f mincp;
        vec3f maxcp;
        
        u32 num_points = sb_count(net.inset_hull);
        for(s32 i = 0; i < num_points; i++)
        {
            f32 d  = maths::distance_on_line(side_start, side_end, net.inset_hull[i]);
            if(d > maxd)
            {
                maxd = d;
                maxcp = maths::closest_point_on_ray(side_start, side_v, net.inset_hull[i]);
            }
            
            if(d < mind)
            {
                mind = d;
                mincp = maths::closest_point_on_ray(side_start, side_v, net.inset_hull[i]);
            }
        }
        net.extent_axis_points[0].p1 = mincp;
        net.extent_axis_points[0].p2 = maxcp;

        // get extents in at axis
        vec3f side_perp_start = mincp - net.at * 1000.0f;
        vec3f side_perp_end = mincp + net.at * 1000.0f;
        vec3f side_perp_v = normalised(side_perp_end - side_perp_start);
        
        net.extent_axis_points[1].p1 = mincp;
        maxd = -FLT_MAX;
        mind = FLT_MAX;
        for(s32 i = 0; i < num_points; i++)
        {
            f32 d  = maths::distance_on_line(side_perp_start, side_perp_end, net.inset_hull[i]);
            if(d < mind)
            {
                mind = d;
                mincp = maths::closest_point_on_ray(side_perp_start, side_perp_v, net.inset_hull[i]);
            }
        }
        
        net.extent_axis_points[1].p2 = mincp;

        net.extent_axis_length[0] = mag(net.extent_axis_points[0].p2 - net.extent_axis_points[0].p1);
        net.extent_axis_length[1] = mag(net.extent_axis_points[1].p2 - net.extent_axis_points[1].p1);

        net.extent_axis[0] = normalised(net.extent_axis_points[0].p2 - net.extent_axis_points[0].p1);
        net.extent_axis[1] = normalised(net.extent_axis_points[1].p2 - net.extent_axis_points[1].p1);    
    }

    void subdivide_as_grid(road_network& net)
    {
        u32 x =  net.extent_axis_length[0] / net.params.subdiv_size;
        u32 y =  net.extent_axis_length[1] / net.params.subdiv_size;
        
        f32 half_size = (net.params.subdiv_size * 0.5f) - net.params.inset;
        f32 size = half_size + net.params.inset;

        bool valid = false;
        u32 count = 0;

        vec3f** valid_sub_hulls = nullptr;
        vec3f** invalid_sub_hulls = nullptr;

        vec2i* valid_grid_index = nullptr;
        vec2i* invalid_grid_index = nullptr;

        vec3f* junction_pos = nullptr;

        u32 num_points = sb_count(net.inset_hull);
        
        for(u32 i = 0; i < x+1; ++i)
        {
            for(u32 j = 0; j < y+1; ++j)
            {
                f32 xt = (f32)i * net.params.subdiv_size;
                f32 yt = (f32)j * net.params.subdiv_size;
                
                vec3f py = net.extent_axis_points[1].p1 + net.extent_axis[1] * (yt + half_size);
                vec3f px = py + net.extent_axis[0] * (xt + half_size);
                
                vec3f corners[4] = {
                    px - net.extent_axis[0] * half_size - net.extent_axis[1] * half_size,
                    px + net.extent_axis[0] * half_size - net.extent_axis[1] * half_size,
                    px + net.extent_axis[0] * half_size + net.extent_axis[1] * half_size,
                    px - net.extent_axis[0] * half_size + net.extent_axis[1] * half_size,
                };

                vec3f junctions[4] = {
                    px - net.extent_axis[0] * size - net.extent_axis[1] * size,
                    px + net.extent_axis[0] * size - net.extent_axis[1] * size,
                    px + net.extent_axis[0] * size + net.extent_axis[1] * size,
                    px - net.extent_axis[0] * size + net.extent_axis[1] * size,
                };

                for (u32 jj = 0; jj < 4; ++jj)
                {
                    if (point_inside_convex_hull(net.inset_hull, num_points, net.up, junctions[jj]))
                    {
                        sb_push(junction_pos, junctions[jj]);
                    }
                }
                
                vec3f* sub_hull_points = nullptr;
                for(u32 c = 0; c < 4; ++c)
                {
                    u32 d = (c + 1) % 4;
                    
                    bool inside = false;

                    if(point_inside_convex_hull(net.inset_hull, num_points, net.up, corners[c]))
                    {
                        valid = true;
                        inside = true;
                        sb_push(sub_hull_points, corners[c]);
                    }
                    else if(point_inside_convex_hull(net.inset_hull, num_points, net.up, corners[d]))
                    {
                        valid = true;
                        inside = true;
                        sb_push(sub_hull_points, corners[d]);
                    }
                    
                    // intersect with hull
                    for(s32 i = 0; i < num_points; i++)
                    {
                        s32 n = (i + 1) % num_points;
                        
                        vec3f p0 = net.inset_hull[i];
                        vec3f p1 = net.inset_hull[n];

                        vec3f ip;
                        if(maths::line_vs_line(p0, p1, corners[c], corners[d], ip))
                        {
                            sb_push(sub_hull_points, ip);
                        }
                        
                        if(point_inside_convex_hull(corners, 4, net.up, p0))
                        {
                            sb_push(sub_hull_points, p0);
                        }
                    }
                }
                
                u32 sp = sb_count(sub_hull_points);
                if(sp > 0)
                {                    
                    vec3f* sub_hull = nullptr;
                    convex_hull_from_points(sub_hull, sub_hull_points, sp);

                    // must be at least a tri
                    if(sb_count(sub_hull) < 3)
                        continue;

                    f32 area = convex_hull_area(sub_hull, sb_count(sub_hull));
                    if (area < net.params.inset)
                    {
                        sb_push(invalid_sub_hulls, sub_hull);
                        sb_push(invalid_grid_index, vec2i(i, j));
                        continue;
                    }
                    else
                    {
                        sb_push(valid_sub_hulls, sub_hull);
                        sb_push(valid_grid_index, vec2i(i, j));
                    }
                    count++;
                }
            }
        }

        // join small hulls to neighbours
        u32 num_invalid = sb_count(invalid_sub_hulls);
        u32 num_valid = sb_count(valid_sub_hulls);
        for (u32 i = 0; i < num_invalid; ++i)
        {
            vec3f vc = get_convex_hull_centre(invalid_sub_hulls[i], sb_count(invalid_sub_hulls[i]));
            //draw_convex_hull(invalid_sub_hulls[i], sb_count(invalid_sub_hulls[i]), vec4f::red());

            vec2i igx = invalid_grid_index[i];

            // find closest hull to join with
            f32 cd = FLT_MAX;
            u32 cj = -1;
            for (u32 j = 0; j < num_valid; ++j)
            {
                vec2i vgx = valid_grid_index[j];

                if (abs(igx.x - vgx.x) + abs(igx.y - vgx.y) > 1)
                    continue;

                vec3f ic = get_convex_hull_centre(valid_sub_hulls[j], sb_count(valid_sub_hulls[j]));
                f32 d = mag2(vc - ic);

                if (d < cd)
                {
                    cd = d;
                    cj = j;
                }
            }

            if (cj != -1)
                for (u32 p = 0; p < sb_count(invalid_sub_hulls[i]); ++p)
                    sb_push(valid_sub_hulls[cj], invalid_sub_hulls[i][p]);
        }

        // 
        vec3f** output_sub_hulls = nullptr;
        for(u32 i = 0; i < num_valid; ++i)
        {
            vec3f* combined_hull = nullptr;
            convex_hull_from_points(combined_hull, valid_sub_hulls[i], sb_count(valid_sub_hulls[i]));
            sb_push(output_sub_hulls, combined_hull);
        }

        net.valid_sub_hulls = output_sub_hulls;
    }

    void find_junctions(road_network& net)
    {
        // intersection point result
        vec3f ip;

        road_section* road_sections = nullptr;

        u32 num_hulls = sb_count(net.valid_sub_hulls);
        for(u32 h = 0; h < num_hulls; ++h)
        {
            u32 num_points = sb_count(net.valid_sub_hulls[h]);
            for(u32 i = 0; i < num_points; ++i)
            {
                // get perp of edge
                auto& loop = net.valid_sub_hulls[h];
                u32 n = (i+1)%num_points;
                vec3f ve = normalised(loop[n] - loop[i]);
                vec3f perp = cross(ve, vec3f::unit_y());

                // intersect perps with the outer hull
                vec3f junction_perps[] = {
                    loop[i] + perp * 0.2f, loop[i] + perp,
                    loop[n] + perp * 0.2f, loop[n] + perp,
                };

                vec3f ips[2] = {
                    junction_perps[1],
                    junction_perps[3]
                };

                bool outer_road = false;
                for(u32 j = 0; j < 4; j +=2)
                {
                    if(line_vs_convex_hull(junction_perps[j], junction_perps[j+1], net.outer_hull, sb_count(net.outer_hull), ip))
                    {
                        outer_road = true;
                        ips[j/2] = ip;
                    }
                }

                if(outer_road)
                {
                    road_section section;
                    section.entry = {junction_perps[0], ips[0]};
                    section.exit = {junction_perps[2], ips[1]};
                    section.dir = ve;
                    sb_push(road_sections, section);
                }
                else
                {
                    // intersect perps with the outer hull
                    vec3f internal_junction_perps[] = {
                        loop[i] + perp * 0.2f, loop[i] + perp * net.params.inset,
                        loop[n] + perp * 0.2f, loop[n] + perp * net.params.inset,
                    };

                    road_section section;
                    section.entry.p1 = internal_junction_perps[0];
                    section.entry.p2 = internal_junction_perps[1];
                    section.exit.p1 = internal_junction_perps[2];
                    section.exit.p2 = internal_junction_perps[3];
                    section.dir = ve;

                    sb_push(road_sections, section);
                }
            }
        }

        // connect
        u32 num_inner_sections = sb_count(road_sections);
        for(u32 i = 0; i < num_inner_sections; ++i)
        {
            vec3f vray = road_sections[i].dir;
            vec3f ray_pos = road_sections[i].exit.p1 + (road_sections[i].exit.p2 - road_sections[i].exit.p1) * 0.5f;

            f32 cd = FLT_MAX;
            s32 cj = -1;
            vec3f cip = vec3f::zero();

            for(u32 j = 0; j < num_inner_sections; ++j)
            {
                if(i != j)
                {
                    if(maths::line_vs_line(ray_pos, ray_pos + vray * 100.0f, road_sections[j].entry.p1, road_sections[j].entry.p2, ip))
                    {
                        f32 d = mag2(ip - ray_pos);
                        if(d < cd)
                        {
                            cj = j;
                            cd = d;
                            cip = ip;
                        }
                    }                    
                }
            }

            if(cj != -1 && 0)
            {
                road_sections[i].entry.p1 = 
                road_sections[i].entry.p2 =
                road_sections[i].exit.p1 = 
                road_sections[i].exit.p2 = vec3f::zero();
            }

            if(cj >= 0)
            {
                sb_push(road_sections[i].destinations, cj);
            }
            
            // check intersection with inset hull
            u32 num_inset_hull_points = sb_count(net.inset_hull);
            for(u32 j = 0; j < num_inset_hull_points; ++j)
            {
                u32 n = (j+1)%num_inset_hull_points;

                vec3f& p1 = road_sections[i].entry.p1;
                vec3f& p2 = road_sections[i].exit.p1;

                // TODO: function
                if(maths::line_vs_line(
                    p1, p2, net.inset_hull[j], net.inset_hull[n], ip))
                {
                    f32 d1 = mag2(road_sections[i].entry.p1 - ip);
                    f32 d2 = mag2(road_sections[i].exit.p1 - ip);

                    if(d1 < d2)
                    {
                        road_sections[i].entry.p1 = ip;
                    }
                    else
                    {
                        road_sections[i].exit.p1 = ip;
                    }
                }

                vec3f& pp1 = road_sections[i].entry.p2;
                vec3f& pp2 = road_sections[i].exit.p2;

                if(maths::line_vs_line(
                    pp1, pp2, net.inset_hull[j], net.inset_hull[n], ip))
                {
                    f32 d1 = mag2(road_sections[i].entry.p2 - ip);
                    f32 d2 = mag2(road_sections[i].exit.p2 - ip);

                    if(d1 < d2)
                    {
                        road_sections[i].entry.p2 = ip;
                    }
                    else
                    {
                        road_sections[i].exit.p2 = ip;
                    }
                }
            }
        }

        net.inner_road_sections = road_sections;
    }

    void generate_curb_mesh(const road_network& net, ecs_scene* scene, u32 cell)
    {
        u32 num_hulls = sb_count(net.valid_sub_hulls);
        for(u32 i = 0; i < num_hulls; ++i)
        {
            edge** edge_strips = nullptr;
            sb_push(edge_strips, nullptr);

            curb(edge_strips, net.valid_sub_hulls[i]);

            pen::renderer_new_frame();

            Str f;
            f.appendf("cell_%i_%i", cell, i);
            mesh_from_strips(f.c_str(), edge_strips);

            auto default_material = get_material_resource(PEN_HASH("default_material"));
            auto geom = get_geometry_resource(PEN_HASH(f.c_str()));
            u32  new_prim = get_new_entity(scene);
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

            pen::thread_sleep_ms(1);
        }
    }

    void generate_road_mesh(const road_network& net, ecs_scene* scene, u32 cell)
    {
        vertex_model* verts = nullptr;

        const road_section* road_sections[] = {
            net.inner_road_sections,
            net.outer_road_sections
        };

        for(u32 type = 0; type < 2; ++type)
        {
            u32 num_sections = sb_count(road_sections[type]);
            for(u32 i = 0; i < num_sections; ++i)
            {
                const auto& section = road_sections[type][i];

                vertex_model v[6];
                v[0].pos.xyz = section.entry.p1;
                v[1].pos.xyz = section.exit.p1;
                v[2].pos.xyz = section.entry.p2;

                v[3].pos.xyz = section.entry.p2;
                v[4].pos.xyz = section.exit.p1;
                v[5].pos.xyz = section.exit.p2;

                for(u32 vi = 0; vi < 6; ++vi)
                {
                    v[vi].pos.w = 1.0f;
                    v[vi].normal = vec4f::unit_y();
                    v[vi].tangent = vec4f::unit_x();
                    v[vi].bitangent = vec4f::unit_z();

                    sb_push(verts, v[vi]);
                }
            }
        }

        Str f;
        f.appendf("road_%i", cell);
        create_primitive_resource_faceted(f, verts, sb_count(verts));

        auto default_material = get_material_resource(PEN_HASH("default_material"));
        auto geom = get_geometry_resource(PEN_HASH(f.c_str()));
        u32  new_prim = get_new_entity(scene);
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

        pen::thread_sleep_ms(1);
        
    }

    road_network generate_road_network(const voronoi_map* voronoi, u32 cell, const road_network_params& params)
    {
        road_network net;
        net.params = params;

        // jvc build from cell
        const jcv_site* sites = jcv_diagram_get_sites( &voronoi->diagram );
        const jcv_site* site = &sites[cell];
        const jcv_graphedge* jvce = site->edges;
                
        // edge points from jvc come in different winding order
        vec3f* edge_points = nullptr;
        while( jvce )
        {
            vec3f p2 = jcv_to_vec(jvce->pos[1]);
            jvce = jvce->next;

            sb_push(edge_points, p2);
        }
        u32 nep = sb_count(edge_points);

        vec3f* inset_edge_points = nullptr;
        vec3f* cell_perps = nullptr;
        
        vec3f prev_line[2];
        vec3f first_line[2];
        
        f32 inset = 0.5f;
        f32 major_inset = 0.75f;
        
        // get outer hull correctly wound and inset w/ overlaps
        vec3f* outer_hull = nullptr;
        vec3f* inset_overlapped = nullptr;
        for(s32 i = nep-1; i >= 0; i--)
        {
            s32 n = i-1 < 0 ? nep-1 : i-1;
            
            vec3f p1 = edge_points[i];
            vec3f p2 = edge_points[n];
                        
            vec3f vl = normalised(vec3f(p2-p1));
            
            vec3f perp = cross(vl, vec3f::unit_y());
            
            perp *= major_inset;
            
            vec3f inset_edge0 = p1 - perp;
            vec3f inset_edge1 = p2 - perp;
            
            sb_push(inset_overlapped, inset_edge0);
            sb_push(inset_overlapped, inset_edge1);
            sb_push(net.outer_hull, p1);

            sb_push(cell_perps, cross(vl, vec3f::unit_y()));
        }
        
        // find intersection points of the overlapped inset edges
        vec3f* intersection_points = nullptr;
        u32 no = sb_count(inset_overlapped);
        for (u32 i = 0; i < no; i+=2)
        {
            for (u32 j = 0; j < no; j += 2)
            {
                if (i == j)
                    continue;

                vec3f p0 = inset_overlapped[i];
                vec3f p1 = inset_overlapped[i + 1];

                vec3f pn0 = inset_overlapped[j];
                vec3f pn1 = inset_overlapped[j + 1];

                if (mag(p1 - p0) < major_inset)
                    continue;

                if (mag(pn1 - pn0) < major_inset)
                    continue;

                // extend slightly to catch difficult intersections
                vec3f xt = normalised(pn1 - pn0);
                pn0 -= xt * inset;
                pn1 += xt * inset;

                vec3f ip;
                if (maths::line_vs_line(p0, p1, pn0, pn1, ip))
                {
                    sb_push(intersection_points, ip);
                }
            }
        }

        if (!intersection_points)
            PEN_ASSERT(0);

        // make a clean tidy inset hull
        convex_hull_from_points(net.inset_hull, intersection_points, sb_count(intersection_points));
        net.inset_hull = convex_hull_tidy(net.inset_hull, sb_count(net.inset_hull), inset);

        // subdidive and build roads
        get_hull_subdivision_extents(net);
        subdivide_as_grid(net);
        find_junctions(net);

        generate_curb_mesh(net, scene, cell);
        generate_road_mesh(net, scene, cell);

        return net;
    }

    #define if_dbg_checkbox(name, def) static bool name = def; ImGui::Checkbox(#name, &name); if(name)

    void debug_render_road_section(const road_section& section, const road_section* sections)
    {
        vec3f* points = nullptr;
        sb_push(points, section.exit.p1);
        sb_push(points, section.exit.p2);
        sb_push(points, section.entry.p1);
        sb_push(points, section.entry.p2);

        vec3f* vsection = nullptr;
        convex_hull_from_points(vsection, points, sb_count(points));
        //draw_convex_hull(vsection, sb_count(vsection));

        sb_free(vsection);
        sb_free(points);

        add_line(section.exit.p1, section.exit.p2, vec4f::red());

        if(section.destinations)
        {
            u32 num_destinations = sb_count(section.destinations);
            for(u32 i = 0; i < num_destinations; ++i)
            {
                auto dest = sections[section.destinations[i]];
                vec3f start = section.exit.p1 + ((section.exit.p2 - section.exit.p1) * 0.5f);
                vec3f end = dest.entry.p1 + ((dest.entry.p2 - dest.entry.p1) * 0.5f);

                add_line(start, end, vec4f::blue());
            }
        }
    }

    void debug_render_road_sections(const road_network& net)
    {
        u32 num_outer_sections = sb_count(net.outer_road_sections);
        for(u32 i = 0; i < num_outer_sections; ++i)
        {
            debug_render_road_section(net.outer_road_sections[i], net.outer_road_sections);
        }

        u32 num_inner_sections = sb_count(net.inner_road_sections);
        for(u32 i = 0; i < num_inner_sections; ++i)
        {
            debug_render_road_section(net.inner_road_sections[i], net.inner_road_sections);
        }
    }

    void debug_render_road_network(const road_network& net)
    {
        if_dbg_checkbox(outer_hull, true)
            draw_convex_hull(net.outer_hull, sb_count(net.outer_hull), vec4f::yellow());
        
        if_dbg_checkbox(inner_hull, true)
            draw_convex_hull(net.inset_hull, sb_count(net.inset_hull), vec4f::orange());

        if_dbg_checkbox(extent_axis, false)
            for(u32 i = 0; i < 2; ++i)
                add_line(net.extent_axis_points[i].p1, net.extent_axis_points[i].p2, vec4f::red());

        debug_render_road_sections(net);
    }

    road_network* nets;
    void generate()
    {
        road_network_params rp;
        rp.inset = 0.5f;
        rp.major_inset = 0.75f;
        rp.subdiv_size = 4.0f;

        for(u32 i = 0; i < voronoi->diagram.numsites; ++i)
        {
            road_network net = generate_road_network(voronoi, i, rp);
            sb_push(nets, net);
            PEN_LOG("generated %i", i);
        }
    }
    
    int on_update(f32 dt)
    {
        if (!g_debug)
            return 0;

        static s32 test_single = 3;
        ImGui::InputInt("Test Single", (s32*)&test_single);
        
        if(test_single != -1)
        {
            debug_render_road_network(nets[test_single]);
        }
        else
        {
            u32 num_nets = sb_count(nets);
            for(u32 i = 0; i < num_nets; ++i)
                debug_render_road_network(nets[i]);
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


