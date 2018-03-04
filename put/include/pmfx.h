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
            hash_id id_name;
            hash_id id_sub_type;
            
            u32 stream_out_shader;
            u32 vertex_shader;
            u32 pixel_shader;
            u32 input_layout;
            u32 program_index;
        };
    
        pmfx_handle     load( const c8* pmfx_name );
        void            release( pmfx_handle handle );
                
        void            set_technique( pmfx_handle handle, u32 index );
        bool            set_technique( pmfx_handle handle, hash_id id_technique, hash_id id_sub_type );
        
        void            poll_for_changes();
    }
}

#endif
