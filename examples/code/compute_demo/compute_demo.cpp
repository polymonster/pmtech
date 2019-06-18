#include "../example_common.h"

using namespace put;
using namespace ecs;

pen::window_creation_params pen_window{
    1280,           // width
    720,            // height
    4,              // MSAA samples
    "compute_demo"  // window title / process name
};

void render_boids(const scene_view& view)
{
    ecs_scene*         scene = view.scene;
    geometry_resource* gr = get_geometry_resource(PEN_HASH("cube"));

    pmfx::set_technique_perm(view.pmfx_shader, view.technique, 0);

    pen::renderer_set_constant_buffer(view.cb_view, 0, pen::CBUFFER_BIND_PS | pen::CBUFFER_BIND_VS);
    pen::renderer_set_vertex_buffer(gr->vertex_buffer, 0, gr->vertex_size, 0);
    pen::renderer_set_index_buffer(gr->index_buffer, gr->index_type, 0);
    pen::renderer_draw_indexed_instanced(10, 0, gr->num_indices, 0, 0, PEN_PT_TRIANGLELIST);
}

void example_setup(ecs_scene* scene, camera& cam)
{
    put::scene_view_renderer svr_boids;
    svr_boids.name = "boids";
    svr_boids.id_name = PEN_HASH(svr_boids.name.c_str());
    svr_boids.render_function = &render_boids;
    pmfx::register_scene_view_renderer(svr_boids);

    pmfx::init("data/configs/compute_demo.jsn");
}

void example_update(ecs::ecs_scene* scene, camera& cam, f32 dt)
{

}
