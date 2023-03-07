// renderer.cpp
// Copyright 2014 - 2023 Alex Dixon.
// License: https://github.com/polymonster/pmtech/blob/master/license.md

#define GL_SILENCE_DEPRECATION
#define GLES_SILENCE_DEPRECATION

#include "console.h"
#include "data_struct.h"
#include "hash.h"
#include "memory.h"
#include "os.h"
#include "pen.h"
#include "pen_string.h"
#include "renderer.h"
#include "renderer_shared.h"
#include "str_utilities.h"
#include "threads.h"
#include "timer.h"

#include <stdlib.h>
#include <vector>

#ifdef __linux__
#include "GL/glew.h"
#elif _WIN32
#define GLEW_STATIC
#include "GL/glew.h"
#include "GL/wglew.h"
#elif PEN_PLATFORM_WEB
#define PEN_GLES3
#define PEN_WEBGL
#include <GLES3/gl32.h>
#include <SDL/SDL.h>
#else // osx
#include <OpenGL/gl3.h>
#include <OpenGL/gl3ext.h>
#endif

#ifdef PEN_GLES3
// mark unsupported features null
#define GL_FILL 0x00 // gl fill is the only polygon mode on gles3
#define GL_LINE 0x00 // gl line (wireframe) usupported
#define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT 0x00
#define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT 0x00
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT 0x00
#define GL_SRC1_COLOR GL_SRC_COLOR
#define GL_ONE_MINUS_SRC1_COLOR GL_ONE_MINUS_SRC_COLOR
#define GL_SRC1_ALPHA GL_SRC_ALPHA
#define GL_ONE_MINUS_SRC1_ALPHA GL_ONE_MINUS_SRC_ALPHA
#define GL_TEXTURE_CUBE_MAP_SEAMLESS GL_CLAMP_TO_EDGE
#define glClearDepth glClearDepthf // gl es has these type suffixes
// gles does not support base vertex offset assert when b is > 0.. rethink how you are rendering stuff
#define glDrawElementsBaseVertex(p, i, f, o, b) glDrawElements(p, i, f, o)
#define glDrawElementsInstancedBaseVertex(p, i, f, o, c, b) glDrawElementsInstanced(p, i, f, o, c)
#define glDrawBuffer
#define PEN_GL_MSAA_SUPPORT false
#define glTexImage2DMultisample(a1, a2, a3, a4, a5, a6) PEN_ASSERT(0)
#define GL_BGRA GL_RGBA
// remap unsupported stuff for rough equivalent were required for gles on ios, but not needed on web
//#define GL_TEXTURE_2D_MULTISAMPLE GL_TEXTURE_2D
// #define GL_GEOMETRY_SHADER 0x00
// #define GL_TEXTURE_COMPRESSED 0x00
// #define GL_CLAMP_TO_BORDER GL_CLAMP_TO_EDGE
#else
#define PEN_GL_MSAA_SUPPORT true
#endif

#define MAX_VERTEX_BUFFERS 4
#define MAX_VERTEX_ATTRIBUTES 16
#define MAX_UNIFORM_BUFFERS 32
#define MAX_SHADER_TEXTURES 32
#define INVALID_LOC 255

// these are required for platform specific gl implementation calls.
extern void pen_make_gl_context_current();
extern void pen_gl_swap_buffers();

using namespace pen;

namespace
{
    u64   s_frame = 0;
    GLint s_backbuffer_fbo = -1;

#if _DEBUG
#define GL_DEBUG_LEVEL 2
#else
#define GL_DEBUG_LEVEL 0
#endif

#if GL_DEBUG_LEVEL > 1
#define GL_ASSERT(V) PEN_ASSERT(V)
#else
#define GL_ASSERT(V) (void)V
#endif

