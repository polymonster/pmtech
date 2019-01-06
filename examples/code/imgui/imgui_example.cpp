#include "audio.h"
#include "debug_render.h"
#include "dev_ui.h"
#include "file_system.h"
#include "loader.h"
#include "memory.h"
#include "pen.h"
#include "pen_string.h"
#include "renderer.h"
#include "threads.h"
#include "timer.h"

pen::window_creation_params pen_window{
    1280,   // width
    720,    // height
    4,      // MSAA samples
    "imgui" // window title / process name
};

u32 clear_state_grey;
u32 raster_state_cull_back;

pen::viewport vp = {0.0f, 0.0f, 1280.0f, 720.0f, 0.0f, 1.0f};

u32 default_depth_stencil_state;

void renderer_state_init()
{
    // create 2 clear states one for the render target and one for the main screen, so we can see the difference
    static pen::clear_state cs = {
        0.5f, 0.5f, 0.5f, 0.5f, 1.0f, 0x00, PEN_CLEAR_COLOUR_BUFFER | PEN_CLEAR_DEPTH_BUFFER,
    };

    clear_state_grey = pen::renderer_create_clear_state(cs);

    // raster state
    pen::rasteriser_state_creation_params rcp;
    pen::memory_zero(&rcp, sizeof(pen::rasteriser_state_creation_params));
    rcp.fill_mode = PEN_FILL_SOLID;
    rcp.cull_mode = PEN_CULL_BACK;
    rcp.depth_bias_clamp = 0.0f;
    rcp.sloped_scale_depth_bias = 0.0f;
    rcp.depth_clip_enable = true;

    raster_state_cull_back = pen::renderer_create_rasterizer_state(rcp);

    // depth stencil state
    pen::depth_stencil_creation_params depth_stencil_params = {0};

    // Depth test parameters
    depth_stencil_params.depth_enable = true;
    depth_stencil_params.depth_write_mask = 1;
    depth_stencil_params.depth_func = PEN_COMPARISON_ALWAYS;

    default_depth_stencil_state = pen::renderer_create_depth_stencil_state(depth_stencil_params);
}

PEN_TRV pen::user_entry(void* params)
{
    // unpack the params passed to the thread and signal to the engine it ok to proceed
    pen::job_thread_params* job_params = (pen::job_thread_params*)params;
    pen::job*               p_thread_info = job_params->job_info;
    pen::semaphore_post(p_thread_info->p_sem_continue, 1);

    // init systems
    renderer_state_init();
    put::dev_ui::init();

    while (1)
    {
        put::dev_ui::new_frame();

        ImGui::Text("Hello World");

        pen::renderer_set_rasterizer_state(raster_state_cull_back);

        static f32 renderer_time = 0.0f;

        // bind back buffer and clear
        pen::renderer_set_depth_stencil_state(default_depth_stencil_state);

        pen::renderer_set_viewport(vp);
        pen::renderer_set_scissor_rect(rect{vp.x, vp.y, vp.width, vp.height});
        pen::renderer_set_targets(PEN_BACK_BUFFER_COLOUR, PEN_BACK_BUFFER_DEPTH);
        pen::renderer_clear(clear_state_grey);

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

        static u32 timer = pen::timer_create("imgui_impl_timer");
        pen::timer_start(timer);

        put::dev_ui::render();

        renderer_time = pen::timer_elapsed_ms(timer);

        // present
        pen::renderer_present();

        pen::renderer_consume_cmd_buffer();

        // msg from the engine we want to terminate
        if (pen::semaphore_try_wait(p_thread_info->p_sem_exit))
        {
            break;
        }
    }

    // clean up mem here

    // signal to the engine the thread has finished
    pen::semaphore_post(p_thread_info->p_sem_terminated, 1);

    return PEN_THREAD_OK;
}
