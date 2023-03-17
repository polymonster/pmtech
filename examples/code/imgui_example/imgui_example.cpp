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
        p.window_width = 1280.0f;
        p.window_height = 720.0f;
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

        // style.Colors[ImGuiCol_ComboBg] = foreground_dark;
        style.Colors[ImGuiCol_CheckMark] = foreground_light;

        style.Colors[ImGuiCol_Button] = ImVec4(0.05f, 0.05f, 0.05f, 0.54f);
        style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.19f, 0.19f, 0.19f, 0.54f);
        style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.20f, 0.22f, 0.23f, 1.00f);

        style.Colors[ImGuiCol_Header] = foreground_dark;
        style.Colors[ImGuiCol_HeaderHovered] = foreground_dark_highlight;
        style.Colors[ImGuiCol_HeaderActive] = foreground;

        // style.Colors[ImGuiCol_Column] = foreground_dark;
        // style.Colors[ImGuiCol_ColumnHovered] = foreground_light;
        // style.Colors[ImGuiCol_ColumnActive] = foreground;

        style.Colors[ImGuiCol_TextSelectedBg] = foreground_dark_highlight;

        // style.Colors[ImGuiCol_CloseButton] = ImVec4(0.8f, 0.4f, 0.4f, 1.0f);
        // style.Colors[ImGuiCol_CloseButtonHovered] = ImVec4(0.9f, 0.45f, 0.45f, 1.0f);
        // style.Colors[ImGuiCol_CloseButtonActive] = ImVec4(0.9f, 0.45f, 0.45f, 1.0f);

        style.Colors[ImGuiCol_PlotLines] = accent;
        style.Colors[ImGuiCol_PlotLinesHovered] = accent_light;
        style.Colors[ImGuiCol_PlotHistogram] = accent;
        style.Colors[ImGuiCol_PlotHistogramHovered] = accent_light;
        // style.Colors[ImGuiCol_ModalWindowDarkening] = ImVec4(0.04f, 0.10f, 0.09f, 0.51f);
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
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
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
        // if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
        //     window_flags |= ImGuiWindowFlags_NoBackground;

        // Important: note that we proceed even if Begin() returns false (aka window is collapsed).
        // This is because we want to keep our DockSpace() active. If a DockSpace() is inactive,
        // all active windows docked into it will lose their parent and become undocked.
        // We cannot preserve the docking relationship between an active window and an inactive docking, otherwise
        // any change of dockspace/settings would lead to windows being stuck in limbo and never being visible.
  
        ImGui::Begin("DockSpace Demo", nullptr, window_flags);

        // Submit the DockSpace

        if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
        {
            ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
            ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
        }
        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("Options"))
            {
                // Disabling fullscreen would allow the window to be moved to the front of other windows,
                // which we can't undo at the moment without finer window depth/z control.
                ImGui::MenuItem("Fullscreen", NULL, &opt_fullscreen);
                ImGui::MenuItem("Padding", NULL, &opt_padding);
                ImGui::Separator();

                if (ImGui::MenuItem("Flag: NoSplit",                "", (dockspace_flags & ImGuiDockNodeFlags_NoSplit) != 0))                 { dockspace_flags ^= ImGuiDockNodeFlags_NoSplit; }
                if (ImGui::MenuItem("Flag: NoResize",               "", (dockspace_flags & ImGuiDockNodeFlags_NoResize) != 0))                { dockspace_flags ^= ImGuiDockNodeFlags_NoResize; }
                if (ImGui::MenuItem("Flag: NoDockingInCentralNode", "", (dockspace_flags & ImGuiDockNodeFlags_NoDockingInCentralNode) != 0))  { dockspace_flags ^= ImGuiDockNodeFlags_NoDockingInCentralNode; }
                if (ImGui::MenuItem("Flag: AutoHideTabBar",         "", (dockspace_flags & ImGuiDockNodeFlags_AutoHideTabBar) != 0))          { dockspace_flags ^= ImGuiDockNodeFlags_AutoHideTabBar; }
                if (ImGui::MenuItem("Flag: PassthruCentralNode",    "", (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode) != 0, opt_fullscreen)) { dockspace_flags ^= ImGuiDockNodeFlags_PassthruCentralNode; }
                ImGui::Separator();
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
           
        }
        ImGui::Begin("Test");
        ImGui::Text("Test");
        ImGui::End();
        ImGui::End();
        put::dev_ui::render();
        static pen::timer* timer = pen::timer_create();
        pen::timer_start(timer);

        renderer_time = pen::timer_elapsed_ms(timer);
        put::dev_ui::render(ImGui::GetDrawData());
        
        ImGui::EndFrame();
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