    void gl_error_break(GLenum err)
    {
        switch (err)
        {
            case GL_INVALID_ENUM:
                PEN_LOG("invalid enum");
                GL_ASSERT(0);
                break;
            case GL_INVALID_VALUE:
                PEN_LOG("gl invalid value");
                GL_ASSERT(0);
                break;
            case GL_INVALID_OPERATION:
                PEN_LOG("gl invalid operation");
                GL_ASSERT(0);
                break;
            case GL_INVALID_FRAMEBUFFER_OPERATION:
                PEN_LOG("gl invalid frame buffer operation");
                GL_ASSERT(0);
                break;
            case GL_OUT_OF_MEMORY:
                PEN_LOG("gl out of memory");
                GL_ASSERT(0);
                break;
            default:
                break;
        }

        PEN_LOG("gl error");
    }

#if GL_DEBUG_LEVEL > 2
#define CHECK_GL_ERROR                                                                                                       \
    {                                                                                                                        \
        GLenum err = glGetError();                                                                                           \
        if (err != GL_NO_ERROR)                                                                                              \
            gl_error_break(err);                                                                                             \
    }
#define CHECK_CALL(C)                                                                                                        \
    C;                                                                                                                       \
    CHECK_GL_ERROR;                                                                                                          \
    printf(#C "\n");
#elif GL_DEBUG_LEVEL > 0
#define CHECK_GL_ERROR                                                                                                       \
    {                                                                                                                        \
        GLenum err = glGetError();                                                                                           \
        if (err != GL_NO_ERROR)                                                                                              \
            gl_error_break(err);                                                                                             \
    }
#define CHECK_CALL(C)                                                                                                        \
    C;                                                                                                                       \
    CHECK_GL_ERROR;
#else
#define CHECK_GL_ERROR
#define CHECK_CALL(C) C
#endif

#ifdef PEN_GLES3
#define PEN_SET_BASE_VERTEX(BV) s_state.base_vertex = BV
#else
#define PEN_SET_BASE_VERTEX(BV) s_state.base_vertex = 0;
#endif

    u32 to_gl_shader_type(u32 pen_shader_type)
    {
        switch (pen_shader_type)
        {
            case PEN_SHADER_TYPE_VS:
                return GL_VERTEX_SHADER;
            case PEN_SHADER_TYPE_PS:
                return GL_FRAGMENT_SHADER;
            case PEN_SHADER_TYPE_GS:
                return GL_GEOMETRY_SHADER;
            case PEN_SHADER_TYPE_SO:
                return GL_VERTEX_SHADER;
#if GL_ARB_compute_shader
            case PEN_SHADER_TYPE_CS:
                return GL_COMPUTE_SHADER;
#endif
        }
        // unimplemented cs on some platforms
        return -1;
    }

    u32 to_gl_polygon_mode(u32 pen_polygon_mode)
    {
        switch (pen_polygon_mode)
        {
            case PEN_FILL_SOLID:
                return GL_FILL;
            case PEN_FILL_WIREFRAME:
                return GL_LINE;
        }
        PEN_ASSERT(0);
        return GL_FILL;
    }

    u32 to_gl_cull_mode(u32 pen_cull_mode)
    {
        switch (pen_cull_mode)
        {
            case PEN_CULL_NONE:
                return 0;
            case PEN_CULL_FRONT:
                return GL_FRONT;
            case PEN_CULL_BACK:
                return GL_BACK;
        }
        PEN_ASSERT(0);
        return PEN_CULL_NONE;
    }

    u32 to_gl_clear_flags(u32 pen_clear_flags)
    {
        u32 res = 0;
        if (pen_clear_flags & PEN_CLEAR_COLOUR_BUFFER)
            res |= GL_COLOR_BUFFER_BIT;
        if (pen_clear_flags & PEN_CLEAR_DEPTH_BUFFER)
            res |= GL_DEPTH_BUFFER_BIT;
        if (pen_clear_flags & PEN_CLEAR_STENCIL_BUFFER)
            res |= GL_STENCIL_BUFFER_BIT;
        return res;
    }

    u32 to_gl_primitive_topology(u32 pen_primitive_topology)
    {
        switch (pen_primitive_topology)
        {
            case PEN_PT_POINTLIST:
                return GL_POINTS;
            case PEN_PT_LINELIST:
                return GL_LINES;
            case PEN_PT_LINESTRIP:
                return GL_LINE_STRIP;
            case PEN_PT_TRIANGLELIST:
                return GL_TRIANGLES;
            case PEN_PT_TRIANGLESTRIP:
                return GL_TRIANGLE_STRIP;
        }
        PEN_ASSERT(0);
        return GL_TRIANGLES;
    }

    u32 to_gl_index_format(u32 pen_index_format)
    {
        switch (pen_index_format)
        {
            case PEN_FORMAT_R16_UINT:
                return GL_UNSIGNED_SHORT;
            case PEN_FORMAT_R32_UINT:
                return GL_UNSIGNED_INT;
        }
        PEN_ASSERT(0);
        return GL_UNSIGNED_SHORT;
    }

    u32 to_gl_usage(u32 pen_usage)
    {
        switch (pen_usage)
        {
            case PEN_USAGE_DEFAULT:
                return GL_STATIC_DRAW;
            case PEN_USAGE_IMMUTABLE:
                return GL_STATIC_DRAW;
            case PEN_USAGE_DYNAMIC:
                return GL_DYNAMIC_DRAW;
            case PEN_USAGE_STAGING:
                return GL_DYNAMIC_DRAW;
        }
        PEN_ASSERT(0);
        return GL_STATIC_DRAW;
    }

    u32 to_gl_bind_flags(u32 pen_bind_flags)
    {
        u32 bf = 0;
        if (pen_bind_flags & PEN_BIND_SHADER_RESOURCE)
            bf |= 0;
        if (pen_bind_flags & PEN_BIND_VERTEX_BUFFER)
            bf |= GL_ARRAY_BUFFER;
        if (pen_bind_flags & PEN_BIND_INDEX_BUFFER)
            bf |= GL_ELEMENT_ARRAY_BUFFER;
        if (pen_bind_flags & PEN_BIND_CONSTANT_BUFFER)
            bf |= GL_UNIFORM_BUFFER;
        if (pen_bind_flags & PEN_BIND_RENDER_TARGET)
            bf |= GL_FRAMEBUFFER;
        if (pen_bind_flags & PEN_BIND_DEPTH_STENCIL)
            bf |= 1;
        if (pen_bind_flags & PEN_BIND_SHADER_WRITE)
            bf |= 2;
        if (pen_bind_flags & PEN_STREAM_OUT_VERTEX_BUFFER)
            bf |= GL_ARRAY_BUFFER;
        return bf;
    }

    u32 to_gl_cpu_access_flags(u32 pen_access_flags)
    {
        u32 af = 0;
        if (pen_access_flags & PEN_CPU_ACCESS_WRITE)
            af |= GL_MAP_WRITE_BIT;
        if (pen_access_flags & PEN_CPU_ACCESS_READ)
            af |= GL_MAP_READ_BIT;
        return af;
    }

    u32 to_gl_texture_address_mode(u32 pen_texture_address_mode)
    {
        switch (pen_texture_address_mode)
        {
            case PEN_TEXTURE_ADDRESS_WRAP:
                return GL_REPEAT;
            case PEN_TEXTURE_ADDRESS_MIRROR:
                return GL_MIRRORED_REPEAT;
            case PEN_TEXTURE_ADDRESS_CLAMP:
                return GL_CLAMP_TO_EDGE;
            case PEN_TEXTURE_ADDRESS_BORDER:
                return GL_CLAMP_TO_BORDER;
            case PEN_TEXTURE_ADDRESS_MIRROR_ONCE:
#if GL_EXT_texture_mirror_clamp
                return GL_MIRROR_CLAMP_EXT;
#else
                return GL_MIRRORED_REPEAT;
#endif
        }
        PEN_ASSERT(0);
        return GL_REPEAT;
    }

    void to_gl_filter_mode(u32 pen_filter_mode, u32* min_filter, u32* mag_filter)
    {
        switch (pen_filter_mode)
        {
            case PEN_FILTER_MIN_MAG_MIP_LINEAR:
            {
                *min_filter = GL_LINEAR_MIPMAP_LINEAR;
                *mag_filter = GL_LINEAR;
            }
                return;
            case PEN_FILTER_MIN_MAG_MIP_POINT:
            {
                *min_filter = GL_NEAREST_MIPMAP_NEAREST;
                *mag_filter = GL_NEAREST;
            }
                return;
            case PEN_FILTER_LINEAR:
            {
                *min_filter = GL_LINEAR;
                *mag_filter = GL_LINEAR;
            }
                return;
            case PEN_FILTER_POINT:
            {
                *min_filter = GL_NEAREST;
                *mag_filter = GL_NEAREST;
            }
                return;
        }
        PEN_ASSERT(0);

        *min_filter = GL_LINEAR;
        *mag_filter = GL_LINEAR;
    }

    u32 to_gl_comparison(u32 pen_comparison)
    {
        switch (pen_comparison)
        {
            case PEN_COMPARISON_NEVER:
                return GL_NEVER;
            case PEN_COMPARISON_LESS:
                return GL_LESS;
            case PEN_COMPARISON_EQUAL:
                return GL_EQUAL;
            case PEN_COMPARISON_LESS_EQUAL:
                return GL_LEQUAL;
            case PEN_COMPARISON_GREATER:
                return GL_GREATER;
            case PEN_COMPARISON_NOT_EQUAL:
                return GL_NOTEQUAL;
            case PEN_COMPARISON_GREATER_EQUAL:
                return GL_GEQUAL;
            case PEN_COMPARISON_ALWAYS:
                return GL_ALWAYS;
        }
        PEN_ASSERT(0);
        return GL_ALWAYS;
    }

    u32 to_gl_stencil_op(u32 pen_stencil_op)
    {
        switch (pen_stencil_op)
        {
            case PEN_STENCIL_OP_KEEP:
                return GL_KEEP;
            case PEN_STENCIL_OP_REPLACE:
                return GL_REPLACE;
            case PEN_STENCIL_OP_ZERO:
                return GL_ZERO;
            case PEN_STENCIL_OP_DECR:
                return GL_DECR_WRAP;
            case PEN_STENCIL_OP_INCR:
                return GL_INCR_WRAP;
            case PEN_STENCIL_OP_DECR_SAT:
                return GL_DECR;
            case PEN_STENCIL_OP_INCR_SAT:
                return GL_INCR;
            case PEN_STENCIL_OP_INVERT:
                return GL_INVERT;
        }
        PEN_ASSERT(0);
        return GL_REPLACE;
    }

    u32 to_gl_blend_factor(u32 pen_blend_factor)
    {
        switch (pen_blend_factor)
        {
            case PEN_BLEND_ZERO:
                return GL_ZERO;
            case PEN_BLEND_ONE:
                return GL_ONE;
            case PEN_BLEND_SRC_COLOR:
                return GL_SRC_COLOR;
            case PEN_BLEND_INV_SRC_COLOR:
                return GL_ONE_MINUS_SRC_COLOR;
            case PEN_BLEND_SRC_ALPHA:
                return GL_SRC_ALPHA;
            case PEN_BLEND_INV_SRC_ALPHA:
                return GL_ONE_MINUS_SRC_ALPHA;
            case PEN_BLEND_DEST_ALPHA:
                return GL_DST_ALPHA;
            case PEN_BLEND_INV_DEST_ALPHA:
                return GL_ONE_MINUS_DST_ALPHA;
            case PEN_BLEND_INV_DEST_COLOR:
                return GL_DST_COLOR;
            case PEN_BLEND_SRC_ALPHA_SAT:
                return GL_SRC_ALPHA_SATURATE;
            case PEN_BLEND_SRC1_COLOR:
                return GL_SRC1_COLOR;
            case PEN_BLEND_INV_SRC1_COLOR:
                return GL_ONE_MINUS_SRC1_COLOR;
            case PEN_BLEND_SRC1_ALPHA:
                return GL_SRC1_ALPHA;
            case PEN_BLEND_INV_SRC1_ALPHA:
                return GL_ONE_MINUS_SRC1_ALPHA;
            case PEN_BLEND_BLEND_FACTOR:
            case PEN_BLEND_INV_BLEND_FACTOR:
                PEN_ASSERT(0);
                break;
        }
        PEN_ASSERT(0);
        return GL_ZERO;
    }

    u32 to_gl_blend_op(u32 pen_blend_op)
    {
        switch (pen_blend_op)
        {
            case PEN_BLEND_OP_ADD:
                return GL_FUNC_ADD;
            case PEN_BLEND_OP_SUBTRACT:
                return GL_FUNC_SUBTRACT;
            case PEN_BLEND_OP_REV_SUBTRACT:
                return GL_FUNC_REVERSE_SUBTRACT;
            case PEN_BLEND_OP_MIN:
                return GL_MIN;
            case PEN_BLEND_OP_MAX:
                return GL_MAX;
        }
        PEN_ASSERT(0);
        return GL_FUNC_ADD;
    }

    struct tex_format_map
    {
        u32 pen_format;
        u32 sized_format;
        u32 format;
        u32 type;
        u32 attachment;
    };

    const tex_format_map k_tex_format_map[] = {
        {PEN_TEX_FORMAT_BGRA8_UNORM, GL_RGBA8, GL_BGRA, GL_UNSIGNED_BYTE, GL_COLOR_ATTACHMENT0},
        {PEN_TEX_FORMAT_RGBA8_UNORM, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, GL_COLOR_ATTACHMENT0},
        {PEN_TEX_FORMAT_D24_UNORM_S8_UINT, GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8,
         GL_DEPTH_STENCIL_ATTACHMENT},
        {PEN_TEX_FORMAT_D32_FLOAT, GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT, GL_DEPTH_ATTACHMENT},
        {PEN_TEX_FORMAT_D32_FLOAT_S8_UINT, GL_DEPTH32F_STENCIL8, GL_DEPTH_STENCIL, GL_FLOAT, GL_DEPTH_ATTACHMENT},
        {PEN_TEX_FORMAT_R32G32B32A32_FLOAT, GL_RGBA32F, GL_RGBA, GL_FLOAT, GL_COLOR_ATTACHMENT0},
        {PEN_TEX_FORMAT_R16G16B16A16_FLOAT, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, GL_COLOR_ATTACHMENT0},
        {PEN_TEX_FORMAT_R32G32_FLOAT, GL_RG32F, GL_RG, GL_FLOAT, GL_COLOR_ATTACHMENT0},
        {PEN_TEX_FORMAT_R32_FLOAT, GL_R32F, GL_RED, GL_FLOAT, GL_COLOR_ATTACHMENT0},
        {PEN_TEX_FORMAT_R16_FLOAT, GL_R16F, GL_RED, GL_HALF_FLOAT, GL_COLOR_ATTACHMENT0},
        {PEN_TEX_FORMAT_R32_UINT, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, GL_COLOR_ATTACHMENT0},
        {PEN_TEX_FORMAT_R8_UNORM, GL_R8, GL_RED, GL_UNSIGNED_BYTE, GL_COLOR_ATTACHMENT0},
        {PEN_TEX_FORMAT_BC1_UNORM, 0, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, GL_TEXTURE_COMPRESSED, GL_NONE},
        {PEN_TEX_FORMAT_BC2_UNORM, 0, GL_COMPRESSED_RGBA_S3TC_DXT3_EXT, GL_TEXTURE_COMPRESSED, GL_NONE},
        {PEN_TEX_FORMAT_BC3_UNORM, 0, GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, GL_TEXTURE_COMPRESSED, GL_NONE}};
    const u32 k_num_tex_maps = sizeof(k_tex_format_map) / sizeof(k_tex_format_map[0]);

    void to_gl_texture_format(u32 pen_format, u32& sized_format, u32& format, u32& type, u32& attachment)
    {
        for (s32 i = 0; i < k_num_tex_maps; ++i)
        {
            if (k_tex_format_map[i].pen_format == pen_format)
            {
                sized_format = k_tex_format_map[i].sized_format;
                format = k_tex_format_map[i].format;
                type = k_tex_format_map[i].type;
                attachment = k_tex_format_map[i].attachment;
                return;
            }
        }
        // unsupported / unhandled texture format
        PEN_ASSERT(0);
    }

    struct vertex_format_map
    {
        u32 pen_format;
        u32 format;
        u32 num_elements;
    };

    const vertex_format_map k_vertex_format_map[] = {
        {PEN_VERTEX_FORMAT_FLOAT1, GL_FLOAT, 1},         {PEN_VERTEX_FORMAT_FLOAT2, GL_FLOAT, 2},
        {PEN_VERTEX_FORMAT_FLOAT3, GL_FLOAT, 3},         {PEN_VERTEX_FORMAT_FLOAT4, GL_FLOAT, 4},
        {PEN_VERTEX_FORMAT_UNORM1, GL_UNSIGNED_BYTE, 1}, {PEN_VERTEX_FORMAT_UNORM2, GL_UNSIGNED_BYTE, 2},
        {PEN_VERTEX_FORMAT_UNORM4, GL_UNSIGNED_BYTE, 4}};
    const u32 k_num_vertex_format_maps = sizeof(k_vertex_format_map) / sizeof(k_vertex_format_map[0]);

    vertex_format_map to_gl_vertex_format(u32 pen_format)
    {
        for (s32 i = 0; i < k_num_vertex_format_maps; ++i)
            if (k_vertex_format_map[i].pen_format == pen_format)
                return k_vertex_format_map[i];
        PEN_ASSERT(0);
        return {0};
    }

} // namespace

namespace pen
{
    a_u64 g_gpu_total;

    struct gpu_perf_result
    {
        u64 elapsed;
        u32 depth;
        u64 frame;
        Str name;
    };
    static gpu_perf_result* s_perf_results = nullptr;

    struct perf_marker
    {
        u32       query = 0;
        u64       frame = 0;
        const c8* name = nullptr;
        bool      issued = false;
        u32       depth = 0;
        bool      pad = false;
        GLuint64  result = 0;
    };

    struct perf_marker_set
    {
        static const u32 num_marker_buffers = 2;
        perf_marker*     markers[num_marker_buffers] = {0};
        u32              pos[num_marker_buffers] = {0};

        u32 buf = 0;
        u32 depth = 0;
    };
    static perf_marker_set s_perf;

    void insert_marker(const c8* name, bool pad = false)
    {
#ifdef GL_TIME_ELAPSED
        u32& buf = s_perf.buf;
        u32& pos = s_perf.pos[buf];
        u32& depth = s_perf.depth;

        if (pos >= sb_count(s_perf.markers[buf]))
        {
            // push a new marker
            perf_marker m;
            glGenQueries(1, &m.query);

            sb_push(s_perf.markers[buf], m);
        }

        // queries have taken longer than 1 frame to obtain results
        // increase num_marker_buffers to avoid losing data
        PEN_ASSERT(!s_perf.markers[buf][pos].issued);

        CHECK_CALL(glBeginQuery(GL_TIME_ELAPSED, s_perf.markers[buf][pos].query));
        s_perf.markers[buf][pos].issued = true;
        s_perf.markers[buf][pos].depth = depth;
        s_perf.markers[buf][pos].frame = s_frame;
        s_perf.markers[buf][pos].pad = pad;
        s_perf.markers[buf][pos].name = name;

        ++pos;
#endif
    }

    void direct::renderer_push_perf_marker(const c8* name)
    {
#ifdef GL_TIME_ELAPSED
        u32& depth = s_perf.depth;

        if (depth > 0)
        {
            CHECK_CALL(glEndQuery(GL_TIME_ELAPSED));
        }

        insert_marker(name);

        ++depth;
#endif
    }

    void direct::renderer_pop_perf_marker()
    {
#ifdef GL_TIME_ELAPSED
        u32& depth = s_perf.depth;
        --depth;

        CHECK_CALL(glEndQuery(GL_TIME_ELAPSED));

        if (depth > 0)
        {
            insert_marker("pad_marker", true);
        }
#endif
    }

