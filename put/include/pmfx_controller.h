#ifndef _pmfx_controller_h
#define _pmfx_controller_h

#include "pen.h"
#include "renderer.h"
#include "ces/ces_scene.h"
#include "camera.h"

namespace put
{
    struct render_target
    {
        hash_id id_name;
        
        s32     width = 0;
        s32     height = 0;
        f32     ratio = 0;
        s32     num_mips;
        u32     format;
        u32     handle;
        u32     samples = 1;
        
#ifndef RC_FINAL
        Str     name;
#endif
    };
    
    namespace pmfx
    {
        void init( const c8* filename );
        
        void update( );
        void render( );
        
        void register_scene( const scene_controller& scene );
        void register_camera( const camera_controller& cam );
        void register_scene_view_renderer( const scene_view_renderer& svr );
        
        const render_target*  get_render_target( hash_id h );
        void                  get_render_target_dimensions( const render_target* rt, f32& w, f32& h);
        u32                   get_render_state_by_name( hash_id id_name );
        
        void show_dev_ui();
    }
}

#endif

