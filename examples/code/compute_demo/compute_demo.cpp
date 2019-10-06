#include "../example_common.h"
#include "../../shader_structs/compute_demo.h"

using namespace put;
using namespace ecs;

pen::window_creation_params pen_window{
    1280,           // width
    720,            // height
    4,              // MSAA samples
    "compute_demo"  // window title / process name
};

namespace
{
    u32 boids_buffer;
    u32 num_boids = 32 * 32;
}

void render_boids(const scene_view& view)
{
    geometry_resource* gr = get_geometry_resource(PEN_HASH("cube"));

    pmfx::set_technique_perm(view.pmfx_shader, view.technique, 0);

    pen::renderer_set_constant_buffer(boids_buffer, 13-4, pen::CBUFFER_BIND_VS);
    pen::renderer_set_constant_buffer(view.cb_view, 0, pen::CBUFFER_BIND_PS | pen::CBUFFER_BIND_VS);
    pen::renderer_set_vertex_buffer(gr->vertex_buffer, 0, gr->vertex_size, 0);
    pen::renderer_set_index_buffer(gr->index_buffer, gr->index_type, 0);
    pen::renderer_draw_indexed_instanced(num_boids, 0, gr->num_indices, 0, 0, PEN_PT_TRIANGLELIST);
}

void update_boids(const scene_view& view)
{
    pmfx::set_technique_perm(view.pmfx_shader, view.technique, 0);
    
    pen::renderer_set_constant_buffer(boids_buffer, 12-4, pen::CBUFFER_BIND_CS);
    
    pen::renderer_dispatch_compute({32*32, 1, 1}, {4*4, 1, 1});
}

void example_setup(ecs_scene* scene, camera& cam)
{
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
    
    vec4f p = vec4f::zero();
    for(u32 i = 0; i < 32; ++i)
    {
        p.x = 0.0f;
        
        for(u32 j = 0; j < 32; ++j)
        {
            u32 ii = j*32+i;
            bb[ii].pos = p;
            
            p.x += 5.0f;
        }
        
        p.y += 5.0f;
    }
    
    // create buffer for boids
    pen::buffer_creation_params bcp;
    bcp.cpu_access_flags = 0;
    bcp.bind_flags = PEN_BIND_SHADER_WRITE;
    bcp.buffer_size = num_boids * sizeof(compute_demo::boid);
    bcp.data = bb;
    
    boids_buffer = pen::renderer_create_buffer(bcp);
}

void example_update(ecs::ecs_scene* scene, camera& cam, f32 dt)
{

}
