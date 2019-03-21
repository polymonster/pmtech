// imgui_renderer.cpp
// Copyright 2014 - 2019 Alex Dixon.
// License: https://github.com/polymonster/pmtech/blob/master/license.md

#include "debug_render.h"
#include "dev_ui.h"
#include "input.h"
#include "loader.h"
#include "memory.h"
#include "os.h"
#include "pen.h"
#include "pmfx.h"
#include "renderer.h"
#include "timer.h"

using namespace pen;
using namespace put;
using namespace pmfx;

extern window_creation_params pen_window;

namespace put
{
    namespace dev_ui
    {
        struct render_handles
        {
            u32 raster_state;
            u32 blend_state;
            u32 depth_stencil_state;
            u32 vertex_buffer;
            u32 index_buffer;
            u32 font_texture;
            u32 font_sampler_state;
            u32 vb_size;
            u32 ib_size;
            u32 constant_buffer;

            void* vb_copy_buffer = nullptr;
            void* ib_copy_buffer = nullptr;

            u32 imgui_shader;
            u32 imgui_ex_shader;
        };

        render_handles g_imgui_rs;

        // internal functions
        void create_texture_atlas();
        void create_render_states();

        void process_input();

        void update_dynamic_buffers(ImDrawData* draw_data);
        void render(ImDrawData* draw_data);

        bool init()
        {
            pen::memory_zero(&g_imgui_rs, sizeof(g_imgui_rs));

            ImGuiIO& io = ImGui::GetIO();
            io.KeyMap[ImGuiKey_Tab] = PK_TAB;
            io.KeyMap[ImGuiKey_LeftArrow] = PK_LEFT;
            io.KeyMap[ImGuiKey_RightArrow] = PK_RIGHT;
            io.KeyMap[ImGuiKey_UpArrow] = PK_UP;
            io.KeyMap[ImGuiKey_DownArrow] = PK_DOWN;
            io.KeyMap[ImGuiKey_PageUp] = PK_PRIOR;
            io.KeyMap[ImGuiKey_PageDown] = PK_NEXT;
            io.KeyMap[ImGuiKey_Home] = PK_HOME;
            io.KeyMap[ImGuiKey_End] = PK_END;
            io.KeyMap[ImGuiKey_Delete] = PK_DELETE;
            io.KeyMap[ImGuiKey_Backspace] = PK_BACK;
            io.KeyMap[ImGuiKey_Enter] = PK_RETURN;
            io.KeyMap[ImGuiKey_Escape] = PK_ESCAPE;
            io.KeyMap[ImGuiKey_A] = PK_A;
            io.KeyMap[ImGuiKey_C] = PK_C;
            io.KeyMap[ImGuiKey_V] = PK_V;
            io.KeyMap[ImGuiKey_X] = PK_X;
            io.KeyMap[ImGuiKey_Y] = PK_Y;
            io.KeyMap[ImGuiKey_Z] = PK_Z;

            io.RenderDrawListsFn = render;

            io.ImeWindowHandle = pen::window_get_primary_display_handle();

            // load shaders
            g_imgui_rs.imgui_shader = pmfx::load_shader("imgui");
            g_imgui_rs.imgui_ex_shader = pmfx::load_shader("imgui_ex");

            create_texture_atlas();

            create_render_states();

            ImGuiStyle& style = ImGui::GetStyle();
            style.Alpha = 1.0;
            style.ChildWindowRounding = 3;
            style.WindowRounding = 3;
            style.GrabRounding = 1;
            style.GrabMinSize = 20;
            style.FrameRounding = 3;

            // ImVec4 debug = ImVec4(1.0f, 0.0f, 1.0f, 1.00f);

            ImVec4 zero = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

            ImVec4 text_light = ImVec4(0.8f, 0.8f, 0.8f, 1.00f);
            ImVec4 text_dark = ImVec4(0.4f, 0.4f, 0.4f, 1.00f);
            ImVec4 window_bg = ImVec4(0.0f, 0.0f, 0.0f, 0.5f);

            ImVec4 foreground = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
            ImVec4 foreground_light = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
            ImVec4 foreground_dark = ImVec4(0.4f, 0.4f, 0.4f, 1.0f);
            ImVec4 foreground_dark_highlight = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
            ImVec4 foreground_inactive = ImVec4(0.3f, 0.3f, 0.3f, 1.0f);

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
            style.Colors[ImGuiCol_ChildWindowBg] = zero;

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

            style.Colors[ImGuiCol_ComboBg] = foreground_dark;
            style.Colors[ImGuiCol_CheckMark] = foreground_light;

            style.Colors[ImGuiCol_Button] = ImVec4(0.00f, 0.65f, 0.65f, 0.46f);
            style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.01f, 1.00f, 1.00f, 0.43f);
            style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.00f, 1.00f, 1.00f, 0.62f);

