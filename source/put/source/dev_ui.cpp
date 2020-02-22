// dev_ui.cpp
// Copyright 2014 - 2019 Alex Dixon.
// License: https://github.com/polymonster/pmtech/blob/master/license.md

#include <fstream>

#include "camera.h"
#include "console.h"
#include "data_struct.h"
#include "dev_ui.h"
#include "file_system.h"
#include "loader.h"
#include "memory.h"
#include "os.h"
#include "pen.h"
#include "pen_json.h"
#include "pen_string.h"
#include "renderer.h"
#include "str_utilities.h"
#include "input.h"
#include "pmfx.h"
#include "timer.h"

using namespace pen;
using namespace put;

namespace
{
    struct render_handles
    {
        u32 clear_state;
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
    
    render_handles  s_imgui_rs;
    pen::json       s_program_preferences;
    Str             s_program_prefs_filename;
    bool            s_console_open = false;
    s32             s_program_prefs_save_timer = 0;
    bool            s_save_program_prefs = false;
    const u32       s_program_prefs_save_timeout = 60; //frames
    bool            s_enable_rendering = true;
    
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

        s_imgui_rs.font_texture = pen::renderer_create_texture(tcp);

        io.Fonts->TexID = IMG(s_imgui_rs.font_texture);
    }

    void update_dynamic_buffers(ImDrawData* draw_data)
    {
        if (s_imgui_rs.vertex_buffer == 0 || (s32)s_imgui_rs.vb_size < draw_data->TotalVtxCount)
        {
            if (s_imgui_rs.vertex_buffer != 0)
            {
                pen::renderer_release_buffer(s_imgui_rs.vertex_buffer);
            };

            s_imgui_rs.vb_size = draw_data->TotalVtxCount + 10000;

            pen::buffer_creation_params bcp;
            bcp.usage_flags = PEN_USAGE_DYNAMIC;
            bcp.bind_flags = PEN_BIND_VERTEX_BUFFER;
            bcp.cpu_access_flags = PEN_CPU_ACCESS_WRITE;
            bcp.buffer_size = s_imgui_rs.vb_size * sizeof(ImDrawVert);
            bcp.data = (void*)nullptr;

            if (s_imgui_rs.vb_copy_buffer == nullptr)
            {
                s_imgui_rs.vb_copy_buffer = pen::memory_alloc(s_imgui_rs.vb_size * sizeof(ImDrawVert));
            }
            else
            {
                s_imgui_rs.vb_copy_buffer =
                    pen::memory_realloc(s_imgui_rs.vb_copy_buffer, s_imgui_rs.vb_size * sizeof(ImDrawVert));
            }

            s_imgui_rs.vertex_buffer = pen::renderer_create_buffer(bcp);
        }

        if (s_imgui_rs.index_buffer == 0 || (s32)s_imgui_rs.ib_size < draw_data->TotalIdxCount)
        {
            if (s_imgui_rs.index_buffer != 0)
            {
                pen::renderer_release_buffer(s_imgui_rs.index_buffer);
            };

            s_imgui_rs.ib_size = draw_data->TotalIdxCount + 5000;

            pen::buffer_creation_params bcp;
            bcp.usage_flags = PEN_USAGE_DYNAMIC;
            bcp.bind_flags = PEN_BIND_INDEX_BUFFER;
            bcp.cpu_access_flags = PEN_CPU_ACCESS_WRITE;
            bcp.buffer_size = s_imgui_rs.ib_size * sizeof(ImDrawIdx);
            bcp.data = (void*)nullptr;

            if (s_imgui_rs.ib_copy_buffer == nullptr)
            {
                s_imgui_rs.ib_copy_buffer = pen::memory_alloc(s_imgui_rs.ib_size * sizeof(ImDrawIdx));
            }
            else
            {
                s_imgui_rs.ib_copy_buffer =
                    pen::memory_realloc(s_imgui_rs.ib_copy_buffer, s_imgui_rs.ib_size * sizeof(ImDrawIdx));
            }

            s_imgui_rs.index_buffer = pen::renderer_create_buffer(bcp);
        }

        u32 vb_offset = 0;
        u32 ib_offset = 0;

        for (int n = 0; n < draw_data->CmdListsCount; n++)
        {
            ImDrawList* cmd_list = draw_data->CmdLists[n];
            u32         vertex_size = cmd_list->VtxBuffer.Size * sizeof(ImDrawVert);
            u32         index_size = cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx);

            c8* vb_mem = (c8*)s_imgui_rs.vb_copy_buffer;
            c8* ib_mem = (c8*)s_imgui_rs.ib_copy_buffer;

            memcpy(&vb_mem[vb_offset], cmd_list->VtxBuffer.Data, vertex_size);
            memcpy(&ib_mem[ib_offset], cmd_list->IdxBuffer.Data, index_size);

            vb_offset += vertex_size;
            ib_offset += index_size;
        }

        pen::renderer_update_buffer(s_imgui_rs.vertex_buffer, s_imgui_rs.vb_copy_buffer, vb_offset);
        pen::renderer_update_buffer(s_imgui_rs.index_buffer, s_imgui_rs.ib_copy_buffer, ib_offset);
        
        float L = 0.0f;
        float R = ImGui::GetIO().DisplaySize.x;
        float B = ImGui::GetIO().DisplaySize.y;
        float T = 0.0f;
        
        mat4 ortho = mat::create_orthographic_projection(L, R, B, T, 0.0f, 1.0f);
        
        pen::renderer_update_buffer(s_imgui_rs.constant_buffer, ortho.m, sizeof(mat4), 0);
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

