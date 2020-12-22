#include "../example_common.h"

#include "../../shader_structs/forward_render.h"

using namespace put;
using namespace put::ecs;

namespace pen
{
    pen_creation_params pen_entry(int argc, char** argv)
    {
        pen::pen_creation_params p;
        p.window_width = 1280;
        p.window_height = 720;
        p.window_title = "rasterizer_state";
        p.window_sample_count = 4;
        p.user_thread_function = user_setup;
        p.flags = pen::e_pen_create_flags::renderer;
        return p;
    }
} // namespace pen

namespace
{
    const u32 num_rs = 4; // cull front, cull back, cull none, wireframe
    u32       cube_entity[num_rs];

    hash_id raster_states[] = {PEN_HASH("default"), PEN_HASH("front_face_cull"), PEN_HASH("no_cull"), PEN_HASH("wireframe")};
} // namespace

void render_raster_states(const scene_view& view)
{
    ecs_scene* scene = view.scene;

    for (u32 i = 0; i < num_rs; ++i)
    {
        u32 ci = cube_entity[i];

        cmp_geometry& geom = scene->geometries[ci];

        pmfx::set_technique_perm(view.pmfx_shader, view.id_technique, 0);
        pen::renderer_set_constant_buffer(view.cb_view, 0, pen::CBUFFER_BIND_PS | pen::CBUFFER_BIND_VS);
        pen::renderer_set_constant_buffer(scene->cbuffer[ci], 1, pen::CBUFFER_BIND_PS | pen::CBUFFER_BIND_VS);
        pen::renderer_set_constant_buffer(scene->materials[ci].material_cbuffer, 7,
                                          pen::CBUFFER_BIND_PS | pen::CBUFFER_BIND_VS);

        // set textures
        cmp_samplers& samplers = scene->samplers[ci];
        for (u32 s = 0; s < e_pmfx_constants::max_technique_sampler_bindings; ++s)
        {
            if (!samplers.sb[s].handle)
                continue;

            pen::renderer_set_texture(samplers.sb[s].handle, samplers.sb[s].sampler_state, samplers.sb[s].sampler_unit,
                                      pen::TEXTURE_BIND_PS);
        }

        // ltc lookups / shadow buffers
        {
            static u32 ltc_mat = put::load_texture("data/textures/ltc/ltc_mat.dds");
            static u32 ltc_mag = put::load_texture("data/textures/ltc/ltc_amp.dds");

            static hash_id id_clamp_linear = PEN_HASH("clamp_linear");
            u32            clamp_linear = pmfx::get_render_state(id_clamp_linear, pmfx::e_render_state::sampler);

            pen::renderer_set_texture(ltc_mat, clamp_linear, 13, pen::TEXTURE_BIND_PS);
            pen::renderer_set_texture(ltc_mag, clamp_linear, 12, pen::TEXTURE_BIND_PS);

            pen::renderer_set_constant_buffer(scene->forward_light_buffer, 3, pen::CBUFFER_BIND_PS);
            pen::renderer_set_constant_buffer(scene->shadow_map_buffer, 4, pen::CBUFFER_BIND_PS);
            pen::renderer_set_constant_buffer(scene->area_light_buffer, 6, pen::CBUFFER_BIND_PS);
        }

        pen::renderer_set_constant_buffer(scene->forward_light_buffer, 3, pen::CBUFFER_BIND_PS);
        pen::renderer_set_vertex_buffer(geom.vertex_buffer, 0, geom.vertex_size, 0);
        pen::renderer_set_index_buffer(geom.index_buffer, geom.index_type, 0);

        // set rs
        u32 rs = pmfx::get_render_state(raster_states[i], pmfx::e_render_state::rasterizer);
        pen::renderer_set_raster_state(rs);

        pen::renderer_draw_indexed(geom.num_indices, 0, 0, PEN_PT_TRIANGLELIST);
    }
}

void example_setup(ecs::ecs_scene* scene, camera& cam)
{
    put::scene_view_renderer svr_raster_states;
    svr_raster_states.name = "ces_render_raster_states";
    svr_raster_states.id_name = PEN_HASH(svr_raster_states.name.c_str());
    svr_raster_states.render_function = &render_raster_states;
    pmfx::register_scene_view_renderer(svr_raster_states);

    pmfx::init("data/configs/rasterizer_state.jsn");

    clear_scene(scene);

    material_resource* default_material = get_material_resource(PEN_HASH("default_material"));

    geometry_resource* box = get_geometry_resource(PEN_HASH("cube"));

    // add light
    u32 light = get_new_entity(scene);
    scene->names[light] = "front_light";
    scene->id_name[light] = PEN_HASH("front_light");
    scene->lights[light].colour = vec3f::one();
    scene->lights[light].direction = vec3f::one();
    scene->lights[light].type = e_light_type::dir;
    scene->transforms[light].translation = vec3f::zero();
    scene->transforms[light].rotation = quat();
    scene->transforms[light].scale = vec3f::one();
    scene->entities[light] |= e_cmp::light;
    scene->entities[light] |= e_cmp::transform;

    // add a few cubes
    vec3f pos = vec3f(-(f32)(num_rs - 1) * 2.0f * 5.0f, 0.0f, 0.0f);
    for (u32 i = 0; i < num_rs; ++i)
    {
        u32 ci = get_new_entity(scene);
        cube_entity[i] = ci;
        scene->names[ci] = "cube";
        scene->transforms[ci].translation = pos;
        scene->transforms[ci].rotation = quat();
        scene->transforms[ci].scale = vec3f(5.0f, 5.0f, 5.0f);
        scene->entities[ci] |= e_cmp::transform;
        scene->parents[ci] = ci;
        instantiate_geometry(box, scene, ci);
        instantiate_material(default_material, scene, ci);
        instantiate_model_cbuffer(scene, ci);

        pos.x += 4.0f * 5.0f;
    }
}

void example_update(ecs::ecs_scene* scene, camera& cam, f32 dt)
{
}
