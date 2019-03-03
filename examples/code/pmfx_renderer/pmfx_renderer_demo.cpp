#include "camera.h"
#include "ecs/ecs_editor.h"
#include "ecs/ecs_resources.h"
#include "ecs/ecs_scene.h"
#include "ecs/ecs_utilities.h"
#include "debug_render.h"
#include "dev_ui.h"
#include "file_system.h"
#include "forward_render.h"
#include "hash.h"
#include "input.h"
#include "loader.h"
#include "pen.h"
#include "pen_json.h"
#include "pen_string.h"
#include "pmfx.h"
#include "renderer.h"
#include "str_utilities.h"
#include "timer.h"

using namespace put;
using namespace ecs;

pen::window_creation_params pen_window{
    1280,           // width
    720,            // height
    4,              // MSAA samples
    "pmfx_renderer" // window title / process name
};

namespace physics
{
    extern PEN_TRV physics_thread_main(void* params);
}

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

    void update_demo(ecs::ecs_scene* scene, f32 dt)
    {
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
                scene->entities[i] &= ~CMP_LIGHT;
                continue;
            }

            scene->transforms[i].translation += anim_dir[dir_index] * dt * 0.1f;
            scene->entities[i] |= CMP_TRANSFORM;

            for (u32 j = 0; j < 3; ++j)
            {
                if (fabs(scene->transforms[i].translation[j]) > scene_size)
                {
                    f32 rrx = (f32)(rand() % 255) / 255.0f;
                    f32 rry = (f32)(rand() % 255) / 255.0f;
                    f32 rrz = (f32)(rand() % 255) / 255.0f;

                    anim_dir[dir_index] = vec3f(rrx, rry, rrz) * vec3f(2.0f) - vec3f(1.0);
                    anim_dir[dir_index] +=
                        normalised(vec3f(0.0f, scene_size / 2.0f, 0.0f) - scene->transforms[i].translation);
                }
            }

            scene->entities[i] |= CMP_LIGHT;
            scene->lights[i].radius = light_radius;

            dir_index++;
        }
    }
} // namespace

void create_scene_objects(ecs::ecs_scene* scene, camera& main_camera)
{
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

            u32 pillar = get_new_node(scene);
            scene->transforms[pillar].rotation = quat();
            scene->transforms[pillar].scale = vec3f(rx, ry, rz);
            scene->transforms[pillar].translation = pos;
            scene->parents[pillar] = pillar;
            scene->entities[pillar] |= CMP_TRANSFORM;
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

        u32 light = get_new_node(scene);
        scene->names[light] = "light";
        scene->id_name[light] = PEN_HASH("light");

        scene->transforms[light].translation = (vec3f(rx, ry, rz) * vec3f(2.0f, 1.0f, 2.0f) + vec3f(-1.0f, 0.0f, -1.0f)) *
                                               vec3f(scene_size, scene_size * 0.1f, scene_size);

        scene->transforms[light].translation.y += scene_size;

        scene->transforms[light].rotation = quat();
        scene->transforms[light].rotation.euler_angles(rrx, rry, rrz);
        scene->transforms[light].scale = vec3f::one();
        scene->entities[light] |= CMP_TRANSFORM;

        scene->lights[light].colour = col.xyz;
        scene->lights[light].radius = light_radius;
        scene->lights[light].type = LIGHT_TYPE_POINT;
        instantiate_light(scene, light);

        anim_dir[i] = vec3f(rrx, rry, rrz) * vec3f(2.0f) - vec3f(1.0);

        if (i == 0)
            lights_start = light;
    }
}

PEN_TRV pen::user_entry(void* params)
{
    // unpack the params passed to the thread and signal to the engine it ok to proceed
    pen::job_thread_params* job_params = (pen::job_thread_params*)params;
    pen::job*               p_thread_info = job_params->job_info;
    pen::semaphore_post(p_thread_info->p_sem_continue, 1);

    pen::jobs_create_job(physics::physics_thread_main, 1024 * 10, nullptr, pen::THREAD_START_DETACHED);

    put::dev_ui::init();
    put::dbg::init();

    // create main camera and controller
    put::camera main_camera;
    put::camera_create_perspective(&main_camera, 60.0f, put::k_use_window_aspect, 0.1f, 1000.0f);

    put::scene_controller cc;
    cc.camera = &main_camera;
    cc.update_function = &ecs::update_model_viewer_camera;
    cc.name = "model_viewer_camera";
    cc.id_name = PEN_HASH(cc.name.c_str());

    // create the main scene and controller
    put::ecs::ecs_scene* main_scene = put::ecs::create_scene("main_scene");
    put::ecs::editor_init(main_scene);

    put::scene_controller sc;
    sc.scene = main_scene;
    sc.update_function = &ecs::update_model_viewer_scene;
    sc.name = "main_scene";
    sc.camera = &main_camera;
    sc.id_name = PEN_HASH(sc.name.c_str());

    // create view renderers
    put::scene_view_renderer svr_main;
    svr_main.name = "ces_render_scene";
    svr_main.id_name = PEN_HASH(svr_main.name.c_str());
    svr_main.render_function = &ecs::render_scene_view;

    put::scene_view_renderer svr_editor;
    svr_editor.name = "ces_render_editor";
    svr_editor.id_name = PEN_HASH(svr_editor.name.c_str());
    svr_editor.render_function = &ecs::render_scene_editor;

    put::scene_view_renderer svr_light_volumes;
    svr_light_volumes.name = "ces_render_light_volumes";
    svr_light_volumes.id_name = PEN_HASH(svr_light_volumes.name.c_str());
    svr_light_volumes.render_function = &ecs::render_light_volumes;

    pmfx::register_scene_view_renderer(svr_main);
    pmfx::register_scene_view_renderer(svr_editor);
    pmfx::register_scene_view_renderer(svr_light_volumes);

    pmfx::register_scene_controller(sc);
    pmfx::register_scene_controller(cc);

    pmfx::init("data/configs/pmfx_demo.jsn");

    create_scene_objects(main_scene, main_camera);

    f32 frame_time = 0.0f;

    while (1)
    {
        static u32 frame_timer = pen::timer_create("frame_timer");
        pen::timer_start(frame_timer);

        put::dev_ui::new_frame();

        update_demo(main_scene, (f32)frame_time);

        pmfx::update();

        pmfx::render();

        pmfx::show_dev_ui();

        put::dev_ui::render();

        frame_time = pen::timer_elapsed_ms(frame_timer);
        user_thread_time = frame_time;

        pen::renderer_present();

        // for unit test
        pen::renderer_test_run();

        pen::renderer_consume_cmd_buffer();

        pmfx::poll_for_changes();
        put::poll_hot_loader();

        // msg from the engine we want to terminate
        if (pen::semaphore_try_wait(p_thread_info->p_sem_exit))
            break;
    }

    ecs::destroy_scene(main_scene);
    ecs::editor_shutdown();

    // clean up mem here
    put::pmfx::shutdown();
    put::dbg::shutdown();
    put::dev_ui::shutdown();

    pen::renderer_consume_cmd_buffer();

    // signal to the engine the thread has finished
    pen::semaphore_post(p_thread_info->p_sem_terminated, 1);

    return PEN_THREAD_OK;
}