    void gather_perf_markers()
    {
#ifdef GL_TIME_ELAPSED
        // unbalance push pop in perf markers
        PEN_ASSERT(s_perf.depth == 0);

        // swap buffers
        u32 bb = (s_perf.buf + 1) % s_perf.num_marker_buffers;

        u32 complete_count = 0;
        for (u32 i = 0; i < s_perf.pos[bb]; ++i)
        {
            perf_marker& m = s_perf.markers[bb][i];
            if (m.issued)
            {
                s32 avail = 0;
                CHECK_CALL(glGetQueryObjectiv(m.query, GL_QUERY_RESULT_AVAILABLE, &avail));

                if (avail)
                {
                    complete_count++;
                    CHECK_CALL(glGetQueryObjectui64v(m.query, GL_QUERY_RESULT, &m.result));
                }
            }
        }

        if (complete_count == s_perf.pos[bb])
        {
            // gather results into a better view
            sb_free(s_perf_results);

            u32 num_timers = s_perf.pos[bb];
            for (u32 i = 0; i < num_timers; ++i)
            {
                perf_marker& m = s_perf.markers[bb][i];

                if (!m.issued)
                    continue;

                // ready for the next frame
                m.issued = false;

                if (m.pad)
                    continue;

                gpu_perf_result p;
                p.name = m.name;
                p.depth = m.depth;
                p.frame = m.frame;

                if (m.name)
                {
                    // release mem that was allocated by the command buffer
                    memory_free((void*)m.name);
                    m.name = nullptr;
                }

                // gather up times from nested calls
                p.elapsed = 0;
                u32 nest_iter = 0;
                for (;;)
                {
                    perf_marker& n = s_perf.markers[bb][i + nest_iter];

                    if (n.depth < p.depth)
                        break;

                    if (n.pad && p.depth == n.depth)
                        break;

                    p.elapsed += n.result;

                    ++nest_iter;
                    if (nest_iter >= num_timers)
                        break;
                }

                Str desc;
                for (u32 j = 0; j < p.depth; ++j)
                    desc.append('\t');

                desc.append(p.name.c_str());

                desc.append(" : ");
                desc.appendf("%llu", p.elapsed);

                if (i == 0)
                    g_gpu_total = p.elapsed;
            }

            s_perf.pos[bb] = 0;
        }

        s_perf.buf = bb;
#endif
    }
} // namespace pen

namespace
{
    struct context_state
    {
        context_state()
        {
        }

        u32 backbuffer_colour;
        u32 backbuffer_depth;
        u32 active_colour_target;
        u32 active_depth_target;
    };

    struct clear_state_internal
    {
        f32 rgba[4];
        f32 depth;
        u8  stencil;
        u32 flags;

        mrt_clear mrt[MAX_MRT];
        u32       num_colour_targets;
    };

    struct vertex_attribute
    {
        u32    location;
        u32    type;
        u32    stride;
        size_t offset;
        u32    num_elements;
        u32    input_slot;
        u32    step_rate;
    };

    struct input_layout
    {
        vertex_attribute* attributes;
        GLuint            vertex_array_handle = 0;
        GLuint            vb_handle = 0;
    };

    struct raster_state
    {
        GLenum cull_face;
        GLenum polygon_mode;
        bool   gles_wireframe;
        bool   front_ccw;
        bool   culling_enabled;
        bool   depth_clip_enabled;
        bool   scissor_enabled;
    };

    struct texture_info
    {
        GLuint handle;
        u32    max_mip_level;
        u32    target;
        u32    attachment;
    };

    struct render_target
    {
        texture_info             texture;
        texture_info             texture_msaa;
        GLuint                   w, h;
        u32                      uid;
        u32                      collection_type;
        texture_creation_params* tcp;
        u32                      invalidate;
    };

    struct framebuffer
    {
        hash_id hash;
        GLuint  _framebuffer;
    };
    framebuffer* s_framebuffers = nullptr;

    enum resource_type : s32
    {
        RES_TEXTURE = 0,
        RES_TEXTURE_3D,
        RES_RENDER_TARGET,
        RES_RENDER_TARGET_MSAA
    };

    struct shader_program
    {
        u32    vs;
        u32    ps;
        u32    gs;
        u32    so;
        u32    cs;
        GLuint program;
        GLuint vflip_uniform;
        u8     uniform_block_location[MAX_UNIFORM_BUFFERS];
        u8     texture_location[MAX_SHADER_TEXTURES];
    };
    shader_program* s_shader_programs = nullptr;

    struct gl_sampler : public sampler_creation_params
    {
        u32 min_filter;
        u32 mag_filter;
    };

    struct gl_sampler_object
    {
        u32  sampler;
        u32  depth_sampler; // gles3 does not allow linear filtering on depth textures unlike other platforms
        bool compare;
    };

    struct resource_allocation
    {
        u8     asigned_flag;
        GLuint type;
        union {
            clear_state_internal           clear_state;
            ::input_layout*                input_layout;
            ::raster_state                 raster_state;
            depth_stencil_creation_params* depth_stencil;
            blend_creation_params*         blend_state;
            GLuint                         handle;
            texture_info                   texture;
            ::render_target                render_target;
            ::shader_program*              shader_program;
            gl_sampler_object              sampler_object;
        };
    };
    res_pool<resource_allocation> _res_pool;

    struct active_state
    {
        u32  vertex_buffer[MAX_VERTEX_BUFFERS] = {0};
        u32  vertex_buffer_stride[MAX_VERTEX_BUFFERS] = {0};
        u32  vertex_buffer_offset[MAX_VERTEX_BUFFERS] = {0};
        u32  num_bound_vertex_buffers = 0;
        u32  index_buffer = 0;
        u32  stream_out_buffer = 0;
        u32  input_layout = 0;
        u32  vertex_shader = 0;
        u32  pixel_shader = 0;
        u32  stream_out_shader = 0;
        u32  compute_shader = 0;
        u32  raster_state = 0;
        u32  base_vertex = 0;
        bool backbuffer_bound = false;
        bool v_flip = false;
        u8   constant_buffer_bindings[MAX_UNIFORM_BUFFERS] = {0};
        u32  index_format = GL_UNSIGNED_SHORT;
        u32  depth_stencil_state;
        u8   stencil_ref;
    };

    active_state  s_state;
    active_state  s_live_state;
    viewport      s_current_vp;
    context_state s_ctx;

    void _clear_resource_table()
    {
        // reserve resource 0 for NULL binding.
        _res_pool[0].asigned_flag |= 0xff;
    }

    // support for gles to allow almost correct wireframe, by using line strip
    u32 _gles_wireframe(u32 primitve_topology)
    {
        if (primitve_topology != GL_LINES && primitve_topology != GL_LINE_STRIP)
            if (_res_pool[s_state.raster_state].raster_state.gles_wireframe)
                return GL_LINE_STRIP;
        return primitve_topology;
    }
#ifdef PEN_GLES3
#define PEN_GLES_WIREFRAME_TOPOLOGY(pt) _gles_wireframe(pt)
#else
#define PEN_GLES_WIREFRAME_TOPOLOGY(pt) pt
#endif

    void _set_depth_stencil_state(u32 dss, u8 ref)
    {
        resource_allocation& res = _res_pool[dss];
        if (!res.depth_stencil)
            return;

        // depth
        if (res.depth_stencil->depth_enable)
        {
            CHECK_CALL(glEnable(GL_DEPTH_TEST));
        }
        else
        {
            CHECK_CALL(glDisable(GL_DEPTH_TEST));
        }

        CHECK_CALL(glDepthFunc(res.depth_stencil->depth_func));
        CHECK_CALL(glDepthMask(res.depth_stencil->depth_write_mask));

        // stencil
        if (res.depth_stencil->stencil_enable)
        {
            CHECK_CALL(glEnable(GL_STENCIL_TEST));
            CHECK_CALL(glStencilMask(res.depth_stencil->stencil_write_mask));

            // front
            CHECK_CALL(glStencilOpSeparate(GL_FRONT, res.depth_stencil->front_face.stencil_failop,
                                           res.depth_stencil->front_face.stencil_depth_failop,
                                           res.depth_stencil->front_face.stencil_passop));

            CHECK_CALL(glStencilFuncSeparate(GL_FRONT, res.depth_stencil->front_face.stencil_func, ref,
                                             res.depth_stencil->stencil_read_mask));

            // back
            CHECK_CALL(glStencilOpSeparate(GL_BACK, res.depth_stencil->back_face.stencil_failop,
                                           res.depth_stencil->back_face.stencil_depth_failop,
                                           res.depth_stencil->back_face.stencil_passop));

            CHECK_CALL(glStencilFuncSeparate(GL_BACK, res.depth_stencil->back_face.stencil_func, ref,
                                             res.depth_stencil->stencil_read_mask));
        }
        else
        {
            CHECK_CALL(glDisable(GL_STENCIL_TEST));
        }
    }

    u32 _link_program_internal(u32 vs, u32 ps, u32 cs, const shader_link_params* params = nullptr)
    {
        // link the shaders
        GLuint program_id = CHECK_CALL(glCreateProgram());
        u32    so = 0;

        if (params)
        {
            // this path is for a proper link with reflection info
            vs = _res_pool[params->vertex_shader].handle;
            ps = _res_pool[params->pixel_shader].handle;
            so = _res_pool[params->stream_out_shader].handle;

            if (vs)
            {
                CHECK_CALL(glAttachShader(program_id, vs));
            }

            if (ps)
            {
                CHECK_CALL(glAttachShader(program_id, ps));
            }

            if (so)
            {
                CHECK_CALL(glAttachShader(program_id, so));
                CHECK_CALL(glTransformFeedbackVaryings(program_id, params->num_stream_out_names,
                                                       (const c8**)params->stream_out_names, GL_INTERLEAVED_ATTRIBS));
            }
        }
        else
        {
            if (vs && ps)
            {
                // on the fly link for bound vs and ps which have not been explicity linked
                // to emulate d3d behaviour of set vs set ps etc
                CHECK_CALL(glAttachShader(program_id, vs));
                CHECK_CALL(glAttachShader(program_id, ps));
            }
            else if (cs)
            {
                CHECK_CALL(glAttachShader(program_id, cs));
            }
        }

        CHECK_CALL(glLinkProgram(program_id));

        // Check the program
        GLint result = GL_FALSE;
        int   info_log_length = 0;

        CHECK_CALL(glGetProgramiv(program_id, GL_LINK_STATUS, &result));
        CHECK_CALL(glGetProgramiv(program_id, GL_INFO_LOG_LENGTH, &info_log_length));

        if (result == GL_FALSE && info_log_length > 0)
        {
            char* info_log_buf = (char*)memory_alloc(info_log_length + 1);

            CHECK_CALL(glGetProgramInfoLog(program_id, info_log_length, NULL, &info_log_buf[0]));
            info_log_buf[info_log_length] = '\0';

            output_debug("%s", info_log_buf);
            memory_free(info_log_buf);
        }

        if (result == GL_FALSE)
        {
            vs = 0;
            cs = 0;
            ps = 0;
        }

        shader_program program;
        program.vs = vs;
        program.ps = ps;
        program.so = so;
        program.cs = cs;
        program.program = program_id;
        program.vflip_uniform = glGetUniformLocation(program.program, "v_flip");

        for (s32 i = 0; i < MAX_UNIFORM_BUFFERS; ++i)
        {
            program.texture_location[i] = INVALID_LOC;
            program.uniform_block_location[i] = INVALID_LOC;
        }

        sb_push(s_shader_programs, program);
        return sb_count(s_shader_programs) - 1;
    }
} // namespace

namespace pen
{
    void direct::renderer_create_clear_state(const clear_state& cs, u32 resource_slot)
    {
        _res_pool.grow(resource_slot);

        _res_pool[resource_slot].clear_state.rgba[0] = cs.r;
        _res_pool[resource_slot].clear_state.rgba[1] = cs.g;
        _res_pool[resource_slot].clear_state.rgba[2] = cs.b;
        _res_pool[resource_slot].clear_state.rgba[3] = cs.a;
        _res_pool[resource_slot].clear_state.depth = cs.depth;
        _res_pool[resource_slot].clear_state.stencil = cs.stencil;
        _res_pool[resource_slot].clear_state.flags = to_gl_clear_flags(cs.flags);

        _res_pool[resource_slot].clear_state.num_colour_targets = cs.num_colour_targets;
        memcpy(&_res_pool[resource_slot].clear_state.mrt, cs.mrt, sizeof(mrt_clear) * MAX_MRT);
    }

    void direct::renderer_sync()
    {
        // unused for opengl, required to sync for metal
    }

    void direct::renderer_retain()
    {
        // unused for opengl, required to retain auto release ojects for metal
    }

    void direct::renderer_new_frame()
    {
        pen_make_gl_context_current();
        _renderer_new_frame();

        // if we resize render targets, delete frame buffers to invalidate and recreate them
        static u32 rs = 0;
        if (_renderer_resize_index() != rs)
        {
            rs = _renderer_resize_index();
            sb_free(s_framebuffers);
            s_framebuffers = nullptr;
        }
    }

    void direct::renderer_end_frame()
    {
        // unused on this platform
    }

    void direct::renderer_clear_texture(u32 clear_state_index, u32 texture)
    {
    }

