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
        p.window_title = "stencil_shadows";
        p.window_sample_count = 4;
        p.user_thread_function = user_setup;
        p.flags = pen::e_pen_create_flags::renderer;
        return p;
    }
} // namespace pen

namespace
{
    struct shadow_volume_vertex
    {
        vec4f pos;
        vec4f face_normal_0;
        vec4f face_normal_1;
    };

    struct shadow_volume_edge
    {
        vec4f pos_0;
        vec4f pos_1;
        vec4f face_normal_0;
        vec4f face_normal_1;
    };

    bool almost_equalf(vec4f v1, vec4f v2, f32 epsilon_sq)
    {
        if (dist2(v1, v2) < epsilon_sq)
            return true;

        return false;
    }

    u32 cb_single_light;
    u32 cube_start = -1;
    u32 cube_end = 0;

    shadow_volume_edge* s_sve;
    geometry_resource   s_sgr;
} // namespace

void render_stencil_shadows(const scene_view& view)
{
    ecs_scene*         scene = view.scene;
    geometry_resource* gr = get_geometry_resource(PEN_HASH("cube"));
    gr = &s_sgr;

    pmm_renderable& r = gr->renderable[e_pmm_renderable::full_vertex_buffer];

    // bind cbuffer
    pen::renderer_set_constant_buffer(cb_single_light, 10, pen::CBUFFER_BIND_VS | pen::CBUFFER_BIND_PS);
    pmfx::set_technique_perm(view.pmfx_shader, view.id_technique, 0);
    pen::renderer_set_constant_buffer(view.cb_view, 0, pen::CBUFFER_BIND_PS | pen::CBUFFER_BIND_VS);
    pen::renderer_set_vertex_buffer(r.vertex_buffer, 0, r.vertex_size, 0);
    pen::renderer_set_index_buffer(r.index_buffer, r.index_type, 0);

    for (u32 i = cube_start; i <= cube_end; ++i)
    {
        pen::renderer_set_constant_buffer(scene->cbuffer[i], 1, pen::CBUFFER_BIND_PS | pen::CBUFFER_BIND_VS);
        pen::renderer_draw_indexed(r.num_indices, 0, 0, PEN_PT_TRIANGLELIST);
    }
}

void render_stencil_tested(const scene_view& view)
{
    // bind cbuffer
    pen::renderer_set_constant_buffer(cb_single_light, 10, pen::CBUFFER_BIND_VS | pen::CBUFFER_BIND_PS);

    // draw
    ecs::render_scene_view(view);
}

void render_multi_pass_lights(const scene_view& view)
{
    ecs_scene* scene = view.scene;
    u32        count = 0;
    for (u32 n = 0; n < scene->num_entities; ++n)
    {
        if (!(scene->entities[n] & e_cmp::light))
            continue;

        u32 t = scene->lights[n].type;

        cmp_draw_call dc;
        dc.world_matrix = scene->world_matrices[n];

        vec3f pos = dc.world_matrix.get_translation();

        light_data ld = {};

        switch (t)
        {
            case e_light_type::dir:
                ld.pos_radius = vec4f(scene->lights[n].direction * 10000.0f, 0.0f);
                ld.dir_cutoff = vec4f(scene->lights[n].direction, 0.0f);
                ld.colour = vec4f(scene->lights[n].colour, 0.0f);
                break;
            case e_light_type::point:
                ld.pos_radius = vec4f(pos, scene->lights[n].radius);
                ld.dir_cutoff = vec4f(scene->lights[n].direction, 0.0f);
                ld.colour = vec4f(scene->lights[n].colour, 0.0f);
                break;
            case e_light_type::spot:
                ld.pos_radius = vec4f(pos, scene->lights[n].radius);
                ld.dir_cutoff = vec4f(-dc.world_matrix.get_column(1).xyz, scene->lights[n].cos_cutoff);
                ld.colour = vec4f(scene->lights[n].colour, 0.0f);
                ld.data = vec4f(scene->lights[n].spot_falloff, 0.0f, 0.0f, 0.0f);
                break;
            default:
                continue;
        }

        count++;

        // update light for this pass
        pen::renderer_update_buffer(cb_single_light, &ld, sizeof(light_data));

        // clear stencil
        pmfx::render_view(PEN_HASH("view_clear_stencil"));

        // render volumes
        pmfx::render_view(PEN_HASH("view_shadow_volume"));

        // render scene stencil tested by shadow
        pmfx::render_view(PEN_HASH("view_single_light"));
    }
}

