#include "debug_render.h"
#include "dev_ui.h"
#include "loader.h"

#include "file_system.h"
#include "memory.h"
#include "pen.h"
#include "pen_string.h"
#include "renderer.h"
#include "threads.h"
#include "timer.h"

using namespace pen;

namespace
{
    void*  user_setup(void* params);
    loop_t user_update();
    void   user_shutdown();
} // namespace

namespace pen
{
    pen_creation_params pen_entry(int argc, char** argv)
    {
        pen::pen_creation_params p;
        p.window_width = 1280;
        p.window_height = 720;
        p.window_title = "imgui_example";
        p.window_sample_count = 4;
        p.user_thread_function = user_setup;
        p.flags = pen::e_pen_create_flags::renderer;
        return p;
    }
} // namespace pen

namespace
{
    job_thread_params* s_job_params = nullptr;
    job*               s_thread_info = nullptr;
    u32                s_clear_state_grey = 0;
    u32                s_raster_state_cull_back = 0;
    u32                s_default_depth_stencil_state = 0;

    void* user_setup(void* params)
    {
        // unpack the params passed to the thread and signal to the engine it ok to proceed
        s_job_params = (pen::job_thread_params*)params;
        s_thread_info = s_job_params->job_info;
        pen::semaphore_post(s_thread_info->p_sem_continue, 1);

        static pen::clear_state cs = {
            0.5f, 0.5f, 0.5f, 1.0f, 1.0f, 0x00, PEN_CLEAR_COLOUR_BUFFER | PEN_CLEAR_DEPTH_BUFFER,
        };

        s_clear_state_grey = pen::renderer_create_clear_state(cs);

        // raster state
        pen::rasteriser_state_creation_params rcp;
        pen::memory_zero(&rcp, sizeof(pen::rasteriser_state_creation_params));
        rcp.fill_mode = PEN_FILL_SOLID;
        rcp.cull_mode = PEN_CULL_BACK;
        rcp.depth_bias_clamp = 0.0f;
        rcp.sloped_scale_depth_bias = 0.0f;
        rcp.depth_clip_enable = true;

        s_raster_state_cull_back = pen::renderer_create_rasterizer_state(rcp);

        // depth stencil state
        pen::depth_stencil_creation_params depth_stencil_params = {0};

        // Depth test parameters
        depth_stencil_params.depth_enable = true;
        depth_stencil_params.depth_write_mask = 1;
        depth_stencil_params.depth_func = PEN_COMPARISON_ALWAYS;

        s_default_depth_stencil_state = pen::renderer_create_depth_stencil_state(depth_stencil_params);

        // init systems
        put::dev_ui::init();

        // we call user_update once per frame
        pen_main_loop(user_update);
        return PEN_THREAD_OK;
    }

    void user_shutdown()
    {
        pen::renderer_release_clear_state(s_clear_state_grey);
        pen::renderer_release_depth_stencil_state(s_default_depth_stencil_state);
        pen::renderer_release_raster_state(s_raster_state_cull_back);

        pen::semaphore_post(s_thread_info->p_sem_terminated, 1);
    }

    loop_t user_update()
    {
        pen::renderer_new_frame();

        pen::renderer_set_targets(PEN_BACK_BUFFER_COLOUR, PEN_BACK_BUFFER_DEPTH);
        pen::viewport vp = {0.0f, 0.0f, PEN_BACK_BUFFER_RATIO, 1.0f, 0.0f, 1.0f};
        pen::renderer_set_viewport(vp);
        pen::renderer_set_scissor_rect(rect{vp.x, vp.y, vp.width, vp.height});
        pen::renderer_clear(s_clear_state_grey);
        pen::renderer_set_rasterizer_state(s_raster_state_cull_back);
        pen::renderer_set_depth_stencil_state(s_default_depth_stencil_state);

        put::dev_ui::new_frame();

        ImGui::Text("Hello World");

        static f32 renderer_time = 0.0f;

        static bool show_test_window = true;
        static bool show_another_window = false;
        ImVec4      clear_col = ImColor(114, 144, 154);

        // 1. Show a simple window
        // Tip: if we don't call ImGui::Begin()/ImGui::End() the widgets appears in a window automatically called "Debug"
        {
            static float f = 0.0f;
            ImGui::Text("Hello, world!");
            ImGui::SliderFloat("float", &f, 0.0f, 1.0f);
            ImGui::ColorEdit3("clear color", (float*)&clear_col);
            if (ImGui::Button("Test Window"))
                show_test_window ^= 1;
            if (ImGui::Button("Another Window"))
                show_another_window ^= 1;
            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate,
                        ImGui::GetIO().Framerate);

            ImGui::Text("Imgui render implementation time : %f", renderer_time);
        }

        // 2. Show another simple window, this time using an explicit Begin/End pair
        if (show_another_window)
        {
            ImGui::SetNextWindowSize(ImVec2(200, 100), ImGuiSetCond_FirstUseEver);
            ImGui::Begin("Another Window", &show_another_window);
            ImGui::Text("Hello");
            ImGui::End();
        }

        // 3. Show the ImGui test window. Most of the sample code is in ImGui::ShowTestWindow()
        if (show_test_window)
        {
            ImGui::SetNextWindowPos(ImVec2(650, 20), ImGuiSetCond_FirstUseEver); // Normally user code doesn't need/want to
                                                                                 // call it because positions are saved in
                                                                                 // .ini file anyway. Here we just want to
                                                                                 // make the demo initial state a bit more
                                                                                 // friendly!
            ImGui::ShowTestWindow(&show_test_window);
        }

        static pen::timer* timer = pen::timer_create();
        pen::timer_start(timer);

        put::dev_ui::render();

        renderer_time = pen::timer_elapsed_ms(timer);

        // present
        pen::renderer_present();
        pen::renderer_consume_cmd_buffer();

        // msg from the engine we want to terminate
        if (pen::semaphore_try_wait(s_thread_info->p_sem_exit))
        {
            user_shutdown();
            pen_main_loop_exit();
        }

        pen_main_loop_continue();
    }
} // namespace