    void direct::renderer_clear(u32 clear_state_index, u32 colour_face, u32 depth_face)
    {
        resource_allocation&  rc = _res_pool[clear_state_index];
        clear_state_internal& cs = rc.clear_state;

        CHECK_CALL(glClearDepth(cs.depth));
        CHECK_CALL(glClearStencil(cs.stencil));

        if (rc.clear_state.flags & GL_DEPTH_BUFFER_BIT)
        {
            // we must enable depth writes when clearing..
            CHECK_CALL(glEnable(GL_DEPTH_TEST));
            CHECK_CALL(glDepthMask(true));
            s_state.depth_stencil_state = PEN_INVALID_HANDLE;
        }

        if (rc.clear_state.flags & GL_STENCIL_BUFFER_BIT)
        {
            CHECK_CALL(glEnable(GL_STENCIL_TEST));
            CHECK_CALL(glStencilMask(0xff));
            s_state.depth_stencil_state = PEN_INVALID_HANDLE;
        }

        if (cs.num_colour_targets == 0)
        {
            CHECK_CALL(
                glClearColor(rc.clear_state.rgba[0], rc.clear_state.rgba[1], rc.clear_state.rgba[2], rc.clear_state.rgba[3]));

            if (!rc.clear_state.flags)
                return;

            CHECK_CALL(glClear(rc.clear_state.flags));

            return;
        }

        u32 masked = rc.clear_state.flags &= ~GL_COLOR_BUFFER_BIT;
        if (!masked)
            return;

        CHECK_CALL(glClear(masked));

        for (s32 i = 0; i < cs.num_colour_targets; ++i)
        {
            if (cs.mrt[i].type == CLEAR_F32)
            {
                CHECK_CALL(glClearBufferfv(GL_COLOR, i, cs.mrt[i].f));
                continue;
            }

            if (cs.mrt[i].type == CLEAR_U32)
            {
                CHECK_CALL(glClearBufferuiv(GL_COLOR, i, cs.mrt[i].u));
                continue;
            }
        }
    }

    void direct::renderer_present()
    {
        pen_gl_swap_buffers();
        _renderer_end_frame();

        s_state = {};

// gpu counters
#ifndef __linux__
        if (s_frame > 0)
            renderer_pop_perf_marker(); // gpu total
        gather_perf_markers();
        s_frame++;

        // gpu total
        renderer_push_perf_marker(nullptr);
#endif
    }

    void direct::renderer_load_shader(const shader_load_params& params, u32 resource_slot)
    {
        _res_pool.grow(resource_slot);

        resource_allocation& res = _res_pool[resource_slot];

        u32 internal_type = to_gl_shader_type(params.type);
        if (internal_type == -1)
            return;

        res.handle = CHECK_CALL(glCreateShader(internal_type));

        CHECK_CALL(glShaderSource(res.handle, 1, (const c8**)&params.byte_code, (s32*)&params.byte_code_size));
        CHECK_CALL(glCompileShader(res.handle));

        // Check compilation status
        GLint result = GL_FALSE;
        int   info_log_length;

        CHECK_CALL(glGetShaderiv(res.handle, GL_COMPILE_STATUS, &result));
        CHECK_CALL(glGetShaderiv(res.handle, GL_INFO_LOG_LENGTH, &info_log_length));

        if (info_log_length > 0)
        {
            // print line by line
            char* lc = (char*)params.byte_code;
            int   line = 2;
            while (*lc != '\0')
            {
                Str str_line = "";

                while (*lc != '\n' && *lc != '\0')
                {
                    str_line.append(*lc);
                    ++lc;
                }

                PEN_LOG("%i: %s", line, str_line.c_str());
                ++line;

                if (*lc == '\0')
                    break;

                ++lc;

                if (lc >= ((char*)params.byte_code + params.byte_code_size))
                    break;
            }

            char* info_log_buf = (char*)memory_alloc(info_log_length + 1);

            CHECK_CALL(glGetShaderInfoLog(res.handle, info_log_length, NULL, &info_log_buf[0]));
            PEN_LOG(info_log_buf);
        }
    }

    void direct::renderer_set_shader(u32 shader_index, u32 shader_type)
    {
        switch (shader_type)
        {
            case PEN_SHADER_TYPE_VS:
                s_live_state.vertex_shader = shader_index;
                s_live_state.stream_out_shader = 0;
                break;
            case PEN_SHADER_TYPE_PS:
                s_live_state.pixel_shader = shader_index;
                break;
            case PEN_SHADER_TYPE_SO:
                s_live_state.stream_out_shader = shader_index;
                s_live_state.vertex_shader = 0;
                s_live_state.pixel_shader = 0;
                break;
#if GL_ARB_compute_shader
            case PEN_SHADER_TYPE_CS:
                s_live_state.compute_shader = shader_index;
                s_live_state.vertex_shader = 0;
                s_live_state.pixel_shader = 0;
                s_live_state.stream_out_shader = 0;
                break;
#endif
        }
    }

    void direct::renderer_create_buffer(const buffer_creation_params& params, u32 resource_slot)
    {
        _res_pool.grow(resource_slot);

        resource_allocation& res = _res_pool[resource_slot];

        u32 usage = to_gl_usage(params.usage_flags);
        u32 gl_bind = to_gl_bind_flags(params.bind_flags);

        CHECK_CALL(glGenBuffers(1, &res.handle));
        CHECK_CALL(glBindBuffer(gl_bind, res.handle));
        CHECK_CALL(glBufferData(gl_bind, params.buffer_size, params.data, usage));

        res.type = gl_bind;
    }

    void direct::renderer_link_shader_program(const shader_link_params& params, u32 resource_slot)
    {
        _res_pool.grow(resource_slot);

        shader_link_params slp = params;
        u32                program_index = _link_program_internal(0, 0, 0, &slp);

        shader_program* linked_program = &s_shader_programs[program_index];

        if (linked_program->ps == 0 && linked_program->vs == 0)
            return;

        GLuint prog = linked_program->program;
        glUseProgram(linked_program->program);

        // build lookup tables for uniform buffers and texture samplers
        for (u32 i = 0; i < params.num_constants; ++i)
        {
            constant_layout_desc& constant = params.constants[i];
            GLint                 loc;

            switch (constant.type)
            {
                case CT_CBUFFER:
                {
                    loc = CHECK_CALL(glGetUniformBlockIndex(prog, constant.name));
                    if (loc != -1)
                    {
                        PEN_ASSERT(loc < MAX_UNIFORM_BUFFERS);
                        linked_program->uniform_block_location[constant.location] = loc;
                        CHECK_CALL(glUniformBlockBinding(prog, loc, constant.location));
                    }
                }
                break;

                case CT_SAMPLER_3D:
                case CT_SAMPLER_2D:
                case CT_SAMPLER_2DMS:
                case CT_SAMPLER_CUBE:
                case CT_SAMPLER_2D_ARRAY:
                case CT_SAMPLER_2D_DEPTH:
                case CT_SAMPLER_2D_DEPTH_ARRAY:
                {
                    loc = CHECK_CALL(glGetUniformLocation(prog, constant.name));
                    if (loc != -1)
                    {
                        linked_program->texture_location[constant.location] = loc;
                        if (loc != -1 && loc != constant.location)
                            CHECK_CALL(glUniform1i(loc, constant.location));
                    }
                }
                break;
                default:
                    break;
            }
        }

        _res_pool[resource_slot].shader_program = linked_program;
    }

    void direct::renderer_set_stream_out_target(u32 buffer_index)
    {
        s_live_state.stream_out_buffer = buffer_index;
    }

    void direct::renderer_draw_auto()
    {
    }

    void direct::renderer_dispatch_compute(uint3 grid, uint3 num_threads)
    {
#if GL_ARB_compute_shader
        // look for linked cs program
        u32             cs = _res_pool[s_live_state.compute_shader].handle;
        shader_program* linked_program = nullptr;
        u32             num_shaders = sb_count(s_shader_programs);
        for (s32 i = 0; i < num_shaders; ++i)
        {
            if (s_shader_programs[i].cs == cs)
            {
                linked_program = &s_shader_programs[i];
                break;
            }
        }

        // link if we need to on the fly
        if (linked_program == nullptr)
        {
            u32 index = _link_program_internal(0, 0, cs, nullptr);
            linked_program = &s_shader_programs[index];
        }

        glUseProgram(linked_program->program);
        CHECK_CALL(glDispatchCompute(grid.x / num_threads.x, grid.y / num_threads.y, grid.z / num_threads.z));
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
        glBindImageTexture(0, 0, 0, 0, 0, 0, 0);

#endif
    }

    void direct::renderer_create_input_layout(const input_layout_creation_params& params, u32 resource_slot)
    {
        _res_pool.grow(resource_slot);

        resource_allocation& res = _res_pool[resource_slot];
        res.input_layout = new input_layout;
        res.input_layout->attributes = nullptr;

        for (u32 i = 0; i < params.num_elements; ++i)
        {
            vertex_attribute  new_attrib;
            vertex_format_map vf = to_gl_vertex_format(params.input_layout[i].format);

            new_attrib.location = i;
            new_attrib.type = vf.format;
            new_attrib.num_elements = vf.num_elements;
            new_attrib.offset = params.input_layout[i].aligned_byte_offset;
            new_attrib.stride = 0;
            new_attrib.input_slot = params.input_layout[i].input_slot;
            new_attrib.step_rate = params.input_layout[i].instance_data_step_rate;

            sb_push(res.input_layout->attributes, new_attrib);
        }
    }

    void direct::renderer_set_vertex_buffers(u32* buffer_indices, u32 num_buffers, u32 start_slot, const u32* strides,
                                             const u32* offsets)
    {
        for (s32 i = 0; i < num_buffers; ++i)
        {
            s_live_state.vertex_buffer[start_slot + i] = buffer_indices[i];
            s_live_state.vertex_buffer_stride[start_slot + i] = strides[i];
            s_live_state.vertex_buffer_offset[start_slot + i] = offsets[i];
        }

        s_live_state.num_bound_vertex_buffers = num_buffers;
    }

    void direct::renderer_set_input_layout(u32 layout_index)
    {
        s_live_state.input_layout = layout_index;
    }

    void direct::renderer_set_index_buffer(u32 buffer_index, u32 format, u32 offset)
    {
        s_state.index_buffer = buffer_index;
        s_state.index_format = to_gl_index_format(format);
    }