void generate_edge_mesh(geometry_resource* gr, shadow_volume_edge** sve_out, geometry_resource* gr_out)
{
    pmm_renderable& r = gr->renderable[e_pmm_renderable::full_vertex_buffer];

    vertex_model* vm = (vertex_model*)r.cpu_vertex_buffer;
    u16*          ib = (u16*)r.cpu_index_buffer;

    static u32 k[] = {1, 2, 0};

    shadow_volume_edge* sve = nullptr;

    static const f32 epsilon = 0.1f;
    for (u32 i = 0; i < r.num_indices; i += 3)
    {
        shadow_volume_edge e[3];
        for (u32 j = 0; j < 3; ++j)
        {
            e[j].pos_0 = vm[ib[i + j]].pos;
            e[j].pos_1 = vm[ib[i + k[j]]].pos;
        }

        vec3f fn = maths::get_normal(e[0].pos_0.xyz, e[1].pos_0.xyz, e[2].pos_0.xyz);
        vec4f fn4 = vec4f(-fn, 1.0f);

        for (u32 j = 0; j < 3; ++j)
        {
            e[j].pos_0 = vm[ib[i + j]].pos;
            e[j].pos_1 = vm[ib[i + k[j]]].pos;

            s32 found = -1;
            u32 ne = sb_count(sve);
            for (u32 x = 0; x < ne; ++x)
            {
                if (almost_equal(sve[x].pos_0, e[j].pos_0, epsilon) && almost_equal(sve[x].pos_1, e[j].pos_1, epsilon))
                {
                    found = x;
                    break;
                }

                if (almost_equal(sve[x].pos_1, e[j].pos_0, epsilon) && almost_equal(sve[x].pos_0, e[j].pos_1, epsilon))
                {
                    found = x;
                    break;
                }
            }

            if (found == -1)
            {
                e[j].face_normal_0 = fn4;
                sb_push(sve, e[j]);
            }
            else
            {
                sve[found].face_normal_1 = fn4;
            }
        }
    }

    // for each edge add 4 vertices and 6 indices to make an extrable edge
    // the normals for each pair of verts are swapped to differentiate between them when extruding

    //     |
    // 0 ----- 1
    //
    // 2 ----- 3
    //     |

    shadow_volume_vertex* svv = nullptr;
    u16*                  sib = nullptr;

    u32 ne = sb_count(sve);
    u16 base_index = 0;
    for (u32 e = 0; e < ne; ++e)
    {
        shadow_volume_vertex v0;
        v0.pos = sve[e].pos_0;
        v0.face_normal_0 = sve[e].face_normal_0;
        v0.face_normal_1 = sve[e].face_normal_1;

        shadow_volume_vertex v1;
        v1.pos = sve[e].pos_1;
        v1.face_normal_0 = sve[e].face_normal_0;
        v1.face_normal_1 = sve[e].face_normal_1;

        shadow_volume_vertex v2;
        v2.pos = sve[e].pos_0;
        v2.face_normal_0 = sve[e].face_normal_1;
        v2.face_normal_1 = sve[e].face_normal_0;

        shadow_volume_vertex v3;
        v3.pos = sve[e].pos_1;
        v3.face_normal_0 = sve[e].face_normal_1;
        v3.face_normal_1 = sve[e].face_normal_0;

        sb_push(svv, v0);
        sb_push(svv, v1);
        sb_push(svv, v2);
        sb_push(svv, v3);

        sb_push(sib, base_index + 2);
        sb_push(sib, base_index + 1);
        sb_push(sib, base_index + 0);

        sb_push(sib, base_index + 2);
        sb_push(sib, base_index + 3);
        sb_push(sib, base_index + 1);

        base_index += 4;
    }

    pmm_renderable& r_out = gr_out->renderable[e_pmm_renderable::full_vertex_buffer];

    // vb
    u32                         num_verts = sb_count(svv);
    pen::buffer_creation_params bcp;
    bcp.usage_flags = PEN_USAGE_DEFAULT;
    bcp.bind_flags = PEN_BIND_VERTEX_BUFFER;
    bcp.cpu_access_flags = 0;
    bcp.buffer_size = sizeof(shadow_volume_vertex) * num_verts;
    bcp.data = (void*)svv;
    r_out.vertex_buffer = pen::renderer_create_buffer(bcp);

    // ib
    u32 num_indices = sb_count(sib);
    bcp.usage_flags = PEN_USAGE_DEFAULT;
    bcp.bind_flags = PEN_BIND_INDEX_BUFFER;
    bcp.cpu_access_flags = 0;
    bcp.buffer_size = 2 * num_indices;
    bcp.data = (void*)sib;
    r_out.index_buffer = pen::renderer_create_buffer(bcp);

    // info
    r_out.num_indices = num_indices;
    r_out.num_vertices = num_verts;
    r_out.vertex_size = sizeof(shadow_volume_vertex);
    r_out.index_type = PEN_FORMAT_R16_UINT;
    gr_out->min_extents = -vec3f::flt_max();
    gr_out->max_extents = vec3f::flt_max();
    gr_out->geometry_name = "shadow_mesh";
    gr_out->hash = PEN_HASH("shadow_mesh");
    gr_out->file_hash = PEN_HASH("shadow_mesh");
    gr_out->filename = "shadow_mesh";
    gr_out->p_skin = nullptr;

    // for debug purposes
    *sve_out = sve;
}

