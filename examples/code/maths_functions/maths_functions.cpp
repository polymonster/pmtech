#include "../example_common.h"
#include "maths/util.h"

using namespace put;
using namespace ecs;

// TODO:
// more quilez functions
// SAT for aabb / obb?
// publish updated version (wasm)
//

vec3f furthest_point(const vec3f& dir, const std::vector<vec3f>& vertices)
{
    f32 fd = -FLT_MAX;
    vec3f fv = vertices[0];
    
    for(auto& v : vertices)
    {
        f32 d = dot(dir, v);
        if(d > fd)
        {
            fv = v;
            fd = d;
        }
    }
    
    return fv;
}

vec3f support_function(const std::vector<vec3f>& convex0, const std::vector<vec3f>& convex1, vec3f dir)
{
    vec3f fp0 = furthest_point(dir, convex0);
    vec3f fp1 = furthest_point(-dir, convex1);
    vec3f s = fp0 - fp1;
    
    return s;
}

vec3f triple_product(const vec3f& a, const vec3f& b, const vec3f& c)
{
    return cross(cross(a, b), c);
}

vec3f support_function_debug(const std::vector<vec3f>& convex0, const std::vector<vec3f>& convex1, vec3f dir, bool render_debug)
{
    vec3f fp0 = furthest_point(dir, convex0);
    vec3f fp1 = furthest_point(-dir, convex1);
    vec3f s = fp0 - fp1;
    
    if(render_debug)
    {
        dbg::add_point(fp0, 0.3f, vec4f::cyan());
        dbg::add_point(fp1, 0.3f, vec4f::yellow());
    }
    
    return s;
}

bool handle_simplex_3d(std::vector<vec3f>& simplex, vec3f& dir, bool render_debug)
{
    if(simplex.size() == 2)
    {
        if(render_debug)
        {
            dbg::add_line(simplex[0], simplex[1]);
        }
        
        vec3f a = simplex[1];
        vec3f b = simplex[0];
        
        vec3f ab = normalize(b - a);
        vec3f ao = normalize(-a);
        
        dir = triple_product(ab, ao, ab);
        
        return false;
    }
    else if(simplex.size() == 3)
    {
        if(render_debug)
        {
            dbg::add_line(simplex[0], simplex[1]);
            dbg::add_line(simplex[1], simplex[2]);
            dbg::add_line(simplex[2], simplex[0]);
        }
        
        vec3f a = simplex[2];
        vec3f b = simplex[1];
        vec3f c = simplex[0];
        
        vec3f ab = normalize(b - a);
        vec3f ac = normalize(c - a);
        vec3f ao = normalize(-a);
        
        dir = cross(ac, ab);

        // ensure it points toward the origin
        if(dot(dir, ao) < 0)
        {
            dir *= -1.0f;
        }

        return false;
    }
    else if(simplex.size() == 4)
    {
        vec3f a = simplex[3];
        vec3f b = simplex[2];
        vec3f c = simplex[1];
        vec3f d = simplex[0];
        
        vec3f centre = (a+b+c+d) * 0.25f;
        
        vec3f ab = normalize(b - a);
        vec3f ac = normalize(c - a);
        vec3f ad = normalize(d - a);
        vec3f ao = normalize(-a);
        
        vec3f abac = cross(ab, ac);
        vec3f acad = cross(ac, ad);
        vec3f adab = cross(ad, ab);
        
        // flip the normals so they always face outward
        vec3f centre_abc = (a + b + c) / 3.0f;
        vec3f centre_acd = (a + c + d) / 3.0f;
        vec3f centre_adb = (a + d + b) / 3.0f;
        
        if(dot(centre - centre_abc, abac) > 0.0f)
        {
            abac *= -1.0f;
        }
        
        if(dot(centre - centre_acd, acad) > 0.0f)
        {
            acad *= -1.0f;
        }
        
        if(dot(centre - centre_adb, adab) > 0.0f)
        {
            adab *= -1.0f;
        }
        
        if(render_debug)
        {
            // identifier points
            dbg::add_point(d, 0.3f, vec4f::red());
            dbg::add_point(c, 0.3f, vec4f::green());
            dbg::add_point(b, 0.3f, vec4f::blue());
            dbg::add_point(a, 0.3f, vec4f::cyan());
            
            // tetra itself
            dbg::add_line(simplex[0], simplex[1]);
            dbg::add_line(simplex[1], simplex[2]);
            dbg::add_line(simplex[2], simplex[0]);
            dbg::add_line(simplex[0], simplex[3]);
            dbg::add_line(simplex[1], simplex[3]);
            dbg::add_line(simplex[2], simplex[3]);
            
            dbg::add_line(centre_abc, centre_abc + normalize(abac), vec4f::orange());
            dbg::add_line(centre_acd, centre_acd + normalize(acad), vec4f::red());
            dbg::add_line(centre_adb, centre_adb + normalize(adab), vec4f::yellow());
            
            //
            dbg::add_line(vec3f::zero(), ao, vec4f::green());
            dbg::add_line(vec3f::zero(), dir, vec4f::yellow());
        }
        
        constexpr f32 k_epsilon = 0.0f;
        
        if(dot(abac, ao) > k_epsilon) // orange
        {
            // erase d
            simplex.erase(simplex.begin() + 0);
            dir = abac;
            
            return false;
        }
        else if(dot(acad, ao) > k_epsilon) // yellow
        {
            // erase c
            simplex.erase(simplex.begin() + 1);
            dir = acad;
            return false;
        }
        else if(dot(adab, ao) > k_epsilon) // red
        {
            // erase b
            simplex.erase(simplex.begin() + 2);
            dir = adab;
            return false;
        }
        
        return true;
    }
    
    // we shouldnt hit this case, we should always have 2 or 3 points in the simplex
    assert(0);
    return false;
}

u32 gjk_3d(const std::vector<vec3f>& convex0, const std::vector<vec3f>& convex1, u32 debug_depth)
{
    // implemented following details in this insightful video: https://www.youtube.com/watch?v=ajv46BSqcK4
    
    // starting direction vector
    vec3f dir = normalize(maths::get_convex_hull_centre(convex0) - maths::get_convex_hull_centre(convex1));
    vec3f support = support_function(convex0, convex1, dir);
    
    std::vector<vec3f> simplex;
    simplex.push_back(support);
    
    u32 depth = 0;
    u32 loops = 0;
    dir = normalize(-support);

    f32 smallest = FLT_MAX;
    for(;;)
    {
        bool render_debug = depth == debug_depth;
        
        vec3f support_dir = dir;
        if(loops > 0)
        {
            support_dir = -support_dir;
        }
        
        vec3f a = support_function_debug(convex0, convex1, support_dir, render_debug);
        
        if(render_debug)
        {
            dbg::add_line(vec3f::zero(), dir, vec4f::cyan());
            dbg::add_line(vec3f::zero(), a, vec4f::magenta());
        }
        
        f32 ndp = dot(normalize(a), normalize(dir));
        f32 dp = dot(a, dir);
        
        if(dp < 0.0f)
        {
            dbg::add_point(vec3f::zero(), 2.0f, vec4f::red());
            return 0;
        }
        simplex.push_back(a);
                
        if(ndp < smallest)
        {
            loops = 0;
            smallest = ndp;
        }
        else if(ndp == smallest)
        {
            loops++;
        }
        
        if(handle_simplex_3d(simplex, dir, render_debug))
        {
            dbg::add_point(vec3f::zero(), 2.0f, vec4f::magenta());
            return 1;
        }
        
        ++depth;
        if(depth > 32)
        {
            dbg::add_point(vec3f::zero(), 2.0f, vec4f::blue());
            ImGui::Text("Smallest: %f, Loops: %u", smallest, loops);
            return 2;
        }
    }
}

u32 obb_vs_obb_debug(const mat4f& obb0, const mat4f& obb1, u32 debug_depth)
{
    // this function is for convenience, you can extract vertices and pass to gjk_3d yourself
    static const vec3f corners[8] = {
        vec3f(-1.0f, -1.0f, -1.0f),
        vec3f( 1.0f, -1.0f, -1.0f),
        vec3f( 1.0f,  1.0f, -1.0f),
        vec3f(-1.0f,  1.0f, -1.0f),
        vec3f(-1.0f, -1.0f,  1.0f),
        vec3f( 1.0f, -1.0f,  1.0f),
        vec3f( 1.0f,  1.0f,  1.0f),
        vec3f(-1.0f,  1.0f,  1.0f),
    };
    
    std::vector<vec3f> verts0;
    std::vector<vec3f> verts1;
        
    for(u32 i = 0; i < 8; ++i)
    {
        verts0.push_back(obb0.transform_vector(corners[i]));
        verts1.push_back(obb1.transform_vector(corners[i]));
    }
    
    return gjk_3d(verts0, verts1, debug_depth);
}

u32 aabb_vs_obb_debug(const vec3f& aabb_min, const vec3f& aabb_max, const mat4f& obb, u32 debug_depth)
{
    // this function is for convenience, you can extract vertices and pass to gjk_3d yourself
    static const vec3f corners[8] = {
        vec3f(-1.0f, -1.0f, -1.0f),
        vec3f( 1.0f, -1.0f, -1.0f),
        vec3f( 1.0f,  1.0f, -1.0f),
        vec3f(-1.0f,  1.0f, -1.0f),
        vec3f(-1.0f, -1.0f,  1.0f),
        vec3f( 1.0f, -1.0f,  1.0f),
        vec3f( 1.0f,  1.0f,  1.0f),
        vec3f(-1.0f,  1.0f,  1.0f),
    };
    
    std::vector<vec3f> verts0 = {
        aabb_min,
        vec3f(aabb_min.x, aabb_min.y, aabb_max.z),
        vec3f(aabb_max.x, aabb_min.y, aabb_min.z),
        vec3f(aabb_max.x, aabb_min.y, aabb_max.z),
        vec3f(aabb_min.x, aabb_max.y, aabb_min.z),
        vec3f(aabb_min.x, aabb_max.y, aabb_max.z),
        vec3f(aabb_max.x, aabb_max.y, aabb_min.z),
        aabb_max
    };
    
    std::vector<vec3f> verts1;
    for(u32 i = 0; i < 8; ++i)
    {
        verts1.push_back(obb.transform_vector(corners[i]));
    }
    
    return gjk_3d(verts0, verts1, debug_depth);
}

