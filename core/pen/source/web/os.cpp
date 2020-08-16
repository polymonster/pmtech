#include "types.h"
#include "pen.h"
#include "threads.h"

#include <SDL/SDL.h>
#include <emscripten.h>
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
        s_ctx.surface = SDL_SetVideoMode(s_ctx.pcp.window_width, s_ctx.pcp.window_height, 32, SDL_SWSURFACE);
        
        pen::default_thread_info thread_info;
        pen::jobs_create_default(thread_info);
    }
    
    void run()
    {
        printf("hello world\n");
    }
}

//
// entry
//

int main() 
{
  	s_ctx.pcp = pen_entry(0, nullptr);
    create_sdl_surface();
    emscripten_set_main_loop(run, 60, 0);
    return 0;
}
