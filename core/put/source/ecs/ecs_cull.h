#include "types.h"
#include "camera.h"

using put::camera;

namespace put
{
    namespace ecs
    {
        struct ecs_scene;
        
        void filter_entities_scalar(const ecs_scene* scene, u32** filtered_entities_out);
        void frustum_cull_aabb_scalar(const ecs_scene* scene, const camera* cam, const u32* entities_in, u32** entities_out);
        void frustum_cull_aabb_simd128(const ecs_scene* scene, const camera* cam, const u32* entities_in, u32** entities_out);
        void frustum_cull_aabb_simd256(const ecs_scene* scene, const camera* cam, const u32* entities_in, u32** entities_out);
        void frustum_cull_sphere_scalar(const ecs_scene* scene, const camera* cam, const u32* entities_in, u32** entities_out);
        void frustum_cull_sphere_simd128(const ecs_scene* scene, const camera* cam, const u32* entities_in, u32** entities_out);
        void frustum_cull_sphere_simd256(const ecs_scene* scene, const camera* cam, const u32* entities_in, u32** entities_out);
    }
}