#define EASING_FUNC(NAME) \
{ \
    f32 t = 0.0f; \
    for(u32 i = 0; i < 32; ++i) \
    { \
        points[i] = NAME(t); \
        t += 1.0f / 32.0f; \
    } \
    ImGui::PlotLines(#NAME, points, 32); \
}

namespace
{
    void print_test_case_plane(const vec3f& x, const vec3f& n)
    {
        PEN_LOG("\tvec3f x = {(f32)%f, (f32)%f, (f32)%f};", x.x, x.y, x.z);
        PEN_LOG("\tvec3f n = {(f32)%f, (f32)%f, (f32)%f};", n.x, n.y, n.z);
    }
    
    void print_test_case_ray(const vec3f& r0, const vec3f& rv)
    {
        PEN_LOG("\tvec3f ip = {(f32)0.0, (f32)0.0, (f32)0.0};");
        PEN_LOG("\tvec3f r0 = {(f32)%f, (f32)%f, (f32)%f};", r0.x, r0.y, r0.z);
        PEN_LOG("\tvec3f rv = {(f32)%f, (f32)%f, (f32)%f};", rv.x, rv.y, rv.z);
    }
    
    void print_test_case_line_0(const vec3f& l0, const vec3f& l1)
    {
        PEN_LOG("\tvec3f l00 = {(f32)%f, (f32)%f, (f32)%f};", l0.x, l0.y, l0.z);
        PEN_LOG("\tvec3f l01 = {(f32)%f, (f32)%f, (f32)%f};", l1.x, l1.y, l1.z);
    }
    
    void print_test_case_line_1(const vec3f& l0, const vec3f& l1)
    {
        PEN_LOG("\tvec3f l10 = {(f32)%f, (f32)%f, (f32)%f};", l0.x, l0.y, l0.z);
        PEN_LOG("\tvec3f l11 = {(f32)%f, (f32)%f, (f32)%f};", l1.x, l1.y, l1.z);
    }
    
    void print_test_case_sphere(const vec3f& sp, f32 r)
    {
        PEN_LOG("\tvec3f sp = {(f32)%f, (f32)%f, (f32)%f};", sp.x, sp.y, sp.z);
        PEN_LOG("\tf32 sr = (f32)%f;", r);
    }
    
    void print_test_case_triangle(const vec3f& t0, const vec3f& t1, const vec3f& t2)
    {
        PEN_LOG("\tvec3f t0 = {(f32)%f, (f32)%f, (f32)%f};", t0.x, t0.y, t0.z);
        PEN_LOG("\tvec3f t1 = {(f32)%f, (f32)%f, (f32)%f};", t1.x, t1.y, t1.z);
        PEN_LOG("\tvec3f t2 = {(f32)%f, (f32)%f, (f32)%f};", t2.x, t2.y, t2.z);
    }
    
    void print_test_case_point(const vec3f& p)
    {
        PEN_LOG("\tvec3f p = {(f32)%f, (f32)%f, (f32)%f};", p.x, p.y, p.z);
    }

    void print_test_case_screen_point(const vec3f& p)
    {
        PEN_LOG("\tvec3f sp = {(f32)%f, (f32)%f, (f32)%f};", p.x, p.y, p.z);
    }

    void print_test_case_viewport(const vec2i& vp)
    {
        PEN_LOG("\tvec2i vp = {(s32)%i, (s32)%i};", vp.x, vp.y);
    }

    void print_test_case_view_proj(const mat4f& m)
    {
        PEN_LOG("\tmat4f view_proj = {(f32)%f, (f32)%f, (f32)%f, (f32)%f,\n\t(f32)%f, (f32)%f, (f32)%f, (f32)%f,\n\t(f32)%f, (f32)%f, (f32)%f, (f32)%f,\n\t(f32)%f, (f32)%f, (f32)%f, (f32)%f};",
            m.m[0], m.m[1], m.m[2], m.m[3],
            m.m[4], m.m[5], m.m[6], m.m[7],
            m.m[8], m.m[9], m.m[10], m.m[11],
            m.m[12], m.m[13], m.m[14], m.m[15]
        );
    }

    void print_test_case_obb(const mat4f& m)
    {
        PEN_LOG("\tmat4f obb = {(f32)%f, (f32)%f, (f32)%f, (f32)%f,\n\t(f32)%f, (f32)%f, (f32)%f, (f32)%f,\n\t(f32)%f, (f32)%f, (f32)%f, (f32)%f,\n\t(f32)%f, (f32)%f, (f32)%f, (f32)%f};",
            m.m[0], m.m[1], m.m[2], m.m[3],
            m.m[4], m.m[5], m.m[6], m.m[7],
            m.m[8], m.m[9], m.m[10], m.m[11],
            m.m[12], m.m[13], m.m[14], m.m[15]
        );
    }

    void print_test_case_obb2(const mat4f& m)
    {
        PEN_LOG("\tmat4f obb2 = {(f32)%f, (f32)%f, (f32)%f, (f32)%f,\n\t(f32)%f, (f32)%f, (f32)%f, (f32)%f,\n\t(f32)%f, (f32)%f, (f32)%f, (f32)%f,\n\t(f32)%f, (f32)%f, (f32)%f, (f32)%f};",
            m.m[0], m.m[1], m.m[2], m.m[3],
            m.m[4], m.m[5], m.m[6], m.m[7],
            m.m[8], m.m[9], m.m[10], m.m[11],
            m.m[12], m.m[13], m.m[14], m.m[15]
        );
    }

    void print_test_case_aabb(const vec3f& aabb_min, const vec3f& aabb_max)
    {
        PEN_LOG("\tvec3f aabb_min = {(f32)%f, (f32)%f, (f32)%f};", aabb_min.x, aabb_min.y, aabb_min.z);
        PEN_LOG("\tvec3f aabb_max = {(f32)%f, (f32)%f, (f32)%f};", aabb_max.x, aabb_max.y, aabb_max.z);
    }
    
    void print_test_case_intersection_point(bool intersect, const vec3f& ip)
    {
        // intersection and point
        PEN_LOG("\tREQUIRE(i == bool(%i));", intersect);
        if(intersect)
        {
            PEN_LOG("\tREQUIRE(require_func(ip, {(f32)%f, (f32)%f, (f32)%f}));", ip.x, ip.y, ip.z);
        }
    }
    
    void print_test_case_closest_point(const vec3f& cp)
    {
        // closest point
        PEN_LOG("\tREQUIRE(require_func(cpp, {(f32)%f, (f32)%f, (f32)%f}));", cp.x, cp.y, cp.z);
    }
    
    void print_test_case_classification(u32 c)
    {
        // classification
        PEN_LOG("\tREQUIRE(c == %i);", c);
    }
    
    void print_test_case_distance(f32 d)
    {
        // classification
        PEN_LOG("\tREQUIRE(require_func(dd, %f));", d);
    }
    
    void print_test_case_capsule(const vec3f& cp0, const vec3f& cp1, f32 cr)
    {
        PEN_LOG("\tvec3f cp0 = {(f32)%f, (f32)%f, (f32)%f};", cp0.x, cp0.y, cp0.z);
        PEN_LOG("\tvec3f cp1 = {(f32)%f, (f32)%f, (f32)%f};", cp1.x, cp1.y, cp1.z);
        PEN_LOG("\tf32 cr = (f32)%f;", cr);
    }

    void print_test_case_capsule_2(const vec3f& cp0, const vec3f& cp1, f32 cr)
    {
        PEN_LOG("\tvec3f cp2 = {(f32)%f, (f32)%f, (f32)%f};", cp0.x, cp0.y, cp0.z);
        PEN_LOG("\tvec3f cp3 = {(f32)%f, (f32)%f, (f32)%f};", cp1.x, cp1.y, cp1.z);
        PEN_LOG("\tf32 cr1 = (f32)%f;", cr);
    }
    
    void print_test_case_cone(const vec3f& cp, const vec3f& cv, f32 h, f32 r)
    {
        PEN_LOG("\tvec3f cp = {(f32)%f, (f32)%f, (f32)%f};", cp.x, cp.y, cp.z);
        PEN_LOG("\tvec3f cv = {(f32)%f, (f32)%f, (f32)%f};", cv.x, cv.y, cv.z);
        PEN_LOG("\tf32 h = (f32)%f;", h);
        PEN_LOG("\tf32 r = (f32)%f;", r);
    }
    
    void print_test_case_cylinder(const vec3f& cy0, const vec3f& cy1, f32 cyr)
    {
        PEN_LOG("\tvec3f cy0 = {(f32)%f, (f32)%f, (f32)%f};", cy0.x, cy0.y, cy0.z);
        PEN_LOG("\tvec3f cy1 = {(f32)%f, (f32)%f, (f32)%f};", cy1.x, cy1.y, cy1.z);
        PEN_LOG("\tf32 cyr = (f32)%f;", cyr);
    }

    void print_test_case_convex_hull(const std::vector<vec2f>& hull)
    {
        PEN_LOG("\tstd::vector<vec2f> hull = {");
        for(auto& v : hull)
        {
            PEN_LOG("\t\t{(f32)%f, (f32)%f},", v.x, v.y);
        }
        PEN_LOG("\t};");
    }

    void print_test_case_convex_hull2(const std::vector<vec2f>& hull)
    {
        PEN_LOG("\tstd::vector<vec2f> hull2 = {");
        for(auto& v : hull)
        {
            PEN_LOG("\t\t{(f32)%f, (f32)%f},", v.x, v.y);
        }
        PEN_LOG("\t};");
    }
    
    void print_test_case_overlap(bool overlap)
    {
        // overlap
        PEN_LOG("\tREQUIRE(overlap == bool(%i));", overlap);
    }
}

namespace pen
{
    pen_creation_params pen_entry(int argc, char** argv)
    {
        pen::pen_creation_params p;
        p.window_width = 1280;
        p.window_height = 720;
        p.window_title = "maths_functions";
        p.window_sample_count = 4;
        p.user_thread_function = user_setup;
        p.flags = pen::e_pen_create_flags::renderer;
        return p;
    }
} // namespace pen

// Small structs for debug maths rendering and test primitives
struct debug_ray
{
    u32   node;
    vec3f origin;
    vec3f direction;
};

struct debug_triangle
{
    u32   node;
    vec3f t0;
    vec3f t1;
    vec3f t2;
};

struct debug_convex_hull
{
    std::vector<vec2f> vertices;
};

struct debug_line
{
    u32   node;
    vec3f l1;
    vec3f l2;
};

struct debug_plane
{
    u32   node;
    vec3f point;
    vec3f normal;
};

struct debug_aabb
{
    u32   node;
    vec3f min;
    vec3f max;
};

struct debug_obb
{
    u32 node;
};

struct debug_cone
{
    u32 node;
};

struct debug_sphere
{
    u32   node;
    vec3f pos;
    f32   radius;
};

struct debug_capsule
{
    u32   nodes[3] = { 0 };
    f32   radius;
    vec3f cp1;
    vec3f cp2;
};

struct debug_cylinder
{
    u32   node;
    f32   radius;
    vec3f cp1;
    vec3f cp2;
};

struct debug_point
{
    u32   node;
    vec3f point;
};

struct debug_extents
{
    vec3f min;
    vec3f max;
};

material_resource* constant_colour_material = new material_resource;

const c8* classifications[]{
    "Intersects",
    "Behind",
    "Infront",
};

const vec4f classification_colours[] = {
    vec4f::red(),
    vec4f::cyan(),
    vec4f::green()
};

// Randomise vector in range of extents
vec3f random_vec_range(const debug_extents& extents)
{
    vec3f range = extents.max - extents.min;
    range *= 100.0f;

    f32 rx = range.x == 0.0 ? 0.0 : rand() % (u32)range.x;
    f32 ry = range.y == 0.0 ? 0.0 : rand() % (u32)range.y;
    f32 rz = range.z == 0.0 ? 0.0 : rand() % (u32)range.z;

    vec3f random = vec3f(rx, ry, rz);
    random /= 100.0f;

    return extents.min + random;
}

// Spawn a randomised point within range of extents
void add_debug_point(const debug_extents& extents, ecs_scene* scene, debug_point& point)
{
    u32 node = ecs::get_new_entity(scene);
    scene->names[node] = "point";
    scene->transforms[node].translation = random_vec_range(extents);
    scene->entities[node] |= e_cmp::transform;
    scene->parents[node] = node;

    point.node = node;
    point.point = scene->transforms[node].translation;
}

// Spawn a random plane which starts at a point within extents
void add_debug_plane(const debug_extents& extents, ecs_scene* scene, debug_plane& plane)
{
    u32 node = ecs::get_new_entity(scene);
    scene->names[node] = "plane";
    scene->transforms[node].translation = random_vec_range(extents);
    scene->entities[node] |= e_cmp::transform;
    scene->parents[node] = node;

    plane.normal = normalize(random_vec_range({-vec3f::one(), vec3f::one()}));
    plane.point = scene->transforms[node].translation;
}

// Spawn a random ray which contains point within extents
void add_debug_ray(const debug_extents& extents, ecs_scene* scene, debug_ray& ray)
{
    u32 node = ecs::get_new_entity(scene);
    scene->names[node] = "ray";
    scene->transforms[node].translation = random_vec_range(extents);
    scene->entities[node] |= e_cmp::transform;
    scene->parents[node] = node;

    ray.direction = normalize(random_vec_range({-vec3f::one(), vec3f::one()}));
    ray.origin = scene->transforms[node].translation;
}

// Spawn a random ray which contains point within extents
void add_debug_ray_targeted(const debug_extents& extents, ecs_scene* scene, debug_ray& ray)
{
    vec3f target = random_vec_range(extents);
    vec3f dir = normalize(random_vec_range({-vec3f::one(), vec3f::one()}));
    
    u32 node = ecs::get_new_entity(scene);
    scene->names[node] = "ray";
    scene->transforms[node].translation = target + dir * mag(extents.max);
    scene->entities[node] |= e_cmp::transform;
    scene->parents[node] = node;

    ray.direction = -dir;
    ray.origin = scene->transforms[node].translation;
}

void add_debug_ray2(const debug_extents& extents, ecs_scene* scene, debug_ray& ray)
{
    u32 node = ecs::get_new_entity(scene);
    scene->names[node] = "ray";
    scene->transforms[node].translation = random_vec_range(extents);
    scene->entities[node] |= e_cmp::transform;
    scene->parents[node] = node;

    ray.direction = normalize(random_vec_range({vec3f::zero(), vec3f::one()}));
    ray.origin = scene->transforms[node].translation;
}

// Spawn a random line which contains both points within extents
void add_debug_line(const debug_extents& extents, ecs_scene* scene, debug_line& line)
{
    u32 node = ecs::get_new_entity(scene);
    scene->names[node] = "line";
    scene->transforms[node].translation = random_vec_range(extents);
    scene->entities[node] |= e_cmp::transform;
    scene->parents[node] = node;

    line.l1 = random_vec_range(extents);
    line.l2 = random_vec_range(extents);
}

// Spawn a random AABB which contains centre within extents and size within extents
void add_debug_aabb(const debug_extents& extents, ecs_scene* scene, debug_aabb& aabb)
{
    u32 node = ecs::get_new_entity(scene);
    scene->names[node] = "aabb";
    scene->transforms[node].translation = random_vec_range(extents);
    scene->entities[node] |= e_cmp::transform;
    scene->parents[node] = node;

    vec3f size = fabs(random_vec_range(extents));

    aabb.min = scene->transforms[node].translation - size;
    aabb.max = scene->transforms[node].translation + size;
}

// Add debug sphere with randon radius within range extents and at random position within extents
void add_debug_sphere(const debug_extents& extents, ecs_scene* scene, debug_sphere& sphere)
{
    geometry_resource* sphere_res = get_geometry_resource(PEN_HASH("sphere"));

    vec3f size = fabs(random_vec_range(extents));

    u32 node = ecs::get_new_entity(scene);
    scene->names[node] = "sphere";

    scene->transforms[node].rotation = quat();
    scene->transforms[node].scale = vec3f(size.x);
    scene->transforms[node].translation = random_vec_range(extents);

    scene->entities[node] |= e_cmp::transform;
    scene->parents[node] = node;

    instantiate_geometry(sphere_res, scene, node);
    instantiate_material(constant_colour_material, scene, node);
    instantiate_model_cbuffer(scene, node);

    sphere.pos = scene->transforms[node].translation;
    sphere.radius = size.x;
    sphere.node = node;
}

void add_debug_capsule(const debug_extents& extents, ecs_scene* scene, debug_capsule& capsule, bool ortho = false)
{
    geometry_resource* sphere_res = get_geometry_resource(PEN_HASH("sphere"));
    geometry_resource* cyl_res = get_geometry_resource(PEN_HASH("cylinder"));

    vec3f size = fabs(random_vec_range(extents));
    size.y = std::max(size.y, size.x);
    
    vec3f pos = random_vec_range(extents);
    
    vec3f rr = random_vec_range(extents);
    if(ortho)
    {
        rr = vec3f::zero();
    }
    
    quat q; mat4 rot_mat;
    q.euler_angles(rr.x, rr.y, rr.z);
    q.get_matrix(rot_mat);
    
    vec3f axis = rot_mat.get_column(1).xyz * size.y;
    
    
    capsule.cp1 = pos - axis;
    capsule.cp2 = pos + axis;
    capsule.radius = size.x;
    
    // sp1
    u32 sp1 = ecs::get_new_entity(scene);
    scene->names[sp1] = "sphere1";
    
    scene->transforms[sp1].rotation = quat();
    scene->transforms[sp1].scale = vec3f(size.x, size.x, size.x);
    scene->transforms[sp1].translation = capsule.cp1;

    scene->entities[sp1] |= e_cmp::transform;
    scene->parents[sp1] = sp1;

    instantiate_geometry(sphere_res, scene, sp1);
    instantiate_material(constant_colour_material, scene, sp1);
    instantiate_model_cbuffer(scene, sp1);
    
    // sp2
    u32 sp2 = ecs::get_new_entity(scene);
    scene->names[sp2] = "sphere2";
    
    scene->transforms[sp2].rotation = quat();
    scene->transforms[sp2].scale = vec3f(size.x, size.x, size.x);
    scene->transforms[sp2].translation = capsule.cp2;

    scene->entities[sp2] |= e_cmp::transform;
    scene->parents[sp2] = sp2;

    instantiate_geometry(sphere_res, scene, sp2);
    instantiate_material(constant_colour_material, scene, sp2);
    instantiate_model_cbuffer(scene, sp2);
    
    // cyl
    u32 cylinder = ecs::get_new_entity(scene);
    scene->names[cylinder] = "cylinder";
    
    scene->transforms[cylinder].rotation = quat();
    scene->transforms[cylinder].rotation.euler_angles(rr.x, rr.y, rr.z);
    
    scene->transforms[cylinder].scale = vec3f(size.x, size.y, size.x);
    scene->transforms[cylinder].translation = pos;

    scene->entities[cylinder] |= e_cmp::transform;
    scene->parents[cylinder] = cylinder;

    instantiate_geometry(cyl_res, scene, cylinder);
    instantiate_material(constant_colour_material, scene, cylinder);
    instantiate_model_cbuffer(scene, cylinder);
    
    // set nodes
    capsule.nodes[0] = sp1;
    capsule.nodes[1] = sp2;
    capsule.nodes[2] = cylinder;
}

void add_debug_solid_aabb(const debug_extents& extents, ecs_scene* scene, debug_aabb& aabb)
{
    geometry_resource* cube_res = get_geometry_resource(PEN_HASH("cube"));

    vec3f size = fabs(random_vec_range(extents));

    u32 node = ecs::get_new_entity(scene);
    scene->names[node] = "cube";

    scene->transforms[node].rotation = quat();
    scene->transforms[node].scale = size;
    scene->transforms[node].translation = random_vec_range(extents);

    scene->entities[node] |= e_cmp::transform;
    scene->parents[node] = node;

    instantiate_geometry(cube_res, scene, node);
    instantiate_material(constant_colour_material, scene, node);
    instantiate_model_cbuffer(scene, node);

    aabb.min = scene->transforms[node].translation - size;
    aabb.max = scene->transforms[node].translation + size;
    aabb.node = node;
}

void add_debug_cylinder(const debug_extents& extents, ecs_scene* scene, debug_cylinder& cylinder)
{
    geometry_resource* cyl_res = get_geometry_resource(PEN_HASH("cylinder"));

    vec3f size = fabs(random_vec_range(extents));
    size.y = std::max(size.y, size.x);
    
    vec3f pos = random_vec_range(extents);
    
    cylinder.cp1 = pos - vec3f(0.0f, size.y, 0.0f);
    cylinder.cp2 = pos + vec3f(0.0f, size.y, 0.0f);
    cylinder.radius = size.x;
    
    // cyl
    u32 c = ecs::get_new_entity(scene);
    scene->names[c] = "cylinder";
    
    scene->transforms[c].rotation = quat();
    scene->transforms[c].scale = vec3f(size.x, size.y, size.x);
    scene->transforms[c].translation = pos;

    scene->entities[c] |= e_cmp::transform;
    scene->parents[c] = c;

    instantiate_geometry(cyl_res, scene, c);
    instantiate_material(constant_colour_material, scene, c);
    instantiate_model_cbuffer(scene, c);
    
    cylinder.node = c;
}

void add_debug_obb(const debug_extents& extents, ecs_scene* scene, debug_obb& obb)
{
    u32 node = ecs::get_new_entity(scene);
    scene->names[node] = "obb";
    scene->transforms[node].translation = random_vec_range(extents);

    vec3f rr = random_vec_range(extents);

    scene->transforms[node].rotation.euler_angles(rr.x, rr.y, rr.z);

    scene->transforms[node].scale = fabs(random_vec_range(extents));
    scene->entities[node] |= e_cmp::transform;
    scene->parents[node] = node;

    obb.node = node;
}

void add_debug_solid_obb(const debug_extents& extents, ecs_scene* scene, debug_obb& obb)
{
    geometry_resource* cube_res = get_geometry_resource(PEN_HASH("cube"));

    u32 node = ecs::get_new_entity(scene);
    scene->names[node] = "obb";
    scene->transforms[node].translation = random_vec_range(extents);

    vec3f rr = random_vec_range(extents);

    scene->transforms[node].rotation.euler_angles(rr.x, rr.y, rr.z);

    scene->transforms[node].scale = fabs(random_vec_range(extents));
    scene->entities[node] |= e_cmp::transform;
    scene->parents[node] = node;

    instantiate_geometry(cube_res, scene, node);
    instantiate_material(constant_colour_material, scene, node);
    instantiate_model_cbuffer(scene, node);

    obb.node = node;
}

void add_debug_solid_cone(const debug_extents& extents, ecs_scene* scene, debug_cone& obb)
{
    geometry_resource* cone_res = get_geometry_resource(PEN_HASH("cone"));

    u32 node = ecs::get_new_entity(scene);
    scene->names[node] = "cone";
    scene->transforms[node].translation = random_vec_range(extents);

    vec3f rr = random_vec_range(extents);

    scene->transforms[node].rotation.euler_angles(rr.x, rr.y, rr.z);

    scene->transforms[node].scale = vec3f(fabs(rr.x), fabs(rr.y), fabs(rr.x));
    scene->entities[node] |= e_cmp::transform;
    scene->parents[node] = node;

    instantiate_geometry(cone_res, scene, node);
    instantiate_material(constant_colour_material, scene, node);
    instantiate_model_cbuffer(scene, node);

    obb.node = node;
}

void add_debug_triangle(const debug_extents& extents, ecs_scene* scene, debug_triangle& tri)
{
    u32 node = ecs::get_new_entity(scene);
    scene->names[node] = "triangle";
    scene->transforms[node].translation = random_vec_range(extents);
    scene->entities[node] |= e_cmp::transform;
    scene->parents[node] = node;

    tri.t0 = random_vec_range(extents);
    tri.t1 = random_vec_range(extents);
    tri.t2 = random_vec_range(extents);
}

void add_debug_convex_hull(const debug_extents& extents, ecs_scene* scene, u32 num_verts, debug_convex_hull& convex)
{
    u32 node = ecs::get_new_entity(scene);
    scene->names[node] = "triangle";
    scene->transforms[node].translation = random_vec_range(extents);
    scene->entities[node] |= e_cmp::transform;
    scene->parents[node] = node;
    
    std::vector<vec2f> verts;
    for(u32 i = 0; i < num_verts; ++i)
    {
        verts.push_back(random_vec_range(extents).xz);
    }
    
    std::vector<vec2f> hull;
    maths::convex_hull_from_points(hull, verts);
    
    convex.vertices.clear();
    for(auto& v : hull)
    {
        convex.vertices.push_back(v);
    }
}

void test_ray_plane_intersect(ecs_scene* scene, bool initialise)
{
    static debug_plane plane;
    static debug_ray   ray;

    static debug_extents e = {vec3f(-10.0, -10.0, -10.0), vec3f(10.0, 10.0, 10.0)};

    bool randomise = ImGui::Button("Randomise");

    if (initialise || randomise)
    {
        ecs::clear_scene(scene);

        add_debug_ray(e, scene, ray);
        add_debug_plane(e, scene, plane);

        ecs::update_scene(scene, 1.0f / 60.0f);
    }

    vec3f ip = maths::ray_vs_plane(ray.origin, ray.direction, plane.point, plane.normal);

    // debug output
    dbg::add_point(ip, 0.3f, vec4f::red());

    dbg::add_plane(plane.point, plane.normal);

    dbg::add_line(ray.origin, ray.origin + ray.direction * 500.0f, vec4f::green());
}

void test_ray_vs_aabb(ecs_scene* scene, bool initialise)
{
    static debug_aabb aabb;
    static debug_ray  ray;

    static debug_extents e = {vec3f(-10.0, -10.0, -10.0), vec3f(10.0, 10.0, 10.0)};

    bool randomise = ImGui::Button("Randomise");

    if (initialise || randomise)
    {
        ecs::clear_scene(scene);

        add_debug_ray(e, scene, ray);
        add_debug_solid_aabb(e, scene, aabb);

        ecs::update_scene(scene, 1.0f / 60.0f);
    }

    vec3f ip = vec3f::zero();
    bool  intersect = maths::ray_vs_aabb(aabb.min, aabb.max, ray.origin, ray.direction, ip);

    vec4f col = vec4f::green();

    if (intersect)
        col = vec4f::red();

    dbg::add_line(ray.origin, ray.origin + ray.direction * 500.0f, col);

    // debug output
    if (intersect)
        dbg::add_point(ip, 0.5f, vec4f::white());

    scene->draw_call_data[aabb.node].v2 = col;
}

void test_ray_vs_obb(ecs_scene* scene, bool initialise)
{
    static debug_obb obb;
    static debug_ray ray;

    static debug_extents e = {vec3f(-10.0, -10.0, -10.0), vec3f(10.0, 10.0, 10.0)};

    bool randomise = ImGui::Button("Randomise");

    static bool show_unit_aabb_space = false;
    ImGui::Checkbox("Show Unit AABB Space", &show_unit_aabb_space);

    if (initialise || randomise)
    {
        ecs::clear_scene(scene);

        add_debug_ray(e, scene, ray);
        add_debug_solid_obb(e, scene, obb);

        ecs::update_scene(scene, 1.0f / 60.0f);
    }

    vec3f ip = vec3f::zero();
    bool  intersect = maths::ray_vs_obb(scene->world_matrices[obb.node], ray.origin, ray.direction, ip);

    if (show_unit_aabb_space)
    {
        mat4  invm = mat::inverse4x4(scene->world_matrices[obb.node]);
        vec3f tr1 = invm.transform_vector(vec4f(ray.origin, 1.0f)).xyz;

        invm.set_translation(vec3f::zero());
        vec3f trv = invm.transform_vector(vec4f(ray.direction, 1.0f)).xyz;

        bool ii = maths::ray_vs_aabb(-vec3f::one(), vec3f::one(), tr1, normalize(trv), ip);

        put::dbg::add_aabb(-vec3f::one(), vec3f::one(), vec4f::cyan());

        put::dbg::add_line(tr1, tr1 + trv * 1000.0f, vec4f::magenta());

        if (ii)
        {
            put::dbg::add_point(ip, 0.1f, vec4f::green());
        }
    }

    vec4f col = vec4f::green();

    if (intersect)
        col = vec4f::red();

    dbg::add_line(ray.origin, ray.origin + ray.direction * 500.0f, col);

    {
        vec3f r1 = ray.origin;
        vec3f rv = ray.direction;
        mat4  mat = scene->world_matrices[obb.node];

        mat4  invm = mat::inverse4x4(mat);
        vec3f tr1 = invm.transform_vector(vec4f(r1, 1.0f)).xyz;

        invm.set_translation(vec3f::zero());
        vec3f trv = invm.transform_vector(vec4f(rv, 1.0f)).xyz;

        vec3f cp = maths::closest_point_on_ray(tr1, normalize(trv), vec3f::zero());
        vec3f ccp = mat.transform_vector(vec4f(cp, 1.0f)).xyz;

        put::dbg::add_point(ccp, 0.1f, vec4f::yellow());

        ip = mat.transform_vector(vec4f(ip, 1.0f)).xyz;
    }

    // debug output
    if (intersect)
        dbg::add_point(ip, 0.5f, vec4f::white());

    scene->draw_call_data[obb.node].v2 = col;
}

void test_ray_vs_sphere(ecs_scene* scene, bool initialise)
{
    static debug_sphere sphere;
    static debug_ray ray;
    
    static debug_extents e = {vec3f(-10.0, -10.0, -10.0), vec3f(10.0, 10.0, 10.0)};

    bool randomise = ImGui::Button("Randomise");

    if (initialise || randomise)
    {
        ecs::clear_scene(scene);

        add_debug_ray_targeted(e, scene, ray);
        add_debug_sphere(e, scene, sphere);

        ecs::update_scene(scene, 1.0f / 60.0f);
    }
    
    vec3f ip = vec3f::zero();
    bool  intersect = maths::ray_vs_sphere(ray.origin, ray.direction, sphere.pos, sphere.radius, ip);

    vec4f col = vec4f::green();

    if (intersect)
        col = vec4f::red();

    dbg::add_point(ray.origin, 1.0f, vec4f::cyan());
    dbg::add_line(ray.origin, ray.origin + ray.direction * 500.0f, col);

    // debug output
    if (intersect)
        dbg::add_point(ip, 0.5f, vec4f::white());
    
    scene->draw_call_data[sphere.node].v2 = col;
    
    bool gen = ImGui::Button("Gen Test");
    if(gen)
    {
        PEN_LOG("{");
        
        //  ray with ip
        PEN_LOG("\tvec3f ip = {(f32)0.0, (f32)0.0, (f32)0.0};");
        PEN_LOG("\tvec3f r0 = {(f32)%f, (f32)%f, (f32)%f};", ray.origin.x, ray.origin.y, ray.origin.z);
        PEN_LOG("\tvec3f rv = {(f32)%f, (f32)%f, (f32)%f};", ray.direction.x, ray.direction.y, ray.direction.z);

        // sphere
        PEN_LOG("\tvec3f sp = {(f32)%f, (f32)%f, (f32)%f};", sphere.pos.x, sphere.pos.y, sphere.pos.z);
        PEN_LOG("\tf32 sr = (f32)%f;", sphere.radius);
        
        // custom
        PEN_LOG("\tbool i = maths::ray_vs_sphere(r0, rv, sp, sr, ip);");
        
        // intersection and point
        PEN_LOG("\tREQUIRE(i == bool(%i));", intersect);
        
        if(intersect)
            PEN_LOG("\tREQUIRE(require_func(ip, {(f32)%f, (f32)%f, (f32)%f}));", ip.x, ip.y, ip.z);
        
        PEN_LOG("}");
    }
}

void test_ray_vs_capsule(ecs_scene* scene, bool initialise)
{
    static debug_capsule capsule;
    static debug_ray ray;
    
    static debug_extents e = {vec3f(-10.0, -10.0, -10.0), vec3f(10.0, 10.0, 10.0)};

    bool randomise = ImGui::Button("Randomise");

    if (initialise || randomise)
    {
        ecs::clear_scene(scene);

        add_debug_ray_targeted(e, scene, ray);
        add_debug_capsule(e, scene, capsule);

        ecs::update_scene(scene, 1.0f / 60.0f);
    }
    
    ImGui::SameLine();
    bool edge_case = ImGui::Button("Edge Case");
    if(edge_case)
    {
        ray.origin = capsule.cp1 + normalize(capsule.cp1 - capsule.cp2) * 10.0f;
        ray.direction = normalize(capsule.cp2 - capsule.cp1);
    }
    
    vec3f ip = vec3f::zero();
    bool  intersect = maths::ray_vs_capsule(ray.origin, ray.direction, capsule.cp1, capsule.cp2, capsule.radius, ip);

    vec4f col = vec4f::green();

    if (intersect)
    {
        col = vec4f::red();
        dbg::add_point(ip, 1.0f, vec4f::white());
    }

    dbg::add_point(ray.origin, 1.0f, vec4f::cyan());
    dbg::add_line(ray.origin, ray.origin + ray.direction * 500.0f, col);
    
    // ..
    static bool hide = false;
    ImGui::Checkbox("Hide Geometry", &hide);
    
    static bool hide_spheres = false;
    ImGui::Checkbox("Hide Spheres", &hide_spheres);
    
    // debug col
    for(u32 i = 0; i < 3; ++i)
    {
        scene->draw_call_data[capsule.nodes[i]].v2 = col;
        
        bool compound_hide = hide;
        if(i < 2)
        {
            compound_hide |= hide_spheres;
        }
                
        if(compound_hide)
        {
            scene->state_flags[capsule.nodes[i]] |= e_state::hidden;
        }
        else
        {
            scene->state_flags[capsule.nodes[i]] &= ~e_state::hidden;
        }
    }
    
    bool gen = ImGui::Button("Gen Test");
    if(gen)
    {
        PEN_LOG("{");
        print_test_case_ray(ray.origin, ray.direction);
        print_test_case_capsule(capsule.cp1, capsule.cp2, capsule.radius);
        PEN_LOG("\tbool i = maths::ray_vs_capsule(r0, rv, cp0, cp1, cr, ip);");
        print_test_case_intersection_point(intersect, ip);
        
        PEN_LOG("}");
    }
}

void test_ray_vs_cylinder(ecs_scene* scene, bool initialise)
{
    static debug_cylinder cylinder;
    static debug_ray ray;
    
    static debug_extents e = {vec3f(-10.0, -10.0, -10.0), vec3f(10.0, 10.0, 10.0)};

    bool randomise = ImGui::Button("Randomise");

    if (initialise || randomise)
    {
        ecs::clear_scene(scene);

        add_debug_ray_targeted(e, scene, ray);
        add_debug_cylinder(e, scene, cylinder);

        ecs::update_scene(scene, 1.0f / 60.0f);
    }
    
    ImGui::SameLine();
    bool edge_case = ImGui::Button("Edge Case");
    if(edge_case)
    {
        ray.origin = cylinder.cp1 + normalize(cylinder.cp1 - cylinder.cp2) * 10.0f;
        ray.direction = normalize(cylinder.cp2 - cylinder.cp1);
    }
    
    vec3f ip = vec3f::zero();
    bool  intersect = maths::ray_vs_cylinder(ray.origin, ray.direction, cylinder.cp1, cylinder.cp2, cylinder.radius, ip);

    vec4f col = vec4f::green();

    if (intersect)
    {
        col = vec4f::red();
        dbg::add_point(ip, 1.0f, vec4f::white());
    }

    dbg::add_point(ray.origin, 1.0f, vec4f::cyan());
    dbg::add_line(ray.origin, ray.origin + ray.direction * 500.0f, col);
    
    // ..
    static bool hide = false;
    ImGui::Checkbox("Hide Geometry", &hide);
    
    scene->draw_call_data[cylinder.node].v2 = col;

    if(hide)
    {
        scene->state_flags[cylinder.node] |= e_state::hidden;
    }
    else
    {
        scene->state_flags[cylinder.node] &= ~e_state::hidden;
    }
    
    // print test
    bool gen = ImGui::Button("Gen Test");
    if(gen)
    {
        PEN_LOG("{");
        print_test_case_ray(ray.origin, ray.direction);
        print_test_case_cylinder(cylinder.cp1, cylinder.cp2, cylinder.radius);
        PEN_LOG("\tbool i = maths::ray_vs_cylinder(r0, rv, cy0, cy1, cyr, ip);");
        print_test_case_intersection_point(intersect, ip);
        PEN_LOG("}");
    }
}

void test_point_plane(ecs_scene* scene, bool initialise)
{
    static debug_point point;
    static debug_plane plane;

    static debug_extents e = {vec3f(-10.0, -10.0, -10.0), vec3f(10.0, 10.0, 10.0)};

    bool randomise = ImGui::Button("Randomise");
    
    ImGui::SameLine();
    bool edge_case = ImGui::Button("Edge Case");
    if(edge_case)
    {
        // edge case is that point is on the plane at 0 distance
        point.point = plane.point;
    }
    
    ImGui::Separator();
    ImGui::Text("%s", "orange line / point = closest point on plane");
    ImGui::Separator();
    
    if (initialise || randomise)
    {
        ecs::clear_scene(scene);

        add_debug_point(e, scene, point);
        add_debug_plane(e, scene, plane);

        ecs::update_scene(scene, 1.0f / 60.0f);
    }

    vec3f cp = maths::closest_point_on_plane(point.point, plane.point, plane.normal);
    f32 distance = maths::point_plane_distance(point.point, plane.point, plane.normal);
    maths::classification c = maths::point_vs_plane(point.point, plane.point, plane.normal);
    
    // debug output
    ImGui::Text("Distance %f", distance);
    ImGui::Text("Classification %s", classifications[c]);

    // point
    dbg::add_point(point.point, 1.0f, classification_colours[c]);
    
    // cloest point
    dbg::add_point(cp, 0.3f, vec4f::orange());
    dbg::add_line(cp, point.point, vec4f::orange());
    
    // plane
    dbg::add_plane(plane.point, plane.normal);
    
    // print test
    bool gen = ImGui::Button("Gen Test");
    if(gen)
    {
        PEN_LOG("{");
        print_test_case_plane(plane.point, plane.normal);
        print_test_case_point(point.point);
        PEN_LOG("\tu32 c = maths::point_vs_plane(p, x, n);");
        print_test_case_classification(c);
        PEN_LOG("}");
    }
}

void test_aabb_vs_plane(ecs_scene* scene, bool initialise)
{
    static debug_aabb  aabb;
    static debug_plane plane;

    static debug_extents e = {vec3f(-10.0, -10.0, -10.0), vec3f(10.0, 10.0, 10.0)};

    bool randomise = ImGui::Button("Randomise");

    if (initialise || randomise)
    {
        ecs::clear_scene(scene);

        add_debug_plane(e, scene, plane);
        add_debug_aabb(e, scene, aabb);

        ecs::update_scene(scene, 1.0f / 60.0f);
    }

    u32 c = maths::aabb_vs_plane(aabb.min, aabb.max, plane.point, plane.normal);

    // debug output
    ImGui::Text("Classification %s", classifications[c]);

    dbg::add_aabb(aabb.min, aabb.max, classification_colours[c]);

    dbg::add_plane(plane.point, plane.normal);
}

void test_sphere_vs_plane(ecs_scene* scene, bool initialise)
{
    static debug_sphere sphere;
    static debug_plane  plane;

    static debug_extents e = {vec3f(-10.0, -10.0, -10.0), vec3f(10.0, 10.0, 10.0)};

    bool randomise = ImGui::Button("Randomise");

    if (initialise || randomise)
    {
        ecs::clear_scene(scene);

        add_debug_plane(e, scene, plane);
        add_debug_sphere(e, scene, sphere);

        ecs::update_scene(scene, 1.0f / 60.0f);
    }

    u32 c = maths::sphere_vs_plane(sphere.pos, sphere.radius, plane.point, plane.normal);

    // debug output
    ImGui::Text("Classification %s", classifications[c]);

    scene->draw_call_data[sphere.node].v2 = vec4f(classification_colours[c]);

    dbg::add_plane(plane.point, plane.normal);
}

void test_capsule_vs_plane(ecs_scene* scene, bool initialise)
{
    static debug_capsule capsule;
    static debug_plane  plane;

    static debug_extents e = {vec3f(-10.0, -10.0, -10.0), vec3f(10.0, 10.0, 10.0)};

    bool randomise = ImGui::Button("Randomise");

    if (initialise || randomise)
    {
        ecs::clear_scene(scene);

        add_debug_plane(e, scene, plane);
        add_debug_capsule(e, scene, capsule);

        ecs::update_scene(scene, 1.0f / 60.0f);
    }

    u32 c = maths::capsule_vs_plane(capsule.cp1, capsule.cp2, capsule.radius, plane.point, plane.normal);

    // debug output
    ImGui::Text("Classification %s", classifications[c]);

    for(u32 i = 0; i < 3; ++i)
    {
        scene->draw_call_data[capsule.nodes[i]].v2 = vec4f(classification_colours[c]);
    }

    dbg::add_plane(plane.point, plane.normal);
    
    // print test
    bool gen = ImGui::Button("Gen Test");
    if(gen)
    {
        PEN_LOG("{");
        print_test_case_plane(plane.point, plane.normal);
        print_test_case_capsule(capsule.cp1, capsule.cp2, capsule.radius);
        PEN_LOG("\tu32 c = maths::capsule_vs_plane(cp0, cp1, cr, x, n);");
        print_test_case_classification(c);
        PEN_LOG("}");
    }
}

void test_cone_vs_plane(ecs_scene* scene, bool initialise)
{
    static debug_cone cone;
    static debug_plane plane;

    static debug_extents e = {vec3f(-10.0, -10.0, -10.0), vec3f(10.0, 10.0, 10.0)};

    bool randomise = ImGui::Button("Randomise");

    if (initialise || randomise)
    {
        ecs::clear_scene(scene);

        add_debug_plane(e, scene, plane);
        add_debug_solid_cone(e, scene, cone);

        ecs::update_scene(scene, 1.0f / 60.0f);
    }

    f32 r = scene->transforms[cone.node].scale.x;
    f32 h = scene->transforms[cone.node].scale.y;

    vec3f cv = normalize(-scene->world_matrices[cone.node].get_column(1).xyz);
    vec3f cp = scene->world_matrices[cone.node].get_translation();
    
    u32 c = maths::cone_vs_plane(cp, cv, h, r, plane.point, plane.normal);

    // debug output
    ImGui::Text("Classification %s", classifications[c]);

    scene->draw_call_data[cone.node].v2 = vec4f(classification_colours[c]);

    dbg::add_plane(plane.point, plane.normal);
    
    // print test
    bool gen = ImGui::Button("Gen Test");
    if(gen)
    {
        PEN_LOG("{");
        print_test_case_plane(plane.point, plane.normal);
        print_test_case_cone(cp, cv, h, r);
        PEN_LOG("\tu32 c = maths::cone_vs_plane(cp, cv, h, r, x, n);");
        print_test_case_classification(c);
        PEN_LOG("}");
    }
}

void test_project(ecs_scene* scene, bool initialise)
{
    static debug_point point;

    static debug_extents e = {vec3f(-10.0, -10.0, -10.0), vec3f(10.0, 10.0, 10.0)};

    bool randomise = ImGui::Button("Randomise");

    if (initialise || randomise)
    {
        ecs::clear_scene(scene);

        add_debug_point(e, scene, point);

        ecs::update_scene(scene, 1.0f / 60.0f);
    }

    vec2i vp;
    pen::window_get_size(vp.x, vp.y);

    mat4  view_proj = main_camera.proj * main_camera.view;
    vec3f screen_point = maths::project_to_sc(point.point, view_proj, vp);
    vec3f unproj = maths::unproject_sc(screen_point, view_proj, vp);
    PEN_UNUSED(unproj);

    //dbg::add_point(point.point, 0.5f, vec4f::magenta());
    //dbg::add_point(point.point, 1.0f, vec4f::cyan());
    
    dbg::add_point_2f(screen_point.xy, vec4f::green());
    dbg::add_point(unproj, 1.0f, vec4f::yellow());
    
    // print test
    bool gen = ImGui::Button("Gen Test");
    if(gen)
    {
        PEN_LOG("{");
        print_test_case_view_proj(view_proj);
        print_test_case_point(point.point);
        print_test_case_viewport(vp);
        PEN_LOG("\tvec3f screen_point = maths::project_to_sc(p, view_proj, vp);");
        PEN_LOG("\tvec3f unproj_point = maths::unproject_sc(screen_point * vec3f(1.0f, 1.0f, 2.0f) - vec3f(0.0f, 0.0f, 1.0f), view_proj, vp);");
        PEN_LOG("\tREQUIRE(require_func(screen_point, {(f32)%f, (f32)%f, (f32)%f}));", screen_point.x, screen_point.y, screen_point.z);
        PEN_LOG("\tREQUIRE(require_func(unproj_point, {(f32)%f, (f32)%f, (f32)%f}));", point.point.x, point.point.y, point.point.z);
        PEN_LOG("}");
    }
}

void test_point_aabb(ecs_scene* scene, bool initialise)
{
    static debug_aabb  aabb;
    static debug_point point;

    static debug_extents e = {vec3f(-10.0, -10.0, -10.0), vec3f(10.0, 10.0, 10.0)};

    bool randomise = ImGui::Button("Randomise");

    if (initialise || randomise)
    {
        ecs::clear_scene(scene);

        add_debug_aabb(e, scene, aabb);
        add_debug_point(e, scene, point);

        ecs::update_scene(scene, 1.0f / 60.0f);
    }

    bool inside = maths::point_inside_aabb(aabb.min, aabb.max, point.point);
    vec3f cp = maths::closest_point_on_aabb(point.point, aabb.min, aabb.max);
    f32 d = maths::point_aabb_distance(point.point, aabb.min, aabb.max);
    
    ImGui::Text("Point Inside: %s", inside ? "true" : "false");
    ImGui::Text("Point Distance: %f", d);

    vec4f col = inside ? vec4f::red() : vec4f::green();

    dbg::add_aabb(aabb.min, aabb.max, col);
    dbg::add_point(point.point, 0.3f, col);

    dbg::add_line(cp, point.point, vec4f::cyan());
    dbg::add_point(cp, 0.3f, vec4f::cyan());
}

void test_point_obb(ecs_scene* scene, bool initialise)
{
    static debug_obb   obb;
    static debug_point point;

    static debug_extents e = {vec3f(-10.0, -10.0, -10.0), vec3f(10.0, 10.0, 10.0)};

    bool randomise = ImGui::Button("Randomise");

    if (initialise || randomise)
    {
        ecs::clear_scene(scene);

        add_debug_obb(e, scene, obb);
        add_debug_point(e, scene, point);

        ecs::update_scene(scene, 1.0f / 60.0f);
    }

    bool inside = maths::point_inside_obb(scene->world_matrices[obb.node], point.point);
    vec3f cp = maths::closest_point_on_obb(scene->world_matrices[obb.node], point.point);
    f32 d = maths::point_obb_distance(point.point, scene->world_matrices[obb.node]);

    vec4f col = inside ? vec4f::red() : vec4f::green();
    
    ImGui::Text("Point Inside: %s", inside ? "true" : "false");
    ImGui::Text("Distance %f", d);

    dbg::add_obb(scene->world_matrices[obb.node], col);

    dbg::add_point(point.point, 0.3f, col);

    dbg::add_line(cp, point.point, vec4f::cyan());
    dbg::add_point(cp, 0.3f, vec4f::cyan());
}

void test_point_line(ecs_scene* scene, bool initialise)
{
    static debug_line  line;
    static debug_point point;

    static debug_extents e = {vec3f(-10.0, -10.0, -10.0), vec3f(10.0, 10.0, 10.0)};

    bool randomise = ImGui::Button("Randomise");

    if (initialise || randomise)
    {
        ecs::clear_scene(scene);

        add_debug_line(e, scene, line);
        add_debug_point(e, scene, point);

        ecs::update_scene(scene, 1.0f / 60.0f);
    }

    vec3f cp = maths::closest_point_on_line(line.l1, line.l2, point.point);

    f32 distance = maths::point_segment_distance(point.point, line.l1, line.l2);
    ImGui::Text("Distance %f", distance);

    dbg::add_line(line.l1, line.l2, vec4f::green());

    dbg::add_line(point.point, cp);

    dbg::add_point(point.point, 0.3f);

    dbg::add_point(cp, 0.3f, vec4f::red());
}

void test_point_ray(ecs_scene* scene, bool initialise)
{
    static debug_point point;
    static debug_ray   ray;

    static debug_extents e = {vec3f(-10.0, -10.0, -10.0), vec3f(10.0, 10.0, 10.0)};

    bool randomise = ImGui::Button("Randomise");

    if (initialise || randomise)
    {
        ecs::clear_scene(scene);

        add_debug_point(e, scene, point);
        add_debug_ray(e, scene, ray);

        ecs::update_scene(scene, 1.0f / 60.0f);
    }

    vec3f cp = maths::closest_point_on_ray(ray.origin, ray.direction, point.point);

    dbg::add_line(ray.origin, ray.origin + ray.direction * 500.0f, vec4f::green());

    dbg::add_line(point.point, cp);

    dbg::add_point(point.point, 0.3f);

    dbg::add_point(cp, 0.3f, vec4f::red());
}

void test_point_triangle(ecs_scene* scene, bool initialise)
{
    static debug_triangle tri;
    static debug_point    point;

    static debug_extents e = {vec3f(-10.0, 0.0, -10.0), vec3f(10.0, 0.0, 10.0)};
    
    static std::vector<debug_point> test_points;

    bool randomise = ImGui::Button("Randomise");

    if (initialise || randomise)
    {
        ecs::clear_scene(scene);

        add_debug_triangle(e, scene, tri);
        add_debug_point(e, scene, point);
        
        test_points.clear();
        test_points.resize(64);
        for(u32 i = 0; i < 64; ++i)
        {
            add_debug_point(e, scene, test_points[i]);
        }

        ecs::update_scene(scene, 1.0f / 60.0f);
    }

    bool inside = maths::point_inside_triangle(point.point, tri.t0, tri.t1, tri.t2);

    f32 distance = maths::point_triangle_distance(point.point, tri.t0, tri.t1, tri.t2);
    
    ImGui::Text("Point Inside: %s", inside ? "true" : "false");
    ImGui::Text("Distance %f", distance);

    vec4f col = inside ? vec4f::red() : vec4f::green();

    dbg::add_point(point.point, 0.3f, col);
    dbg::add_triangle(tri.t0, tri.t1, tri.t2, col);

    // debug get normal
    vec3f n = maths::get_normal(tri.t0, tri.t1, tri.t2);
    vec3f cc = (tri.t0 + tri.t1 + tri.t2) / 3.0f;
    dbg::add_line(cc, cc + n, col);

    f32   side = 1.0f;
    vec3f cp = maths::closest_point_on_triangle(point.point, tri.t0, tri.t1, tri.t2, side);

    Vec4f col2 = vec4f::cyan();
    if (side < 1.0)
        col2 = vec4f::magenta();

    dbg::add_line(cp, point.point, col2);
    dbg::add_point(cp, 0.3f, col2);
    
    // ..
    for(auto& p : test_points)
    {
        bool inside = maths::point_inside_triangle(p.point, tri.t0, tri.t1, tri.t2);
        vec4f col = inside ? vec4f::red() : vec4f::green();
        dbg::add_point(p.point, 0.3f, col);
    }
}

void test_ray_triangle(ecs_scene* scene, bool initialise)
{
    static debug_triangle tri;
    static debug_ray ray;

    static debug_extents e = {vec3f(-10.0, 0.0, -10.0), vec3f(10.0, 0.0, 10.0)};
    static debug_extents te = {vec3f(-50.0, 0.0, -50.0), vec3f(50.0, 0.0, 50.0)};

    bool randomise = ImGui::Button("Randomise");

    if (initialise || randomise)
    {
        ecs::clear_scene(scene);

        add_debug_triangle(te, scene, tri);
        add_debug_ray2(e, scene, ray);

        ecs::update_scene(scene, 1.0f / 60.0f);
    }

    vec3f ip;
    bool intersect = maths::ray_vs_triangle(ray.origin, ray.direction, tri.t0, tri.t1, tri.t2, ip);
    
    vec4f col = vec4f::green();
    if (intersect)
        col = vec4f::red();

    dbg::add_line(ray.origin, ray.origin + ray.direction * 500.0f, col);

    // intersection point
    if (intersect)
        dbg::add_point(ip, 0.5f, vec4f::white());
    
    // triangle
    dbg::add_triangle(tri.t0, tri.t1, tri.t2, col);
    
    // print test
    bool gen = ImGui::Button("Gen Test");
    if(gen)
    {
        PEN_LOG("{");
        print_test_case_ray(ray.origin, ray.direction);
        print_test_case_triangle(tri.t0, tri.t1, tri.t2);
        PEN_LOG("\tbool i = maths::ray_triangle_intersect(r0, rv, t0, t1, t2, ip);");
        print_test_case_intersection_point(intersect, ip);
        PEN_LOG("}");
    }
}

void test_sphere_vs_sphere(ecs_scene* scene, bool initialise)
{
    static debug_sphere sphere0;
    static debug_sphere sphere1;

    static debug_extents e = {vec3f(-10.0, -10.0, -10.0), vec3f(10.0, 10.0, 10.0)};

    bool randomise = ImGui::Button("Randomise");

    if (initialise || randomise)
    {
        ecs::clear_scene(scene);

        add_debug_sphere(e, scene, sphere0);
        add_debug_sphere(e, scene, sphere1);

        ecs::update_scene(scene, 1.0f / 60.0f);
    }

    bool i = maths::sphere_vs_sphere(sphere0.pos, sphere0.radius, sphere1.pos, sphere1.radius);

    // debug output
    vec4f col = vec4f::green();
    if (i)
        col = vec4f::red();

    scene->draw_call_data[sphere0.node].v2 = vec4f(col);
    scene->draw_call_data[sphere1.node].v2 = vec4f(col);
}

void test_sphere_vs_capsule(ecs_scene* scene, bool initialise)
{
    static debug_sphere sphere0;
    static debug_capsule capsule;

    static debug_extents e = {vec3f(-10.0, -10.0, -10.0), vec3f(10.0, 10.0, 10.0)};

    bool randomise = ImGui::Button("Randomise");

    if (initialise || randomise)
    {
        ecs::clear_scene(scene);

        add_debug_sphere(e, scene, sphere0);
        add_debug_capsule(e, scene, capsule);

        ecs::update_scene(scene, 1.0f / 60.0f);
    }

    bool i = maths::sphere_vs_capsule(sphere0.pos, sphere0.radius, capsule.cp1, capsule.cp2, capsule.radius);

    // debug output
    vec4f col = vec4f::green();
    if (i)
        col = vec4f::red();

    scene->draw_call_data[sphere0.node].v2 = vec4f(col);
    
    for(u32 j = 0; j < 3; ++j)
        scene->draw_call_data[capsule.nodes[j]].v2 = vec4f(col);
        
    // print test
    bool gen = ImGui::Button("Gen Test");
    if(gen)
    {
        PEN_LOG("{");
        print_test_case_sphere(sphere0.pos, sphere0.radius);
        print_test_case_capsule(capsule.cp1, capsule.cp2, capsule.radius);
        PEN_LOG("\tbool overlap = maths::sphere_vs_capsule(sp, sr, cp0, cp1, cr);");
        print_test_case_overlap(i);
        PEN_LOG("}");
    }
}

void test_capsule_vs_capsule(ecs_scene* scene, bool initialise)
{
    static debug_capsule capsule0;
    static debug_capsule capsule1;

    static debug_extents e = {vec3f(-10.0, -10.0, -10.0), vec3f(10.0, 10.0, 10.0)};

    bool randomise = ImGui::Button("Randomise");
    bool edge_case = ImGui::Button("Orthogonal Edge Case");

    if (initialise || randomise || edge_case)
    {
        ecs::clear_scene(scene);

        add_debug_capsule(e, scene, capsule0, edge_case);
        add_debug_capsule(e, scene, capsule1, edge_case);

        ecs::update_scene(scene, 1.0f / 60.0f);
    }

    bool i = maths::capsule_vs_capsule(
        capsule0.cp1, capsule0.cp2, capsule0.radius,
        capsule1.cp1, capsule1.cp2, capsule1.radius);

    // debug output
    vec4f col = vec4f::green();
    if (i)
        col = vec4f::red();
    
    //
    for(u32 j = 0; j < 3; ++j)
        scene->draw_call_data[capsule0.nodes[j]].v2 = vec4f(col);
    
    for(u32 j = 0; j < 3; ++j)
        scene->draw_call_data[capsule1.nodes[j]].v2 = vec4f(col);

    // print test
    bool gen = ImGui::Button("Gen Test");
    if(gen)
    {
        PEN_LOG("{");
        print_test_case_capsule(capsule0.cp1, capsule0.cp2, capsule0.radius);
        print_test_case_capsule_2(capsule1.cp1, capsule1.cp2, capsule1.radius);
        
        PEN_LOG("\tbool overlap = maths::capsule_vs_capsule(cp0, cp1, cr, cp2, cp3, cr1);");
        print_test_case_overlap(i);
        PEN_LOG("}");
    }
}

void test_convex_hull_vs_convex_hull(ecs_scene* scene, bool initialise)
{
    static debug_extents e0 = {vec3f(-10.0, -10.0, -10.0), vec3f(10.0, 10.0, 10.0)};
    static debug_extents e1 = {vec3f(-5.0, -5.0, -5.0), vec3f(15.0, 15.0, 15.0)};
    
    static debug_convex_hull conv0;
    static debug_convex_hull conv1;

    bool randomise = ImGui::Button("Randomise");
    static bool continuous_rand = false;
    ImGui::SameLine();
    ImGui::Checkbox("Continuous Randomise", &continuous_rand);
    if(continuous_rand)
    {
        randomise = true;
    }

    if (initialise || randomise)
    {
        ecs::clear_scene(scene);
        
        add_debug_convex_hull(e0, scene, 6, conv0);
        add_debug_convex_hull(e1, scene, 8, conv1);

        ecs::update_scene(scene, 1.0f / 60.0f);
    }
    
    static vec2f offset = vec2f::zero();
    ImGui::SliderFloat2("Translate Shape", (f32*)&offset, -50.0f, 50.0f);
    
    std::vector<vec2f> transformed_conv1;
    
    // apply translation for debugging
    for(auto& v : conv1.vertices)
    {
        transformed_conv1.push_back(v + offset);
    }
    
    bool i = maths::convex_hull_vs_convex_hull(conv0.vertices, transformed_conv1);
    
    // debug output
    vec4f col = vec4f::green();
    if (i)
        col = vec4f::red();
    
    // draw
    u32 s0 = (u32)conv0.vertices.size();
    for(u32 i = 0; i < s0; ++i)
    {
        u32 j = (i + 1) % s0;
        dbg::add_line(vec3f(conv0.vertices[i].x, 0.0f, conv0.vertices[i].y), vec3f(conv0.vertices[j].x, 0.0f, conv0.vertices[j].y), col);
    }
    
    u32 s1 = (u32)transformed_conv1.size();
    for(u32 i = 0; i < s1; ++i)
    {
        u32 j = (i + 1) % s1;
        dbg::add_line(vec3f(transformed_conv1[i].x, 0.0f, transformed_conv1[i].y), vec3f(transformed_conv1[j].x, 0.0f, transformed_conv1[j].y), col);
    }
    
    bool gen = ImGui::Button("Gen Test");
    if(gen)
    {
        PEN_LOG("{");
        print_test_case_convex_hull(conv0.vertices);
        print_test_case_convex_hull2(transformed_conv1);
        PEN_LOG("\tbool overlap = maths::convex_hull_vs_convex_hull(hull, hull2);");
        print_test_case_overlap(i);
        PEN_LOG("}");
    }
}

void test_aabb_vs_obb(ecs_scene* scene, bool initialise)
{
    static debug_aabb aabb;
    static debug_obb obb;

    static debug_extents e = {vec3f(-10.0, -10.0, -10.0), vec3f(10.0, 10.0, 10.0)};

    bool randomise = ImGui::Button("Randomise");
    
    static bool continuous_rand = false;
    ImGui::SameLine();
    ImGui::Checkbox("Continuous Randomise", &continuous_rand);
    if(continuous_rand)
    {
        randomise = true;
    }

    static vec3f debug_offset = vec3f::zero();
    if (initialise || randomise)
    {
        ecs::clear_scene(scene);

        add_debug_obb(e, scene, obb);
        add_debug_aabb(e, scene, aabb);

        ecs::update_scene(scene, 1.0f / 60.0f);
        
        debug_offset = scene->transforms[obb.node].translation;
    }
    
    ImGui::SliderFloat3("Translate Shape", (f32*)&debug_offset, -10.0f, 10.0f);
    scene->transforms[obb.node].translation = debug_offset;
    scene->entities[obb.node] |= e_cmp::transform;
    
    bool i = maths::aabb_vs_obb(aabb.min, aabb.max, scene->world_matrices[obb.node]);
    
    /*
    static s32 debug_depth = 0;
    ImGui::InputInt("Debug Depth", &debug_depth);
    u32 r = aabb_vs_obb_debug(aabb.min, aabb.max, scene->world_matrices[obb.node], debug_depth);
    
    bool i = r == 1 ? true : false;
    
    if(r == 2)
    {
        continuous_rand = false;
    }
    */
    
    // debug output
    vec4f col = vec4f::green();
    if (i)
        col = vec4f::red();
    
    dbg::add_obb(scene->world_matrices[obb.node], col);
    dbg::add_aabb(aabb.min, aabb.max, col);
    
    bool gen = ImGui::Button("Gen Test");
    if(gen)
    {
        PEN_LOG("{");
        print_test_case_aabb(aabb.min, aabb.max);
        print_test_case_obb(scene->world_matrices[obb.node]);
        PEN_LOG("\tbool overlap = maths::aabb_vs_obb(aabb_min, aabb_max, obb);");
        print_test_case_overlap(i);
        PEN_LOG("}");
    }
}

void test_obb_vs_obb(ecs_scene* scene, bool initialise)
{
    static debug_obb obb0;
    static debug_obb obb1;

    static debug_extents e = {vec3f(-10.0, -10.0, -10.0), vec3f(10.0, 10.0, 10.0)};

    bool randomise = ImGui::Button("Randomise");
    
    static u32 rcount = 0;
    static bool continuous_rand = false;
    ImGui::SameLine();
    ImGui::Checkbox("Continuous Randomise", &continuous_rand);
    ImGui::SameLine();
    ImGui::Text("%u", rcount);
    if(continuous_rand)
    {
        randomise = true;
    }

    static vec3f debug_offset = vec3f::zero();
    if (initialise || randomise)
    {
        ecs::clear_scene(scene);

        add_debug_obb(e, scene, obb0);
        add_debug_obb(e, scene, obb1);

        ecs::update_scene(scene, 1.0f / 60.0f);
        
        debug_offset = scene->transforms[obb1.node].translation;
        rcount++;
    }
    
    ImGui::SliderFloat3("Translate Shape", (f32*)&debug_offset, -10.0f, 10.0f);
    scene->transforms[obb1.node].translation = debug_offset;
    scene->entities[obb1.node] |= e_cmp::transform;
    
    /*
    static s32 debug_depth = 0;
    ImGui::InputInt("Debug Depth", &debug_depth);
    u32 r = obb_vs_obb_debug(scene->world_matrices[obb0.node], scene->world_matrices[obb1.node], debug_depth);
    
    bool i = r == 1 ? true : false;
    if(r == 2)
    {
        continuous_rand = false;
    }
    */
    
    bool i = maths::obb_vs_obb(scene->world_matrices[obb0.node], scene->world_matrices[obb1.node]);

    // debug output
    vec4f col = vec4f::green();
    if (i)
        col = vec4f::red();
    
    dbg::add_obb(scene->world_matrices[obb0.node], col);
    dbg::add_obb(scene->world_matrices[obb1.node], col);
    
    bool gen = ImGui::Button("Gen Test");
    if(gen)
    {
        PEN_LOG("{");
        print_test_case_obb(scene->world_matrices[obb0.node]);
        print_test_case_obb2(scene->world_matrices[obb1.node]);
        PEN_LOG("\tbool overlap = maths::obb_vs_obb(obb, obb2);");
        print_test_case_overlap(i);
        PEN_LOG("}");
    }
}

void test_sphere_vs_aabb(ecs_scene* scene, bool initialise)
{
    static debug_aabb   aabb;
    static debug_sphere sphere;

    static debug_extents e = {vec3f(-10.0, -10.0, -10.0), vec3f(10.0, 10.0, 10.0)};

    bool randomise = ImGui::Button("Randomise");

    if (initialise || randomise)
    {
        ecs::clear_scene(scene);

        add_debug_solid_aabb(e, scene, aabb);
        add_debug_sphere(e, scene, sphere);

        ecs::update_scene(scene, 1.0f / 60.0f);
    }

    bool i = maths::sphere_vs_aabb(sphere.pos, sphere.radius, aabb.min, aabb.max);

    // debug output
    vec4f col = vec4f::green();
    if (i)
        col = vec4f::red();

    scene->draw_call_data[sphere.node].v2 = vec4f(col);
    scene->draw_call_data[aabb.node].v2 = vec4f(col);
}

void test_sphere_vs_obb(ecs_scene* scene, bool initialise)
{
    static debug_obb    obb;
    static debug_sphere sphere;

    static debug_extents e = {vec3f(-10.0, -10.0, -10.0), vec3f(10.0, 10.0, 10.0)};

    bool randomise = ImGui::Button("Randomise");

    if (initialise || randomise)
    {
        ecs::clear_scene(scene);

        add_debug_obb(e, scene, obb);
        add_debug_sphere(e, scene, sphere);

        ecs::update_scene(scene, 1.0f / 60.0f);
    }

    bool i = maths::sphere_vs_obb(sphere.pos, sphere.radius, scene->world_matrices[obb.node]);

    // debug output
    vec4f col = vec4f::green();
    if (i)
        col = vec4f::red();
    
    dbg::add_obb(scene->world_matrices[obb.node], col);
    scene->draw_call_data[sphere.node].v2 = vec4f(col);
    scene->draw_call_data[obb.node].v2 = vec4f(col);
    
    bool gen = ImGui::Button("Gen Test");
    if(gen)
    {
        PEN_LOG("{");
        print_test_case_sphere(sphere.pos, sphere.radius);
        print_test_case_obb(scene->world_matrices[obb.node]);
        PEN_LOG("\tbool overlap = maths::sphere_vs_obb(sp, sr, obb);");
        print_test_case_overlap(i);
        PEN_LOG("}");
    }
}

void test_aabb_vs_aabb(ecs_scene* scene, bool initialise)
{
    static debug_aabb aabb0;
    static debug_aabb aabb1;

    static debug_extents e = {vec3f(-10.0, -10.0, -10.0), vec3f(10.0, 10.0, 10.0)};

    bool randomise = ImGui::Button("Randomise");

    if (initialise || randomise)
    {
        ecs::clear_scene(scene);

        add_debug_solid_aabb(e, scene, aabb0);
        add_debug_solid_aabb(e, scene, aabb1);

        ecs::update_scene(scene, 1.0f / 60.0f);
    }

    bool i = maths::aabb_vs_aabb(aabb0.min, aabb0.max, aabb1.min, aabb1.max);

    // debug output
    vec4f col = vec4f::green();
    if (i)
        col = vec4f::red();

    scene->draw_call_data[aabb0.node].v2 = vec4f(col);
    scene->draw_call_data[aabb1.node].v2 = vec4f(col);
}

void test_sphere_vs_frustum(ecs_scene* scene, bool initialise)
{
    static debug_sphere sphere;

    static debug_extents e = {vec3f(-10.0, -10.0, -10.0), vec3f(10.0, 10.0, 10.0)};

    bool randomise = ImGui::Button("Randomise");

    if (initialise || randomise)
    {
        ecs::clear_scene(scene);

        add_debug_sphere(e, scene, sphere);

        ecs::update_scene(scene, 1.0f / 60.0f);
    }

    static camera dc;
    camera_create_perspective(&dc, 60.0f, 16.0f / 9.0f, 0.01f, 50.0f);
    camera_update_look_at(&dc);
    camera_update_frustum(&dc);

    dbg::add_frustum(dc.camera_frustum.corners[0], dc.camera_frustum.corners[1]);

    vec4f planes[6];
    mat4  view_proj = dc.proj * dc.view;
    maths::get_frustum_planes_from_matrix(view_proj, &planes[0]);

    bool i = maths::sphere_vs_frustum(sphere.pos, sphere.radius, &planes[0]);

    if (ImGui::Button("Gen Test"))
    {
        std::cout << "vec3f pos = {" << sphere.pos << "};\n";
        std::cout << "f32 radius = {" << sphere.radius << "};\n";
    }

    // debug output
    vec4f col = vec4f::green();
    if (!i)
        col = vec4f::red();

    scene->draw_call_data[sphere.node].v2 = vec4f(col);
}

void test_point_vs_frustum(ecs_scene* scene, bool initialise)
{
    static debug_point point;
    static debug_extents e = {vec3f(-100.0, -10.0, -100.0), vec3f(100.0, 10.0, 100.0)};

    bool randomise = ImGui::Button("Randomise");

    if (initialise || randomise)
    {
        ecs::clear_scene(scene);
        add_debug_point(e, scene, point);
        ecs::update_scene(scene, 1.0f / 60.0f);
    }

    static camera dc;
    camera_create_perspective(&dc, 60.0f, 16.0f / 9.0f, 0.01f, 100.0f);
    camera_update_look_at(&dc);
    camera_update_frustum(&dc);

    dbg::add_frustum(dc.camera_frustum.corners[0], dc.camera_frustum.corners[1]);

    vec4f planes[6];
    mat4  view_proj = dc.proj * dc.view;
    maths::get_frustum_planes_from_matrix(view_proj, &planes[0]);

    bool i = maths::point_inside_frustum(point.point, &planes[0]);

    // debug output
    vec4f col = vec4f::green();
    if (!i)
        col = vec4f::red();

    dbg::add_point(point.point, 0.3f, col);
}

void test_aabb_vs_frustum(ecs_scene* scene, bool initialise)
{
    static debug_aabb aabb0;
    static debug_aabb aabb1;

    static debug_extents e = {vec3f(-10.0, -10.0, -10.0), vec3f(10.0, 10.0, 10.0)};

    bool randomise = ImGui::Button("Randomise");

    if (initialise || randomise)
    {
        ecs::clear_scene(scene);

        add_debug_solid_aabb(e, scene, aabb0);
        ecs::update_scene(scene, 1.0f / 60.0f);
    }

    static camera dc;
    camera_create_perspective(&dc, 60.0f, 16.0f / 9.0f, 0.01f, 50.0f);
    camera_update_look_at(&dc);
    camera_update_frustum(&dc);

    dbg::add_frustum(dc.camera_frustum.corners[0], dc.camera_frustum.corners[1]);

    vec4f planes[6];
    mat4  view_proj = dc.proj * dc.view;
    maths::get_frustum_planes_from_matrix(view_proj, &planes[0]);

    vec3f epos = aabb0.min + (aabb0.max - aabb0.min) * 0.5f;
    bool  i = maths::aabb_vs_frustum(epos, aabb0.max - epos, &planes[0]);

    if (ImGui::Button("Gen Test"))
    {
        std::cout << "mat4 view_proj = {\n";
        std::cout << view_proj;
        std::cout << "}\n";

        std::cout << "vec3f epos = {" << epos << "};\n";
        std::cout << "vec3f eext = {" << (aabb0.max - epos) << "};\n";
    }

    // debug output
    vec4f col = vec4f::green();
    if (!i)
        col = vec4f::red();

    scene->draw_call_data[aabb0.node].v2 = vec4f(col);
}

void test_point_sphere(ecs_scene* scene, bool initialise)
{
    static debug_point  point;
    static debug_sphere sphere;

    static debug_extents e = {vec3f(-10.0, -10.0, -10.0), vec3f(10.0, 10.0, 10.0)};

    bool randomise = ImGui::Button("Randomise");

    if (initialise || randomise)
    {
        ecs::clear_scene(scene);

        add_debug_point(e, scene, point);
        add_debug_sphere(e, scene, sphere);

        ecs::update_scene(scene, 1.0f / 60.0f);
    }

    bool  i = maths::point_inside_sphere(sphere.pos, sphere.radius, point.point);
    vec3f cp = maths::closest_point_on_sphere(sphere.pos, sphere.radius, point.point);
    f32 d = maths::point_sphere_distance(point.point, sphere.pos, sphere.radius);
    
    ImGui::Text("Point Inside: %s", i ? "true" : "false");
    ImGui::Text("Point Distance: %f", d);

    // debug output
    vec4f col = vec4f::green();
    vec4f col2 = vec4f::white();
    if (i)
        col = vec4f::red();

    dbg::add_point(cp, 0.3f, col2);
    dbg::add_line(cp, point.point, col2);

    dbg::add_point(point.point, 0.4f, col);

    scene->draw_call_data[sphere.node].v2 = vec4f(col);
    
    // print test
    bool gen = ImGui::Button("Gen Test");
    if(gen)
    {
        // point sphere distance
        PEN_LOG("{");
        print_test_case_sphere(sphere.pos, sphere.radius);
        print_test_case_point(point.point);
        PEN_LOG("\tf32 dd = maths::point_sphere_distance(p, sp, sr);");
        print_test_case_distance(d);
        PEN_LOG("}");
    }
}

void test_point_cone(ecs_scene* scene, bool initialise)
{
    static debug_point point;
    static debug_cone  cone;

    static debug_extents e = {vec3f(-10.0, 0.0, -10.0), vec3f(15.0, 15.0, 15.0)};

    bool randomise = ImGui::Button("Randomise");
    ImGui::Separator();
    ImGui::Text("%s", "yellow point / line = closest point on cone");
    ImGui::Separator();

    if (initialise || randomise)
    {
        ecs::clear_scene(scene);

        add_debug_point(e, scene, point);
        add_debug_solid_cone(e, scene, cone);

        ecs::update_scene(scene, 1.0f / 60.0f);
    }

    f32 r = scene->transforms[cone.node].scale.x;
    f32 h = scene->transforms[cone.node].scale.y;

    vec3f cv = normalize(-scene->world_matrices[cone.node].get_column(1).xyz);
    vec3f cp = scene->world_matrices[cone.node].get_translation();
    
    dbg::add_point(cp, 0.4f, vec4f::cyan());
    dbg::add_point(cp + cv * h, 0.4f, vec4f::magenta());

    bool i = maths::point_inside_cone(point.point, cp, cv, h, r);
    vec3f closest = maths::closest_point_on_cone(point.point, cp, cv, h, r);
    f32 d = maths::point_cone_distance(point.point, cp, cv, h, r);
    
    ImGui::Text("%s : %s", "Point Inside", i ? "true" : "false");
    ImGui::Text("%s : %f", "Point Distance", d);
    
    dbg::add_point(closest, 0.3f, vec4f::yellow());
    dbg::add_line(point.point, closest, vec4f::yellow());
    
    // debug output
    vec4f col = vec4f::green();
    if (i)
        col = vec4f::red();

    dbg::add_point(point.point, 0.4f, col);

    scene->draw_call_data[cone.node].v2 = vec4f(col);
    
    static bool hide = false;
    ImGui::Checkbox("Hide Geometry", &hide);
    
    scene->draw_call_data[cone.node].v2 = col;

    if(hide)
    {
        scene->state_flags[cone.node] |= e_state::hidden;
    }
    else
    {
        scene->state_flags[cone.node] &= ~e_state::hidden;
    }
    
    // print test
    bool gen = ImGui::Button("Gen Test");
    if(gen)
    {
        // point sphere distance
        PEN_LOG("{");
        print_test_case_cone(cp, cv, h, r);
        print_test_case_point(point.point);
        PEN_LOG("\tbool overlap = maths::point_inside_cone(p, cp, cv, h, r);");
        PEN_LOG("\tvec3f cpp = maths::closest_point_on_cone(p, cp, cv, h, r);");
        PEN_LOG("\tf32 dd = maths::point_cone_distance(p, cp, cv, h, r);");
        PEN_LOG("\t// point_cone_distance");
        print_test_case_distance(d);
        PEN_LOG("\t// point_inside_cone");
        print_test_case_overlap(i);
        PEN_LOG("\t// closest_point_on_cone");
        print_test_case_closest_point(closest);
        PEN_LOG("}");
    }
}

void test_line_vs_line(ecs_scene* scene, bool initialise)
{
    static debug_line line0;
    static debug_line line1;

    static debug_extents e = {vec3f(-10.0, 0.0, -10.0), vec3f(10.0, 0.0, 10.0)};

    bool randomise = ImGui::Button("Randomise");

    if (initialise || randomise)
    {
        ecs::clear_scene(scene);

        add_debug_line(e, scene, line0);
        add_debug_line(e, scene, line1);

        ecs::update_scene(scene, 1.0f / 60.0f);
    }

    vec3f ip = vec3f::zero();
    bool  intersect = maths::line_vs_line(line0.l1, line0.l2, line1.l1, line1.l2, ip);

    if (intersect)
        dbg::add_point(ip, 0.3f, vec4f::yellow());

    vec4f col = intersect ? vec4f::red() : vec4f::green();

    dbg::add_line(line0.l1, line0.l2, col);
    dbg::add_line(line1.l1, line1.l2, col);
}

void test_ray_vs_line_segment(ecs_scene* scene, bool initialise)
{
    static debug_line line0;
    static debug_ray  ray;

    static debug_extents e = {vec3f(-10.0, 0.0, -10.0), vec3f(10.0, 0.0, 10.0)};

    bool randomise = ImGui::Button("Randomise");

    if (initialise || randomise)
    {
        ecs::clear_scene(scene);

        add_debug_line(e, scene, line0);
        add_debug_ray_targeted(e, scene, ray);
        
        // flatten
        ray.direction.y = 0.0f;
        ray.origin = 0.0f;
        ray.direction = normalize(ray.direction);

        ecs::update_scene(scene, 1.0f / 60.0f);
    }

    vec3f ip = vec3f::zero();
    bool  intersect = maths::ray_vs_line_segment(line0.l1, line0.l2, ray.origin, ray.direction, ip);

    if (intersect)
        dbg::add_point(ip, 0.3f, vec4f::yellow());

    vec4f col = intersect ? vec4f::red() : vec4f::green();

    dbg::add_line(line0.l1, line0.l2, col);
    
    dbg::add_point(ray.origin, 0.3f, vec4f::cyan());
    dbg::add_line(ray.origin, ray.origin + ray.direction * 100.0f, col);
    
    // print test
    bool gen = ImGui::Button("Gen Test");
    if(gen)
    {
        PEN_LOG("{");
        print_test_case_ray(ray.origin, ray.direction);
        print_test_case_line_0(line0.l1, line0.l2);
        PEN_LOG("\tbool i = maths::ray_vs_line_segment(l00, l01, r0, rv, ip);");
        print_test_case_intersection_point(intersect, ip);
        PEN_LOG("}");
    }
}

void test_line_segment_between_line_segments(ecs_scene* scene, bool initialise)
{
    static debug_line line0;
    static debug_line line1;

    static debug_extents e = {vec3f(-10.0, 0.0, -10.0), vec3f(10.0, 0.0, 10.0)};

    bool randomise = ImGui::Button("Randomise");

    if (initialise || randomise)
    {
        ecs::clear_scene(scene);

        add_debug_line(e, scene, line0);
        add_debug_line(e, scene, line1);

        ecs::update_scene(scene, 1.0f / 60.0f);
    }
    
    ImGui::SameLine();
    if(ImGui::Button("Edge Case"))
    {
        line1.l2 = line1.l1 + (line0.l2 - line0.l1);
    }

    vec3f r0 = vec3f::zero();
    vec3f r1 = vec3f::zero();
    bool has_line = maths::shortest_line_segment_between_line_segments(line0.l1, line0.l2, line1.l1, line1.l2, r0, r1);
    
    if(has_line)
    {
        dbg::add_line(r0, r1, vec4f::green());
    }

    vec4f col = has_line ? vec4f::white() : vec4f::red();

    dbg::add_line(line0.l1, line0.l2, col);
    dbg::add_line(line1.l1, line1.l2, col);
    
    // print test
    bool gen = ImGui::Button("Gen Test");
    if(gen)
    {
        PEN_LOG("{");
        print_test_case_line_0(line0.l1, line0.l2);
        print_test_case_line_1(line1.l1, line1.l2);
        PEN_LOG("\tvec3f r0 = vec3f::zero();");
        PEN_LOG("\tvec3f r1 = vec3f::zero();");
        PEN_LOG("\tbool has = maths::shortest_line_segment_between_line_segments(l00, l01, l10, l11, r0, r1);");
        PEN_LOG("\tREQUIRE(has == bool(%i));", has_line);
        PEN_LOG("\tif(has) REQUIRE(require_func(r0, {(f32)%f, (f32)%f, (f32)%f}));", r0.x, r0.y, r0.z);
        PEN_LOG("\tif(has) REQUIRE(require_func(r1, {(f32)%f, (f32)%f, (f32)%f}));", r1.x, r1.y, r1.z);
        PEN_LOG("}");
    }
}

void test_line_segment_between_lines(ecs_scene* scene, bool initialise)
{
    static debug_line line0;
    static debug_line line1;

    static debug_extents e = {vec3f(-10.0, -5.0, -10.0), vec3f(10.0, 5.0, 10.0)};

    bool randomise = ImGui::Button("Randomise");

    if (initialise || randomise)
    {
        ecs::clear_scene(scene);

        add_debug_line(e, scene, line0);
        add_debug_line(e, scene, line1);

        ecs::update_scene(scene, 1.0f / 60.0f);
    }
    
    ImGui::SameLine();
    if(ImGui::Button("Edge Case"))
    {
        line1.l2 = line1.l1 + (line0.l2 - line0.l1);
    }

    vec3f r0 = vec3f::zero();
    vec3f r1 = vec3f::zero();
    bool has_line = maths::shortest_line_segment_between_lines(line0.l1, line0.l2, line1.l1, line1.l2, r0, r1);
    
    if(has_line)
    {
        dbg::add_line(r0, r1, vec4f::green());
    }

    vec4f col = has_line ? vec4f::white() : vec4f::red();

    vec3f v1 = normalize(line0.l2 - line0.l1) * 1000.0f;
    dbg::add_line(line0.l1 - v1, line0.l2 + v1, col);
    
    vec3f v2 = normalize(line1.l2 - line1.l1) * 1000.0f;
    dbg::add_line(line1.l1 - v2, line1.l2 + v2, col);
    
    // print test
    bool gen = ImGui::Button("Gen Test");
    if(gen)
    {
        PEN_LOG("{");
        print_test_case_line_0(line0.l1, line0.l2);
        print_test_case_line_1(line1.l1, line1.l2);
        PEN_LOG("\tvec3f r0 = vec3f::zero();");
        PEN_LOG("\tvec3f r1 = vec3f::zero();");
        PEN_LOG("\tbool has = maths::shortest_line_segment_between_lines(l00, l01, l10, l11, r0, r1);");
        PEN_LOG("\tREQUIRE(has == bool(%i));", has_line);
        PEN_LOG("\tif(has) REQUIRE(require_func(r0, {(f32)%f, (f32)%f, (f32)%f}));", r0.x, r0.y, r0.z);
        PEN_LOG("\tif(has) REQUIRE(require_func(r1, {(f32)%f, (f32)%f, (f32)%f}));", r1.x, r1.y, r1.z);
        PEN_LOG("}");
    }
}

void test_barycentric(ecs_scene* scene, bool initialise)
{
    static debug_extents e = {vec3f(-10.0, -10.0, -10.0), vec3f(10.0, 10.0, 10.0)};
    
    static debug_triangle tri;
    static vec3f random_point;
    static f32 fratio[3];
    
    bool randomise = ImGui::Button("Randomise");
    
    if (initialise || randomise)
    {
        ecs::clear_scene(scene);

        add_debug_triangle(e, scene, tri);
        
        u8 ratio[3];
        ratio[0] = (rand() % 255);
        ratio[1] = (rand() % (255 - ratio[0]));
        ratio[2] = 255 - ratio[1] - ratio[0];
        
        for(u32 r = 0; r < 3; ++r)
            fratio[r] = (f32)ratio[r] / 255.0f;
            
        random_point = tri.t0 * fratio[0] + tri.t1 * fratio[1] + tri.t2 * fratio[2];
        
        ecs::update_scene(scene, 1.0f / 60.0f);
    }
    
    vec3f b = maths::barycentric<3, f32>(random_point, tri.t0, tri.t1, tri.t2);
    
    ImGui::Text("barycentric: [%f (u), %f (v), %f (w)]", b.x, b.y, b.z);
    ImGui::Text("created with ratio: [%f (u), %f (v), %f (w)]", fratio[0], fratio[1], fratio[2]);
    
    dbg::add_point(random_point, 0.3f, vec4f::white());
    dbg::add_triangle(tri.t0, tri.t1, tri.t2, vec4f::orange());
    
    dbg::add_line(random_point, tri.t0, vec4f::green());
    dbg::add_line(random_point, tri.t1, vec4f::green());
    dbg::add_line(random_point, tri.t2, vec4f::green());
}

void test_convex_hull(ecs_scene* scene, bool initialise)
{
    static debug_extents e = {vec3f(-10.0, 0.0, -10.0), vec3f(10.0, 0.0, 10.0)};
    static debug_extents e2 = {vec3f(-20.0, 0.0, -20.0), vec3f(20.0, 0.0, 20.0)};
    
    static std::vector<debug_point> debug_points;
    static std::vector<debug_point> test_inside_points;
    
    static std::vector<vec3f> vec3_points;
    static std::vector<vec2f> vec2_points;
    static std::vector<vec2f> hull_points;
    
    static u32 num_src_points;
    
    bool randomise = ImGui::Button("Randomise");
    ImGui::Separator();
    ImGui::Text("magenta points = src point cloud");
    ImGui::Text("orange point = rightmost convex hull point (start of winding)");
    ImGui::Text("white line = hull generated by src points (discard the inside)");
    ImGui::Text("green points = outside hull");
    ImGui::Text("red points = inside hull");
    ImGui::Text("cyan point / line = closest point on convex hull");
    ImGui::Separator();
    
    if (initialise || randomise)
    {
        constexpr u32 data_size = 64;
        
        num_src_points = (data_size/2) + (rand() % (data_size/2));
        
        debug_points.clear();
        vec3_points.clear();
        vec2_points.clear();
        
        debug_points.resize(num_src_points);
        for(u32 i = 0; i < num_src_points; ++i)
        {
            add_debug_point(e, scene, debug_points[i]);
            vec3_points.push_back(debug_points[i].point);
            vec2_points.push_back(debug_points[i].point.xz);
        }

        test_inside_points.clear();
        test_inside_points.resize(data_size);
        for(u32 i = 0; i < data_size; ++i)
        {
            add_debug_point(e2, scene, test_inside_points[i]);
        }
        
        hull_points.clear();
        maths::convex_hull_from_points(hull_points, vec2_points);
    }
    
    // draw src points
    for(auto& p : vec3_points)
    {
        dbg::add_point(p, 0.3f, vec4f::magenta());
    }
    
    // hull from points
    dbg::add_point(vec3f(hull_points[0].x, 0.0f, hull_points[0].y), 1.0f, vec4f::orange());
    
    // draw hull
    for(s32 i = 0; i < hull_points.size(); ++i)
    {
        s32 j = (i + 1) % hull_points.size();
        vec3f p1 = vec3f(hull_points[i].x, 0.0f, hull_points[i].y);
        vec3f p2 = vec3f(hull_points[j].x, 0.0f, hull_points[j].y);
        dbg::add_line(p1, p2, vec4f::white());
    }
    
    // inside hull points
    for(auto& p : test_inside_points)
    {
        vec4f col = vec4f::green();
        if(maths::point_inside_convex_hull(p.point.xz, hull_points))
        {
            col = vec4f::red();
        }
        
        dbg::add_point(p.point, 0.3f, col);
    }
    
    // closest point
    vec2f fcp = test_inside_points[0].point.xz;
    vec2f cp = maths::closest_point_on_polygon(fcp, hull_points);
    dbg::add_line(test_inside_points[0].point, vec3f(cp.x, 0.0f, cp.y), vec4f::cyan());
    dbg::add_point(vec3f(cp.x, 0.0f, cp.y), 0.3f, vec4f::cyan());
    ImGui::Text("%s : %f", "Point Convex Hull Distance", maths::point_convex_hull_distance(fcp, hull_points));
}

void test_polygon(ecs_scene* scene, bool initialise)
{
    static debug_extents e = {vec3f(-10.0, 0.0, -10.0), vec3f(10.0, 0.0, 10.0)};
    static debug_extents lengths = {vec3f(5.0, M_PI * 0.25f, 0.0), vec3f(10.0, M_PI * 0.75f, 10.0)};
    
    bool randomise = ImGui::Button("Randomise");
    ImGui::Separator();
    ImGui::Text("%s", "green points = outside");
    ImGui::Text("%s", "red points = inside");
    ImGui::Text("%s", "cyan point / line = closest point on polygon");
    ImGui::Separator();
    
    static std::vector<vec2f> polygon;
    static std::vector<debug_point> test_inside_points;
    
    static u32 num_src_points = 0;
    if (initialise || randomise)
    {
        constexpr u32 data_size = 64;
        num_src_points = (data_size/2) + (rand() % (data_size/2));
        
        vec2f start = random_vec_range(e).xz;
        vec2f start_dir = vec2f::unit_y();
        
        vec2f cur_pos = start;
        vec2f cur_dir = start_dir;
        
        // random poly points
        polygon.clear();
        polygon.push_back(start);
        
        f32 accumulated = 0.0f;
        for(u32 i = 0; i < 16; ++i)
        {
            vec3f size = fabs(random_vec_range(lengths));
            
            f32 angle = size.y;
            
            accumulated += angle;
            if(accumulated > M_PI * 2.0f)
            {
                break;
            }
            
            vec2f new_dir = vec2f(cur_dir.x * cos(angle) - cur_dir.y * sin(angle),
                                  cur_dir.x * sin(angle) + cur_dir.y * cos(angle));
            
            vec2f new_pos = cur_pos + new_dir * size.x;
            
            polygon.push_back(new_pos);
        }
        
        // test points
        test_inside_points.clear();
        test_inside_points.resize(data_size);
        for(u32 i = 0; i < data_size; ++i)
        {
            add_debug_point(e, scene, test_inside_points[i]);
        }
    }
    
    // draw line from a point to closest point
    vec2f fcp = test_inside_points[0].point.xz;
    vec2f cp = maths::closest_point_on_polygon(fcp, polygon);
    dbg::add_line(test_inside_points[0].point, vec3f(cp.x, 0.0f, cp.y), vec4f::cyan());
    dbg::add_point(vec3f(cp.x, 0.0f, cp.y), 0.3f, vec4f::cyan());
    ImGui::Text("%s : %f", "Point Polygon Distance", maths::point_convex_hull_distance(fcp, polygon));
    
    // draw polygon
    for(s32 i = 0; i < polygon.size(); ++i)
    {
        s32 j = (i + 1) % polygon.size();
        vec3f p1 = vec3f(polygon[i].x, 0.0f, polygon[i].y);
        vec3f p2 = vec3f(polygon[j].x, 0.0f, polygon[j].y);
        dbg::add_line(p1, p2, vec4f::white());
    }
    
    // inside polygon points
    for(auto& p : test_inside_points)
    {
        vec4f col = vec4f::green();
        if(maths::point_inside_poly(p.point.xz, polygon))
        {
            col = vec4f::red();
        }
        
        dbg::add_point(p.point, 0.3f, col);
    }
}

typedef void (*maths_test_function)(ecs_scene*, bool);

// clang-format off
const c8* test_names[] {
    "AABB Plane Classification",
    "Sphere Plane Classification",
    "Capsule Plane Classification",
    "Cone Plane Classification",
    
    "Project / Unproject",
    
    "Point / Plane",
    "Point / Sphere",
    "Point / AABB",
    "Point / Line",
    "Point / Ray",
    "Point / Triangle",
    "Point / OBB",
    "Point / Cone",
    
    "Sphere vs Sphere",
    "Sphere vs Capsule",
    "Sphere vs AABB",
    "Sphere vs OBB",
    "AABB vs AABB",
    "AABB vs OBB",
    "OBB vs OBB",
    "Capsule vs Capsule",
    "Convex Hull vs Convex Hull",
    
    "Ray vs Plane",
    "Ray vs AABB",
    "Ray vs OBB",
    "Ray vs Sphere",
    "Ray vs Triangle",
    "Ray vs Capsule",
    "Ray vs Cylinder",
    "Ray vs Line Segment",
    
    "Line vs Line",
    "Shortest Line Segment Between Line Segments",
    "Shortest Line Segment Between Lines",
    
    "AABB vs Frustum",
    "Sphere vs Frustum",
    "Point vs Frustum",
    
    "Barycentric Coordinates",
    "Convex Hull",
    "Polygon",
};

maths_test_function test_functions[] = {
    test_aabb_vs_plane,
    test_sphere_vs_plane,
    test_capsule_vs_plane,
    test_cone_vs_plane,
    
    test_project,
    
    test_point_plane,
    test_point_sphere,
    test_point_aabb,
    test_point_line,
    test_point_ray,
    test_point_triangle,
    test_point_obb,
    test_point_cone,
    
    test_sphere_vs_sphere,
    test_sphere_vs_capsule,
    test_sphere_vs_aabb,
    test_sphere_vs_obb,
    test_aabb_vs_aabb,
    test_aabb_vs_obb,
    test_obb_vs_obb,
    test_capsule_vs_capsule,
    test_convex_hull_vs_convex_hull,
    
    test_ray_plane_intersect,
    test_ray_vs_aabb,
    test_ray_vs_obb,
    test_ray_vs_sphere,
    test_ray_triangle,
    test_ray_vs_capsule,
    test_ray_vs_cylinder,
    test_ray_vs_line_segment,
    
    test_line_vs_line,
    test_line_segment_between_line_segments,
    test_line_segment_between_lines,
    
    test_aabb_vs_frustum,
    test_sphere_vs_frustum,
    test_point_vs_frustum,
    
    test_barycentric,
    test_convex_hull,
    test_polygon
};
// clang-format on

static_assert(PEN_ARRAY_SIZE(test_functions) == PEN_ARRAY_SIZE(test_names), "array size mismatch");

int _test_stack_depth = 0;

void maths_test_ui(ecs_scene* scene)
{
    static s32 test_index = 0;
    static s32 test_variant_index = 0;

    static bool animate = false;
    static bool initialise = true;
    static bool gen_tests = false;
    bool        opened = true;

    ImGui::Begin("Maths Functions", &opened, ImGuiWindowFlags_AlwaysAutoResize);

    if (ImGui::Combo("Test", &test_index, test_names, PEN_ARRAY_SIZE(test_names)))
        initialise = true;

    ImGui::SameLine();
    ImGui::Checkbox("Animate", &animate);
    ImGui::Separator();

    if (animate)
    {
        static f32         swap_timer = 0.0f;
        static pen::timer* at = pen::timer_create();
        swap_timer += pen::timer_elapsed_ms(at);
        pen::timer_start(at);

        if (swap_timer > 500.0f)
        {
            initialise = true;
            swap_timer = 0.0f;
            test_index++;
        }

        if (test_index >= PEN_ARRAY_SIZE(test_names))
            test_index = 0;
    }

    if (gen_tests)
    {
        scene->view_flags |= e_scene_view_flags::hide;
        initialise = true;

        if (test_variant_index >= 10)
        {
            std::cout << "}\n";
            test_index++;
            test_variant_index = 0;
        }

        if (test_variant_index == 0)
            std::cout << "TEST_CASE( \"" << test_names[test_index] << "\", \"[maths]\") \n{\n";

        test_variant_index++;
        _test_stack_depth = 0;
    }

    test_functions[test_index](scene, initialise);

    if (gen_tests)
        _test_stack_depth = -1;

    initialise = false;
    
    if(ImGui::CollapsingHeader("Curves"))
    {
        static f32 points[32];
                
        EASING_FUNC(smooth_start2);
        EASING_FUNC(smooth_start3);
        EASING_FUNC(smooth_start4);
        EASING_FUNC(smooth_start5);
        
        EASING_FUNC(smooth_stop2);
        EASING_FUNC(smooth_stop3);
        EASING_FUNC(smooth_stop4);
        EASING_FUNC(smooth_stop5);
        
        ImGui::Separator();
        ImGui::PushID("parabola");
        {
            static f32 k = 1.0f;

            f32 t = 0.0f;
            for(u32 i = 0; i < 32; ++i)
            {
                points[i] = parabola(t, k);
                t += 1.0f / 32.0f;
            }
            ImGui::PlotLines("parabola", points, 32);
            
            ImGui::SliderFloat("k", &k, 0.0, 10.0f);
        }
        ImGui::PopID();
        
        ImGui::Separator();
        ImGui::PushID("exp_step");
        {
            static f32 k = 1.0f;
            static f32 n = 1.0f;

            f32 t = 0.0f;
            for(u32 i = 0; i < 32; ++i)
            {
                points[i] = exp_step(t, k, n);
                t += 1.0f / 32.0f;
            }
            ImGui::PlotLines("exp_step", points, 32);
            
            ImGui::SliderFloat("k", &k, 0.0, 10.0f);
            ImGui::SliderFloat("n", &n, 0.0, 10.0f);
        }
        ImGui::PopID();
        
        ImGui::Separator();
        ImGui::PushID("impulse");
        {
            static f32 k = 1.0f;

            f32 t = 0.0f;
            for(u32 i = 0; i < 32; ++i)
            {
                points[i] = impulse(k, t);
                t += 1.0f / 32.0f;
            }
            ImGui::PlotLines("impulse", points, 32);
            
            ImGui::SliderFloat("k", &k, 0.0, 10.0f);
        }
        ImGui::PopID();
        
        ImGui::Separator();
        ImGui::PushID("cubic_pulse");
        {
            static f32 c = 1.0f;
            static f32 w = 1.0f;

            f32 t = 0.0f;
            for(u32 i = 0; i < 32; ++i)
            {
                points[i] = cubic_pulse(c, w, t);
                t += 1.0f / 32.0f;
            }
            ImGui::PlotLines("cubic_pulse", points, 32);
            
            ImGui::SliderFloat("c", &c, 0.0, 1.0f);
            ImGui::SliderFloat("w", &w, 0.0, 1.0f);
        }
        ImGui::PopID();
        
        ImGui::Separator();
        ImGui::PushID("exp_sustained_impulse");
        {
            static f32 f = 1.0f;
            static f32 k = 1.0f;

            f32 t = 0.0f;
            for(u32 i = 0; i < 32; ++i)
            {
                points[i] = exp_sustained_impulse(t, f, k);
                t += 1.0f / 32.0f;
            }
            ImGui::PlotLines("exp_sustained_impulse", points, 32);
            
            ImGui::SliderFloat("f", &f, 0.0, 1.0f);
            ImGui::SliderFloat("k", &k, 0.0, 10.0f);
        }
        ImGui::PopID();
        
        ImGui::Separator();
        ImGui::PushID("sinc");
        {
            static f32 k = 4.5f;

            f32 t = 0.0f;
            for(u32 i = 0; i < 32; ++i)
            {
                points[i] = sinc(t, k);
                t += 1.0f / 32.0f;
            }
            ImGui::PlotLines("sinc", points, 32);
            
            ImGui::SliderFloat("k", &k, 0.0, 10.0f);
        }
        ImGui::PopID();
        
        ImGui::Separator();
        ImGui::PushID("gain");
        {
            static f32 k = 5.0f;

            f32 t = 0.0f;
            for(u32 i = 0; i < 32; ++i)
            {
                points[i] = gain(t, k);
                t += 1.0f / 32.0f;
            }
            ImGui::PlotLines("gain", points, 32);
            
            ImGui::SliderFloat("k", &k, 0.0, 10.0f);
        }
        ImGui::PopID();
        
        ImGui::Separator();
        ImGui::PushID("almost_identity");
        {
            static f32 m = 0.1f;
            static f32 n = 0.01f;

            f32 t = 0.0f;
            for(u32 i = 0; i < 32; ++i)
            {
                points[i] = almost_identity(t, m, n);
                t += 1.0f / 32.0f;
            }
            ImGui::PlotLines("almost_identity", points, 32);
            
            ImGui::SliderFloat("m", &m, 0.0, 10.0f);
            ImGui::SliderFloat("n", &n, 0.0, 10.0f);
        }
        ImGui::PopID();
        
        ImGui::Separator();
        ImGui::PushID("integral_smoothstep");
        {
            static f32 t = 5.0f;

            f32 x = 0.0f;
            for(u32 i = 0; i < 32; ++i)
            {
                points[i] = integral_smoothstep(x, t);
                x += 1.0f / 32.0f;
            }
            ImGui::PlotLines("integral_smoothstep", points, 32);
            
            ImGui::SliderFloat("t", &t, 0.0, 10.0f);
        }
        ImGui::PopID();
        
        ImGui::Separator();
        ImGui::PushID("quad_impulse");
        {
            static f32 k = 7.0f;

            f32 x = 0.0f;
            for(u32 i = 0; i < 32; ++i)
            {
                points[i] = quad_impulse(k, x);
                x += 1.0f / 32.0f;
            }
            ImGui::PlotLines("quad_impulse", points, 32);
            
            ImGui::SliderFloat("k", &k, 0.0, 10.0f);
        }
        ImGui::PopID();
        
        ImGui::Separator();
        ImGui::PushID("poly_impulse");
        {
            static f32 k = 7.0f;
            static f32 n = 4.0f;

            f32 x = 0.0f;
            for(u32 i = 0; i < 32; ++i)
            {
                points[i] = poly_impulse(k, n, x);
                x += 1.0f / 32.0f;
            }
            ImGui::PlotLines("poly_impulse", points, 32);
            
            ImGui::SliderFloat("k", &k, 0.0, 10.0f);
            ImGui::SliderFloat("n", &n, 0.0, 10.0f);
        }
        ImGui::PopID();
    }
    
    if(ImGui::CollapsingHeader("Colours"))
    {
        static u32 rgba = 0xff00ffff;
        
        static vec4f col = vec4f::magenta();
        ImGui::InputFloat4("vec4f", &col.v[0]);
        rgba = maths::vec4f_to_rgba8(col);
        ImGui::Separator();
        
        ImGui::PushStyleColor(ImGuiCol_Button, rgba);
        ImGui::Button("vec4f_to_rgba");
        ImGui::PopStyleColor();
        
        vec4f result = maths::rgba8_to_vec4f(rgba);
        ImGui::InputFloat4("rgba8_to_vec4f", &result.v[0]);
    }
    
    if (ImGui::CollapsingHeader("Controls"))
    {
        ImGui::Text("Alt / Option + Left Mouse Drag To Rotate");
        ImGui::Text("Cmd / Ctrl + Left Mouse Drag To Pan");
        ImGui::Text("Mouse Wheel / Scroll To Zoom");
    }
    
    ImGui::End();
}

void example_setup(ecs::ecs_scene* scene, camera& cam)
{
    dev_ui::enable(true);
    dev_ui::enable_main_menu_bar(false);
    
    scene->view_flags &= ~e_scene_view_flags::hide_debug;

    // create constant col material
    constant_colour_material->material_name = "constant_colour";
    constant_colour_material->shader_name = "pmfx_utility";
    constant_colour_material->id_shader = PEN_HASH("pmfx_utility");
    constant_colour_material->id_technique = PEN_HASH("constant_colour");
    add_material_resource(constant_colour_material);
}

void example_update(ecs::ecs_scene* scene, camera& cam, f32 dt)
{
    maths_test_ui(scene);
}
