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

struct live_lib
{
    u32                 box_start = 0;
    u32                 box_end;
    camera              cull_cam;
    ecs_scene*          scene;
    
    void init(live_context* ctx)
    {
        scene = ctx->scene;
        
        ecs::clear_scene(scene);
                
        return;
        
        // primitive resources
        material_resource* default_material = get_material_resource(PEN_HASH("default_material"));
    
        const c8* primitive_names[] = {
            "tetrahedron",
            "cube",
            "octahedron",
            "dodecahedron",
            "icosahedron",
            "sphere",
            "cone",
            "capsule",
            "cylinder",
            "torus"
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
        scene->lights[light].flags = e_light_flags::shadow_map;
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
    }
    
    struct edge
    {
        vec3f start;
        vec3f end;
    };
    edge* s_edges = nullptr;
    
    void extrude(u32 edge, vec3f dir)
    {
        struct edge ne;
        ne.start = s_edges[edge].start + dir;
        ne.end = s_edges[edge].end + dir;
        
        sb_push(s_edges, ne);
    }
    
    int on_load(live_context* ctx)
    {
        init(ctx);
        
        if(s_edges)
        {
            sb_free(s_edges);
            s_edges = nullptr;
        }
        
        edge e;
        e.start = vec3f(0.5f);
        e.end = vec3f(0.5f) + vec3f::unit_x() * 5.0f;
        sb_push(s_edges, e);
        
        extrude(0, vec3f::unit_y() * 10.0f);
        extrude(1, vec3f::unit_y() * 10.0f);
        extrude(2, vec3f::unit_z() * 10.0f);
        extrude(3, vec3f::unit_x() * 10.0f);
        
        return 0;
    }
            
    int on_update(f32 dt)
    {
        u32 ec = sb_count(s_edges);
        for(u32 i = 0; i < ec; ++i)
        {
            add_line(s_edges[i].start, s_edges[i].end, vec4f::magenta());
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


