#include "camera.h"
#include "debug_render.h"
#include "dev_ui.h"
#include "ecs/ecs_editor.h"
#include "ecs/ecs_resources.h"
#include "ecs/ecs_scene.h"
#include "ecs/ecs_utilities.h"
#include "loader.h"
#include "pmfx.h"

#include "file_system.h"
#include "hash.h"
#include "input.h"
#include "pen.h"
#include "pen_json.h"
#include "pen_string.h"
#include "renderer.h"
#include "str_utilities.h"
#include "timer.h"

using namespace pen;
using namespace put;
using namespace ecs;

namespace
{
    void*  user_setup(void* params);
    loop_t user_update();
    void   user_shutdown();
} // namespace

namespace physics
{
    extern void* physics_thread_main(void* params);
}

namespace pen
{
    pen_creation_params pen_entry(int argc, char** argv)
    {
        pen::pen_creation_params p;
        p.window_width = 1280;
        p.window_height = 720;
        p.window_title = "blend_modes";
        p.window_sample_count = 4;
        p.user_thread_function = user_setup;
        p.flags = pen::e_pen_create_flags::renderer;
        return p;
    }
} // namespace pen

namespace
{
    pen::job_thread_params* job_params;
    pen::job*               p_thread_info;

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
        static pmm_renderable&         r = quad->renderable[e_pmm_renderable::full_vertex_buffer];

        static u32 background_texture = put::load_texture("data/textures/blend_test_bg.dds");
        static u32 foreground_texture = put::load_texture("data/textures/blend_test_fg.dds");

        u32 wrap_linear = pmfx::get_render_state(PEN_HASH("wrap_linear"), pmfx::e_render_state::sampler);
        u32 disable_blend = pmfx::get_render_state(PEN_HASH("disabled"), pmfx::e_render_state::blend);

        if (!is_valid(scene_view.pmfx_shader))
            return;

        if (!pmfx::set_technique_perm(scene_view.pmfx_shader, scene_view.id_technique))
            PEN_ASSERT(0);

        pen::renderer_set_constant_buffer(scene_view.cb_view, 0, pen::CBUFFER_BIND_VS);
        pen::renderer_set_index_buffer(r.index_buffer, r.index_type, 0);
        pen::renderer_set_vertex_buffer(r.vertex_buffer, 0, r.vertex_size, 0);

        // background
        pen::renderer_set_blend_state(disable_blend);
        pen::renderer_set_texture(background_texture, wrap_linear, 0, pen::TEXTURE_BIND_PS);
        pen::renderer_draw_indexed(r.num_indices, 0, 0, PEN_PT_TRIANGLELIST);

        // foreground
        pen::renderer_set_blend_state(scene_view.blend_state);
        pen::renderer_set_texture(foreground_texture, wrap_linear, 0, pen::TEXTURE_BIND_PS);
        pen::renderer_draw_indexed(r.num_indices, 0, 0, PEN_PT_TRIANGLELIST);
    }

    void* user_setup(void* params)
    {
        // unpack the params passed to the thread and signal to the engine it ok to proceed
        job_params = (pen::job_thread_params*)params;
        p_thread_info = job_params->job_info;
        pen::semaphore_post(p_thread_info->p_sem_continue, 1);

        pen::jobs_create_job(physics::physics_thread_main, 1024 * 10, nullptr, pen::e_thread_start_flags::detached);

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

        pen_main_loop(user_update);
        return PEN_THREAD_OK;
    }

    void user_shutdown()
    {
        pen::renderer_new_frame();

        ecs::editor_shutdown();
        put::pmfx::shutdown();
        put::dbg::shutdown();
        put::dev_ui::shutdown();

        pen::renderer_present();
        pen::renderer_consume_cmd_buffer();

        pen::semaphore_post(p_thread_info->p_sem_terminated, 1);
    }

    loop_t user_update()
    {
        static pen::timer* frame_timer = pen::timer_create();
        pen::timer_start(frame_timer);

        put::dev_ui::new_frame();

        pmfx::render();

        pmfx::show_dev_ui();

        blend_mode_ui();

        put::dev_ui::render();

        pen::renderer_present();
        pen::renderer_consume_cmd_buffer();

        pmfx::poll_for_changes();
        put::poll_hot_loader();

        // msg from the engine we want to terminate
        if (pen::semaphore_try_wait(p_thread_info->p_sem_exit))
        {
            user_shutdown();
            pen_main_loop_exit();
        }

        pen_main_loop_continue();
    }
} // namespace