void example_setup(ecs::ecs_scene* scene, camera& cam)
{
    editor_set_transform_mode(e_transform_mode::physics);

    put::scene_view_renderer svr_stencil_shadow_volumes;
    svr_stencil_shadow_volumes.name = "stencil_shadow_volumes";
    svr_stencil_shadow_volumes.id_name = PEN_HASH(svr_stencil_shadow_volumes.name.c_str());
    svr_stencil_shadow_volumes.render_function = &render_stencil_shadows;
    pmfx::register_scene_view_renderer(svr_stencil_shadow_volumes);

    put::scene_view_renderer svr_stencil_tested;
    svr_stencil_tested.name = "scene_stencil_tested";
    svr_stencil_tested.id_name = PEN_HASH(svr_stencil_tested.name.c_str());
    svr_stencil_tested.render_function = &render_stencil_tested;
    pmfx::register_scene_view_renderer(svr_stencil_tested);

    put::scene_view_renderer svr_multi_pass_lights;
    svr_multi_pass_lights.name = "render_multi_pass_lights";
    svr_multi_pass_lights.id_name = PEN_HASH(svr_multi_pass_lights.name.c_str());
    svr_multi_pass_lights.render_function = &render_multi_pass_lights;
    pmfx::register_scene_view_renderer(svr_multi_pass_lights);

    pmfx::init("data/configs/stencil_shadows.jsn");

    clear_scene(scene);

    material_resource* default_material = get_material_resource(PEN_HASH("default_material"));

    geometry_resource* box = get_geometry_resource(PEN_HASH("cube"));

    // add lights
    u32 light = get_new_entity(scene);
    scene->names[light] = "front_light0";
    scene->id_name[light] = PEN_HASH("front_light0");
    scene->lights[light].colour = vec3f(0.2f, 0.8f, 0.1f);
    scene->lights[light].direction = vec3f::one() * vec3f(1.0f, 0.7f, 1.0f);
    scene->lights[light].type = e_light_type::dir;
    maths::xyz_to_azimuth_altitude(scene->lights[light].direction, scene->lights[light].azimuth,
                                   scene->lights[light].altitude);
    scene->transforms[light].translation = vec3f::zero();
    scene->transforms[light].rotation = quat();
    scene->transforms[light].scale = vec3f::one();
    scene->entities[light] |= e_cmp::light;
    scene->entities[light] |= e_cmp::transform;

    light = get_new_entity(scene);
    scene->names[light] = "front_light1";
    scene->id_name[light] = PEN_HASH("front_light1");
    scene->lights[light].colour = vec3f(0.8f, 0.2f, 0.2f);
    scene->lights[light].direction = vec3f::one() * vec3f(-1.0f, 0.7f, 1.0f);
    scene->lights[light].type = e_light_type::dir;
    maths::xyz_to_azimuth_altitude(scene->lights[light].direction, scene->lights[light].azimuth,
                                   scene->lights[light].altitude);
    scene->transforms[light].translation = vec3f::zero();
    scene->transforms[light].rotation = quat();
    scene->transforms[light].scale = vec3f::one();
    scene->entities[light] |= e_cmp::light;
    scene->entities[light] |= e_cmp::transform;

    light = get_new_entity(scene);
    scene->names[light] = "front_light2";
    scene->id_name[light] = PEN_HASH("front_light2");
    scene->lights[light].colour = vec3f(0.1f, 0.2f, 0.8f);
    scene->lights[light].direction = vec3f::one() * vec3f(-1.0f, 0.7f, -1.0f);
    scene->lights[light].type = e_light_type::dir;
    maths::xyz_to_azimuth_altitude(scene->lights[light].direction, scene->lights[light].azimuth,
                                   scene->lights[light].altitude);
    scene->transforms[light].translation = vec3f::zero();
    scene->transforms[light].rotation = quat();
    scene->transforms[light].scale = vec3f::one();
    scene->entities[light] |= e_cmp::light;
    scene->entities[light] |= e_cmp::transform;

    light = get_new_entity(scene);
    scene->names[light] = "front_light4";
    scene->id_name[light] = PEN_HASH("front_light4");
    scene->lights[light].colour = vec3f(0.6f, 0.1f, 0.8f);
    scene->lights[light].direction = vec3f::one() * vec3f(1.0f, 0.7f, -1.0f);
    scene->lights[light].type = e_light_type::dir;
    maths::xyz_to_azimuth_altitude(scene->lights[light].direction, scene->lights[light].azimuth,
                                   scene->lights[light].altitude);
    scene->transforms[light].translation = vec3f::zero();
    scene->transforms[light].rotation = quat();
    scene->transforms[light].scale = vec3f::one();
    scene->entities[light] |= e_cmp::light;
    scene->entities[light] |= e_cmp::transform;

    // ground
    u32 ground = get_new_entity(scene);
    scene->names[ground] = "ground";
    scene->transforms[ground].translation = vec3f::zero();
    scene->transforms[ground].rotation = quat();
    scene->transforms[ground].scale = vec3f(30.0f, 1.0f, 30.0f);
    scene->entities[ground] |= e_cmp::transform;
    scene->parents[ground] = ground;
    scene->physics_data[ground].rigid_body.shape = physics::e_shape::box;
    scene->physics_data[ground].rigid_body.mass = 0.0f;
    instantiate_geometry(box, scene, ground);
    instantiate_material(default_material, scene, ground);
    instantiate_model_cbuffer(scene, ground);
    instantiate_rigid_body(scene, ground);

    // cubes
    vec3f start_pos = vec3f(-6.0f, 6.0f, -6.0f);
    vec3f cur_pos = start_pos;

    for (s32 i = 0; i < 6; ++i)
    {
        cur_pos.y = start_pos.y;

        for (s32 j = 0; j < 6; ++j)
        {
            cur_pos.x = start_pos.x;

            for (s32 k = 0; k < 6; ++k)
            {
                f32 rx = ((f32)(rand() % 255) / 255.0f) * M_PI * 2.0f;
                f32 ry = ((f32)(rand() % 255) / 255.0f) * M_PI * 2.0f;
                f32 rz = ((f32)(rand() % 255) / 255.0f) * M_PI * 2.0f;

                u32 new_prim = get_new_entity(scene);
                scene->names[new_prim] = "box";
                scene->names[new_prim].appendf("%i", new_prim);
                scene->transforms[new_prim].rotation = quat(rx, ry, rz);
                scene->transforms[new_prim].scale = vec3f::one();
                scene->transforms[new_prim].translation = cur_pos;
                scene->entities[new_prim] |= e_cmp::transform;
                scene->parents[new_prim] = new_prim;
                instantiate_geometry(box, scene, new_prim);
                instantiate_material(default_material, scene, new_prim);
                instantiate_model_cbuffer(scene, new_prim);

                scene->physics_data[new_prim].rigid_body.shape = physics::e_shape::box;
                scene->physics_data[new_prim].rigid_body.mass = 1.0f;
                instantiate_rigid_body(scene, new_prim);

                cube_start = min(new_prim, cube_start);
                cube_end = max(new_prim, cube_end);

                cur_pos.x += 2.5f;
            }

            cur_pos.y += 2.5f;
        }

        cur_pos.z += 2.5f;
    }

    generate_edge_mesh(box, &s_sve, &s_sgr);

    // generate cbuffer for light passes
    pen::buffer_creation_params bcp;
    bcp.usage_flags = PEN_USAGE_DYNAMIC;
    bcp.bind_flags = PEN_BIND_CONSTANT_BUFFER;
    bcp.cpu_access_flags = PEN_CPU_ACCESS_WRITE;
    bcp.buffer_size = sizeof(light_data);
    bcp.data = nullptr;
    cb_single_light = pen::renderer_create_buffer(bcp);
}