    void bind_state(u32 primitive_topology)
    {
        // bind shaders
        if (s_live_state.stream_out_shader != 0 && (s_live_state.stream_out_shader != s_state.stream_out_shader))
        {
            // stream out
            s_state.vertex_shader = s_live_state.vertex_shader;
            s_state.pixel_shader = s_live_state.pixel_shader;
            s_state.stream_out_shader = s_live_state.stream_out_shader;

            shader_program* linked_program = nullptr;

            u32 so_handle = _res_pool[s_state.stream_out_shader].handle;

            u32 num_shaders = sb_count(s_shader_programs);
            for (s32 i = 0; i < num_shaders; ++i)
            {
                if (s_shader_programs[i].so == so_handle)
                {
                    linked_program = &s_shader_programs[i];
                    break;
                }
            }

            if (linked_program == nullptr)
            {
                // we need to link ahead of time to setup the transform feedback varying
                PEN_ASSERT(0);
            }

            CHECK_CALL(glUseProgram(linked_program->program));

            // constant buffers and texture locations
            for (s32 i = 0; i < MAX_UNIFORM_BUFFERS; ++i)
                if (linked_program->texture_location[i] != INVALID_LOC)
                    CHECK_CALL(glUniform1i(linked_program->texture_location[i], i));

            CHECK_CALL(glEnable(GL_RASTERIZER_DISCARD));
        }
        else
        {
            if (s_live_state.vertex_shader != s_state.vertex_shader || s_live_state.pixel_shader != s_state.pixel_shader ||
                s_live_state.v_flip != s_state.v_flip)
            {
                s_state.vertex_shader = s_live_state.vertex_shader;
                s_state.pixel_shader = s_live_state.pixel_shader;
                s_state.stream_out_shader = s_live_state.stream_out_shader;
                s_state.v_flip = s_live_state.v_flip;

                shader_program* linked_program = nullptr;

                auto vs_handle = _res_pool[s_state.vertex_shader].handle;
                auto ps_handle = _res_pool[s_state.pixel_shader].handle;

                u32 num_shaders = sb_count(s_shader_programs);
                for (s32 i = 0; i < num_shaders; ++i)
                {
                    if (s_shader_programs[i].vs == vs_handle && s_shader_programs[i].ps == ps_handle)
                    {
                        linked_program = &s_shader_programs[i];
                        break;
                    }
                }

                if (linked_program == nullptr)
                {
                    u32 index = _link_program_internal(vs_handle, ps_handle, 0);
                    linked_program = &s_shader_programs[index];
                }

                CHECK_CALL(glUseProgram(linked_program->program));

                // constant buffers and texture locations
                for (s32 i = 0; i < MAX_UNIFORM_BUFFERS; ++i)
                    if (linked_program->texture_location[i] != INVALID_LOC)
                        CHECK_CALL(glUniform1i(linked_program->texture_location[i], i));

                // we need to flip all geometry that is rendered into render targets to be consistent with d3d
                float v_flip = 1.0f;
                if (!s_live_state.backbuffer_bound)
                    v_flip = -1.0f;

                glUniform1f(linked_program->vflip_uniform, v_flip);
            }
        }

        s_state.input_layout = s_live_state.input_layout;
        if (s_live_state.input_layout)
        {
            auto* input_res = _res_pool[s_live_state.input_layout].input_layout;
            if (input_res->vertex_array_handle == 0)
            {
                CHECK_CALL(glGenVertexArrays(1, &input_res->vertex_array_handle));
            }

            CHECK_CALL(glBindVertexArray(input_res->vertex_array_handle));

            for (s32 v = 0; v < s_live_state.num_bound_vertex_buffers; ++v)
            {
                s_state.vertex_buffer[v] = s_live_state.vertex_buffer[v];
                s_state.vertex_buffer_stride[v] = s_live_state.vertex_buffer_stride[v];

                auto& res = _res_pool[s_state.vertex_buffer[v]].handle;
                CHECK_CALL(glBindBuffer(GL_ARRAY_BUFFER, res));

                u32 num_attribs = sb_count(input_res->attributes);

                for (u32 a = 0; a < num_attribs; ++a)
                {
                    auto& attribute = input_res->attributes[a];

                    if (attribute.input_slot != v)
                        continue;

                    CHECK_CALL(glEnableVertexAttribArray(attribute.location));

                    u32 base_vertex_offset = s_state.vertex_buffer_stride[v] * s_state.base_vertex;

                    CHECK_CALL(glVertexAttribPointer(attribute.location, attribute.num_elements, attribute.type,
                                                     attribute.type == GL_UNSIGNED_BYTE ? true : false,
                                                     s_state.vertex_buffer_stride[v],
                                                     (void*)(attribute.offset + base_vertex_offset)));

                    CHECK_CALL(glVertexAttribDivisor(attribute.location, attribute.step_rate));
                }
            }
        }

        if (s_state.raster_state != s_live_state.raster_state || s_state.backbuffer_bound != s_live_state.backbuffer_bound)
        {
            s_state.raster_state = s_live_state.raster_state;
            s_state.backbuffer_bound = s_live_state.backbuffer_bound;

            auto& rs = _res_pool[s_state.raster_state].raster_state;

            bool ccw = rs.front_ccw;
            if (!s_live_state.backbuffer_bound)
                ccw = !ccw;

            if (ccw)
            {
                CHECK_CALL(glFrontFace(GL_CCW));
            }
            else
            {
                CHECK_CALL(glFrontFace(GL_CW));
            }

            if (rs.culling_enabled)
            {
                CHECK_CALL(glEnable(GL_CULL_FACE));
                CHECK_CALL(glCullFace(rs.cull_face));
            }
            else
            {
                CHECK_CALL(glDisable(GL_CULL_FACE));
            }

#ifdef GL_DEPTH_CLAMP
            if (rs.depth_clip_enabled)
            {
                CHECK_CALL(glDisable(GL_DEPTH_CLAMP));
            }
            else
            {
                CHECK_CALL(glEnable(GL_DEPTH_CLAMP));
            }
#endif

#ifndef PEN_GLES3
            CHECK_CALL(glPolygonMode(GL_FRONT_AND_BACK, rs.polygon_mode));
#endif

            if (rs.scissor_enabled)
            {
                CHECK_CALL(glEnable(GL_SCISSOR_TEST));
            }
            else
            {
                CHECK_CALL(glDisable(GL_SCISSOR_TEST));
            }
        }

        if (s_live_state.stream_out_buffer != s_state.stream_out_buffer)
        {
            s_state.stream_out_buffer = s_live_state.stream_out_buffer;

            if (s_state.stream_out_buffer)
            {
                u32 so_buffer = _res_pool[s_state.stream_out_buffer].handle;

                CHECK_CALL(glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, so_buffer));
                CHECK_CALL(glBeginTransformFeedback(primitive_topology));
            }
        }

        if (s_state.depth_stencil_state != s_live_state.depth_stencil_state ||
            s_state.stencil_ref != s_live_state.stencil_ref)
        {
            _set_depth_stencil_state(s_live_state.depth_stencil_state, s_live_state.stencil_ref);
            s_state.depth_stencil_state = s_live_state.depth_stencil_state;
            s_state.stencil_ref = s_live_state.stencil_ref;
        }
    }

