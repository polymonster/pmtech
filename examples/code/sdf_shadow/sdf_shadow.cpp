#include "../example_common.h"

using namespace put;
using namespace put::ecs;

namespace pen
{
    pen_creation_params pen_entry(int argc, char** argv)
    {
        pen::pen_creation_params p;
        p.window_width = 1280;
        p.window_height =  720;
        p.window_title = "sdf_shadpow";
        p.window_sample_count = 4;
        p.user_thread_function = user_entry;
        p.flags = pen::e_pen_create_flags::renderer;
        return p;
    }
}

vec3f random_vel(f32 min, f32 max)
{
    f32 x = min + (((f32)(rand() % RAND_MAX) / RAND_MAX) * (max - min));
    f32 y = min + (((f32)(rand() % RAND_MAX) / RAND_MAX) * (max - min));
    f32 z = min + (((f32)(rand() % RAND_MAX) / RAND_MAX) * (max - min));

    return vec3f(x, y, z);
}

void animate_lights(ecs_scene* scene, f32 dt)
{
    if (scene->flags & e_scene_flags::pause_update)
        return;

    extents e = scene->renderable_extents;
    e.min -= vec3f(10.0f, 2.0f, 10.0f);
    e.max += vec3f(10.0f, 10.0f, 10.0f);

    static f32 t = 0.0f;
    t += dt * 0.001f;

    static vec3f s_velocities[e_scene_limits::max_forward_lights];
    static bool  s_initialise = true;
    if (s_initialise)
    {
        s_initialise = false;
        srand(pen::get_time_us());

        for (u32 i = 0; i < e_scene_limits::max_forward_lights; ++i)
            s_velocities[i] = random_vel(-1.0f, 1.0f);
    }

    u32 vel_index = 0;

    for (u32 n = 0; n < scene->num_entities; ++n)
    {
        if (!(scene->entities[n] & e_cmp::light))
            continue;

        if (scene->lights[n].type == e_light_type::dir)
            continue;

        if (vel_index == 0)
        {
            f32 tx = sin(t);
            scene->transforms[n].translation = vec3f(tx * -20.0f, 4.0f, 15.0f);
            scene->entities[n] |= e_cmp::transform;
        }

        if (vel_index == 1)
        {
            f32 tz = cos(t);
            scene->transforms[n].translation = vec3f(-15.0f, 3.0f, tz * 20.0f);
            scene->entities[n] |= e_cmp::transform;
        }

        if (vel_index == 2)
        {
            f32 tx = sin(t * 0.5);
            f32 tz = cos(t * 0.5);
            scene->transforms[n].translation = vec3f(tx * 40.0f, 1.0f, tz * 30.0f);
            scene->entities[n] |= e_cmp::transform;
        }

        if (vel_index == 3)
        {
            f32 tx = cos(t * 0.25);
            f32 tz = sin(t * 0.25);
            scene->transforms[n].translation = vec3f(tx * 30.0f, 6.0f, tz * 30.0f);
            scene->entities[n] |= e_cmp::transform;
        }

        ++vel_index;
    }
}

void example_setup(ecs::ecs_scene* scene, camera& cam)
{
    // load scene
    put::ecs::load_scene("data/scene/sdf.pms", scene);
}

void example_update(ecs::ecs_scene* scene, camera& cam, f32 dt)
{
    animate_lights(scene, dt * 10000.0f);
}
