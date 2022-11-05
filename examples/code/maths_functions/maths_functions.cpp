#define MATHS_PRINT_TESTS 1
#include "../example_common.h"
#include "maths/util.h"

using namespace put;
using namespace ecs;

// tests ------------------------------------------------------------
// point inside frustum
// ray vs sphere
// ray triangle
// convex hull from points
// point inside hull
// line_segment_between_line_segment
// quat
// point inside triangle, closest point on triangle

// demos ------------------------------------------------------------
// point inside poly
// capsule_vs_plane
// cone_vs_plane

// functions --------------------------------------------------------
// ray_vs_capsule
// sphere_vs_capsule
// point sphere distance
// closest point on hull
// closest point on poly
// point hull distance
// point poly distance
// closest point on cone
// point cone distance
// ray vs line segment

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

// Add debug sphere with randon radius within range extents and at random position within extents
void add_debug_capsule(const debug_extents& extents, ecs_scene* scene, debug_capsule& capsule)
{
    geometry_resource* capsule_res = get_geometry_resource(PEN_HASH("capsule"));

    vec3f size = fabs(random_vec_range(extents));

    u32 node = ecs::get_new_entity(scene);
    scene->names[node] = "sphere";

    scene->transforms[node].rotation = quat();
    scene->transforms[node].scale = vec3f(size.x, size.y, size.x);
    scene->transforms[node].translation = random_vec_range(extents);

    scene->entities[node] |= e_cmp::transform;
    scene->parents[node] = node;

    instantiate_geometry(capsule_res, scene, node);
    instantiate_material(constant_colour_material, scene, node);
    instantiate_model_cbuffer(scene, node);

    capsule.cp1 = scene->transforms[node].translation - vec3f(0.0f, size.y, 0.0f);
    capsule.cp2 = scene->transforms[node].translation + vec3f(0.0f, size.y, 0.0f);
    capsule.radius = size.x;
    capsule.node = node;
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

    vec3f ip = maths::ray_plane_intersect(ray.origin, ray.direction, plane.point, plane.normal);

    // debug output
    dbg::add_point(ip, 0.3f, vec4f::red());

    dbg::add_plane(plane.point, plane.normal);

    dbg::add_line(ray.origin, ray.origin + ray.direction * 500.0f, vec4f::green());
}

void test_ray_sphere_intersect(ecs_scene* scene, bool initialise)
{
    static debug_sphere sphere;
    static debug_ray    ray;

    static debug_extents e = {vec3f(-10.0, -10.0, -10.0), vec3f(10.0, 10.0, 10.0)};

    bool randomise = ImGui::Button("Randomise");

    if (initialise || randomise)
    {
        ecs::clear_scene(scene);

        add_debug_ray(e, scene, ray);
        add_debug_sphere(e, scene, sphere);

        ecs::update_scene(scene, 1.0f / 60.0f);
    }

    vec3f ip; 
    bool intersect = maths::ray_sphere_intersect(ray.origin, ray.direction, sphere.pos, sphere.radius, ip);

    // debug output
    dbg::add_point(ip, 0.3f, vec4f::green());

    vec4f col = vec4f::green();

    if (intersect)
        col = vec4f::red();

    dbg::add_line(ray.origin, ray.origin + ray.direction * 500.0f, col);

    scene->draw_call_data[sphere.node].v2 = col;
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

        add_debug_ray(e, scene, ray);
        add_debug_sphere(e, scene, sphere);

        ecs::update_scene(scene, 1.0f / 60.0f);
    }
    
    vec3f ip = vec3f::zero();
    bool  intersect = maths::ray_sphere_intersect(ray.origin, ray.direction, sphere.pos, sphere.radius, ip);

    vec4f col = vec4f::green();

    if (intersect)
        col = vec4f::red();

    dbg::add_line(ray.origin, ray.origin + ray.direction * 500.0f, col);

    // debug output
    if (intersect)
        dbg::add_point(ip, 0.5f, vec4f::white());
    
    scene->draw_call_data[sphere.node].v2 = col;
}

void test_point_plane_distance(ecs_scene* scene, bool initialise)
{
    static debug_point point;
    static debug_plane plane;

    static debug_extents e = {vec3f(-10.0, -10.0, -10.0), vec3f(10.0, 10.0, 10.0)};

    bool randomise = ImGui::Button("Randomise");

    if (initialise || randomise)
    {
        ecs::clear_scene(scene);

        add_debug_point(e, scene, point);
        add_debug_plane(e, scene, plane);

        ecs::update_scene(scene, 1.0f / 60.0f);
    }

    f32 distance = maths::point_plane_distance(point.point, plane.point, plane.normal);

    // debug output
    ImGui::Text("Distance %f", distance);

    dbg::add_point(point.point, 0.3f, vec4f::green());

    dbg::add_plane(plane.point, plane.normal);
}

const c8* classifications[]{
    "Intersects",
    "Behind",
    "Infront",
};