    void direct::renderer_draw(u32 vertex_count, u32 start_vertex, u32 primitive_topology)
    {
        primitive_topology = to_gl_primitive_topology(primitive_topology);
        bind_state(primitive_topology);
        primitive_topology = PEN_GLES_WIREFRAME_TOPOLOGY(primitive_topology);

        CHECK_CALL(glDrawArrays(primitive_topology, start_vertex, vertex_count));

        if (s_state.stream_out_buffer)
        {
            s_live_state.stream_out_buffer = 0;
            s_state.stream_out_buffer = 0;

            CHECK_CALL(glEndTransformFeedback());
            CHECK_CALL(glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, 0));
            CHECK_CALL(glDisable(GL_RASTERIZER_DISCARD));
        }
    }

    void direct::renderer_draw_indexed(u32 index_count, u32 start_index, u32 base_vertex, u32 primitive_topology)
    {
        PEN_SET_BASE_VERTEX(base_vertex);

        primitive_topology = to_gl_primitive_topology(primitive_topology);
        bind_state(primitive_topology);
        primitive_topology = PEN_GLES_WIREFRAME_TOPOLOGY(primitive_topology);

        // bind index buffer -this must always be re-bound
        GLuint res = _res_pool[s_state.index_buffer].handle;
        CHECK_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, res));

        void* offset = (void*)(size_t)(start_index * 2);

        CHECK_CALL(glDrawElementsBaseVertex(primitive_topology, index_count, s_state.index_format, offset, base_vertex));
    }

    void direct::renderer_draw_indexed_instanced(u32 instance_count, u32 start_instance, u32 index_count, u32 start_index,
                                                 u32 base_vertex, u32 primitive_topology)
    {
        PEN_SET_BASE_VERTEX(base_vertex);

        primitive_topology = to_gl_primitive_topology(primitive_topology);
        bind_state(primitive_topology);
        primitive_topology = PEN_GLES_WIREFRAME_TOPOLOGY(primitive_topology);

        // bind index buffer -this must always be re-bound
        GLuint res = _res_pool[s_state.index_buffer].handle;
        CHECK_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, res));

        // todo this needs to check index size 32 or 16 bit
        void* offset = (void*)(size_t)(start_index * 2);

        CHECK_CALL(glDrawElementsInstancedBaseVertex(primitive_topology, index_count, s_state.index_format, offset,
                                                     instance_count, base_vertex));
    }

    texture_info create_texture_internal(const texture_creation_params& tcp)
    {
        u32 sized_format, format, type, attachment;
        to_gl_texture_format(tcp.format, sized_format, format, type, attachment);

        u32 mip_w = tcp.width;
        u32 mip_h = tcp.height;
        c8* mip_data = (c8*)tcp.data;

        bool is_msaa = PEN_GL_MSAA_SUPPORT && tcp.sample_count > 1;

        u32 texture_target = is_msaa ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
        u32 base_texture_target = texture_target;

        u32 num_slices = 1;
        u32 num_faces = tcp.num_arrays;

        switch ((texture_collection_type)tcp.collection_type)
        {
            case TEXTURE_COLLECTION_NONE:
                texture_target = is_msaa ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
                base_texture_target = texture_target;
                break;
            case TEXTURE_COLLECTION_CUBE:
                is_msaa = false;
                texture_target = GL_TEXTURE_CUBE_MAP;
                base_texture_target = GL_TEXTURE_CUBE_MAP_POSITIVE_X;
                break;
            case TEXTURE_COLLECTION_CUBE_ARRAY:
                is_msaa = false;
                texture_target = GL_TEXTURE_CUBE_MAP_ARRAY;
                num_slices = tcp.num_arrays;
                num_faces = 1;
                base_texture_target = GL_TEXTURE_CUBE_MAP_ARRAY;
                break;
            case TEXTURE_COLLECTION_VOLUME:
                is_msaa = false;
                num_slices = tcp.num_arrays;
                num_faces = 1;
                texture_target = GL_TEXTURE_3D;
                base_texture_target = texture_target;
                break;
            case TEXTURE_COLLECTION_ARRAY:
                is_msaa = false;
                num_slices = tcp.num_arrays;
                num_faces = 1;
                texture_target = is_msaa ? GL_TEXTURE_2D_MULTISAMPLE_ARRAY : GL_TEXTURE_2D_ARRAY;
                base_texture_target = texture_target;
                break;
            default:
                texture_target = is_msaa ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
                base_texture_target = texture_target;
                break;
        }

        u32 mip_d = num_slices;

        GLuint handle;
        CHECK_CALL(glGenTextures(1, &handle));
        CHECK_CALL(glBindTexture(texture_target, handle));

        if (base_texture_target == GL_TEXTURE_3D)
        {
            // 3d textures rgba / bgra are reversed for some reason
            if (format == GL_RGBA)
            {
                format = GL_BGRA;
            }
            else if (format == GL_BGRA)
            {
                format = GL_RGBA;
            }
        }

        // slightly confusing and messy, faces is used for cubemaps they have 6
        // mip_d is number of slices in array or 3D textures, and with 3D texture they downsize.
        // with arrays mip_d remains constant for all mip levels
        for (u32 a = 0; a < num_faces; ++a)
        {
            mip_w = tcp.width;
            mip_h = tcp.height;
            mip_d = num_slices;

            for (u32 mip = 0; mip < tcp.num_mips; ++mip)
            {
                u32 mip_size = calc_mip_level_size(mip_w, mip_h, mip_d, tcp.block_size, tcp.pixels_per_block);

                if (is_msaa)
                {
                    CHECK_CALL(
                        glTexImage2DMultisample(base_texture_target, tcp.sample_count, sized_format, mip_w, mip_h, GL_TRUE));
                }
                else
                {
                    if (type == GL_TEXTURE_COMPRESSED)
                    {
                        CHECK_CALL(glCompressedTexImage2D(base_texture_target + a, mip, format, mip_w, mip_h, 0, mip_size,
                                                          mip_data));
                    }
                    else
                    {
                        if (base_texture_target == GL_TEXTURE_3D)
                        {
                            CHECK_CALL(glTexImage3D(base_texture_target, mip, sized_format, mip_w, mip_h, mip_d, 0, format,
                                                    type, mip_data));
                        }
                        else if (tcp.collection_type == TEXTURE_COLLECTION_ARRAY ||
                                 tcp.collection_type == TEXTURE_COLLECTION_CUBE_ARRAY)
                        {
                            CHECK_CALL(glTexImage3D(base_texture_target, mip, sized_format, mip_w, mip_h, mip_d, 0, format,
                                                    type, nullptr));
                        }
                        else
                        {
                            // cubemap
                            CHECK_CALL(glTexImage2D(base_texture_target + a, mip, sized_format, mip_w, mip_h, 0, format, type,
                                                    mip_data));
                        }
                    }
                }

                if (mip_data != nullptr)
                    mip_data += mip_size;

                mip_w /= 2;
                mip_h /= 2;

                mip_w = max<u32>(1, mip_w);
                mip_h = max<u32>(1, mip_h);

                if (base_texture_target == GL_TEXTURE_3D)
                {
                    mip_d /= 2;
                    mip_d = max<u32>(1, mip_d);
                }
            }

            // array layouts coming for dds do not plug in to glTexImage3D in a friendly way
            // glTexSubImage3D is required to supply per-slice, per-mip
            if (tcp.data)
            {
                if (tcp.collection_type == TEXTURE_COLLECTION_ARRAY)
                {
                    mip_data = (c8*)tcp.data;
                    for (u32 a = 0; a < num_slices; ++a)
                    {
                        mip_w = tcp.width;
                        mip_h = tcp.height;

                        for (u32 mip = 0; mip < tcp.num_mips; ++mip)
                        {
                            u32 pitch = tcp.block_size * (mip_w / tcp.pixels_per_block);
                            u32 depth_pitch = pitch * (mip_h / tcp.pixels_per_block);

                            glTexSubImage3D(base_texture_target, mip, 0, 0, a, mip_w, mip_h, 1, format, type, mip_data);

                            mip_w /= 2;
                            mip_h /= 2;
                            mip_w = max<u32>(1, mip_w);
                            mip_h = max<u32>(1, mip_h);

                            mip_data += depth_pitch;
                        }
                    }
                }
            }
        }

        CHECK_CALL(glBindTexture(texture_target, 0));

        texture_info ti;
        ti.handle = handle;
        ti.max_mip_level = tcp.num_mips - 1;
        ti.target = texture_target;
        ti.attachment = attachment;

        if (tcp.width != tcp.height && ti.max_mip_level > 0)
        {
            ti.max_mip_level = tcp.num_mips - 2;
        }

        return ti;
    }

    void direct::renderer_create_render_target(const texture_creation_params& tcp, u32 resource_slot, bool track)
    {
        PEN_ASSERT(tcp.width != 0 && tcp.height != 0);
        _res_pool.grow(resource_slot);

        resource_allocation& res = _res_pool[resource_slot];

        res.type = RES_RENDER_TARGET;

        texture_creation_params _tcp = _renderer_tcp_resolve_ratio(tcp);

        if (track)
            _renderer_track_managed_render_target(tcp, resource_slot);

        res.render_target.uid = (u32)get_time_ms();

        if (tcp.num_mips == -1)
            _tcp.num_mips = calc_num_mips(_tcp.width, _tcp.height);

        // generate mips or resolve
        if (tcp.num_mips > 1)
            res.render_target.invalidate = 1;

        // null handles
        res.render_target.texture_msaa.handle = 0;
        res.render_target.texture.handle = 0;
        res.render_target.collection_type = tcp.collection_type;

        if (tcp.sample_count > 1 && PEN_GL_MSAA_SUPPORT)
        {
            res.type = RES_RENDER_TARGET_MSAA;

            res.render_target.texture_msaa = create_texture_internal(_tcp);

            res.render_target.tcp = new texture_creation_params;
            *res.render_target.tcp = tcp;
        }
        else
        {
            // non-msaa
            texture_creation_params tcp_no_msaa = _tcp;
            tcp_no_msaa.sample_count = 1;

            res.render_target.texture = create_texture_internal(tcp_no_msaa);
        }
    }

    void direct::renderer_set_targets(const u32* const colour_targets, u32 num_colour_targets, u32 depth_target,
                                      u32 colour_face, u32 depth_face)
    {
        static const GLenum k_draw_buffers[MAX_MRT] = {
            GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3,
            GL_COLOR_ATTACHMENT4, GL_COLOR_ATTACHMENT5, GL_COLOR_ATTACHMENT6, GL_COLOR_ATTACHMENT7,
        };

        bool use_back_buffer = false;
        if (depth_target == PEN_BACK_BUFFER_DEPTH)
            use_back_buffer = true;

        if (num_colour_targets)
            if (colour_targets[0] == PEN_BACK_BUFFER_COLOUR)
                use_back_buffer = true;

        if (use_back_buffer)
        {
            s_live_state.backbuffer_bound = true;
            s_live_state.v_flip = false;

            CHECK_CALL(glBindFramebuffer(GL_FRAMEBUFFER, s_backbuffer_fbo));
            CHECK_CALL(glDrawBuffer(GL_BACK));
            return;
        }

        s_live_state.v_flip = true;
        s_live_state.backbuffer_bound = false;

        bool msaa = false;

        hash_murmur hh;
        hh.begin();
        hh.add(colour_face);
        hh.add(depth_face);
        hh.add(num_colour_targets);

        if (depth_target != PEN_NULL_DEPTH_BUFFER)
        {
            resource_allocation& depth_res = _res_pool[depth_target];
            hh.add(depth_res.render_target.uid);
            hh.add(depth_target);
            depth_res.render_target.invalidate = 1;
        }
        else
        {
            hh.add(-1);
            hh.add(-1);
        }

        for (s32 i = 0; i < num_colour_targets; ++i)
        {
            resource_allocation& colour_res = _res_pool[colour_targets[i]];
            hh.add(colour_res.render_target.uid);
            hh.add(colour_targets[i]);
            colour_res.render_target.invalidate = 1;

            if (colour_res.type == RES_RENDER_TARGET_MSAA)
                msaa = true;
        }

        hash_id h = hh.end();

        u32 num_fb = sb_count(s_framebuffers);
        for (u32 i = 0; i < num_fb; ++i)
        {
            auto& fb = s_framebuffers[i];

            if (fb.hash == h)
            {
                CHECK_CALL(glBindFramebuffer(GL_FRAMEBUFFER, fb._framebuffer));
                CHECK_CALL(glDrawBuffers(num_colour_targets, k_draw_buffers));
                return;
            }
        }

        GLuint fbh;
        CHECK_CALL(glGenFramebuffers(1, &fbh));
        CHECK_CALL(glBindFramebuffer(GL_FRAMEBUFFER, fbh));
        CHECK_CALL(glDrawBuffers(num_colour_targets, k_draw_buffers));

        if (depth_target != PEN_NULL_DEPTH_BUFFER)
        {
            resource_allocation& depth_res = _res_pool[depth_target];

            if (depth_res.render_target.collection_type == pen::TEXTURE_COLLECTION_ARRAY ||
                depth_res.render_target.collection_type == pen::TEXTURE_COLLECTION_CUBE_ARRAY)
            {
                u32 attachment = depth_res.render_target.texture.attachment;
                glFramebufferTextureLayer(GL_FRAMEBUFFER, attachment, depth_res.render_target.texture.handle, 0, depth_face);
            }
            else
            {
                u32 target = GL_TEXTURE_2D;
                if (depth_res.render_target.collection_type == pen::TEXTURE_COLLECTION_CUBE)
                    target = GL_TEXTURE_CUBE_MAP_POSITIVE_X + depth_face;

                if (msaa)
                {
                    u32 attachment = depth_res.render_target.texture_msaa.attachment;
                    CHECK_CALL(glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D_MULTISAMPLE,
                                                      depth_res.render_target.texture_msaa.handle, 0));
                }
                else
                {
                    u32 attachment = depth_res.render_target.texture.attachment;
                    CHECK_CALL(glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, target,
                                                      depth_res.render_target.texture.handle, 0));
                }
            }
        }

        for (s32 i = 0; i < num_colour_targets; ++i)
        {
            resource_allocation& colour_res = _res_pool[colour_targets[i]];

            if (colour_res.render_target.collection_type == pen::TEXTURE_COLLECTION_ARRAY ||
                colour_res.render_target.collection_type == pen::TEXTURE_COLLECTION_CUBE_ARRAY)
            {
                glFramebufferTextureLayer(GL_FRAMEBUFFER, k_draw_buffers[i], colour_res.render_target.texture.handle, 0,
                                          colour_face);
            }
            else
            {
                u32 target = GL_TEXTURE_2D;
                if (colour_res.render_target.collection_type == pen::TEXTURE_COLLECTION_CUBE)
                    target = GL_TEXTURE_CUBE_MAP_POSITIVE_X + colour_face;

                if (msaa)
                {
                    CHECK_CALL(glFramebufferTexture2D(GL_FRAMEBUFFER, k_draw_buffers[i], GL_TEXTURE_2D_MULTISAMPLE,
                                                      colour_res.render_target.texture_msaa.handle, 0));
                }
                else
                {
                    CHECK_CALL(glFramebufferTexture2D(GL_FRAMEBUFFER, k_draw_buffers[i], target,
                                                      colour_res.render_target.texture.handle, 0));
                }
            }
        }

        GLenum status = CHECK_CALL(glCheckFramebufferStatus(GL_FRAMEBUFFER));
        PEN_ASSERT(status == GL_FRAMEBUFFER_COMPLETE);

        framebuffer new_fb;
        new_fb.hash = h;
        new_fb._framebuffer = fbh;

        sb_push(s_framebuffers, new_fb);
    }

    void direct::renderer_resolve_target(u32 target, e_msaa_resolve_type type, resolve_resources res)
    {
        resource_allocation& colour_res = _res_pool[target];

        hash_id hash[2] = {0, 0};

        if (!colour_res.render_target.tcp)
            return;

        f32 w = colour_res.render_target.tcp->width;
        f32 h = colour_res.render_target.tcp->height;

        s32 ww, wh;
        pen::window_get_size(ww, wh);
        if (colour_res.render_target.tcp->width == -1)
        {
            w = ww / h;
            h = wh / h;
        }

        if (colour_res.render_target.texture.handle == 0)
        {
            texture_creation_params& _tcp = *colour_res.render_target.tcp;
            _tcp.sample_count = 0;
            _tcp.width = w;
            _tcp.height = h;

            if (_tcp.format == PEN_TEX_FORMAT_D24_UNORM_S8_UINT)
                _tcp.format = PEN_TEX_FORMAT_R32_FLOAT;

            colour_res.render_target.texture = create_texture_internal(*colour_res.render_target.tcp);
        }

        hash_murmur hh;
        hh.add(target);
        hh.add(colour_res.render_target.texture_msaa.handle);
        hh.add(1);
        hh.add(colour_res.render_target.uid);
        hash[0] = hh.end();

        hh.begin();
        hh.add(target);
        hh.add(colour_res.render_target.texture.handle);
        hh.add(1);
        hh.add(colour_res.render_target.uid);
        hash[1] = hh.end();

        GLuint fbos[2] = {0, 0};
        u32    num_fb = sb_count(s_framebuffers);
        for (u32 i = 0; i < num_fb; ++i)
        {
            auto& fb = s_framebuffers[i];
            for (s32 i = 0; i < 2; ++i)
                if (fb.hash == hash[i])
                    fbos[i] = fb._framebuffer;
        }

        for (s32 i = 0; i < 2; ++i)
        {
            if (fbos[i] == 0)
            {
                CHECK_CALL(glGenFramebuffers(1, &fbos[i]));
                CHECK_CALL(glBindFramebuffer(GL_FRAMEBUFFER, fbos[i]));

                if (i == 0) // src msaa
                {
                    CHECK_CALL(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE,
                                                      colour_res.render_target.texture_msaa.handle, 0));
                }
                else
                {
                    CHECK_CALL(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                                      colour_res.render_target.texture.handle, 0));
                }

                framebuffer fb = {hash[i], fbos[i]};
                sb_push(s_framebuffers, fb);
            }
        }

        if (type == RESOLVE_CUSTOM)
        {
            resolve_cbuffer cbuf = {w, h, 0.0f, 0.0f};

            CHECK_CALL(glBindFramebuffer(GL_FRAMEBUFFER, fbos[1]));

            direct::renderer_update_buffer(res.constant_buffer, &cbuf, sizeof(cbuf), 0);
            direct::renderer_set_constant_buffer(res.constant_buffer, 0, pen::CBUFFER_BIND_PS);

            viewport vp = {0.0f, 0.0f, w, h, 0.0f, 1.0f};
            direct::renderer_set_viewport(vp);
            direct::renderer_set_scissor_rect(rect{0.0f, 0.0f, w, h});

            u32 stride = 24;
            u32 offset = 0;
            direct::renderer_set_vertex_buffers(&res.vertex_buffer, 1, 0, &stride, &offset);
            direct::renderer_set_index_buffer(res.index_buffer, PEN_FORMAT_R16_UINT, 0);

            direct::renderer_set_texture(target, 0, 0, TEXTURE_BIND_MSAA | pen::TEXTURE_BIND_PS);

            direct::renderer_draw_indexed(6, 0, 0, PEN_PT_TRIANGLELIST);
        }
        else
        {
            CHECK_CALL(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbos[1]));
            CHECK_CALL(glBindFramebuffer(GL_READ_FRAMEBUFFER, fbos[0]));

            CHECK_CALL(glBlitFramebuffer(0, 0, (u32)w, (u32)h, 0, 0, (u32)w, (u32)h, GL_COLOR_BUFFER_BIT, GL_LINEAR));
        }
    }

    void direct::renderer_create_texture(const texture_creation_params& tcp, u32 resource_slot)
    {
        _res_pool.grow(resource_slot);

        _res_pool[resource_slot].type = RES_TEXTURE;

        if (tcp.collection_type == TEXTURE_COLLECTION_VOLUME)
            _res_pool[resource_slot].type = RES_TEXTURE_3D;

        _res_pool[resource_slot].texture = create_texture_internal(tcp);
    }

    void direct::renderer_create_sampler(const sampler_creation_params& scp, u32 resource_slot)
    {
        _res_pool.grow(resource_slot);

        gl_sampler sampler;
        sampler.address_u = to_gl_texture_address_mode(scp.address_u);
        sampler.address_v = to_gl_texture_address_mode(scp.address_v);
        sampler.address_w = to_gl_texture_address_mode(scp.address_w);
        sampler.comparison_func = PEN_COMPARISON_DISABLED;
        if (scp.comparison_func != PEN_COMPARISON_DISABLED)
            sampler.comparison_func = to_gl_comparison(scp.comparison_func);
        to_gl_filter_mode(scp.filter, &sampler.min_filter, &sampler.mag_filter);

        u32 sampler_objects[2];
        glGenSamplers(2, &sampler_objects[0]);
        _res_pool[resource_slot].sampler_object.compare = false;

        // creates 2 samplers, one of which is safe for depth textures by forcing nearest filtering
        for (u32 i = 0; i < 2; ++i)
        {
            if (i == 0)
            {
                CHECK_CALL(glSamplerParameteri(sampler_objects[i], GL_TEXTURE_MIN_FILTER, sampler.min_filter));
                CHECK_CALL(glSamplerParameteri(sampler_objects[i], GL_TEXTURE_MAG_FILTER, sampler.mag_filter));
            }
            else
            {
                // GLES3 has issues with bilinear on depth textures, this is temporary
                CHECK_CALL(glSamplerParameteri(sampler_objects[i], GL_TEXTURE_MIN_FILTER, GL_NEAREST));
                CHECK_CALL(glSamplerParameteri(sampler_objects[i], GL_TEXTURE_MAG_FILTER, GL_NEAREST));
            }

            CHECK_CALL(glSamplerParameteri(sampler_objects[i], GL_TEXTURE_WRAP_S, sampler.address_u));
            CHECK_CALL(glSamplerParameteri(sampler_objects[i], GL_TEXTURE_WRAP_T, sampler.address_v));
            CHECK_CALL(glSamplerParameteri(sampler_objects[i], GL_TEXTURE_WRAP_R, sampler.address_w));

            // mip control
#ifdef GL_TEXTURE_LOD_BIAS
            CHECK_CALL(glSamplerParameterf(sampler_objects[i], GL_TEXTURE_LOD_BIAS, sampler.mip_lod_bias));
#endif
            if (sampler.max_lod > -1.0f)
                CHECK_CALL(glSamplerParameterf(sampler_objects[i], GL_TEXTURE_MAX_LOD, sampler.max_lod));

            if (sampler.min_lod > -1.0f)
                CHECK_CALL(glSamplerParameterf(sampler_objects[i], GL_TEXTURE_MIN_LOD, sampler.min_lod));

            if (sampler.comparison_func != PEN_COMPARISON_DISABLED)
            {
                CHECK_CALL(glSamplerParameteri(sampler_objects[i], GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE));
                CHECK_CALL(glSamplerParameteri(sampler_objects[i], GL_TEXTURE_COMPARE_FUNC, GL_LESS));
                _res_pool[resource_slot].sampler_object.compare = true;
            }
            else
            {
                CHECK_CALL(glSamplerParameteri(sampler_objects[i], GL_TEXTURE_COMPARE_MODE, GL_NONE));
                CHECK_CALL(glSamplerParameteri(sampler_objects[i], GL_TEXTURE_COMPARE_FUNC, GL_ALWAYS));
            }

#ifndef PEN_GLES3
            CHECK_CALL(glSamplerParameteri(sampler_objects[i], GL_TEXTURE_CUBE_MAP_SEAMLESS, 1));
#endif
        }

        _res_pool[resource_slot].sampler_object.sampler = sampler_objects[0];
        _res_pool[resource_slot].sampler_object.depth_sampler = sampler_objects[1];
    }

    void direct::renderer_set_texture(u32 texture_index, u32 sampler_index, u32 unit, u32 bind_flags)
    {
        if (texture_index == 0)
            return;

        resource_allocation& res = _res_pool[texture_index];

        CHECK_CALL(glActiveTexture(GL_TEXTURE0 + unit));

        u32 max_mip = 0;
        u32 target = _res_pool[texture_index].texture.target;

#if GL_ARB_compute_shader
        // bind image textures for cs rw
        if (bind_flags & pen::TEXTURE_BIND_CS)
        {
            if (unit == 0)
            {
                glBindTexture(GL_TEXTURE_2D, res.texture.handle);
                CHECK_CALL(glBindImageTexture(unit, res.texture.handle, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8));
            }
            else
            {
                glBindTexture(GL_TEXTURE_2D, res.texture.handle);
                CHECK_CALL(glBindImageTexture(unit, res.texture.handle, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8));
            }
            return;
        }
#endif

        if (res.type == RES_TEXTURE || res.type == RES_TEXTURE_3D)
        {
            CHECK_CALL(glBindTexture(target, res.texture.handle));
            max_mip = res.texture.max_mip_level;
        }
        else
        {
            // render target
            if (bind_flags & TEXTURE_BIND_MSAA)
            {
                target = GL_TEXTURE_2D_MULTISAMPLE;
                CHECK_CALL(glBindTexture(target, res.render_target.texture_msaa.handle));
                max_mip = res.render_target.texture_msaa.max_mip_level;

                // auto mip map
                if (max_mip > 1 && res.render_target.invalidate)
                {
                    res.render_target.invalidate = 0;
                    CHECK_CALL(glGenerateMipmap(GL_TEXTURE_2D_MULTISAMPLE));
                }
            }
            else
            {
                CHECK_CALL(glBindTexture(target, res.render_target.texture.handle));
                max_mip = res.render_target.texture.max_mip_level;

                // auto mip map
                if (max_mip > 1 && res.render_target.invalidate)
                {
                    res.render_target.invalidate = 0;
                    CHECK_CALL(glGenerateMipmap(target));
                }
            }
        }

        // unbind sampler typically this is for cs texture binds, they dont need a sampler
        if (sampler_index == 0)
        {
            glBindSampler(unit, 0);
            return;
        }

        u32 sampler_object = _res_pool[sampler_index].sampler_object.sampler;

#ifdef PEN_GLES3
        // gles3 does not allow linear sampling on depth textures unless its compare to texture
        // this forces nearest filtering to maintain parity woth other gl versions and other rendering api's
        if (!_res_pool[sampler_index].sampler_object.compare)
        {
            if (res.texture.attachment == GL_DEPTH_ATTACHMENT || res.texture.attachment == GL_DEPTH_STENCIL_ATTACHMENT)
            {
                sampler_object = _res_pool[sampler_index].sampler_object.depth_sampler;
            }
        }
#endif

        glBindSampler(unit, sampler_object);

        if (target == GL_TEXTURE_2D_ARRAY)
            CHECK_CALL(glTexParameteri(target, GL_TEXTURE_BASE_LEVEL, 0));

        // handling textures with no mips
        CHECK_CALL(glTexParameteri(target, GL_TEXTURE_MAX_LEVEL, max_mip));
    }

    void direct::renderer_create_raster_state(const raster_state_creation_params& rscp, u32 resource_slot)
    {
        _res_pool.grow(resource_slot);

        auto& rs = _res_pool[resource_slot].raster_state;

        rs = {0};

        if (rscp.cull_mode != PEN_CULL_NONE)
        {
            rs.culling_enabled = true;
            rs.cull_face = to_gl_cull_mode(rscp.cull_mode);
        }

        rs.front_ccw = rscp.front_ccw;

        rs.depth_clip_enabled = rscp.depth_clip_enable;
        rs.scissor_enabled = rscp.scissor_enable;

        rs.polygon_mode = to_gl_polygon_mode(rscp.fill_mode);
        rs.gles_wireframe = false;
#ifdef PEN_GLES3
        if (rscp.fill_mode == PEN_FILL_WIREFRAME)
            rs.gles_wireframe = true;
#endif
    }

    void direct::renderer_set_raster_state(u32 rasterizer_state_index)
    {
        s_live_state.raster_state = rasterizer_state_index;
    }

    void direct::renderer_set_viewport(const viewport& vp)
    {
        viewport _vp = _renderer_resolve_viewport_ratio(vp);
        s_current_vp = _vp;
        CHECK_CALL(glViewport(_vp.x, _vp.y, _vp.width, _vp.height));
        CHECK_CALL(glDepthRangef(_vp.min_depth, _vp.max_depth));
    }

    void direct::renderer_set_scissor_rect(const rect& r)
    {
        rect _r = _renderer_resolve_scissor_ratio(r);
        f32  top = s_current_vp.height - _r.bottom;
        CHECK_CALL(glScissor(_r.left, top, _r.right - _r.left, _r.bottom - _r.top));
    }

    void direct::renderer_create_blend_state(const blend_creation_params& bcp, u32 resource_slot)
    {
        _res_pool.grow(resource_slot);

        _res_pool[resource_slot].blend_state = (blend_creation_params*)memory_alloc(sizeof(blend_creation_params));

        blend_creation_params* blend_state = _res_pool[resource_slot].blend_state;

        *blend_state = bcp;

        blend_state->render_targets =
            (render_target_blend*)memory_alloc(sizeof(render_target_blend) * bcp.num_render_targets);

        for (s32 i = 0; i < bcp.num_render_targets; ++i)
        {
            blend_state->render_targets[i] = bcp.render_targets[i];

            render_target_blend& rtb = blend_state->render_targets[i];
            rtb.src_blend = to_gl_blend_factor(rtb.src_blend);
            rtb.dest_blend = to_gl_blend_factor(rtb.dest_blend);
            rtb.blend_op = to_gl_blend_op(rtb.blend_op);
            rtb.src_blend_alpha = to_gl_blend_factor(rtb.src_blend_alpha);
            rtb.dest_blend_alpha = to_gl_blend_factor(rtb.dest_blend_alpha);
            rtb.blend_op_alpha = to_gl_blend_op(rtb.blend_op_alpha);
        }
    }

    void direct::renderer_set_blend_state(u32 blend_state_index)
    {
        auto* blend_state = _res_pool[blend_state_index].blend_state;
        if (!blend_state)
            return;

        for (s32 i = 0; i < blend_state->num_render_targets; ++i)
        {
            auto& rt_blend = blend_state->render_targets[i];

            if (i == 0)
            {
                u32 mask = rt_blend.render_target_write_mask;
                glColorMask(mask & 1, mask & 2, mask & 4, mask & 8);

                if (rt_blend.blend_enable)
                {
                    CHECK_CALL(glEnable(GL_BLEND));

                    CHECK_CALL(glBlendFuncSeparate(rt_blend.src_blend, rt_blend.dest_blend, rt_blend.src_blend_alpha,
                                                   rt_blend.dest_blend_alpha));

                    CHECK_CALL(glBlendEquationSeparate(rt_blend.blend_op, rt_blend.blend_op_alpha));
                }
                else
                {
                    CHECK_CALL(glDisable(GL_BLEND));
                }
            }
        }
    }

    void direct::renderer_set_constant_buffer(u32 buffer_index, u32 unit, u32 flags)
    {
        resource_allocation& res = _res_pool[buffer_index];
        CHECK_CALL(glBindBufferBase(GL_UNIFORM_BUFFER, unit, res.handle));
    }

    void direct::renderer_set_structured_buffer(u32 buffer_index, u32 unit, u32 flags)
    {
        PEN_ASSERT(0); // stubbed.. use metal on mac or d3d / vulkan on windows
    }

    void direct::renderer_update_buffer(u32 buffer_index, const void* data, u32 data_size, u32 offset)
    {
        resource_allocation& res = _res_pool[buffer_index];
        if (res.type == 0 || data_size == 0)
            return;

        CHECK_CALL(glBindBuffer(res.type, res.handle));

#ifndef PEN_GLES3
        void* mapped_data = CHECK_CALL(glMapBuffer(res.type, GL_WRITE_ONLY));

        if (mapped_data)
        {
            c8* mapped_offset = ((c8*)mapped_data) + offset;
            memcpy(mapped_offset, data, data_size);
        }

        CHECK_CALL(glUnmapBuffer(res.type));
#else
        CHECK_CALL(glBufferSubData(res.type, offset, data_size, data));
#endif
        CHECK_CALL(glBindBuffer(res.type, 0));
    }

    void update_backbuffer_texture()
    {
    }

    void direct::renderer_read_back_resource(const resource_read_back_params& rrbp)
    {
#ifndef PEN_GLES3
        resource_allocation& res = _res_pool[rrbp.resource_index];

        GLuint t = res.type;
        if (rrbp.resource_index == PEN_BACK_BUFFER_COLOUR)
        {
            // special case reading the backbuffer
            u32 w = rrbp.row_pitch / rrbp.block_size;
            u32 h = rrbp.depth_pitch / rrbp.row_pitch;

#if 0
            // this path is old, i cant remember if it even worked.
            static u32 resolve_buffer = -1;
            if (resolve_buffer == -1)
            {
                GLuint handle;
                CHECK_CALL(glGenTextures(1, &handle));
                CHECK_CALL(glBindTexture(0, handle));
                CHECK_CALL(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr));

                CHECK_CALL(glGenFramebuffers(1, &resolve_buffer));
                CHECK_CALL(glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, handle, 0));
            }

            CHECK_CALL(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resolve_buffer));
            CHECK_CALL(glBindFramebuffer(GL_READ_FRAMEBUFFER, 0));
            CHECK_CALL(glBlitFramebuffer(0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_LINEAR));

            CHECK_CALL(glBindFramebuffer(GL_FRAMEBUFFER, resolve_buffer));

            void* data = memory_alloc(rrbp.data_size);
            CHECK_CALL(glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, data));

            rrbp.call_back_function(data, rrbp.row_pitch, rrbp.depth_pitch, rrbp.block_size);

            memory_free(data);

            CHECK_CALL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
