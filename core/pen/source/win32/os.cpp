// os.cpp
// Copyright 2014 - 2019 Alex Dixon.
// License: https://github.com/polymonster/pmtech/blob/master/license.md

#include <windows.h>

#include "os.h"

#include "data_struct.h"
#include "hash.h"
#include "input.h"
#include "pen.h"
#include "pen_string.h"
#include "renderer.h"
#include "renderer_shared.h"
#include "str_utilities.h"
#include "threads.h"
#include "timer.h"

// the last global extern!
pen::window_creation_params pen_window;

//
// OpenGL Render Context
//

#ifdef PEN_RENDERER_OPENGL
#define GLEW_STATIC
#include "GL/glew.c"
namespace
{
    struct wgl_context
    {
        HDC   dc;
        HGLRC glrc;
    };
    wgl_context s_glctx;

    void create_gl_context(HWND wnd)
    {
        s_glctx.dc = GetDC(wnd);

        PIXELFORMATDESCRIPTOR pfd;
        memset(&pfd, 0, sizeof(pfd));

        pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
        pfd.nVersion = 1;
        pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        pfd.iPixelType = PFD_TYPE_RGBA;
        pfd.cColorBits = 32;
        pfd.cAlphaBits = 8;
        pfd.cDepthBits = 24;
        pfd.cStencilBits = 8;
        pfd.iLayerType = PFD_MAIN_PLANE;

        int pf = ChoosePixelFormat(s_glctx.dc, &pfd);
        DescribePixelFormat(s_glctx.dc, pf, sizeof(PIXELFORMATDESCRIPTOR), &pfd);
        int   result = SetPixelFormat(s_glctx.dc, pf, &pfd);
        HGLRC temp = wglCreateContext(s_glctx.dc);
        wglMakeCurrent(s_glctx.dc, temp);

        int attribs[] = {WGL_CONTEXT_MAJOR_VERSION_ARB, 4, WGL_CONTEXT_MINOR_VERSION_ARB, 3, WGL_CONTEXT_FLAGS_ARB, 0, 0};

        // create gl 4 context
        int error = glewInit();
        if (wglewIsSupported("WGL_ARB_create_context") == 1)
        {
            s_glctx.glrc = wglCreateContextAttribsARB(s_glctx.dc, 0, attribs);
            wglMakeCurrent(NULL, NULL);
            wglDeleteContext(temp);
            result = wglMakeCurrent(s_glctx.dc, s_glctx.glrc);

            // todo.
            //glEnable(GL_DEBUG_OUTPUT);
            //glDebugMessageCallback(debugCallback, nullptr);
        }
        else
        {
            s_glctx.glrc = temp;
        }
    }
} // namespace

extern void pen_make_gl_context_current()
{
    wglMakeCurrent(s_glctx.dc, s_glctx.glrc);
}

extern void pen_gl_swap_buffers()
{
    SwapBuffers(s_glctx.dc);
}
#define create_ctx(wnd) create_gl_context(wnd)
#else
#define create_ctx(wnd) (void)wnd
#endif

namespace
{
    namespace e_os_cmd
    {
        enum os_cmd_t
        {
            null = 0,
            set_window_frame
        };
    }

    struct os_cmd
    {
        u32 cmd_index;

        union {
            struct
            {
                pen::window_frame frame;
            };
        };
    };

    struct window_params
    {
        HINSTANCE hinstance;
        int       cmdshow;
    };

    struct os_context
    {
        pen::user_info           user_info = {};
        HWND                     hwnd = nullptr;
        HINSTANCE                hinstance = nullptr;
        bool                     terminate_app = false;
        u32                      return_code = 0;
        pen::pen_creation_params creation_params = {};
        pen::ring_buffer<os_cmd> cmd_buffer;
    };
    os_context s_ctx;

    void pen_window_resize(u32 w, u32 h)
    {
        pen::_renderer_resize_backbuffer(w, h);
    }

    u32 pen_run_windowed(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, INT nCmdShow)
    {
        Str str_cmd = lpCmdLine;
        if (pen::str_find(str_cmd, "-test") != -1)
        {
            pen::renderer_test_enable();
        }

        window_params wp;
        wp.cmdshow = nCmdShow;
        wp.hinstance = hInstance;

        if (pen::window_init(((void*)&wp)))
            return 0;

        // renderer_init will enter a loop wait for rendering commands, and call os update
        HWND hwnd = (HWND)pen::window_get_primary_display_handle();
        create_ctx(hwnd);
        pen::renderer_init((void*)&hwnd, true, s_ctx.creation_params.max_renderer_commands);

        return s_ctx.return_code;
    }

    u32 pen_run_console()
    {
        for (;;)
        {
            pen::os_update();
        }

        return s_ctx.return_code;
    }
} // namespace

namespace pen
{
    LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

