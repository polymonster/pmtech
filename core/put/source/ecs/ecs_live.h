#include "ecs/ecs_scene.h"

namespace put
{
    struct live_context
    {
        ecs::ecs_scene* scene = nullptr;
        put::camera*    main_camera = nullptr;
        f32             dt = 0.0f;
        u32             load_index = 0;
    };
}