            style.Colors[ImGuiCol_Header] = foreground_dark;
            style.Colors[ImGuiCol_HeaderHovered] = foreground_dark_highlight;
            style.Colors[ImGuiCol_HeaderActive] = foreground;

            style.Colors[ImGuiCol_Column] = foreground_dark;
            style.Colors[ImGuiCol_ColumnHovered] = foreground_light;
            style.Colors[ImGuiCol_ColumnActive] = foreground;

            style.Colors[ImGuiCol_TextSelectedBg] = foreground_dark_highlight;

            style.Colors[ImGuiCol_CloseButton] = ImVec4(0.8f, 0.4f, 0.4f, 1.0f);
            style.Colors[ImGuiCol_CloseButtonHovered] = ImVec4(0.9f, 0.45f, 0.45f, 1.0f);
            style.Colors[ImGuiCol_CloseButtonActive] = ImVec4(0.9f, 0.45f, 0.45f, 1.0f);

            style.Colors[ImGuiCol_PlotLines] = accent;
            style.Colors[ImGuiCol_PlotLinesHovered] = accent_light;
            style.Colors[ImGuiCol_PlotHistogram] = accent;
            style.Colors[ImGuiCol_PlotHistogramHovered] = accent_light;
            style.Colors[ImGuiCol_ModalWindowDarkening] = ImVec4(0.04f, 0.10f, 0.09f, 0.51f);

            dev_ui::util_init();

