#include "types.h"
#include "camera.h"

using put::camera;

namespace put
{
    namespace ecs
    {
        struct ecs_scene;
        
        // frustum_cull_xxx_scalar versions scalar float cross platform implementations,
        // frustum_cull_xxx functions are replaced by simd where available and fall back to scalar if no simd is available
        
        void filter_entities_scalar(const ecs_scene* scene, u32** filtered_entities_out);
        void frustum_cull_aabb_scalar(const ecs_scene* scene, const camera* cam, u32* entities_in, u32** entities_out);
        void frustum_cull_aabb(const ecs_scene* scene, const camera* cam, u32* entities_in, u32** entities_out);
        void frustum_cull_sphere_scalar(const ecs_scene* scene, const camera* cam, u32* entities_in, u32** entities_out);
        void frustum_cull_sphere(const ecs_scene* scene, const camera* cam, u32* entities_in, u32** entities_out);
    }
}

