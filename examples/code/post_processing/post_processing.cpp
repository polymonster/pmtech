#include "../example_common.h"

using namespace put;
using namespace put::ecs;

namespace pen
{
    pen_creation_params pen_entry(int argc, char** argv)
    {
        pen::pen_creation_params p;
        p.window_width = 1280;
        p.window_height = 720;
        p.window_title = "post_processing";
        p.window_sample_count = 4;
        p.user_thread_function = user_entry;
        p.flags = pen::e_pen_create_flags::renderer;
        return p;
    }
} // namespace pen

void example_setup(ecs::ecs_scene* scene, camera& cam)
{
    pmfx::init("data/configs/pp_demo.jsn");

    ecs::editor_enable_camera(false);
}

void example_update(ecs::ecs_scene* scene, camera& cam, f32 dt)
{
    // animate camera
    static bool start = true;

    if (start)
    {
        cam.pos = vec3f(0.0f, 0.0f, 0.0f);
        start = false;
    }

    cam.pos += vec3f::unit_x();

    cam.view.set_row(2, vec4f(0.0f, 0.0f, 1.0f, cam.pos.x));
    cam.view.set_row(1, vec4f(0.0f, 1.0f, 0.0f, cam.pos.y));
    cam.view.set_row(0, vec4f(1.0f, 0.0f, 0.0f, cam.pos.z));
    cam.view.set_row(3, vec4f(0.0f, 0.0f, 0.0f, 1.0f));

    cam.flags |= e_camera_flags::invalidated;
}
