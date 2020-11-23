#include "camera.h"
#include "types.h"

using put::camera;

namespace put
{
    namespace ecs
    {
        struct ecs_scene;

        // run time detect of simd extensions and setup function pointers to the fastest implementation
        void simd_init();

        // frustum_cull_xxx_scalar versions scalar float cross platform implementations,
        void filter_entities_scalar(const ecs_scene* scene, u32** filtered_entities_out);
        void frustum_cull_aabb_scalar(const ecs_scene* scene, const camera* cam, u32* entities_in, u32** entities_out);
        void frustum_cull_sphere_scalar(const ecs_scene* scene, const camera* cam, u32* entities_in, u32** entities_out);

        // frustum_cull_xxx functions are replaced by simd where available and fall back to scalar if no simd is available
        void frustum_cull_aabb(const ecs_scene* scene, const camera* cam, u32* entities_in, u32** entities_out);
        void frustum_cull_sphere(const ecs_scene* scene, const camera* cam, u32* entities_in, u32** entities_out);
    } // namespace ecs
} // namespace put
