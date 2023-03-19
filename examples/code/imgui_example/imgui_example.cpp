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

    ImGuiStyle& custom_theme()
    {
        put::dev_ui::create_context();

        ImGuiStyle &style = ImGui::GetStyle();
        style.Alpha = 1.0;
        style.ChildRounding = 3;
        style.WindowRounding = 1;
        style.GrabRounding = 1;
        style.GrabMinSize = 20;
        style.FrameRounding = 3;

        ImVec4 zero = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

        ImVec4 text_light = ImVec4(0.8f, 0.8f, 0.8f, 1.00f);
        ImVec4 text_dark = ImVec4(0.4f, 0.4f, 0.4f, 1.00f);
        ImVec4 window_bg = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);

        ImVec4 foreground = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
        ImVec4 foreground_light = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
        ImVec4 foreground_dark = ImVec4(0.20f, 0.22f, 0.23f, 1.00f);
        ImVec4 foreground_dark_highlight = ImVec4(0.19f, 0.19f, 0.19f, 0.54f);
        ImVec4 foreground_inactive = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);

        ImVec4 accent = ImVec4(0.00f, 0.65f, 0.65f, 0.46f);
        ImVec4 accent_light = ImVec4(0.00f, 0.75f, 0.75f, 0.46f);

        style.Colors[ImGuiCol_Text] = text_light;
        style.Colors[ImGuiCol_TextDisabled] = text_dark;

        style.Colors[ImGuiCol_FrameBg] = foreground_inactive;
        style.Colors[ImGuiCol_FrameBgHovered] = foreground_dark_highlight;
        style.Colors[ImGuiCol_FrameBgActive] = foreground_dark;

        style.Colors[ImGuiCol_TitleBg] = foreground_inactive;
        style.Colors[ImGuiCol_TitleBgCollapsed] = foreground_inactive;
        style.Colors[ImGuiCol_TitleBgActive] = foreground_dark;

        style.Colors[ImGuiCol_WindowBg] = window_bg;
        style.Colors[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

        style.Colors[ImGuiCol_Border] = foreground;
        style.Colors[ImGuiCol_BorderShadow] = zero;

        style.Colors[ImGuiCol_MenuBarBg] = foreground_dark;

        style.Colors[ImGuiCol_ScrollbarBg] = foreground_dark;
        style.Colors[ImGuiCol_ScrollbarGrab] = foreground;
        style.Colors[ImGuiCol_ScrollbarGrabHovered] = foreground_light;
        style.Colors[ImGuiCol_ScrollbarGrabActive] = foreground_light;

        style.Colors[ImGuiCol_ResizeGrip] = foreground_dark;
        style.Colors[ImGuiCol_ResizeGripHovered] = foreground_light;
        style.Colors[ImGuiCol_ResizeGripActive] = foreground;

        style.Colors[ImGuiCol_SliderGrab] = foreground_light;
        style.Colors[ImGuiCol_SliderGrabActive] = foreground;

        style.Colors[ImGuiCol_PopupBg] = foreground_dark;
        style.Colors[ImGuiCol_CheckMark] = foreground_light;

        style.Colors[ImGuiCol_Button] = ImVec4(0.05f, 0.05f, 0.05f, 0.54f);
        style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.19f, 0.19f, 0.19f, 0.54f);
        style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.20f, 0.22f, 0.23f, 1.00f);

        style.Colors[ImGuiCol_Header] = foreground_dark;
        style.Colors[ImGuiCol_HeaderHovered] = foreground_dark_highlight;
        style.Colors[ImGuiCol_HeaderActive] = foreground;

        style.Colors[ImGuiCol_Separator] = foreground_dark;
        style.Colors[ImGuiCol_SeparatorHovered] = foreground_light;
        style.Colors[ImGuiCol_SeparatorActive] = foreground;

        style.Colors[ImGuiCol_TextSelectedBg] = foreground_dark_highlight;

        style.Colors[ImGuiCol_Button] = ImVec4(0.8f, 0.4f, 0.4f, 1.0f);
        style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.9f, 0.45f, 0.45f, 1.0f);
        style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.9f, 0.45f, 0.45f, 1.0f);

        style.Colors[ImGuiCol_PlotLines] = accent;
        style.Colors[ImGuiCol_PlotLinesHovered] = accent_light;
        style.Colors[ImGuiCol_PlotHistogram] = accent;
        style.Colors[ImGuiCol_PlotHistogramHovered] = accent_light;
        style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.04f, 0.10f, 0.09f, 0.51f);
        return style;
    }

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
        pen::raster_state_creation_params rcp;
        pen::memory_zero(&rcp, sizeof(pen::raster_state_creation_params));
        rcp.fill_mode = PEN_FILL_SOLID;
        rcp.cull_mode = PEN_CULL_BACK;
        rcp.depth_bias_clamp = 0.0f;
        rcp.sloped_scale_depth_bias = 0.0f;
        rcp.depth_clip_enable = true;

        s_raster_state_cull_back = pen::renderer_create_raster_state(rcp);

        // depth stencil state
        pen::depth_stencil_creation_params depth_stencil_params = {0};

        // Depth test parameters
        depth_stencil_params.depth_enable = true;
        depth_stencil_params.depth_write_mask = 1;
        depth_stencil_params.depth_func = PEN_COMPARISON_ALWAYS;

        s_default_depth_stencil_state = pen::renderer_create_depth_stencil_state(depth_stencil_params);

        // init systems
        put::dev_ui::enable(true);
        put::dev_ui::init(custom_theme());

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
        ImGui::DestroyContext();
    }

    loop_t user_update()
    {
        pen::renderer_new_frame();

        pen::renderer_set_targets(PEN_BACK_BUFFER_COLOUR, PEN_BACK_BUFFER_DEPTH);
        pen::viewport vp = {0.0f, 0.0f, PEN_BACK_BUFFER_RATIO, 1.0f, 0.0f, 1.0f};
        pen::renderer_set_viewport(vp);
        pen::renderer_set_scissor_rect(rect{vp.x, vp.y, vp.width, vp.height});
        pen::renderer_clear(s_clear_state_grey);
        pen::renderer_set_raster_state(s_raster_state_cull_back);
        pen::renderer_set_depth_stencil_state(s_default_depth_stencil_state);
        
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

        put::dev_ui::new_frame();
        
        ImGui::Text("Hello World");

        static f32 renderer_time = 0.0f;
        bool show_demo_window = true;
        static bool show_another_window = false;
        ImVec4      clear_col = ImColor(114, 144, 154);
        static bool opt_fullscreen = true;
        static bool opt_padding = false;
        static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

        // We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
        // because it would be confusing to have two docking targets within each others.
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
        if (opt_fullscreen)
        {
            const ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->WorkPos);
            ImGui::SetNextWindowSize(viewport->WorkSize);
            ImGui::SetNextWindowViewport(viewport->ID);

            window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
            window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
        }
        else
        {
            dockspace_flags &= ~ImGuiDockNodeFlags_PassthruCentralNode;
        }

        // When using ImGuiDockNodeFlags_PassthruCentralNode, DockSpace() will render our background
        // and handle the pass-thru hole, so we ask Begin() to not render a background.
        if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
            window_flags |= ImGuiWindowFlags_NoBackground;

        static ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

        // 1. Show a simple window.
        // Tip: if we don't call ImGui::Begin()/ImGui::End() the widgets automatically appears in a window called "Debug".
        {
            static float f = 0.0f;
            static int counter = 0;
            ImGui::Text("Hello, world!");                           // Display some text (you can use a format string too)
            ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
            ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

            ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our windows open/close state
            ImGui::Checkbox("Another Window", &show_another_window);

            if (ImGui::Button("Button"))                            // Buttons return true when clicked (NB: most widgets return true when edited/activated)
                counter++;
            ImGui::SameLine();
            ImGui::Text("counter = %d", counter);

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        }

        // 2. Show another simple window. In most cases you will use an explicit Begin/End pair to name your windows.
        if (show_another_window)
        {
            ImGui::Begin("Another Window", &show_another_window);
            ImGui::Text("Hello from another window!");
            if (ImGui::Button("Close Me"))
                show_another_window = false;
            ImGui::End();
        }

        // 3. Show the ImGui demo window. Most of the sample code is in ImGui::ShowDemoWindow(). Read its code to learn more about Dear ImGui!
        if (show_demo_window)
        {
            ImGui::SetNextWindowPos(ImVec2(650, 20), ImGuiCond_FirstUseEver); // Normally user code doesn't need/want to call this because positions are saved in .ini file anyway. Here we just want to make the demo initial state a bit more friendly!
            ImGui::ShowDemoWindow(&show_demo_window);
        }

        put::dev_ui::render();

        static pen::timer* timer = pen::timer_create();
        pen::timer_start(timer);

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
