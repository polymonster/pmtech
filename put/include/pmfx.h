#ifndef _pmfx_h
#define _pmfx_h

#include "definitions.h"
#include "renderer.h"
#include "put_math.h"

namespace put
{
    namespace pmfx
    {
        typedef u32 shader_program_handle;
        typedef u32 pmfx_handle;

        struct shader_program
        {
            u32 vertex_shader;
            u32 pixel_shader;
            u32 input_layout;
            u32 program_index;
        };
    
        pmfx_handle     load( const c8* filename );
        void            release( pmfx_handle handle );
        
        void            set_technique( pmfx_handle handle, u32 technique_index );
        
        void            poll_for_changes();
    }
}

#endif
