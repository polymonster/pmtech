#include "renderer_shared.h"
#include "data_struct.h"
#include "pen.h"

extern pen::window_creation_params pen_window;

namespace
{
    namespace e_shared_flags
    {
        enum shared_flags_t
        {
            backbuffer_resize = 1
        };
    }
    
    struct managed_rt
    {
        pen::texture_creation_params* tcp;
        u32 rt;
    };

    struct renderer_shared
    {
        managed_rt* managed_rts = nullptr;
        u32         flags = 0;
        a_u64       frame_index = { 0 };
        a_u64       resize_index = { 0 };
    };
    renderer_shared s_shared_ctx;
}

namespace pen
{
    void _renderer_new_frame()
    {
        _renderer_resize_managed_targets();
    }
    
    void _renderer_end_frame()
    {
        s_shared_ctx.frame_index++;
    }
    
    texture_creation_params _renderer_tcp_resolve_ratio(const texture_creation_params& tcp)
    {
        texture_creation_params _tcp = tcp;

        if (_tcp.width == -1)
        {
            _tcp.width = pen_window.width / _tcp.height;
            _tcp.height = pen_window.height / _tcp.height;
        }

        return _tcp;
    }
    
    void _renderer_resize_backbuffer(u32 width, u32 height)
    {
        // no need to do anything if the size is the same
        if (pen_window.width == width && pen_window.height == height)
            return;

        // want to remove this global extern in favour of function calls.
        pen_window.width = width;
        pen_window.height = height;
        
        // trigger render target resize
        s_shared_ctx.flags |= e_shared_flags::backbuffer_resize;
        s_shared_ctx.resize_index++;
    }

    void _renderer_track_managed_render_target(const texture_creation_params& tcp, u32 texture_handle)
    {
        // PEN_INVALID_HANDLE denotes a backbuffer ratio target
        if(tcp.width == PEN_INVALID_HANDLE)
        {
            managed_rt rt = {
                new texture_creation_params(tcp),
                texture_handle
            };
            sb_push(s_shared_ctx.managed_rts, rt);
        }
    }
        
    void _renderer_resize_managed_targets()
    {
        if (!(s_shared_ctx.flags & e_shared_flags::backbuffer_resize))
            return;

        u32 couunt = sb_count(s_shared_ctx.managed_rts);
        for (u32 i = 0; i < couunt; ++i)
        {
            auto& manrt = s_shared_ctx.managed_rts[i];
            direct::renderer_release_render_target(manrt.rt);
            direct::renderer_create_render_target(*manrt.tcp, manrt.rt, false);
        }
        
        s_shared_ctx.flags &= ~e_shared_flags::backbuffer_resize;
    }
    
    u64 _renderer_frame_index()
    {
        return s_shared_ctx.frame_index.load();
    }
    
    u64 _renderer_resize_index()
    {
        return s_shared_ctx.resize_index.load();
    }
}