            return true;
        }

        void shutdown()
        {
            ImGui::Shutdown();
        }

        void create_texture_atlas()
        {
            ImGuiIO&  io = ImGui::GetIO();
            const c8* cousine_reg = pen::os_path_for_resource("data/fonts/cousine-regular.ttf");
            io.Fonts->AddFontFromFileTTF(cousine_reg, 14);

            ImFontConfig config;
            config.MergeMode = true;
            static const ImWchar icon_ranges[] = {ICON_MIN_FA, ICON_MAX_FA, 0};
            const c8*            font_awesome = pen::os_path_for_resource("data/fonts/fontawesome-webfont.ttf");
            io.Fonts->AddFontFromFileTTF(font_awesome, 13.0f, &config, icon_ranges);

            // Build texture atlas
            unsigned char* pixels;
            int            width, height;
            io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

            // fill out texture_creation_params
            texture_creation_params tcp;
            tcp.width = width;
            tcp.height = height;
            tcp.format = PEN_TEX_FORMAT_BGRA8_UNORM;
            tcp.num_mips = 1;
            tcp.num_arrays = 1;
            tcp.sample_count = 1;
            tcp.sample_quality = 0;
            tcp.usage = PEN_USAGE_DEFAULT;
            tcp.bind_flags = PEN_BIND_SHADER_RESOURCE;
            tcp.cpu_access_flags = 0;
            tcp.flags = 0;
            tcp.block_size = 4;
            tcp.pixels_per_block = 1;
            tcp.data_size = tcp.block_size * width * height;
            tcp.data = pixels;
            tcp.collection_type = pen::TEXTURE_COLLECTION_NONE;

            g_imgui_rs.font_texture = pen::renderer_create_texture(tcp);

            io.Fonts->TexID = IMG(g_imgui_rs.font_texture);
        }

        void update_dynamic_buffers(ImDrawData* draw_data)
        {
            if (g_imgui_rs.vertex_buffer == 0 || (s32)g_imgui_rs.vb_size < draw_data->TotalVtxCount)
            {
                if (g_imgui_rs.vertex_buffer != 0)
                {
                    pen::renderer_release_buffer(g_imgui_rs.vertex_buffer);
                };

                g_imgui_rs.vb_size = draw_data->TotalVtxCount + 10000;

                pen::buffer_creation_params bcp;
                bcp.usage_flags = PEN_USAGE_DYNAMIC;
                bcp.bind_flags = PEN_BIND_VERTEX_BUFFER;
                bcp.cpu_access_flags = PEN_CPU_ACCESS_WRITE;
                bcp.buffer_size = g_imgui_rs.vb_size * sizeof(ImDrawVert);
                bcp.data = (void*)nullptr;

                if (g_imgui_rs.vb_copy_buffer == nullptr)
                {
                    g_imgui_rs.vb_copy_buffer = pen::memory_alloc(g_imgui_rs.vb_size * sizeof(ImDrawVert));
                }
                else
                {
                    g_imgui_rs.vb_copy_buffer =
                        pen::memory_realloc(g_imgui_rs.vb_copy_buffer, g_imgui_rs.vb_size * sizeof(ImDrawVert));
                }

                g_imgui_rs.vertex_buffer = pen::renderer_create_buffer(bcp);
            }

            if (g_imgui_rs.index_buffer == 0 || (s32)g_imgui_rs.ib_size < draw_data->TotalIdxCount)
            {
                if (g_imgui_rs.index_buffer != 0)
                {
                    pen::renderer_release_buffer(g_imgui_rs.index_buffer);
                };

                g_imgui_rs.ib_size = draw_data->TotalIdxCount + 5000;

                pen::buffer_creation_params bcp;
                bcp.usage_flags = PEN_USAGE_DYNAMIC;
                bcp.bind_flags = PEN_BIND_INDEX_BUFFER;
                bcp.cpu_access_flags = PEN_CPU_ACCESS_WRITE;
                bcp.buffer_size = g_imgui_rs.ib_size * sizeof(ImDrawIdx);
                bcp.data = (void*)nullptr;

                if (g_imgui_rs.ib_copy_buffer == nullptr)
                {
                    g_imgui_rs.ib_copy_buffer = pen::memory_alloc(g_imgui_rs.ib_size * sizeof(ImDrawIdx));
                }
                else
                {
                    g_imgui_rs.ib_copy_buffer =
                        pen::memory_realloc(g_imgui_rs.ib_copy_buffer, g_imgui_rs.ib_size * sizeof(ImDrawIdx));
                }

                g_imgui_rs.index_buffer = pen::renderer_create_buffer(bcp);
            }

            u32 vb_offset = 0;
            u32 ib_offset = 0;

            for (int n = 0; n < draw_data->CmdListsCount; n++)
            {
                ImDrawList* cmd_list = draw_data->CmdLists[n];
                u32         vertex_size = cmd_list->VtxBuffer.Size * sizeof(ImDrawVert);
                u32         index_size = cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx);

                c8* vb_mem = (c8*)g_imgui_rs.vb_copy_buffer;
                c8* ib_mem = (c8*)g_imgui_rs.ib_copy_buffer;

                memcpy(&vb_mem[vb_offset], cmd_list->VtxBuffer.Data, vertex_size);
                memcpy(&ib_mem[ib_offset], cmd_list->IdxBuffer.Data, index_size);

                vb_offset += vertex_size;
                ib_offset += index_size;
            }

            pen::renderer_update_buffer(g_imgui_rs.vertex_buffer, g_imgui_rs.vb_copy_buffer, vb_offset);
            pen::renderer_update_buffer(g_imgui_rs.index_buffer, g_imgui_rs.ib_copy_buffer, ib_offset);

            float L = 0.0f;
            float R = ImGui::GetIO().DisplaySize.x;
            float B = ImGui::GetIO().DisplaySize.y;
            float T = 0.0f;
            float mvp[4][4] = {
                {2.0f / (R - L), 0.0f, 0.0f, 0.0f},
                {0.0f, 2.0f / (T - B), 0.0f, 0.0f},
                {0.0f, 0.0f, 0.5f, 0.0f},
                {(R + L) / (L - R), (T + B) / (B - T), 0.5f, 1.0f},
            };

            pen::renderer_update_buffer(g_imgui_rs.constant_buffer, mvp, sizeof(mvp), 0);
        }

        void render(ImDrawData* draw_data)
        {
            // set to main viewport
            pen::viewport vp = {0.0f, 0.0f, (f32)ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y, 0.0f, 1.0};

            pen::renderer_set_targets(PEN_BACK_BUFFER_COLOUR, PEN_BACK_BUFFER_DEPTH);
            pen::renderer_set_viewport(vp);
            pen::renderer_set_scissor_rect({vp.x, vp.y, vp.width, vp.height});

            update_dynamic_buffers(draw_data);

            pen::renderer_set_rasterizer_state(g_imgui_rs.raster_state);
            pen::renderer_set_blend_state(g_imgui_rs.blend_state);
            pen::renderer_set_depth_stencil_state(g_imgui_rs.depth_stencil_state);

            pmfx::set_technique(g_imgui_rs.imgui_shader, 0);

            pen::renderer_set_vertex_buffer(g_imgui_rs.vertex_buffer, 0, sizeof(ImDrawVert), 0);
            pen::renderer_set_index_buffer(g_imgui_rs.index_buffer, PEN_FORMAT_R16_UINT, 0);

            pen::renderer_set_constant_buffer(g_imgui_rs.constant_buffer, 1, pen::CBUFFER_BIND_VS);

            int vtx_offset = 0;
            int idx_offset = 0;
            for (int n = 0; n < draw_data->CmdListsCount; n++)
            {
                const ImDrawList* cmd_list = draw_data->CmdLists[n];
                for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
                {
                    const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
                    if (pcmd->UserCallback)
                    {
                        pcmd->UserCallback(cmd_list, pcmd);
                    }
                    else
                    {
                        pen::renderer_set_texture((u32)(intptr_t)pcmd->TextureId, g_imgui_rs.font_sampler_state, 0,
                                                  pen::TEXTURE_BIND_PS);

                        pen::rect r = {pcmd->ClipRect.x, pcmd->ClipRect.y, pcmd->ClipRect.z, pcmd->ClipRect.w};

                        pen::renderer_set_scissor_rect(r);

                        pen::renderer_draw_indexed(pcmd->ElemCount, idx_offset, vtx_offset, PEN_PT_TRIANGLELIST);
                    }
                    idx_offset += pcmd->ElemCount;
                }
                vtx_offset += cmd_list->VtxBuffer.Size;
            }
        }

        void create_render_states()
        {
            // raster state
            rasteriser_state_creation_params rcp;
            memory_zero(&rcp, sizeof(rasteriser_state_creation_params));
            rcp.fill_mode = PEN_FILL_SOLID;
            rcp.cull_mode = PEN_CULL_NONE;
            rcp.depth_bias_clamp = 0.0f;
            rcp.sloped_scale_depth_bias = 0.0f;
            rcp.scissor_enable = true;
            rcp.depth_clip_enable = true;

            g_imgui_rs.raster_state = pen::renderer_create_rasterizer_state(rcp);

            // create a sampler object so we can sample a texture
            pen::sampler_creation_params scp;
            pen::memory_zero(&scp, sizeof(pen::sampler_creation_params));
            scp.filter = PEN_FILTER_MIN_MAG_MIP_LINEAR;
            scp.address_u = PEN_TEXTURE_ADDRESS_WRAP;
            scp.address_v = PEN_TEXTURE_ADDRESS_WRAP;
            scp.address_w = PEN_TEXTURE_ADDRESS_WRAP;
            scp.comparison_func = PEN_COMPARISON_ALWAYS;
            scp.min_lod = 0.0f;
            scp.max_lod = 0.0f;

            g_imgui_rs.font_sampler_state = pen::renderer_create_sampler(scp);

            // constant buffer
            pen::buffer_creation_params bcp;
            bcp.usage_flags = PEN_USAGE_DYNAMIC;
            bcp.bind_flags = PEN_BIND_CONSTANT_BUFFER;
            bcp.cpu_access_flags = PEN_CPU_ACCESS_WRITE;
            bcp.buffer_size = sizeof(float) * 16;
            bcp.data = (void*)nullptr;

            g_imgui_rs.constant_buffer = pen::renderer_create_buffer(bcp);

            // blend state
            pen::render_target_blend rtb;
            rtb.blend_enable = 1;
            rtb.blend_op = PEN_BLEND_OP_ADD;
            rtb.blend_op_alpha = PEN_BLEND_OP_ADD;
            rtb.dest_blend = PEN_BLEND_INV_SRC_ALPHA;
            rtb.src_blend = PEN_BLEND_SRC_ALPHA;
            rtb.dest_blend_alpha = PEN_BLEND_INV_SRC_ALPHA;
            rtb.src_blend_alpha = PEN_BLEND_SRC_ALPHA;
            rtb.render_target_write_mask = 0x0F;

            pen::blend_creation_params blend_params;
            blend_params.alpha_to_coverage_enable = 0;
            blend_params.independent_blend_enable = 0;
            blend_params.render_targets = &rtb;
            blend_params.num_render_targets = 1;

            g_imgui_rs.blend_state = pen::renderer_create_blend_state(blend_params);

            // depth stencil state
            pen::depth_stencil_creation_params depth_stencil_params = {0};

            // Depth test parameters
            depth_stencil_params.depth_enable = true;
            depth_stencil_params.depth_write_mask = 1;
            depth_stencil_params.depth_func = PEN_COMPARISON_ALWAYS;

            g_imgui_rs.depth_stencil_state = pen::renderer_create_depth_stencil_state(depth_stencil_params);
        }

        void process_input()
        {
            ImGuiIO& io = ImGui::GetIO();

            // mouse wheel state
            const pen::mouse_state& ms = pen::input_get_mouse_state();

            io.MouseDown[0] = ms.buttons[PEN_MOUSE_L];
            io.MouseDown[1] = ms.buttons[PEN_MOUSE_R];
            io.MouseDown[2] = ms.buttons[PEN_MOUSE_M];

            static f32 prev_mouse_wheel = (f32)ms.wheel;

            io.MouseWheel += (f32)ms.wheel - prev_mouse_wheel;
            io.MousePos.x = (f32)ms.x;
            io.MousePos.y = (f32)ms.y;

            prev_mouse_wheel = (f32)ms.wheel;

            static f32 input_key_timer[512] = {0.0f};
            static f32 repeat_ms = 150.0f;

            static u32 timer_index = pen::timer_create("imgui_input_timer");

            f32 dt_ms = pen::timer_elapsed_ms(timer_index);
            pen::timer_start(timer_index);

            for (u32 i = 0; i < 512; ++i)
            {
                if (pen::input_get_unicode_key(i))
                {
                    if (input_key_timer[i] <= 0.0f)
                    {
                        io.KeysDown[i] = true;
                        input_key_timer[i] = repeat_ms;
                        io.AddInputCharacter(i);
                    }

                    input_key_timer[i] -= dt_ms;
                }
                else
                {
                    io.KeysDown[i] = false;
                    input_key_timer[i] = 0.0f;
                }
            }

            // Read keyboard modifiers inputs
            io.KeyCtrl = pen::input_key(PK_CONTROL);
            io.KeyShift = pen::input_key(PK_SHIFT);
            io.KeyAlt = pen::input_key(PK_MENU);
            io.KeySuper = false;
        }

        struct custom_draw_call
        {
            e_shader shader;
            u32      cbuffer;
        };

        void _set_shader_cb(const ImDrawList* parent_list, const ImDrawCmd* cmd)
        {
            custom_draw_call cd = *(custom_draw_call*)cmd->UserCallbackData;
            delete (custom_draw_call*)cmd->UserCallbackData;

            static hash_id ids[] = {PEN_HASH("tex_2d"), PEN_HASH("tex_cube"), PEN_HASH("tex_volume")};

            if (cd.shader == SHADER_DEFAULT)
            {
                pmfx::set_technique(g_imgui_rs.imgui_shader, 0);
                return;
            }

            pen::renderer_set_constant_buffer(cd.cbuffer, 7, pen::CBUFFER_BIND_PS | pen::CBUFFER_BIND_VS);

            pmfx::set_technique_perm(g_imgui_rs.imgui_ex_shader, ids[cd.shader]);
        }

        void set_shader(e_shader shader, u32 cbuffer)
        {
            custom_draw_call* cd = new custom_draw_call();
            cd->shader = shader;
            cd->cbuffer = cbuffer;

            ImGui::GetWindowDrawList()->AddCallback(&_set_shader_cb, (void*)cd);
        }

        void new_frame()
        {
            process_input();

            ImGuiIO& io = ImGui::GetIO();

            // set delta time
            f32        cur_time = pen::get_time_ms();
            static f32 prev_time = cur_time;
            io.DeltaTime = max((cur_time - prev_time) / 1000.0f, 0.0f);
            prev_time = cur_time;

            io.DisplaySize = ImVec2((f32)pen_window.width, (f32)pen_window.height);

            // Hide OS mouse cursor if ImGui is drawing it
            if (io.MouseDrawCursor)
                pen::input_show_cursor(false);

            ImGui::NewFrame();

            put::dev_ui::update();
        }

        u32 want_capture()
        {
            ImGuiIO& io = ImGui::GetIO();

            u32 flags = 0;

            if (io.WantCaptureMouse)
                flags |= dev_ui::MOUSE;

            if (io.WantCaptureKeyboard)
                flags |= dev_ui::KEYBOARD;

            if (io.WantTextInput)
                flags |= dev_ui::TEXT;

            return flags;
        }

        void render()
        {
            static bool enable_rendering = true;
            static bool debounce = false;

            if ((pen::input_key(PK_CONTROL) || pen::input_key(PK_COMMAND)) && pen::input_key(PK_X))
            {
                if (!debounce)
                {
                    enable_rendering = !enable_rendering;
                    debounce = true;
                }
            }
            else
            {
                debounce = false;
            }

            if (enable_rendering)
                ImGui::Render();
        }
    } // namespace dev_ui
} // namespace put
