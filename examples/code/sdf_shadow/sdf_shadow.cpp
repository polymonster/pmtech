#include "../example_common.h"

using namespace put;
using namespace put::ecs;

pen::window_creation_params pen_window{
    1280,        // width
    720,         // height
    4,           // MSAA samples
    "sdf_shadow" // window title / process name
};

vec3f random_vel(f32 min, f32 max)
{
    f32 x = min + (((f32)(rand() % RAND_MAX) / RAND_MAX) * (max - min));
    f32 y = min + (((f32)(rand() % RAND_MAX) / RAND_MAX) * (max - min));
    f32 z = min + (((f32)(rand() % RAND_MAX) / RAND_MAX) * (max - min));

    return vec3f(x, y, z);
}

void animate_lights(ecs_scene* scene, f32 dt)
{
    if (scene->flags & PAUSE_UPDATE)
        return;

    extents e = scene->renderable_extents;
    e.min -= vec3f(10.0f, 2.0f, 10.0f);
    e.max += vec3f(10.0f, 10.0f, 10.0f);

    static f32 t = 0.0f;
    t += dt * 0.001f;

    static vec3f s_velocities[MAX_FORWARD_LIGHTS];
    static bool  s_initialise = true;
    if (s_initialise)
    {
        s_initialise = false;
        srand(pen::get_time_us());

        for (u32 i = 0; i < MAX_FORWARD_LIGHTS; ++i)
            s_velocities[i] = random_vel(-1.0f, 1.0f);
    }

    u32 vel_index = 0;

    for (u32 n = 0; n < scene->num_entities; ++n)
    {
        if (!(scene->entities[n] & CMP_LIGHT))
            continue;

        if (scene->lights[n].type == LIGHT_TYPE_DIR)
            continue;

        if (vel_index == 0)
        {
            f32 tx = sin(t);
            scene->transforms[n].translation = vec3f(tx * -20.0f, 4.0f, 15.0f);
            scene->entities[n] |= CMP_TRANSFORM;
        }

        if (vel_index == 1)
        {
            f32 tz = cos(t);
            scene->transforms[n].translation = vec3f(-15.0f, 3.0f, tz * 20.0f);
            scene->entities[n] |= CMP_TRANSFORM;
        }

        if (vel_index == 2)
        {
            f32 tx = sin(t * 0.5);
            f32 tz = cos(t * 0.5);
            scene->transforms[n].translation = vec3f(tx * 40.0f, 1.0f, tz * 30.0f);
            scene->entities[n] |= CMP_TRANSFORM;
        }

        if (vel_index == 3)
        {
            f32 tx = cos(t * 0.25);
            f32 tz = sin(t * 0.25);
            scene->transforms[n].translation = vec3f(tx * 30.0f, 6.0f, tz * 30.0f);
            scene->entities[n] |= CMP_TRANSFORM;
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