        s_imgui_rs.raster_state = pen::renderer_create_rasterizer_state(rcp);

        // create a sampler object so we can sample a texture
        pen::sampler_creation_params scp;
        pen::memory_zero(&scp, sizeof(pen::sampler_creation_params));
        scp.filter = PEN_FILTER_MIN_MAG_MIP_LINEAR;
        scp.address_u = PEN_TEXTURE_ADDRESS_WRAP;
        scp.address_v = PEN_TEXTURE_ADDRESS_WRAP;
        scp.address_w = PEN_TEXTURE_ADDRESS_WRAP;
        scp.comparison_func = PEN_COMPARISON_ALWAYS;
        scp.min_lod = 0.0f;
        scp.max_lod = 1000.0f;

        s_imgui_rs.font_sampler_state = pen::renderer_create_sampler(scp);

        // constant buffer
        pen::buffer_creation_params bcp;
        bcp.usage_flags = PEN_USAGE_DYNAMIC;
        bcp.bind_flags = PEN_BIND_CONSTANT_BUFFER;
        bcp.cpu_access_flags = PEN_CPU_ACCESS_WRITE;
        bcp.buffer_size = sizeof(float) * 16;
        bcp.data = (void*)nullptr;

        s_imgui_rs.constant_buffer = pen::renderer_create_buffer(bcp);

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

        s_imgui_rs.blend_state = pen::renderer_create_blend_state(blend_params);

        // depth stencil state
        pen::depth_stencil_creation_params depth_stencil_params = {0};

        // Depth test parameters
        depth_stencil_params.depth_enable = true;
        depth_stencil_params.depth_write_mask = 1;
        depth_stencil_params.depth_func = PEN_COMPARISON_ALWAYS;

        s_imgui_rs.depth_stencil_state = pen::renderer_create_depth_stencil_state(depth_stencil_params);
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
        
        Str input = pen::input_get_unicode_input();
        io.AddInputCharactersUTF8(input.c_str());

        for(s32 i = 0; i < PK_COUNT; ++i)
            io.KeysDown[i] = pen::input_key(i);

        // Read keyboard modifiers inputs
        io.KeyCtrl = pen::input_key(PK_CONTROL);
        io.KeyShift = pen::input_key(PK_SHIFT);
        io.KeyAlt = pen::input_key(PK_MENU);
        io.KeySuper = false;
    }
}

namespace put
{
    namespace dev_ui
    {
        struct app_console;
        static app_console* sp_dev_console;
        
        void render(ImDrawData* draw_data);

