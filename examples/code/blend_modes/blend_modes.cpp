#include "camera.h"
#include "debug_render.h"
#include "dev_ui.h"
#include "ecs/ecs_editor.h"
#include "ecs/ecs_resources.h"
#include "ecs/ecs_scene.h"
#include "ecs/ecs_utilities.h"
#include "file_system.h"
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
    1280,         // width
    720,          // height
    4,            // MSAA samples
    "blend_modes" // window title / process name
};

namespace physics
{
    extern PEN_TRV physics_thread_main(void* params);
}

void blend_mode_ui()
{
    bool opened = true;
    ImGui::Begin("Blend Modes", &opened, ImGuiWindowFlags_AlwaysAutoResize);

    static hash_id rt_ids[] = {
        PEN_HASH("rt_no_blend"), PEN_HASH("rt_alpha_blend"), PEN_HASH("rt_additive"), PEN_HASH("rt_premultiplied_alpha"),
        PEN_HASH("rt_min"),      PEN_HASH("rt_max"),         PEN_HASH("rt_subtract"), PEN_HASH("rt_rev_subtract")};

    int c = 0;
    for (hash_id id : rt_ids)
    {
        const pmfx::render_target* r = pmfx::get_render_target(id);
        if (!r)
            continue;

        f32 w, h;
        pmfx::get_render_target_dimensions(r, w, h);

        ImVec2 size(256, 256);

        ImGui::Image(IMG(r->handle), size);

        if (c != 3)
        {
            ImGui::SameLine();
            c++;
        }
        else
        {
            c = 0;
        }
    }

    ImGui::End();
}

void blend_layers(const scene_view& scene_view)
{
    static ecs::geometry_resource* quad = ecs::get_geometry_resource(PEN_HASH("full_screen_quad"));

    static u32 background_texture = put::load_texture("data/textures/blend_test_bg.dds");
    static u32 foreground_texture = put::load_texture("data/textures/blend_test_fg.dds");

    static u32 wrap_linear = pmfx::get_render_state(PEN_HASH("wrap_linear"), pmfx::RS_SAMPLER);
    static u32 disable_blend = pmfx::get_render_state(PEN_HASH("disabled"), pmfx::RS_BLEND);

    if (!is_valid(scene_view.pmfx_shader))
        return;

    if (!pmfx::set_technique_perm(scene_view.pmfx_shader, scene_view.technique))
        PEN_ASSERT(0);

    pen::renderer_set_constant_buffer(scene_view.cb_view, 0, pen::CBUFFER_BIND_VS);
    pen::renderer_set_index_buffer(quad->index_buffer, quad->index_type, 0);
    pen::renderer_set_vertex_buffer(quad->vertex_buffer, 0, quad->vertex_size, 0);

    // background
    pen::renderer_set_blend_state(disable_blend);
    pen::renderer_set_texture(background_texture, wrap_linear, 0, pen::TEXTURE_BIND_PS);
    pen::renderer_draw_indexed(quad->num_indices, 0, 0, PEN_PT_TRIANGLELIST);

    // foreground
    pen::renderer_set_blend_state(scene_view.blend_state);
    pen::renderer_set_texture(foreground_texture, wrap_linear, 0, pen::TEXTURE_BIND_PS);
    pen::renderer_draw_indexed(quad->num_indices, 0, 0, PEN_PT_TRIANGLELIST);
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

    put::ecs::create_geometry_primitives();

    // create view renderers
    put::scene_view_renderer svr_main;
    svr_main.name = "blend_layers";
    svr_main.id_name = PEN_HASH(svr_main.name.c_str());
    svr_main.render_function = &blend_layers;

    pmfx::register_scene_view_renderer(svr_main);

    pmfx::init("data/configs/blend_modes.jsn");

    while (1)
    {
        static pen::timer* frame_timer = pen::timer_create();
        pen::timer_start(frame_timer);

        put::dev_ui::new_frame();

        pmfx::render();

        pmfx::show_dev_ui();

        blend_mode_ui();

        put::dev_ui::render();

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
