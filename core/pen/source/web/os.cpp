#include "types.h"
#include "pen.h"
#include "threads.h"
#include "console.h"

#include <emscripten.h>
#include <GLES3/gl32.h>
#include <SDL/SDL.h>

#include <semaphore.h>
#include <sys/types.h>
#include <unistd.h>

#include <stdio.h>

using namespace pen;

namespace pen
{
	extern void* user_entry(void* params);
 
    hash_id window_get_id()
    {
        return 0;
    }
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

        // user thread
        PEN_LOG("Start User Thread");
        pen::default_thread_info thread_info;
        pen::jobs_create_default(thread_info);
        PEN_LOG("Finished User Thread");
    }
    
    void run()
    {        
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        
        SDL_GL_SwapBuffers();
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