    bool os_update()
    {
        static bool init_jobs = true;
        if (init_jobs)
        {
            auto& pcp = s_ctx.creation_params;
            jobs_create_job(pcp.user_thread_function, 1024 * 1024, pcp.user_data, pen::e_thread_start_flags::detached);
            init_jobs = false;
        }

        for (;;)
        {
            MSG msg = {0};
            if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);

                if (WM_QUIT == msg.message)
                    s_ctx.terminate_app = true;
            }
            else
            {
                break;
            }
        }

        os_cmd* cmd = s_ctx.cmd_buffer.get();
        while (cmd)
        {
            // process cmd
            switch (cmd->cmd_index)
            {
                case e_os_cmd::set_window_frame:
                {
                    RECT r;
                    SetWindowPos(s_ctx.hwnd, HWND_TOP, cmd->frame.x, cmd->frame.y, cmd->frame.width, cmd->frame.height, 0);
                    GetClientRect(s_ctx.hwnd, &r);
                    pen_window_resize(r.right - r.left, r.bottom - r.top);
                }
                break;
                default:
                    break;
            }

            // get next
            cmd = s_ctx.cmd_buffer.get();
        }

        pen::input_gamepad_update();

        if (s_ctx.terminate_app)
        {
            // waits for other threads to terminate gracefully
            if (pen::jobs_terminate_all())
                return false;
        }

        // continue updating
        return true;
    }

    void os_terminate(u32 return_code)
    {
        s_ctx.return_code = return_code;
        s_ctx.terminate_app = true;
    }

    u32 window_init(void* params)
    {
        s_ctx.cmd_buffer.create(32);

        window_params* wp = (window_params*)params;

        // Register class
        WNDCLASSEXA wcex;
        ZeroMemory(&wcex, sizeof(WNDCLASSEXA));
        wcex.cbSize = sizeof(WNDCLASSEXA);
        wcex.style = CS_HREDRAW | CS_VREDRAW;
        wcex.lpfnWndProc = WndProc;
        wcex.cbClsExtra = 0;
        wcex.cbWndExtra = 0;
        wcex.hInstance = wp->hinstance;
        wcex.hIcon = NULL;
        wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wcex.lpszMenuName = nullptr;
        wcex.lpszClassName = pen_window.window_title;
        wcex.hIconSm = NULL;

        if (!RegisterClassExA(&wcex))
            return E_FAIL;

        // Create window
        s_ctx.hinstance = wp->hinstance;

        // pass in as params
        RECT rc = {0, 0, (LONG)pen_window.width, (LONG)pen_window.height};
        AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

        RECT desktop_rect;
        GetClientRect(GetDesktopWindow(), &desktop_rect);

        LONG screen_mid_x = (desktop_rect.right - desktop_rect.left) / 2;
        LONG screen_mid_y = (desktop_rect.bottom - desktop_rect.top) / 2;

        LONG half_window_x = (rc.right - rc.left) / 2;
        LONG half_window_y = (rc.bottom - rc.top) / 2;

        s_ctx.hwnd = CreateWindowA(pen_window.window_title, pen_window.window_title, WS_OVERLAPPEDWINDOW,
                                   screen_mid_x - half_window_x, screen_mid_y - half_window_y, rc.right - rc.left,
                                   rc.bottom - rc.top, nullptr, nullptr, wp->hinstance, nullptr);

        DWORD lasterror = GetLastError();

        if (!s_ctx.hwnd)
            return E_FAIL;

        ShowWindow(s_ctx.hwnd, wp->cmdshow);

        SetForegroundWindow(s_ctx.hwnd);

        return S_OK;
    }

    void set_unicode_key(u32 key_index, bool down)
    {
        static wchar_t buf[32];
        static c8      utf8[32];

        // modifiers
        BYTE key_state[256] = {0};
        GetKeyboardState(key_state);

        s32 result =
            ToUnicodeEx(key_index, MapVirtualKey(key_index, MAPVK_VK_TO_VSC), key_state, buf, _countof(buf), 0, nullptr);

        s32 num = WideCharToMultiByte(CP_UTF8, 0, buf, -1, utf8, 32, nullptr, nullptr);

        if (down)
            pen::input_add_unicode_input(utf8);

        // not ideal.. need a better solution to track unicode text input.
        u32 unicode = buf[0];
        if (unicode > 511 || unicode == 0)
            return;

        if (down)
        {
            pen::input_set_unicode_key_down(unicode);
        }
        else
        {
            pen::input_set_unicode_key_up(unicode);
        }
    }

    LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        PAINTSTRUCT ps;
        HDC         hdc;

        switch (message)
        {
            case WM_PAINT:
                hdc = BeginPaint(hWnd, &ps);
                EndPaint(hWnd, &ps);
                break;

            case WM_DESTROY:
                PostQuitMessage(0);
                break;

            case WM_KEYDOWN:
            case WM_SYSKEYDOWN:
                set_unicode_key((u32)wParam, true);
                pen::input_set_key_down((u32)wParam);
                break;

            case WM_KEYUP:
            case WM_SYSKEYUP:
                set_unicode_key((u32)wParam, false);
                pen::input_set_key_up((u32)wParam);
                break;

            case WM_LBUTTONDOWN:
            case WM_MBUTTONDOWN:
            case WM_RBUTTONDOWN:
                pen::input_set_mouse_down((message - 0x201) / 3);
                break;

            case WM_LBUTTONUP:
            case WM_MBUTTONUP:
            case WM_RBUTTONUP:
                pen::input_set_mouse_up((message - 0x202) / 3);
                break;

            case WM_MOUSEMOVE:
                pen::input_set_mouse_pos((f32)LOWORD(lParam), (f32)HIWORD(lParam));
                break;

            case WM_MOUSEWHEEL:
            {
                s16 low_w = (s16)LOWORD(wParam);
                s16 hi_w = (s16)HIWORD(wParam);

                s16 low_l = (s16)LOWORD(lParam);
                s16 hi_l = (s16)HIWORD(lParam);

                pen::input_set_mouse_wheel((f32)(hi_w / WHEEL_DELTA) * 10.0f);
            }
            break;

            case WM_SIZE:
            {
                s16 lo = LOWORD(wParam);
                if (lo == SIZE_MINIMIZED)
                    break;

                pen_window_resize(LOWORD(lParam), HIWORD(lParam));
            }
            break;

            default:
                return DefWindowProcA(hWnd, message, wParam, lParam);
        }

        return 0;
    }

    void* window_get_primary_display_handle()
    {
        return s_ctx.hwnd;
    }

    void window_get_frame(window_frame& f)
    {
        RECT r;
        GetWindowRect(s_ctx.hwnd, &r);

        f.x = r.left;
        f.y = r.top;

        f.width = r.right - r.left;
        f.height = r.bottom - r.top;
    }

    void window_set_frame(const window_frame& f)
    {
        os_cmd cmd;
        cmd.cmd_index = e_os_cmd::set_window_frame;
        cmd.frame = f;
        s_ctx.cmd_buffer.put(cmd);
    }

    void os_set_cursor_pos(u32 client_x, u32 client_y)
    {
        HWND  hw = (HWND)pen::window_get_primary_display_handle();
        POINT p = {(LONG)client_x, (LONG)client_y};

        ClientToScreen(hw, &p);
        SetCursorPos(p.x, p.y);
    }

    void os_show_cursor(bool show)
    {
        ShowCursor(show);
    }

    bool input_undo_pressed()
    {
        return pen::input_key(PK_CONTROL) && pen::input_key(PK_Z);
    }

    bool input_redo_pressed()
    {
        return pen::input_key(PK_CONTROL) && pen::input_key(PK_Y);
    }

    const Str os_path_for_resource(const c8* filename)
    {
        return filename;
    }

    const user_info& os_get_user_info()
    {
        return s_ctx.user_info;
    }

    void window_get_size(s32& width, s32& height)
    {
        width = pen_window.width;
        height = pen_window.height;
    }

    f32 window_get_aspect()
    {
        return (f32)pen_window.width / (f32)pen_window.height;
    }

    const c8* window_get_title()
    {
        return pen_window.window_title;
    }

    hash_id window_get_id()
    {
        static hash_id window_id = PEN_HASH(pen_window.window_title);
        return window_id;
    }
} // namespace pen