        bool init()
        {
            pen::memory_zero(&s_imgui_rs, sizeof(s_imgui_rs));

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
            s_imgui_rs.imgui_shader = pmfx::load_shader("imgui");
            s_imgui_rs.imgui_ex_shader = pmfx::load_shader("imgui_ex");

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

        void render(ImDrawData* draw_data)
        {
            update_dynamic_buffers(draw_data);

            // set to main viewport
            s32 iw, ih;
            pen::window_get_size(iw, ih);
            pen::viewport vp = {0.0f, 0.0f, PEN_BACK_BUFFER_RATIO, 1.0f, 0.0f, 1.0};

            pen::renderer_set_targets(PEN_BACK_BUFFER_COLOUR, PEN_BACK_BUFFER_DEPTH);
            
            pen::renderer_set_viewport(vp);
            pen::renderer_set_scissor_rect({vp.x, vp.y, vp.width, vp.height});
            pen::renderer_set_rasterizer_state(s_imgui_rs.raster_state);
            pen::renderer_set_blend_state(s_imgui_rs.blend_state);
            pen::renderer_set_depth_stencil_state(s_imgui_rs.depth_stencil_state);

            pmfx::set_technique(s_imgui_rs.imgui_shader, 0);

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
                        pen::renderer_set_texture((u32)(intptr_t)pcmd->TextureId, s_imgui_rs.font_sampler_state, 0,
                                                  pen::TEXTURE_BIND_PS);

                        pen::rect r = {pcmd->ClipRect.x, pcmd->ClipRect.y, pcmd->ClipRect.z, pcmd->ClipRect.w};
                        
                        // convert scissor to ratios, for safe window resizing
                        r.left /= (f32)iw;
                        r.top /= (f32)ih;
                        r.right /= (f32)iw;
                        r.bottom /= (f32)ih;

                        pen::renderer_set_scissor_rect_ratio(r);

                        pen::renderer_set_vertex_buffer(s_imgui_rs.vertex_buffer, 0, sizeof(ImDrawVert), 0);
                        pen::renderer_set_index_buffer(s_imgui_rs.index_buffer, PEN_FORMAT_R16_UINT, 0);
                        pen::renderer_set_constant_buffer(s_imgui_rs.constant_buffer, 1, pen::CBUFFER_BIND_VS);

                        pen::renderer_draw_indexed(pcmd->ElemCount, idx_offset, vtx_offset, PEN_PT_TRIANGLELIST);
                    }
                    idx_offset += pcmd->ElemCount;
                }
                vtx_offset += cmd_list->VtxBuffer.Size;
            }
        }

        struct custom_draw_call
        {
            ui_shader shader;
            u32       cbuffer;
        };

        void _set_shader_cb(const ImDrawList* parent_list, const ImDrawCmd* cmd)
        {
            custom_draw_call cd = *(custom_draw_call*)cmd->UserCallbackData;
            delete (custom_draw_call*)cmd->UserCallbackData;

            static hash_id ids[] = {
                PEN_HASH("tex_2d"),
                PEN_HASH("tex_cube"),
                PEN_HASH("tex_volume"),
                PEN_HASH("tex_2d_array"),
                PEN_HASH("tex_cube_array")
            };

            if (cd.shader == e_ui_shader::imgui)
            {
                pmfx::set_technique(s_imgui_rs.imgui_shader, 0);
                return;
            }

            pmfx::set_technique_perm(s_imgui_rs.imgui_ex_shader, ids[cd.shader]);

            pen::renderer_set_constant_buffer(cd.cbuffer, 7, pen::CBUFFER_BIND_PS | pen::CBUFFER_BIND_VS);
        }

        void set_shader(ui_shader shader, u32 cbuffer)
        {
            custom_draw_call* cd = new custom_draw_call();
            cd->shader = shader;
            cd->cbuffer = cbuffer;

            ImGui::GetWindowDrawList()->AddCallback(&_set_shader_cb, (void*)cd);
        }

        void new_frame()
        {
            process_input();
            
            // toggle enable disable
            static bool _db = false;
            if ((pen::input_key(PK_CONTROL) || pen::input_key(PK_COMMAND)) && pen::press_debounce(PK_X, _db))
                s_enable_rendering = !s_enable_rendering;

            ImGuiIO& io = ImGui::GetIO();

            // set delta time
            f32        cur_time = pen::get_time_ms();
            static f32 prev_time = cur_time;
            io.DeltaTime = max((cur_time - prev_time) / 1000.0f, 0.0f);
            prev_time = cur_time;

            s32 w, h;
            pen::window_get_size(w, h);
            io.DisplaySize = ImVec2((f32)w, (f32)h);

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
                flags |= dev_ui::e_io_capture::mouse;

            if (io.WantCaptureKeyboard)
                flags |= dev_ui::e_io_capture::keyboard;

            if (io.WantTextInput)
                flags |= dev_ui::e_io_capture::text;

            return flags;
        }

        void render()
        {
            if (s_enable_rendering)
                ImGui::Render();
        }

        void load_program_preferences()
        {
            s_program_prefs_filename = pen::window_get_title();
            s_program_prefs_filename.append("_prefs.jsn");

            s_program_preferences = pen::json::load_from_file(s_program_prefs_filename.c_str());
        }

        void perform_save_program_prefs()
        {
            if (s_save_program_prefs && s_program_prefs_save_timer < 0)
            {
                s_save_program_prefs = false;

                std::ofstream ofs(s_program_prefs_filename.c_str());

                ofs << s_program_preferences.dumps().c_str();

                ofs.close();
            }

            s_program_prefs_save_timer--;
        }

        void save_program_preferences()
        {
            s_save_program_prefs = true;
            s_program_prefs_save_timer = s_program_prefs_save_timeout;
        }

        void set_last_used_directory(Str& dir)
        {
            // make a copy of the string to format
            Str formatted = dir;

            formatted = str_replace_chars(formatted, '\\', '/');

            // strip the file
            s32 last_dir = str_find_reverse(formatted, "/");
            s32 ext = str_find_reverse(formatted, ".");

            Str final = "\"";

            if (last_dir < ext)
            {
                // strip file
                for (u32 i = 0; i < last_dir + 1; ++i)
                    final.append(formatted.c_str()[i]);
            }
            else
            {
                // directory
                final.append(formatted.c_str());
            }
            final.append("\"");

            s_program_preferences.set_filename("last_used_directory", final);

            save_program_preferences();
        }

        void set_program_preference(const c8* name, f32 val)
        {
            Str str_val;
            str_val.appendf("%f", val);
            set_program_preference(name, str_val);
        }

        void set_program_preference(const c8* name, s32 val)
        {
            Str str_val;
            str_val.appendf("%i", val);
            set_program_preference(name, str_val);
        }

        void set_program_preference(const c8* name, bool val)
        {
            Str str_val = "false";
            if (val)
                str_val = "true";

            set_program_preference(name, str_val);
        }

        void set_program_preference(const c8* name, Str val)
        {
            s_program_preferences.set(name, val);
            save_program_preferences();
        }

        void set_program_preference_filename(const c8* name, Str val)
        {
            val = pen::str_replace_chars(val, ':', '@');
            s_program_preferences.set(name, val);

            save_program_preferences();
        }

        pen::json get_program_preference(const c8* name)
        {
            return s_program_preferences[name];
        }

        Str get_program_preference_filename(const c8* name, const c8* default_value)
        {
            Str temp = s_program_preferences[name].as_filename(default_value);
            return temp;
        }

        const c8** get_last_used_directory(s32& directory_depth)
        {
            static const s32 max_directory_depth = 32;
            static c8*       directories[max_directory_depth];

            if (s_program_preferences.type() != JSMN_UNDEFINED)
            {
                pen::json last_dir = s_program_preferences["last_used_directory"];

                if (last_dir.type() != JSMN_UNDEFINED)
                {
                    Str path = last_dir.as_filename();

                    s32 dir_pos = 0;
                    directory_depth = 0;
                    bool finished = false;
                    while (!finished)
                    {
                        s32 prev_pos = dir_pos;
                        dir_pos = str_find(path, "/", prev_pos);

                        if (dir_pos != -1)
                        {
                            dir_pos += 1;
                        }
                        else
                        {
                            dir_pos = path.length();
                            finished = true;
                        }

                        if (dir_pos - prev_pos > 0)
                        {
                            if (directories[directory_depth])
                                pen::memory_free(directories[directory_depth]);

                            directories[directory_depth] = (c8*)pen::memory_alloc((dir_pos - prev_pos) + 1);

                            s32 j = 0;
                            for (s32 i = prev_pos; i < dir_pos; ++i)
                            {
                                // first dir on unix can be '/' otherwise we just want the diretcory names with no slashes
                                if (path[i] == '/' && directory_depth > pen::filesystem_exclude_slash_depth())
                                {
                                    // otherwise exclude slashes from the dirname
                                    continue;
                                }

                                directories[directory_depth][j] = path[i];
                                ++j;
                            }

                            directories[directory_depth][j] = '\0';

                            ++directory_depth;
                        }
                    }

                    return (const c8**)directories;
                }
            }

            s32        default_depth = 0;
            const c8** default_dir = pen::filesystem_get_user_directory(default_depth);

            directory_depth = default_depth;

            return default_dir;
        }

        const c8* file_browser(bool& dialog_open, file_browser_flags flags, s32 num_filetypes, ...)
        {
            static bool initialise = true;
            static s32  current_depth = 1;
            static s32  selection_stack[128] = {-1};
            static c8   user_filename_buf[1024];

            Str        current_path;
            Str        search_path;
            static Str selected_path;
            static Str selected_path_and_file;
            static Str last_result;

            static pen::fs_tree_node fs_enumeration;

            if (initialise)
            {
                s32        default_depth = 0;
                const c8** default_dir = get_last_used_directory(default_depth);

                selected_path = put::dev_ui::get_program_preference_filename("last_used_directory");

                pen::filesystem_enum_volumes(fs_enumeration);

                pen::fs_tree_node* fs_iter = &fs_enumeration;

                for (s32 c = 0; c < default_depth; ++c)
                {
                    for (u32 entry = 0; entry < fs_iter->num_children; ++entry)
                    {
                        if (pen::string_compare(fs_iter->children[entry].name, default_dir[c]) == 0)
                        {
                            current_path.append(fs_iter->children[entry].name);
                            if (fs_iter->children[entry].name[0] != '/')
                                current_path.append("/");

                            va_list wildcards;
                            va_start(wildcards, num_filetypes);

                            pen::filesystem_enum_directory(current_path.c_str(), fs_iter->children[entry], num_filetypes,
                                                           wildcards);

                            va_end(wildcards);

                            selection_stack[c] = entry;

                            fs_iter = &fs_iter->children[entry];

                            current_depth = c + 2;

                            break;
                        }
                    }
                }

                initialise = false;
            }

            ImGui::SetNextWindowSize(ImVec2(800, 450), ImGuiCond_FirstUseEver);
            if (ImGui::Begin("File Browser", &dialog_open))
            {
                ImGui::Text("%s", selected_path.c_str());

                const c8* return_value = nullptr;

                ImGuiButtonFlags button_flags = 0;
                if (selected_path == "")
                {
                    button_flags |= ImGuiButtonFlags_Disabled;
                }

                if (flags & e_file_browser_flags::save)
                {
                    user_filename_buf[0] = '/';
                    ImGui::InputText("", &user_filename_buf[1], 1023);
                }
                else
                {
                    user_filename_buf[0] = '\0';
                }

                if (ImGui::ButtonEx("OK", ImVec2(0, 0), button_flags))
                {
                    set_last_used_directory(selected_path);

                    selected_path_and_file = selected_path;
                    if (pen::string_length(user_filename_buf) > 1)
                        selected_path_and_file.append(user_filename_buf);

                    if (num_filetypes == 1 && flags & e_file_browser_flags::save)
                    {
                        // auto add extension
                        va_list wildcards;
                        va_start(wildcards, num_filetypes);

                        const c8* ft = va_arg(wildcards, const char*);

                        s32 ext_len = pen::string_length(ft);

                        s32       offset = 0;
                        const c8* fti = ft + ext_len - 1;
                        while (*fti-- != '.')
                        {
                            ext_len--;
                            offset++;
                        }

                        const c8* fts = ft + offset;

                        s32 filename_len = selected_path_and_file.length();

                        const c8* fn = &selected_path_and_file.c_str()[filename_len - ext_len];

                        if (pen::string_compare(fts, fn) != 0)
                        {
                            selected_path_and_file.append('.');
                            selected_path_and_file.append(fts);
                        }

                        va_end(wildcards);
                    }

                    last_result = selected_path_and_file;
                    return_value = last_result.c_str();

                    selected_path_and_file.clear();

                    dialog_open = false;
                }

                ImGui::SameLine();
                if (ImGui::Button("Cancel"))
                {
                    dialog_open = false;
                }

                ImGui::SameLine();
                if (ImGui::Button(ICON_FA_HOME))
                {
                    // set to home
                    initialise = true;
                    Str home = pen::filesystem_get_user_directory();
                    set_last_used_directory(home);
                }

                ImGui::SameLine();
                if (ImGui::Button("."))
                {
                    pen::filesystem_toggle_hidden_files();
                    set_last_used_directory(selected_path);
                    initialise = true;
                }
                dev_ui::set_tooltip("Toggle show hidden files");

                ImGui::BeginChild("scrolling", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);

                static s32 base_frame = current_depth - 4;
                if (ImGui::Button(ICON_FA_ARROW_LEFT))
                    base_frame--;
                ImGui::SameLine();
                if (ImGui::Button(ICON_FA_ARROW_RIGHT))
                    base_frame++;

                base_frame = std::min<s32>(current_depth - 4, base_frame);
                base_frame = std::max<s32>(0, base_frame);

                static const s32 max_depth = 4;
                ImGui::Columns(std::min<s32>(max_depth, current_depth), "directories");

                ImGui::Separator();
                pen::fs_tree_node* fs_iter = &fs_enumeration;

                for (s32 d = 0; d < current_depth; ++d)
                {
                    if (d >= base_frame && d < base_frame + max_depth)
                    {
                        ImGui::Text("%s", fs_iter->name);
                        ImGui::NextColumn();
                    }

                    fs_iter = &fs_iter->children[selection_stack[d]];
                }

                ImGui::Separator();

                current_path = "";
                search_path = "";

                fs_iter = &fs_enumeration;

                for (s32 c = 0; c < current_depth; ++c)
                {
                    if (c >= base_frame && c < base_frame + max_depth)
                    {
                        for (u32 entry = 0; entry < fs_iter->num_children; ++entry)
                        {
                            ImGui::PushID(fs_iter);

                            if (ImGui::Selectable(fs_iter->children[entry].name))
                            {
                                search_path = current_path;
                                search_path.append(fs_iter->children[entry].name);

                                va_list wildcards;
                                va_start(wildcards, num_filetypes);

                                pen::filesystem_enum_directory(search_path.c_str(), fs_iter->children[entry], num_filetypes,
                                                               wildcards);

                                va_end(wildcards);

                                if (fs_iter->children[entry].num_children > 0)
                                {
                                    for (s32 i = c; i < current_depth; ++i)
                                    {
                                        selection_stack[i] = -1;
                                    }

                                    current_depth = c + 2;
                                    selection_stack[c] = entry;

                                    selected_path = search_path;
                                }
                                else
                                {
                                    selected_path = "";
                                    selected_path = search_path;
                                }

                                base_frame++;
                            }

                            ImGui::PopID();
                        }

                        if (selection_stack[c] >= 0)
                            ImGui::NextColumn();
                    }

                    if (selection_stack[c] >= 0)
                    {
                        fs_iter = &fs_iter->children[selection_stack[c]];

                        if (fs_iter)
                            current_path.append(fs_iter->name);

                        if (!(current_path.c_str()[0] == '/' && current_path.length() == 1))
                            current_path.append("/");
                    }
                    else
                    {
                        break;
                    }
                }

                ImGui::EndChild();
                ImGui::Columns(1);

                ImGui::End();

                if (!dialog_open)
                {
                    initialise = true;
                    filesystem_enum_free_mem(fs_enumeration);
                }

                return return_value;
            }

            return nullptr;
        }

        bool state_button(const c8* text, bool state_active)
        {
            ImVec4 button_col = (&ImGui::GetStyle())->Colors[ImGuiCol_Button];

            if (state_active)
            {
                button_col.x *= 0.5f;
                button_col.y *= 0.5f;
                button_col.z *= 0.5f;
            }

            ImGui::PushStyleColor(ImGuiCol_Button, button_col);

            if (ImGui::Button(text))
            {
                ImGui::PopStyleColor();
                return true;
            }

            ImGui::PopStyleColor();

            return false;
        }

        void set_tooltip(const c8* fmt, ...)
        {
            static f32       k_tooltip_timer = 0.0f;
            static const f32 k_delay = 1.0f;
            static ImGuiID   k_current = 0;

            f32 dt = ImGui::GetIO().DeltaTime;

            if (!ImGui::IsItemHovered())
            {
                return;
            }

            ImGuiID now = ImGui::GetID(fmt);
            if (k_current != now)
            {
                k_tooltip_timer = 0.0f;
                k_current = now;
            }

            k_tooltip_timer += dt;
            if (k_tooltip_timer > k_delay)
            {
                va_list args;
                va_start(args, fmt);
                ImGui::SetTooltipV(fmt, args);
                va_end(args);
            }
        }

        struct console_item
        {
            u32 level;
            c8* message;
        };

        struct app_console
        {
            char                   InputBuf[256];
            ImVector<console_item> Items;
            bool                   ScrollToBottom;
            ImVector<char*>        History;
            int                    HistoryPos; // -1: new line, 0..History.Size-1 browsing history.
            ImVector<const char*>  Commands;

            app_console()
            {
                ClearLog();
                memset(InputBuf, 0, sizeof(InputBuf));
                HistoryPos = -1;
                Commands.push_back("help");
                Commands.push_back("history");
                Commands.push_back("clear");
                Commands.push_back("classify"); // "classify" is here to provide an example of "C"+[tab] completing to "CL"
                                                // and displaying matches.
            }

            ~app_console()
            {
                ClearLog();
                for (int i = 0; i < History.Size; i++)
                    free(History[i]);
            }

            // Portable helpers
            static int Stricmp(const char* str1, const char* str2)
            {
                int d;
                while ((d = toupper(*str2) - toupper(*str1)) == 0 && *str1)
                {
                    str1++;
                    str2++;
                }
                return d;
            }
            static int Strnicmp(const char* str1, const char* str2, int n)
            {
                int d = 0;
                while (n > 0 && (d = toupper(*str2) - toupper(*str1)) == 0 && *str1)
                {
                    str1++;
                    str2++;
                    n--;
                }
                return d;
            }
            static char* Strdup(const char* str)
            {
                size_t len = strlen(str) + 1;
                void*  buff = malloc(len);
                return (char*)memcpy(buff, (const void*)str, len);
            }

            void ClearLog()
            {
                for (int i = 0; i < Items.Size; i++)
                    free(Items[i].message);
                Items.clear();
                ScrollToBottom = true;
            }

            void AddLogV(u32 type, const char* fmt, va_list args)
            {
                static char buf[4096];
                vsnprintf(buf, IM_ARRAYSIZE(buf), fmt, args);
                buf[IM_ARRAYSIZE(buf) - 1] = 0;
                Items.push_back({type, Strdup(buf)});
                ScrollToBottom = true;
            }

            void Draw(const char* title, bool* p_open)
            {
                ImGui::SetNextWindowSize(ImVec2(520, 600), ImGuiCond_FirstUseEver);
                if (!ImGui::Begin(title, p_open))
                {
                    ImGui::End();
                    return;
                }

                if (ImGui::SmallButton("Clear"))
                {
                    ClearLog();
                }
                ImGui::SameLine();
                bool copy_to_clipboard = ImGui::SmallButton("Copy");
                ImGui::SameLine();
                if (ImGui::SmallButton("Scroll to bottom"))
                    ScrollToBottom = true;

                ImGui::Separator();

                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
                static ImGuiTextFilter filter;
                filter.Draw("Filter (\"incl,-excl\") (\"error\")", 180);
                ImGui::PopStyleVar();
                ImGui::Separator();

                ImGui::BeginChild("ScrollingRegion", ImVec2(0, -ImGui::GetItemsLineHeightWithSpacing()), false,
                                  ImGuiWindowFlags_HorizontalScrollbar);
                if (ImGui::BeginPopupContextWindow())
                {
                    if (ImGui::Selectable("Clear"))
                        ClearLog();
                    ImGui::EndPopup();
                }

                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1)); // Tighten spacing
                if (copy_to_clipboard)
                    ImGui::LogToClipboard();
                for (int i = 0; i < Items.Size; i++)
                {
                    console_item item = Items[i];
                    if (!filter.PassFilter(item.message))
                        continue;

                    static ImVec4 level_colours[] = {
                        ImVec4(1.0f, 1.0f, 1.0f, 1.0f),
                        ImVec4(1.0f, 0.78f, 0.58f, 1.0f),
                        ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                    };

                    ImVec4 col = level_colours[item.level];

                    ImGui::PushStyleColor(ImGuiCol_Text, col);
                    ImGui::TextUnformatted(item.message);
                    ImGui::PopStyleColor();
                }

                if (copy_to_clipboard)
                    ImGui::LogFinish();

                if (ScrollToBottom)
                    ImGui::SetScrollHere();

                ScrollToBottom = false;
                ImGui::PopStyleVar();
                ImGui::EndChild();
                ImGui::Separator();

                // Command-line
                if (ImGui::InputText("Input", InputBuf, IM_ARRAYSIZE(InputBuf),
                                     ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackCompletion |
                                         ImGuiInputTextFlags_CallbackHistory,
                                     &TextEditCallbackStub, (void*)this))
                {
                    char* input_end = InputBuf + strlen(InputBuf);
                    while (input_end > InputBuf && input_end[-1] == ' ')
                    {
                        input_end--;
                    }
                    *input_end = 0;
                    if (InputBuf[0])
                        ExecCommand(InputBuf);
                    strcpy(InputBuf, "");
                }

                // Demonstrate keeping auto focus on the input box
                if (ImGui::IsItemHovered() ||
                    (ImGui::IsRootWindowOrAnyChildFocused() && !ImGui::IsAnyItemActive() && !ImGui::IsMouseClicked(0)))
                    ImGui::SetKeyboardFocusHere(-1); // Auto focus previous widget

                ImGui::End();
            }

            void ExecCommand(const char* command_line)
            {
                /*
                AddLog("# %s\n", command_line);

                // Insert into history. First find match and delete it so it can be pushed to the back. This isn't trying to
                be smart or optimal. HistoryPos = -1; for (int i = History.Size-1; i >= 0; i--) if (Stricmp(History[i],
                command_line) == 0)
                    {
                        free(History[i]);
                        History.erase(History.begin() + i);
                        break;
                    }
                History.push_back(Strdup(command_line));

                // Process command
                if (Stricmp(command_line, "CLEAR") == 0)
                {
                    ClearLog();
                }
                else if (Stricmp(command_line, "HELP") == 0)
                {
                    AddLog("Commands:");
                    for (int i = 0; i < Commands.Size; i++)
                        AddLog("- %s", Commands[i]);
                }
                else if (Stricmp(command_line, "HISTORY") == 0)
                {
                    int first = History.Size - 10;
                    for (int i = first > 0 ? first : 0; i < History.Size; i++)
                        AddLog("%3d: %s\n", i, History[i]);
                }
                else
                {
                    AddLog("Unknown command: '%s'\n", command_line);
                }
                 */
            }

            static int TextEditCallbackStub(ImGuiTextEditCallbackData* data) // In C++11 you are better off using lambdas for
                                                                             // this sort of forwarding callbacks
            {
                app_console* console = (app_console*)data->UserData;
                return console->TextEditCallback(data);
            }

            int TextEditCallback(ImGuiTextEditCallbackData* data)
            {
                // AddLog("cursor: %d, selection: %d-%d", data->CursorPos, data->SelectionStart, data->SelectionEnd);
                switch (data->EventFlag)
                {
                    case ImGuiInputTextFlags_CallbackCompletion:
                    {
                        // Example of TEXT COMPLETION

                        // Locate beginning of current word
                        const char* word_end = data->Buf + data->CursorPos;
                        const char* word_start = word_end;
                        while (word_start > data->Buf)
                        {
                            const char c = word_start[-1];
                            if (c == ' ' || c == '\t' || c == ',' || c == ';')
                                break;
                            word_start--;
                        }

                        // Build a list of candidates
                        ImVector<const char*> candidates;
                        for (int i = 0; i < Commands.Size; i++)
                            if (Strnicmp(Commands[i], word_start, (int)(word_end - word_start)) == 0)
                                candidates.push_back(Commands[i]);

                        if (candidates.Size == 0)
                        {
                            // No match
                            // AddLog("No match for \"%.*s\"!\n", (int)(word_end-word_start), word_start);
                        }
                        else if (candidates.Size == 1)
                        {
                            // Single match. Delete the beginning of the word and replace it entirely so we've got nice casing
                            data->DeleteChars((int)(word_start - data->Buf), (int)(word_end - word_start));
                            data->InsertChars(data->CursorPos, candidates[0]);
                            data->InsertChars(data->CursorPos, " ");
                        }
                        else
                        {
                            // Multiple matches. Complete as much as we can, so inputing "C" will complete to "CL" and display
                            // "CLEAR" and "CLASSIFY"
                            int match_len = (int)(word_end - word_start);
                            for (;;)
                            {
                                int  c = 0;
                                bool all_candidates_matches = true;
                                for (int i = 0; i < candidates.Size && all_candidates_matches; i++)
                                    if (i == 0)
                                        c = toupper(candidates[i][match_len]);
                                    else if (c == 0 || c != toupper(candidates[i][match_len]))
                                        all_candidates_matches = false;
                                if (!all_candidates_matches)
                                    break;
                                match_len++;
                            }

                            if (match_len > 0)
                            {
                                data->DeleteChars((int)(word_start - data->Buf), (int)(word_end - word_start));
                                data->InsertChars(data->CursorPos, candidates[0], candidates[0] + match_len);
                            }

                            // List matches
                            // AddLog("Possible matches:\n");
                            // for (int i = 0; i < candidates.Size; i++)
                            // AddLog("- %s\n", candidates[i]);
                        }

                        break;
                    }
                    case ImGuiInputTextFlags_CallbackHistory:
                    {
                        // Example of HISTORY
                        const int prev_history_pos = HistoryPos;
                        if (data->EventKey == ImGuiKey_UpArrow)
                        {
                            if (HistoryPos == -1)
                                HistoryPos = History.Size - 1;
                            else if (HistoryPos > 0)
                                HistoryPos--;
                        }
                        else if (data->EventKey == ImGuiKey_DownArrow)
                        {
                            if (HistoryPos != -1)
                                if (++HistoryPos >= History.Size)
                                    HistoryPos = -1;
                        }

                        // A better implementation would preserve the data on the current input line along with cursor
                        // position.
                        if (prev_history_pos != HistoryPos)
                        {
                            data->CursorPos = data->SelectionStart = data->SelectionEnd = data->BufTextLen = (int)snprintf(
                                data->Buf, (size_t)data->BufSize, "%s", (HistoryPos >= 0) ? History[HistoryPos] : "");
                            data->BufDirty = true;
                        }
                    }
                }
                return 0;
            }
        };

        void console()
        {
            if (s_console_open)
            {
                sp_dev_console->Draw("Console", &s_console_open);
            }
        }

        void show_console(bool val)
        {
            s_console_open = val;
        }
        
        bool is_console_open()
        {
            return s_console_open;
        }

        void log(const c8* fmt, ...)
        {
            va_list args;
            va_start(args, fmt);
            sp_dev_console->AddLogV(0, fmt, args);
            va_end(args);
        }

        void log_level(u32 level, const c8* fmt, ...)
        {
            va_list args;
            va_start(args, fmt);
            sp_dev_console->AddLogV(level, fmt, args);
            va_end(args);

            if (level > 1)
                s_console_open = true;
        }

        void util_init()
        {
            load_program_preferences();
            sp_dev_console = new app_console();
        }
        
        void enable(bool enabled)
        {
            s_enable_rendering = enabled;
        }

        void update()
        {
            // check for window frame updates
            static window_frame f;
            window_frame        f2;

            pen::window_get_frame(f2);

            if (memcmp(&f, &f2, sizeof(window_frame)) != 0)
            {
                f = f2;

                bool valid = true;
                if ((s32)f.width <= 0 || (s32)f.height <= 0 || (s32)f.x <= 0 || (s32)f.y <= 0)
                    valid = false;

                if (valid)
                {
                    set_program_preference("window_x", (s32)f.x);
                    set_program_preference("window_y", (s32)f.y);
                    set_program_preference("window_width", (s32)f.width);
                    set_program_preference("window_height", (s32)f.height);
                }
            }

            // update console
            console();

            // perform program prefs save
            perform_save_program_prefs();
        }

        void show_platform_info()
        {
            static bool opened = false;
            if (opened)
            {
                ImGui::Begin("Platform Info", &opened, ImGuiWindowFlags_AlwaysAutoResize);

                const pen::renderer_info& ri = pen::renderer_get_info();
                ImGui::Text("Graphics Api: %s", ri.api_version);
                ImGui::Text("Shader Version: %s", ri.shader_version);
                ImGui::Text("Renderer: %s", ri.renderer);
                ImGui::Text("Vendor: %s", ri.vendor);

                ImGui::End();
            }
        }

        struct image_cbuffer
        {
            vec4f colour_mask = vec4f(1.0f, 1.0f, 1.0f, 1.0f); // mask for rgba channels
            vec4f params = vec4f(-1.0f, -1.0f, 1.0f, 0.0f);    // x = mip, y = array, z = pow, w = viewport flip
            mat4  inverse_wvp = mat4::create_identity();       // inverse wvp for cubemap, sdf and volume ray march
        };

        struct image_ex_data
        {
            u32           handle;
            camera        cam;
            image_cbuffer cbuffer;
            u32           cbuffer_handle;
        };

        void image_ex(u32 handle, vec2f size, ui_shader shader)
        {
            ImVec2 canvas_size = ImVec2(size.x, size.y);

            // cache params for rotations and settings
            static image_ex_data* _image = nullptr;
            u32                   num_cbuf = sb_count(_image);
            s32                   ix = -1;
            for (u32 i = 0; i < num_cbuf; ++i)
            {
                if (_image[i].handle == handle)
                {
                    ix = i;
                    break;
                }
            }

            image_cbuffer _cb; // copy of cb to compare for changes
            bool          force_update = false;
            if (ix == -1)
            {
                ix = num_cbuf;
                force_update = true;

                image_ex_data new_data;
                new_data.handle = handle;
                camera_create_perspective(&new_data.cam, 60.0f, 1.0f, 0.1f, 10.0f);
                sb_push(_image, new_data);

                pen::buffer_creation_params bcp;
                bcp.usage_flags = PEN_USAGE_DYNAMIC;
                bcp.bind_flags = PEN_BIND_CONSTANT_BUFFER;
                bcp.cpu_access_flags = PEN_CPU_ACCESS_WRITE;
                bcp.buffer_size = sizeof(camera_cbuffer);
                bcp.data = nullptr;

                _image[ix].cbuffer_handle = pen::renderer_create_buffer(bcp);
            }
            else
            {
                _cb = _image[ix].cbuffer;
            }

            image_cbuffer& cb = _image[ix].cbuffer;

            // viewport
            cb.params.w = 0.0f;

            // mips
            if (ImGui::Button(ICON_FA_MINUS))
                cb.params.x--;
            ImGui::SameLine();
            if (ImGui::Button(ICON_FA_PLUS))
                cb.params.x++;

            // arrays
            if (shader == e_ui_shader::texture_array ||
                shader == e_ui_shader::texture_cube_array)
            {
                ImGui::SameLine();
                ImGui::PushID("Array");
                ImGui::SameLine();
                if (ImGui::Button(ICON_FA_ARROW_DOWN))
                    cb.params.y--;
                ImGui::SameLine();
                if (ImGui::Button(ICON_FA_ARROW_UP))
                    cb.params.y++;
                ImGui::PopID();
            }

            // rgba channel mask
            ImGui::SameLine();
            static const vec4f colours[] = {vec4f(0.7f, 0.0f, 0.0f, 1.0f), vec4f(0.0f, 0.7f, 0.0f, 1.0f),
                                            vec4f(0.0f, 0.0f, 0.7f, 1.0f), vec4f(0.7f, 0.7f, 0.7f, 1.0f)};
            static const char* buttons[] = {"R", "G", "B", "A"};
            for (u32 i = 0; i < PEN_ARRAY_SIZE(buttons); ++i)
            {
                f32 b = 1.0f;
                if (!cb.colour_mask[i])
                    b = 0.2f;

                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(colours[i].r * b, colours[i].g * b, colours[i].b * b, 1.0f));
                if (ImGui::Button(buttons[i]))
                {
                    if (cb.colour_mask[i] == 1.0f)
                    {
                        cb.colour_mask[i] = 0.0f;
                    }
                    else
                    {
                        cb.colour_mask[i] = 1.0;
                    }
                }
                ImGui::PopStyleColor();
                if (i < PEN_ARRAY_SIZE(buttons) - 1)
                    ImGui::SameLine();
            }

            // slider for pow range
            ImGui::SliderFloat("Pow", &cb.params.b, 1.0f, 1000.0f);

            //
            camera& cam = _image[ix].cam;

            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            ImVec2      canvas_pos = ImGui::GetCursorScreenPos();

            ImVec2 uv0 = ImVec2(0, 0);
            ImVec2 uv1 = ImVec2(1, 1);
            ImVec4 tint_col = ImVec4(1, 1, 1, 1);

            ImVec2 bb_max;
            bb_max.x = canvas_pos.x + canvas_size.x;
            bb_max.y = canvas_pos.y + canvas_size.y;

            ImGui::InvisibleButton("canvas", canvas_size);

            if (ImGui::IsItemHovered() || force_update)
            {
                if (ImGui::GetIO().MouseDown[0])
                {
                    cam.rot.x += ImGui::GetIO().MouseDelta.y * 0.1f;
                    cam.rot.y += ImGui::GetIO().MouseDelta.x * 0.1f;
                }

                cam.zoom += ImGui::GetIO().MouseWheel;
                cam.zoom = max(cam.zoom, 1.0f);
                cam.zoom = 3.0f;

                camera_update_look_at(&cam);
                cb.inverse_wvp = mat::inverse4x4(cam.proj * cam.view);
            }

            // update cbuffer if dirty
            if (memcmp(&_cb, &cb, sizeof(image_cbuffer)) != 0 || force_update)
            {
                pen::renderer_update_buffer(_image[ix].cbuffer_handle, &cb, sizeof(image_cbuffer));
            }

            dev_ui::set_shader(shader, _image[ix].cbuffer_handle);

            draw_list->AddImage(IMG(handle), canvas_pos, bb_max, uv0, uv1, ImGui::GetColorU32(tint_col));

            dev_ui::set_shader(dev_ui::e_ui_shader::imgui, 0);
        }
    } // namespace dev_ui
} // namespace put
