#include "types.h"
#include "pen.h"
#include "threads.h"
#include "console.h"
#include "hash.h"
#include "renderer.h"
#include "timer.h"
#include "input.h"

#include <emscripten.h>
#include <GLES3/gl32.h>
#include <SDL/SDL.h>

#include <semaphore.h>
#include <sys/types.h>
#include <unistd.h>

#include <stdio.h>
#include <map>

#include <emscripten.h>
EM_JS(int, get_canvas_width, (), {
  return canvas.clientWidth;
});

EM_JS(int, get_canvas_height, (), {
  return canvas.clientHeight;
});

using namespace pen;

void pen_make_gl_context_current()
{
}

void pen_gl_swap_buffers()
{
    SDL_GL_SwapBuffers();
}

namespace
{
    struct os_context
    {
        SDL_Surface* 		surface;
        pen_creation_params	pcp;
    };
    os_context s_ctx;

    std::map<u32, virtual_key> k_key_map = {{SDLK_0, PK_0},
                                            {SDLK_1, PK_1},
                                            {SDLK_2, PK_2},
                                            {SDLK_3, PK_3},
                                            {SDLK_4, PK_4},
                                            {SDLK_5, PK_5},
                                            {SDLK_6, PK_6},
                                            {SDLK_7, PK_7},
                                            {SDLK_8, PK_8},
                                            {SDLK_9, PK_9},
                                            {SDLK_a, PK_A},
                                            {SDLK_b, PK_B},
                                            {SDLK_c, PK_C},
                                            {SDLK_d, PK_D},
                                            {SDLK_e, PK_E},
                                            {SDLK_f, PK_F},
                                            {SDLK_g, PK_G},
                                            {SDLK_h, PK_H},
                                            {SDLK_i, PK_I},
                                            {SDLK_j, PK_J},
                                            {SDLK_k, PK_K},
                                            {SDLK_l, PK_L},
                                            {SDLK_m, PK_M},
                                            {SDLK_n, PK_N},
                                            {SDLK_o, PK_O},
                                            {SDLK_p, PK_P},
                                            {SDLK_q, PK_Q},
                                            {SDLK_r, PK_R},
                                            {SDLK_s, PK_S},
                                            {SDLK_t, PK_T},
                                            {SDLK_u, PK_U},
                                            {SDLK_v, PK_V},
                                            {SDLK_w, PK_W},
                                            {SDLK_x, PK_X},
                                            {SDLK_y, PK_Y},
                                            {SDLK_z, PK_Z},
                                            {SDLK_KP_0, PK_NUMPAD0},
                                            {SDLK_KP_1, PK_NUMPAD1},
                                            {SDLK_KP_2, PK_NUMPAD2},
                                            {SDLK_KP_3, PK_NUMPAD3},
                                            {SDLK_KP_4, PK_NUMPAD4},
                                            {SDLK_KP_5, PK_NUMPAD5},
                                            {SDLK_KP_6, PK_NUMPAD6},
                                            {SDLK_KP_7, PK_NUMPAD7},
                                            {SDLK_KP_8, PK_NUMPAD8},
                                            {SDLK_KP_9, PK_NUMPAD9},
                                            {SDLK_KP_MULTIPLY, PK_MULTIPLY},
                                            {SDLK_KP_PLUS, PK_ADD},
                                            {SDLK_KP_MINUS, PK_SUBTRACT},
                                            {SDLK_KP_DECIMAL, PK_DECIMAL},
                                            {SDLK_KP_DIVIDE, PK_DIVIDE},
                                            {SDLK_F1, PK_F1},
                                            {SDLK_F2, PK_F2},
                                            {SDLK_F3, PK_F3},
                                            {SDLK_F4, PK_F4},
                                            {SDLK_F5, PK_F5},
                                            {SDLK_F6, PK_F6},
                                            {SDLK_F7, PK_F7},
                                            {SDLK_F8, PK_F8},
                                            {SDLK_F9, PK_F9},
                                            {SDLK_F10, PK_F10},
                                            {SDLK_F11, PK_F11},
                                            {SDLK_F12, PK_F12},
                                            {SDLK_CANCEL, PK_CANCEL},
                                            {SDLK_BACKSPACE, PK_BACK},
                                            {SDLK_TAB, PK_TAB},
                                            {SDLK_CLEAR, PK_CLEAR},
                                            {SDLK_RETURN, PK_RETURN},
                                            {SDLK_LSHIFT, PK_SHIFT},
                                            {SDLK_RSHIFT, PK_SHIFT},
                                            {SDLK_LCTRL, PK_CONTROL},
                                            {SDLK_RCTRL, PK_CONTROL},
                                            {SDLK_LALT, PK_MENU},
                                            {SDLK_CAPSLOCK, PK_CAPITAL},
                                            {SDLK_ESCAPE, PK_ESCAPE},
                                            {SDLK_SPACE, PK_SPACE},
                                            {SDLK_PAGEDOWN, PK_PRIOR},
                                            {SDLK_PAGEUP, PK_NEXT},
                                            {SDLK_END, PK_END},
                                            {SDLK_HOME, PK_HOME},
                                            {SDLK_LEFT, PK_LEFT},
                                            {SDLK_UP, PK_UP},
                                            {SDLK_RIGHT, PK_RIGHT},
                                            {SDLK_DOWN, PK_DOWN},
                                            {SDLK_INSERT, PK_INSERT},
                                            {SDLK_DELETE, PK_DELETE},
                                            {SDLK_LGUI, PK_COMMAND},
                                            {SDLK_LEFTBRACKET, PK_OPEN_BRACKET},
                                            {SDLK_RIGHTBRACKET, PK_CLOSE_BRACKET},
                                            {SDLK_COLON, PK_SEMICOLON},
                                            {SDLK_QUOTE, PK_APOSTRAPHE},
                                            {SDLK_BACKSLASH, PK_BACK_SLASH},
                                            {SDLK_SLASH, PK_FORWARD_SLASH},
                                            {SDLK_COMMA, PK_COMMA},
                                            {SDLK_PERIOD, PK_PERIOD},
                                            {SDLK_MINUS, PK_MINUS},
                                            {SDLK_EQUALS, PK_EQUAL},
                                            {0x00, PK_TILDE},
                                            {SDLK_BACKQUOTE, PK_GRAVE}};
    