INT WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, INT nCmdShow)
{
    // initilaise any generic systems
    pen::timer_system_intialise();
    pen::input_gamepad_init();

    // get working directory name
    char module_filename[MAX_PATH];
    GetModuleFileNameA(hInstance, module_filename, MAX_PATH);

    static Str working_directory = module_filename;
    working_directory = pen::str_normalise_filepath(working_directory);

    // remove exe
    u32 dir = pen::str_find_reverse(working_directory, "/");
    working_directory = pen::str_substr(working_directory, 0, dir + 1);

    s_ctx.user_info.working_directory = working_directory.c_str();

    // call user entry / setup
    pen::pen_creation_params pc = pen::pen_entry(0, nullptr);
    pen_window.width = pc.window_width;
    pen_window.height = pc.window_height;
    pen_window.window_title = pc.window_title;
    pen_window.sample_count = pc.window_sample_count;
    s_ctx.creation_params = pc;

    if (pc.flags & pen::e_pen_create_flags::renderer)
    {
        // console for std output..
        if (!AttachConsole(ATTACH_PARENT_PROCESS))
        {
            AllocConsole();
        }

        freopen("CONIN$", "r", stdin);
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);

        // windowed mode with renderer
        pen_run_windowed(hInstance, hPrevInstance, lpCmdLine, nCmdShow);
    }
    else
    {
        // headless mode
        pen_run_console();
    }

    // exit program
    return s_ctx.return_code;
}