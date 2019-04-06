#include "../example_common.h"

using namespace put;
using namespace put::ecs;

pen::window_creation_params pen_window{
    1280,             // width
    720,              // height
    4,                // MSAA samples
    "post_processing" // window title / process name
};

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
    
    cam.flags |= CF_INVALIDATED;
}