#else
            // this path is test working on macos
            u8* data = (u8*)memory_alloc(rrbp.data_size);

            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            CHECK_CALL(glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, data));

            // flip y coords
            u8* flipped = (u8*)memory_alloc(rrbp.data_size);
            u8* flip_pos = flipped + rrbp.depth_pitch - rrbp.row_pitch;
            u8* data_pos = data;
            for (u32 i = 0; i < rrbp.depth_pitch; i += rrbp.row_pitch)
            {
                memcpy(flip_pos, data_pos, rrbp.row_pitch);
                flip_pos -= rrbp.row_pitch;
                data_pos += rrbp.row_pitch;
            }

            rrbp.call_back_function(flipped, rrbp.row_pitch, rrbp.depth_pitch, rrbp.block_size);

            memory_free(data);
#endif
        }
        else if (t == RES_TEXTURE || t == RES_RENDER_TARGET || t == RES_RENDER_TARGET_MSAA)
        {
            u32 sized_format, format, type, attachment;
            to_gl_texture_format(rrbp.format, sized_format, format, type, attachment);

            s32 target_handle = res.texture.handle;
            if (t == RES_RENDER_TARGET || t == RES_RENDER_TARGET_MSAA)
                target_handle = res.render_target.texture.handle;

            CHECK_CALL(glBindTexture(GL_TEXTURE_2D, res.texture.handle));

            void* data = memory_alloc(rrbp.data_size);
            CHECK_CALL(glGetTexImage(GL_TEXTURE_2D, 0, format, type, data));

            rrbp.call_back_function(data, rrbp.row_pitch, rrbp.depth_pitch, rrbp.block_size);

            memory_free(data);
        }
        else if (t == GL_ELEMENT_ARRAY_BUFFER || t == GL_UNIFORM_BUFFER || t == GL_ARRAY_BUFFER)
        {
            CHECK_CALL(glBindBuffer(t, res.handle));
            void* map = glMapBuffer(t, GL_READ_ONLY);

            rrbp.call_back_function(map, rrbp.row_pitch, rrbp.depth_pitch, rrbp.block_size);

            CHECK_CALL(glUnmapBuffer(t));
        }