const vec4f classification_colours[] = {vec4f::red(), vec4f::cyan(), vec4f::green()};

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

    scene->draw_call_data[capsule.node].v2 = vec4f(classification_colours[c]);

    dbg::add_plane(plane.point, plane.normal);
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

    // TODO: cone util
    f32 r = scene->transforms[cone.node].scale.x;
    f32 h = scene->transforms[cone.node].scale.y;
    
    vec3f cv = normalize(scene->world_matrices[cone.node].get_column(1).xyz);
    vec3f cp = scene->world_matrices[cone.node].get_translation() - cv * h;
    
    u32 c = maths::cone_vs_plane(cp, cv, h, r, plane.point, plane.normal);

    // debug output
    ImGui::Text("Classification %s", classifications[c]);

    scene->draw_call_data[cone.node].v2 = vec4f(classification_colours[c]);

    dbg::add_plane(plane.point, plane.normal);
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

    // vec3f unproj = maths::unproject_sc(screen_point, view_proj, vp);

    dbg::add_point(point.point, 0.5f, vec4f::magenta());
    dbg::add_point(point.point, 1.0f, vec4f::cyan());
    dbg::add_point_2f(screen_point.xy, vec4f::green());
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

    vec4f col = inside ? vec4f::red() : vec4f::green();

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
    bool intersect = maths::ray_triangle_intersect(ray.origin, ray.direction, tri.t0, tri.t1, tri.t2, ip);
    
    vec4f col = vec4f::green();
    if (intersect)
        col = vec4f::red();

    dbg::add_line(ray.origin, ray.origin + ray.direction * 500.0f, col);

    // intersection point
    if (intersect)
        dbg::add_point(ip, 0.5f, vec4f::white());
    
    // triangle
    dbg::add_triangle(tri.t0, tri.t1, tri.t2, col);
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

    // debug output
    vec4f col = vec4f::green();
    vec4f col2 = vec4f::white();
    if (i)
        col = vec4f::red();

    dbg::add_point(cp, 0.3f, col2);
    dbg::add_line(cp, point.point, col2);

    dbg::add_point(point.point, 0.4f, col);

    scene->draw_call_data[sphere.node].v2 = vec4f(col);
}

void test_point_cone(ecs_scene* scene, bool initialise)
{
    static debug_point point;
    static debug_cone  cone;

    static debug_extents e = {vec3f(-5.0, -5.0, -10.0), vec3f(5.0, 5.0, 5.0)};

    bool randomise = ImGui::Button("Randomise");

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

    bool i = maths::point_inside_cone(point.point, cp, cv, h, r);

    // debug output
    vec4f col = vec4f::green();
    if (i)
        col = vec4f::red();

    dbg::add_point(point.point, 0.4f, col);

    scene->draw_call_data[cone.node].v2 = vec4f(col);
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
    
    static std::vector<debug_point> debug_points;
    static std::vector<debug_point> test_inside_points;
    
    static std::vector<vec3f> vec3_points;
    static std::vector<vec2f> vec2_points;
    static std::vector<vec2f> hull_points;
    
    static u32 num_src_points;
    
    bool randomise = ImGui::Button("Randomise");
    ImGui::Text("magenta points = src point cloud");
    ImGui::Text("orange point = rightmost convex hull point (start of winding)");
    ImGui::Text("white line = hull generated by src points (discard the inside)");
    ImGui::Text("green points = outside hull");
    ImGui::Text("red points = inside hull");
    
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
            add_debug_point(e, scene, test_inside_points[i]);
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
}

typedef void (*maths_test_function)(ecs_scene*, bool);

// clang-format off
const c8* test_names[]{
    "Point Plane Distance",
    "Ray Plane Intersect",
    "Ray Sphere Intersect",
    "AABB Plane Classification",
    "Sphere Plane Classification",
    "Capsule Plane Classification",
    "Cone Plane Classification",
    "Project / Unproject",
    "Point Inside AABB / Closest Point on AABB",
    "Closest Point on Line / Point Segment Distance",
    "Closest Point on Ray",
    "Point Inside Triangle / Point Triangle Distance / Closest Point on Triangle / Get Normal",
    "Sphere vs Sphere",
    "Sphere vs AABB",
    "AABB vs AABB",
    "Point inside Sphere",
    "Ray vs AABB",
    "Ray vs OBB",
    "Ray vs Sphere",
    "Ray vs Triangle",
    "Line vs Line",
    "Point Inside OBB / Closest Point on OBB",
    "Point Inside Cone",
    "AABB vs Frustum",
    "Sphere vs Frustum",
    "Point Inside Frustum",
    "Barycentric Coordinates",
    "Convex Hull",
    "Shortest Line Segment Between Line Segments",
    "Shortest Line Segment Between Lines"
};

maths_test_function test_functions[] = {
    test_point_plane_distance,
    test_ray_plane_intersect,
    test_ray_sphere_intersect,
    test_aabb_vs_plane,
    test_sphere_vs_plane,
    test_capsule_vs_plane,
    test_cone_vs_plane,
    test_project,
    test_point_aabb,
    test_point_line,
    test_point_ray,
    test_point_triangle,
    test_sphere_vs_sphere,
    test_sphere_vs_aabb,
    test_aabb_vs_aabb,
    test_point_sphere,
    test_ray_vs_aabb,
    test_ray_vs_obb,
    test_ray_vs_sphere,
    test_ray_triangle,
    test_line_vs_line,
    test_point_obb,
    test_point_cone,
    test_aabb_vs_frustum,
    test_sphere_vs_frustum,
    test_point_vs_frustum,
    test_barycentric,
    test_convex_hull,
    test_line_segment_between_line_segments,
    test_line_segment_between_lines
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

    ImGui::Checkbox("Animate", &animate);

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