void example_update(ecs::ecs_scene* scene, camera& cam, f32 dt)
{
    dt *= 0.1f;

    // rotate lights
    for (u32 i = 0; i < 4; ++i)
    {
        cmp_light& snl = scene->lights[i];
        snl.azimuth += dt * 10.0f;

        f32 dir = 1.0f;
        if (i >= 2)
            dir = -1.0f;

        snl.direction = maths::azimuth_altitude_to_xyz(snl.azimuth * dir, fabs(snl.altitude));
    }

#if 0 //debug
    geometry_resource* box = get_geometry_resource(PEN_HASH("cube"));
    
    vertex_model* vm = (vertex_model*)box->cpu_vertex_buffer;
    mat4 wm = scene->world_matrices[cube_entity];
    
    for(u32 i = 0; i < box->num_vertices; ++i)
    {
        vec4f tp = wm.transform_vector(vm[i].pos);
        dbg::add_point(tp.xyz, 1.0f);
    }
    
    static s32 edge = 0;
    static bool isolate = false;
    ImGui::Checkbox("Isolate", &isolate);
    ImGui::InputInt("Edge", &edge);
    
    u32 ne = sb_count(s_sve);
    for(u32 j = 0; j < ne; ++j)
    {
        if(isolate)
            if(j != edge)
                continue;
        
        vec4f p0 = wm.transform_vector(s_sve[j].pos_0);
        vec4f p1 = wm.transform_vector(s_sve[j].pos_1);
        
        vec3f c = p0.xyz + (p1.xyz - p0.xyz) * 0.5f;
        
        dbg::add_line(c, c + s_sve[j].face_normal_0.xyz, vec4f::red());
        dbg::add_line(c, c + s_sve[j].face_normal_1.xyz, vec4f::magenta());
        
        vec3f ld = -vec3f::one();
        f32 d0 = dot(ld, s_sve[j].face_normal_0.xyz);
        f32 d1 = dot(ld, s_sve[j].face_normal_1.xyz);
        
        if((d0 > 0.0f && d1 < 0.0f) || (d1 > 0.0f && d0 < 0.0f))
        {
            dbg::add_line(p0.xyz, p0.xyz + ld * 100.0f);
            dbg::add_line(p1.xyz, p1.xyz + ld * 100.0f);
            dbg::add_line(p0.xyz, p1.xyz, vec4f::white());
        }
        else
        {
            dbg::add_line(p0.xyz, p1.xyz, vec4f::green());
        }
    }
#endif
}
