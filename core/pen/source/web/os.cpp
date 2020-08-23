#include "types.h"
#include "pen.h"
#include "threads.h"
#include "console.h"
#include "hash.h"
#include "renderer.h"

#include <emscripten.h>
#include <GLES3/gl32.h>
#include <SDL/SDL.h>

#include <semaphore.h>
#include <sys/types.h>
#include <unistd.h>

#include <stdio.h>

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
    
    void create_sdl_surface()
    {
        SDL_Init(SDL_INIT_VIDEO);
        s_ctx.surface = SDL_SetVideoMode(s_ctx.pcp.window_width, s_ctx.pcp.window_height, 32, SDL_OPENGL);
    }

    void init()
    {
        create_sdl_surface();
        pen::renderer_init(nullptr, false, 1024);

        // creates user thread
        jobs_create_job(s_ctx.pcp.user_thread_function, 1024 * 1024, s_ctx.pcp.user_data, pen::e_thread_start_flags::detached);
    }
    
    void run()
    {        
        pen::renderer_dispatch();
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
