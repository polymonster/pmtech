#include "../example_common.h"

#include "../../shader_structs/compute_demo.h"

using namespace put;
using namespace ecs;

namespace pen
{
    pen_creation_params pen_entry(int argc, char** argv)
    {
        pen::pen_creation_params p;
        p.window_width = 1280;
        p.window_height = 720;
        p.window_title = "compute_demo";
        p.window_sample_count = 4;
        p.user_thread_function = user_setup;
        p.flags = pen::e_pen_create_flags::renderer;
        return p;
    }
} // namespace pen

namespace
{
    u32 boids_buffer;
    u32 control_buffer;

    u32 boid_sqrt = 32;
    u32 num_boids = boid_sqrt * boid_sqrt;

    struct controls
    {
        float4 info; // x = boid count
        float4 target;
    };

    controls s_controls;
} // namespace

void render_boids(const scene_view& view)
{
    geometry_resource* gr = get_geometry_resource(PEN_HASH("cube"));
    pmm_renderable&    r = gr->renderable[e_pmm_renderable::full_vertex_buffer];

    pmfx::set_technique_perm(view.pmfx_shader, view.id_technique, 0);

    pen::renderer_set_structured_buffer(boids_buffer, 13, pen::SBUFFER_BIND_VS | pen::SBUFFER_BIND_READ);

    pen::renderer_set_constant_buffer(view.cb_view, 0, pen::CBUFFER_BIND_PS | pen::CBUFFER_BIND_VS);
    pen::renderer_set_vertex_buffer(r.vertex_buffer, 0, r.vertex_size, 0);
    pen::renderer_set_index_buffer(r.index_buffer, r.index_type, 0);
    pen::renderer_draw_indexed_instanced(num_boids, 0, r.num_indices, 0, 0, PEN_PT_TRIANGLELIST);

    // unbind
    pen::renderer_set_structured_buffer(0, 13, pen::SBUFFER_BIND_VS | pen::SBUFFER_BIND_READ);
}

void update_boids(const scene_view& view)
{
    ImGui::Begin("controls");
    s_controls.info.y = 0.0;
    if (ImGui::Button("reset"))
        s_controls.info.y = 1.0f;

    static f32 pt = pen::get_time_ms();
    static f32 tt = 0.0f;
    tt = pen::get_time_ms() - pt;

    if (tt > 3000.0f)
    {
        s_controls.target = vec4f(rand() % 100 - 50, rand() % 100 - 50, rand() % 100 - 50, 1.0f);
        tt = 0.0f;
    }

    pen::renderer_update_buffer(control_buffer, &s_controls, sizeof(controls));

    ImGui::End();

    pmfx::set_technique_perm(view.pmfx_shader, view.id_technique, 0);

    pen::renderer_set_constant_buffer(control_buffer, 0, pen::CBUFFER_BIND_CS);

    pen::renderer_set_structured_buffer(boids_buffer, 12, pen::SBUFFER_BIND_CS | pen::SBUFFER_BIND_RW);

    pen::renderer_dispatch_compute({num_boids, 1, 1}, {1024, 1, 1});

    // unbind
    pen::renderer_set_structured_buffer(0, 12, pen::SBUFFER_BIND_CS | pen::SBUFFER_BIND_RW);
}

void example_setup(ecs_scene* scene, camera& cam)
{
    scene->view_flags &= ~e_scene_view_flags::hide_debug;
    put::dev_ui::enable(true);

    put::scene_view_renderer svr_boids;
    svr_boids.name = "boids";
    svr_boids.id_name = PEN_HASH(svr_boids.name.c_str());
    svr_boids.render_function = &render_boids;
    pmfx::register_scene_view_renderer(svr_boids);

    put::scene_view_renderer svr_update_boids;
    svr_update_boids.name = "boids_update";
    svr_update_boids.id_name = PEN_HASH(svr_update_boids.name.c_str());
    svr_update_boids.render_function = &update_boids;
    pmfx::register_scene_view_renderer(svr_update_boids);

    pmfx::init("data/configs/compute_demo.jsn");

    compute_demo::boid* bb = new compute_demo::boid[num_boids];

    // create buffer for boids
    pen::buffer_creation_params bcp;
    bcp.cpu_access_flags = PEN_CPU_ACCESS_READ;
    bcp.usage_flags = PEN_USAGE_DEFAULT;
    bcp.bind_flags = PEN_BIND_SHADER_WRITE | PEN_BIND_SHADER_RESOURCE; // rw
    bcp.buffer_size = num_boids * sizeof(compute_demo::boid);
    bcp.data = bb;
    bcp.stride = sizeof(compute_demo::boid);

    boids_buffer = pen::renderer_create_buffer(bcp);

    s_controls.info = {(f32)num_boids, 1.0, (f32)boid_sqrt, 0.0};
    s_controls.target = vec4f::zero();

    // buffer to control the shader
    bcp.usage_flags = PEN_USAGE_DYNAMIC;
    bcp.cpu_access_flags = PEN_CPU_ACCESS_WRITE;
    bcp.bind_flags = PEN_BIND_CONSTANT_BUFFER;
    bcp.buffer_size = sizeof(controls);
    bcp.data = (void*)&s_controls;

    control_buffer = pen::renderer_create_buffer(bcp);
}

void example_update(ecs::ecs_scene* scene, camera& cam, f32 dt)
{
}