    void handle_key_event(bool down, u32 key)
    {
        if (k_key_map.find(key) != k_key_map.end())
        {
            u32 pk = k_key_map[key];   
            if (down)
            {
                pen::input_set_key_down(pk);
            }
            else
            {
                pen::input_set_key_up(pk);
            }
        }
    }

    void handle_mouse()
    {
        s32 x, y;
        u32 b = SDL_GetMouseState(&x, &y);
        input_set_mouse_pos((f32)x, (f32)y);
        if(b == SDL_BUTTON_LEFT)
        {
            input_set_mouse_down(PEN_MOUSE_L);
        }
        else
        {
            input_set_mouse_up(PEN_MOUSE_L);
        }

        if(b == SDL_BUTTON_RIGHT)
        {
            input_set_mouse_down(PEN_MOUSE_R);
        }
        else
        {
            input_set_mouse_up(PEN_MOUSE_R);
        }

        if(b == SDL_BUTTON_MIDDLE)
        {
            input_set_mouse_down(PEN_MOUSE_M);
        }
        else
        {
            input_set_mouse_up(PEN_MOUSE_M);
        }
    }

    void handle_window_resize()
    {
        if(s_ctx.pcp.window_width != get_canvas_width() || s_ctx.pcp.window_height != get_canvas_height())
        {
            s_ctx.pcp.window_width = get_canvas_width();
            s_ctx.pcp.window_height = get_canvas_height();

            if(s_ctx.surface)
                SDL_FreeSurface(s_ctx.surface);

            s_ctx.surface = SDL_SetVideoMode(s_ctx.pcp.window_width, s_ctx.pcp.window_height, 32, SDL_OPENGL | SDL_RESIZABLE);
        }
    }
    
    void run()
    {     
        SDL_PumpEvents();

        SDL_Event event;
        while( SDL_PollEvent( &event ) )
        {
            switch( event.type ){
            case SDL_KEYDOWN:
                handle_key_event(true, (u32)event.key.keysym.sym);
                break;
            case SDL_KEYUP:
                handle_key_event(false, (u32)event.key.keysym.sym);
                break;
            case SDL_MOUSEWHEEL:
                input_set_mouse_wheel(event.wheel.y);
                break;
            default:
                break;
            }
        }

        handle_mouse();
        handle_window_resize();

        pen::renderer_dispatch();
    }

    void create_sdl_surface()
    {
        SDL_Init(SDL_INIT_VIDEO);

        if(s_ctx.pcp.window_sample_count > 1)
        {
            SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
            SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, s_ctx.pcp.window_sample_count);
        }

        handle_window_resize();
    }

    void init()
    {
        create_sdl_surface();
        SDL_EnableUNICODE(1);
        timer_system_intialise();
        pen::renderer_init(nullptr, false, 1024);

        // creates user thread
        jobs_create_job(s_ctx.pcp.user_thread_function, 1024 * 1024, s_ctx.pcp.user_data, pen::e_thread_start_flags::detached);
    }
}

//
// os public api
//

namespace pen
{
    bool os_update()
    {
        return true;
    }

    void os_terminate(u32 return_code)
    {
        
    }

    const c8* os_path_for_resource(const c8* filename)
    {
        return filename;
    }

    const c8* window_get_title()
    {
        return s_ctx.pcp.window_title;
    }

    void window_get_size(s32& width, s32& height)
    {
        width = s_ctx.pcp.window_width;
        height = s_ctx.pcp.window_height;
    }

    f32 window_get_aspect()
    {
        return (f32)s_ctx.pcp.window_width / (f32)s_ctx.pcp.window_height;
    }

    hash_id window_get_id()
    {
        static hash_id window_id = PEN_HASH(s_ctx.pcp.window_title);
        return window_id;
    }

    void window_get_frame(window_frame& f)
    {
        f = {0, 0, s_ctx.pcp.window_width, s_ctx.pcp.window_height};
    }

    void window_set_frame(const window_frame& f)
    {


    }

    void* window_get_primary_display_handle()
    {
        return (void*)s_ctx.surface;
    }

    bool input_undo_pressed()
    {
        return false;
    }

    bool input_redo_pressed()
    {
        return false;
    }

    const user_info& os_get_user_info()
    {
        static user_info u;
        return u;
    }
}

//
// entry
//

int main() 
{
  	s_ctx.pcp = pen_entry(0, nullptr);
    init();
    emscripten_set_main_loop(run, 0, 1);
    return 0;
}
