#include "../example_common.h"
#include "shader_structs/forward_render.h"

using namespace put;
using namespace put::ecs;
using namespace forward_render;

namespace pen
{
    pen_creation_params pen_entry(int argc, char** argv)
    {
        pen::pen_creation_params p;
        p.window_width = 1280;
        p.window_height = 720;
        p.window_title = "pmfx_renderer";
        p.window_sample_count = 4;
        p.user_thread_function = user_entry;
        p.flags = pen::e_pen_create_flags::renderer;
        return p;
    }
} // namespace pen

namespace
{
    const s32 max_lights = 100;

    u32 lights_start = 0;
    f32 light_radius = 50.0f;
    s32 num_lights = max_lights;
    f32 scene_size = 200.0f;

    vec3f anim_dir[max_lights];

    const c8* render_methods[] = {"forward_render", "forward_render_zprepass", "deferred_render", "deferred_render_msaa"};
    s32       render_method = 0;

    f32 user_thread_time = 0.0f;
} // namespace

void example_update(ecs::ecs_scene* scene, camera& cam, f32 dt)
{
    dt *= 5000.0f;

    ImGui::Begin("Lighting", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::SliderFloat("Light Radius", &light_radius, 0.0f, 300.0f);
    ImGui::SliderInt("Lights", &num_lights, 0, max_lights);

    if (ImGui::Combo("Method", &render_method, &render_methods[0], PEN_ARRAY_SIZE(render_methods)))
    {
        pmfx::set_view_set(render_methods[render_method]);
    }

    f32 render_gpu = 0.0f;
    f32 render_cpu = 0.0f;
    pen::renderer_get_present_time(render_cpu, render_gpu);

    ImGui::Separator();
    ImGui::Text("Stats:");
    ImGui::Text("User Thread: %2.2f ms", user_thread_time);
    ImGui::Text("Render Thread: %2.2f ms", render_cpu);
    ImGui::Text("GPU: %2.2f ms", render_gpu);
    ImGui::Separator();

    ImGui::End();

    static f32 t = 0.0f;
    t += dt * 0.01f;

    u32 lights_end = lights_start + num_lights;
    u32 light_nodes_end = lights_start + max_lights;
    u32 dir_index = 0;
    for (u32 i = lights_start; i < light_nodes_end; ++i)
    {
        if (i >= lights_end)
        {
            scene->entities[i] &= ~e_cmp::light;
            continue;
        }

        scene->transforms[i].translation += anim_dir[dir_index] * dt * 0.01f;
        scene->entities[i] |= e_cmp::transform;

        for (u32 j = 0; j < 3; ++j)
        {
            if (fabs(scene->transforms[i].translation[j]) > scene_size)
            {
                f32 rrx = (f32)(rand() % 255) / 255.0f;
                f32 rry = (f32)(rand() % 255) / 255.0f;
                f32 rrz = (f32)(rand() % 255) / 255.0f;

                anim_dir[dir_index] = vec3f(rrx, rry, rrz) * vec3f(2.0f) - vec3f(1.0);
                anim_dir[dir_index] += normalised(vec3f(0.0f, scene_size / 2.0f, 0.0f) - scene->transforms[i].translation);
            }
        }

        scene->entities[i] |= e_cmp::light;
        scene->lights[i].radius = light_radius;

        dir_index++;
    }
}

void example_setup(ecs::ecs_scene* scene, camera& main_camera)
{
    pmfx::init("data/configs/pmfx_demo.jsn");

    clear_scene(scene);

    // set camera start pos
    main_camera.zoom = 495.0f;
    main_camera.rot = vec2f(-0.8f, 0.37f);

    material_resource* default_material = get_material_resource(PEN_HASH("default_material"));
    geometry_resource* box_resource = get_geometry_resource(PEN_HASH("cube"));

    // pbt textures for material
    u32 albedo_tex = put::load_texture("data/textures/pbr/metalgrid2_basecolor.dds");
    u32 normal_tex = put::load_texture("data/textures/pbr/metalgrid2_normal.dds");
    u32 matallic_tex = put::load_texture("data/textures/pbr/metalgrid2_metallic.dds");
    u32 roughness_tex = put::load_texture("data/textures/pbr/metalgrid2_roughness.dds");

    // add some pillars for overdraw and illumination
    f32   num_pillar_rows = 20;
    f32   pillar_size = 20.0f;
    f32   d = scene_size / num_pillar_rows;
    f32   s = -d * (f32)num_pillar_rows;
    vec3f start_pos = vec3f(s, pillar_size, s);
    vec3f pos = start_pos;

    for (s32 i = 0; i < num_pillar_rows; ++i)
    {
        pos.z = start_pos.z;

        for (s32 j = 0; j < num_pillar_rows; ++j)
        {
            f32 rx = 0.1f + (f32)(rand() % 255) / 255.0f * pillar_size;
            f32 ry = 0.1f + (f32)(rand() % 255) / 255.0f * pillar_size * 4.0f;
            f32 rz = 0.1f + (f32)(rand() % 255) / 255.0f * pillar_size;

            pos.y = ry;

            // quantize
            // box mesh is -1 to 1, so scale snap is x2
            f32 uv_scale = 0.05f;
            f32 uv_snap = (uv_scale * 40.0f);
            f32 inv_uv_snap = 1.0f / (uv_snap);

            rx = std::max<f32>(floor(rx * inv_uv_snap) * uv_snap, uv_snap);
            ry = std::max<f32>(floor(ry * inv_uv_snap) * uv_snap, uv_snap);
            rz = std::max<f32>(floor(rz * inv_uv_snap) * uv_snap, uv_snap);

            // pos is 0 - 1 so snap is half
            uv_snap = (uv_scale * 20.0f);
            inv_uv_snap = 1.0f / (uv_snap);
            pos = floor(pos * inv_uv_snap) * uv_snap;

            u32 pillar = get_new_entity(scene);
            scene->transforms[pillar].rotation = quat();
            scene->transforms[pillar].scale = vec3f(rx, ry, rz);
            scene->transforms[pillar].translation = pos;
            scene->parents[pillar] = pillar;
            scene->entities[pillar] |= e_cmp::transform;
            scene->names[pillar] = "pillar";

            instantiate_geometry(box_resource, scene, pillar);
            instantiate_model_cbuffer(scene, pillar);

            // uv scale
            scene->material_permutation[pillar] |= FORWARD_LIT_UV_SCALE;
            instantiate_material(default_material, scene, pillar);

            forward_lit_uv_scale* m = (forward_lit_uv_scale*)&scene->material_data[pillar].data[0];
            m->m_albedo = vec4f::one();
            m->m_roughness = 0.1f;
            m->m_reflectivity = 0.3f;
            m->m_uv_scale = vec2f(uv_scale, uv_scale);

            scene->samplers[pillar].sb[0].handle = albedo_tex;
            scene->samplers[pillar].sb[1].handle = normal_tex;
            scene->samplers[pillar].sb[2].handle = roughness_tex;
            scene->samplers[pillar].sb[3].handle = matallic_tex;

            for (u32 j = 0; j < 4; ++j)
                scene->samplers[pillar].sb[j].id_sampler_state = PEN_HASH("wrap_linear");

            pos.z += d * 2.0f;
        }

        pos.x += d * 2.0f;
    }

    static vec4f _pallete[] = {
        vec4f(117.0f, 219.0f, 205.0f, 255.0f) / 255.0f, vec4f(201.0f, 219.0f, 186.0f, 255.0f) / 255.0f,
        vec4f(220.0f, 219.0f, 169.0f, 255.0f) / 255.0f, vec4f(245.0f, 205.0f, 167.0f, 255.0f) / 255.0f,
        vec4f(250.0f, 163.0f, 129.0f, 255.0f) / 255.0f,
    };

    for (s32 i = 0; i < max_lights; ++i)
    {
        f32 rx = (f32)(rand() % 255) / 255.0f;
        f32 ry = (f32)(rand() % 255) / 255.0f;
        f32 rz = (f32)(rand() % 255) / 255.0f;

        f32 rrx = (f32)(rand() % 255) / 255.0f;
        f32 rry = (f32)(rand() % 255) / 255.0f;
        f32 rrz = (f32)(rand() % 255) / 255.0f;

        ImColor ii = ImColor::HSV((rand() % 255) / 255.0f, (rand() % 255) / 255.0f, (rand() % 255) / 255.0f);
        vec4f   col = normalised(vec4f(ii.Value.x, ii.Value.y, ii.Value.z, 1.0f));

        col = _pallete[rand() % 5];

        u32 light = get_new_entity(scene);
        scene->names[light] = "light";
        scene->id_name[light] = PEN_HASH("light");

        scene->transforms[light].translation = (vec3f(rx, ry, rz) * vec3f(2.0f, 1.0f, 2.0f) + vec3f(-1.0f, 0.0f, -1.0f)) *
                                               vec3f(scene_size, scene_size * 0.1f, scene_size);

        scene->transforms[light].translation.y += scene_size;

        scene->transforms[light].rotation = quat();
        scene->transforms[light].rotation.euler_angles(rrx, rry, rrz);
        scene->transforms[light].scale = vec3f::one();
        scene->entities[light] |= e_cmp::transform;

        instantiate_light(scene, light);
        scene->lights[light].colour = col.xyz;
        scene->lights[light].radius = light_radius;
        scene->lights[light].type = e_light_type::point;

        anim_dir[i] = vec3f(rrx, rry, rrz) * vec3f(2.0f) - vec3f(1.0);

        if (i == 0)
            lights_start = light;
    }
}
