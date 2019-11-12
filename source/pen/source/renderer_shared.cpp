#include "renderer_shared.h"
#include "data_struct.h"
#include "pen.h"

extern pen::window_creation_params pen_window;

namespace
{
    struct managed_rt
    {
        pen::texture_creation_params* tcp;
        u32 rt;
    };

    struct renderer_shared
    {
        managed_rt* managed_rts = nullptr;
    };
    renderer_shared s_shared_ctx;
}

namespace pen
{
    texture_creation_params renderer_tcp_resolve_ratio(const texture_creation_params& tcp)
    {
        texture_creation_params _tcp = tcp;

        if (_tcp.width == -1)
        {
            _tcp.width = pen_window.width / _tcp.height;
            _tcp.height = pen_window.height / _tcp.height;
        }

        return _tcp;
    }

    void renderer_track_managed_render_target(const texture_creation_params& tcp, u32 texture_handle)
    {
        //sb_push(s_shared_ctx.managed_rts, { new texture_creation_params(tcp), texture_handle });
    }
}