#endif
    }

    void direct::renderer_create_depth_stencil_state(const depth_stencil_creation_params& dscp, u32 resource_slot)
    {
        _res_pool.grow(resource_slot);

        _res_pool[resource_slot].depth_stencil = (depth_stencil_creation_params*)memory_alloc(sizeof(dscp));
        memcpy(_res_pool[resource_slot].depth_stencil, &dscp, sizeof(dscp));

        depth_stencil_creation_params* depth_stencil = _res_pool[resource_slot].depth_stencil;
        depth_stencil->depth_func = to_gl_comparison(dscp.depth_func);

        if (depth_stencil->stencil_enable)
        {
            // front
            depth_stencil->front_face.stencil_func = to_gl_comparison(dscp.front_face.stencil_func);
            depth_stencil->front_face.stencil_failop = to_gl_stencil_op(dscp.front_face.stencil_failop);
            depth_stencil->front_face.stencil_depth_failop = to_gl_stencil_op(dscp.front_face.stencil_depth_failop);
            depth_stencil->front_face.stencil_passop = to_gl_stencil_op(dscp.front_face.stencil_passop);

            //back
            depth_stencil->back_face.stencil_func = to_gl_comparison(dscp.back_face.stencil_func);
            depth_stencil->back_face.stencil_failop = to_gl_stencil_op(dscp.back_face.stencil_failop);
            depth_stencil->back_face.stencil_depth_failop = to_gl_stencil_op(dscp.back_face.stencil_depth_failop);
            depth_stencil->back_face.stencil_passop = to_gl_stencil_op(dscp.back_face.stencil_passop);
        }
    }

    void direct::renderer_set_depth_stencil_state(u32 depth_stencil_state)
    {
        s_live_state.depth_stencil_state = depth_stencil_state;
    }

    void direct::renderer_set_stencil_ref(u8 ref)
    {
        s_live_state.stencil_ref = ref;
    }

    void direct::renderer_release_shader(u32 shader_index, u32 shader_type)
    {
        resource_allocation& res = _res_pool[shader_index];
        CHECK_CALL(glDeleteShader(res.handle));

        res.handle = 0;
    }

    void direct::renderer_release_buffer(u32 buffer_index)
    {
        resource_allocation& res = _res_pool[buffer_index];
        CHECK_CALL(glDeleteBuffers(1, &res.handle));

        res.handle = 0;
    }

    void direct::renderer_release_texture(u32 texture_index)
    {
        resource_allocation& res = _res_pool[texture_index];
        CHECK_CALL(glDeleteTextures(1, &res.handle));

        res.handle = 0;
    }

    void direct::renderer_release_raster_state(u32 raster_state_index)
    {
        // no gl objects associated with raster state
    }

    void direct::renderer_release_blend_state(u32 blend_state)
    {
        resource_allocation& res = _res_pool[blend_state];

        if (res.blend_state)
        {
            // clear rtb
            memory_free(res.blend_state->render_targets);
            res.blend_state->render_targets = nullptr;

            memory_free(res.blend_state);

            res.blend_state = nullptr;
        }
    }

    void direct::renderer_release_render_target(u32 render_target)
    {
        _renderer_untrack_managed_render_target(render_target);

        resource_allocation& res = _res_pool[render_target];

        if (res.render_target.texture.handle > 0)
        {
            CHECK_CALL(glDeleteTextures(1, &res.render_target.texture.handle));
        }

        if (res.render_target.texture_msaa.handle > 0)
        {
            CHECK_CALL(glDeleteTextures(1, &res.render_target.texture_msaa.handle));
        }

        delete res.render_target.tcp;
        res.render_target.tcp = nullptr;
    }

    void direct::renderer_release_input_layout(u32 input_layout)
    {
        resource_allocation& res = _res_pool[input_layout];

        memory_free(res.input_layout);
    }

    void direct::renderer_release_sampler(u32 sampler)
    {
        resource_allocation& res = _res_pool[sampler];
        glDeleteSamplers(1, &res.sampler_object.sampler);
        glDeleteSamplers(1, &res.sampler_object.depth_sampler);
    }

    void direct::renderer_release_depth_stencil_state(u32 depth_stencil_state)
    {
        resource_allocation& res = _res_pool[depth_stencil_state];

        memory_free(res.depth_stencil);
    }

    void direct::renderer_release_clear_state(u32 clear_state)
    {
    }

    void direct::renderer_replace_resource(u32 dest, u32 src, e_renderer_resource type)
    {
        switch (type)
        {
            case RESOURCE_TEXTURE:
                direct::renderer_release_texture(dest);
                break;
            case RESOURCE_BUFFER:
                direct::renderer_release_buffer(dest);
                break;
            case RESOURCE_VERTEX_SHADER:
                direct::renderer_release_shader(dest, PEN_SHADER_TYPE_VS);
                break;
            case RESOURCE_PIXEL_SHADER:
                direct::renderer_release_shader(dest, PEN_SHADER_TYPE_PS);
                break;
            case RESOURCE_RENDER_TARGET:
                direct::renderer_release_render_target(dest);
                break;
            default:
                break;
        }

        _res_pool[dest] = _res_pool[src];
    }

    static renderer_info s_renderer_info;

    u32 direct::renderer_initialise(void*, u32, u32)
    {
        _res_pool.init(2048);

        static Str str_glsl_version;
        static Str str_gl_version;
        static Str str_gl_renderer;
        static Str str_gl_vendor;

        // todo renderer caps
        const GLubyte* glsl_version = glGetString(GL_SHADING_LANGUAGE_VERSION);
        const GLubyte* gl_version = glGetString(GL_VERSION);
        const GLubyte* gl_renderer = glGetString(GL_RENDERER);
        const GLubyte* gl_vendor = glGetString(GL_VENDOR);

        str_glsl_version = (const c8*)glsl_version;
        str_gl_version = (const c8*)gl_version;
        str_gl_renderer = (const c8*)gl_renderer;
        str_gl_vendor = (const c8*)gl_vendor;

        // version ints
        u32 major_pos = str_find(str_gl_version, ".", 0);
        u32 minor_pos = str_find(str_gl_version, ".", major_pos + 1);
        u32 major = atoi(str_substr(str_gl_version, 0, major_pos).c_str());
        u32 minor = atoi(str_substr(str_gl_version, major_pos + 1, minor_pos).c_str());

        s_renderer_info.shader_version = str_glsl_version.c_str();
        s_renderer_info.api_version = str_gl_version.c_str();
        s_renderer_info.renderer = str_gl_renderer.c_str();
        s_renderer_info.vendor = str_gl_vendor.c_str();

        s_renderer_info.renderer_cmd = "-renderer opengl";

        // gles base fbo is not 0
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &s_backbuffer_fbo);
        s_renderer_info.caps |= PEN_CAPS_VUP;

#ifndef PEN_GLES3
        // opengl caps
        s_renderer_info.caps |= PEN_CAPS_TEX_FORMAT_BC1;
        s_renderer_info.caps |= PEN_CAPS_TEX_FORMAT_BC2;
        s_renderer_info.caps |= PEN_CAPS_TEX_FORMAT_BC3;
        s_renderer_info.caps |= PEN_CAPS_GPU_TIMER;
        s_renderer_info.caps |= PEN_CAPS_DEPTH_CLAMP;
        if (major >= 4)
            s_renderer_info.caps |= PEN_CAPS_TEXTURE_CUBE_ARRAY;
        if (major >= 4 && minor >= 6)
            s_renderer_info.caps |= PEN_CAPS_COMPUTE;
#endif
        return PEN_ERR_OK;
    }

    const renderer_info& renderer_get_info()
    {
        return s_renderer_info;
    }

    void direct::renderer_shutdown()
    {
        // todo device / stray resource shutdown
    }

    const c8* renderer_get_shader_platform()
    {
        return "glsl";
    }

    bool renderer_viewport_vup()
    {
        return true;
    }

    bool renderer_depth_0_to_1()
    {
        return false;
    }
} // namespace pen
