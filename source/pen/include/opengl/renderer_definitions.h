// renderer_definitions.h
// Copyright 2014 - 2019 Alex Dixon.
// License: https://github.com/polymonster/pmtech/blob/master/license.md

#ifndef _renderer_definitions_h
#define _renderer_definitions_h

#define PEN_RENDERER_OPENGL

#if __APPLE__
#include "TargetConditionals.h"
#if TARGET_OS_IPHONE
#define PEN_GLES3
#endif
#endif
#ifdef PEN_GLES3
#include <OpenGLES/ES3/gl.h>
#include <OpenGLES/ES3/glext.h>
// for portability with regular gl
// mark unsupported features null
#define GL_FILL 0x00               // gl fill is the only polygon mode on gles3
#define GL_LINE 0x00               // gl line (wireframe) usupported
#define GL_GEOMETRY_SHADER 0x00    // gl geometry shader unsupported
#define GL_TEXTURE_COMPRESSED 0x00 //
#define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT 0x00
#define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT 0x00
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT 0x00
// remap unsupported stuff for rough equivalent
#define GL_CLAMP_TO_BORDER GL_CLAMP_TO_EDGE
#define GL_SRC1_COLOR GL_SRC_COLOR
#define GL_ONE_MINUS_SRC1_COLOR GL_ONE_MINUS_SRC_COLOR
#define GL_SRC1_ALPHA GL_SRC_ALPHA
#define GL_ONE_MINUS_SRC1_ALPHA GL_ONE_MINUS_SRC_ALPHA/
#define GL_TEXTURE_2D_MULTISAMPLE GL_TEXTURE_2D
#define glClearDepth glClearDepthf // gl es has these type suffixes
// gles does not support base vertex offset assert when b is > 0.. rethink how you are rendering stuff
#define glDrawElementsBaseVertex(p, i, f, o, b) glDrawElements(p, i, f, o)
#define glDrawElementsInstancedBaseVertex(p, i, f, o, c, b) glDrawElementsInstanced(p, i, f, o, c)
#define glDrawBuffer
#define glTexImage2DMultisample(a1, a2, a3, a4, a5, a6) PEN_ASSERT(0)
#else
#ifdef __linux__
#include "GL/glew.h"
#elif _WIN32
#define GLEW_STATIC
#include "GL/glew.h"
#include "GL/wglew.h"
#else // osx
#include <OpenGL/gl3.h>
#include <OpenGL/gl3ext.h>
#endif
#endif

enum null_values
{
    PEN_NULL_DEPTH_BUFFER = -1,
    PEN_NULL_COLOUR_BUFFER = -1,
    PEN_NULL_PIXEL_SHADER = -1,
};

enum shader_type
{
    PEN_SHADER_TYPE_VS,
    PEN_SHADER_TYPE_PS,
    PEN_SHADER_TYPE_GS,
    PEN_SHADER_TYPE_SO,
    PEN_SHADER_TYPE_CS
};

enum fill_mode
{
    PEN_FILL_SOLID,
    PEN_FILL_WIREFRAME
};

enum cull_mode
{
    PEN_CULL_NONE,
    PEN_CULL_FRONT,
    PEN_CULL_BACK
};

enum default_targets
{
    PEN_BACK_BUFFER_COLOUR = 0,
    PEN_BACK_BUFFER_DEPTH = 0
};

enum texture_format
{
    // integer
    PEN_TEX_FORMAT_BGRA8_UNORM,
    PEN_TEX_FORMAT_RGBA8_UNORM,
    PEN_TEX_FORMAT_D24_UNORM_S8_UINT,

    // floating point
    PEN_TEX_FORMAT_R32G32B32A32_FLOAT,
    PEN_TEX_FORMAT_R32_FLOAT,
    PEN_TEX_FORMAT_R16G16B16A16_FLOAT,
    PEN_TEX_FORMAT_R16_FLOAT,
    PEN_TEX_FORMAT_R32_UINT,
    PEN_TEX_FORMAT_R8_UNORM,
    PEN_TEX_FORMAT_R32G32_FLOAT,

    // bc compressed
    PEN_TEX_FORMAT_BC1_UNORM,
    PEN_TEX_FORMAT_BC2_UNORM,
    PEN_TEX_FORMAT_BC3_UNORM,
    PEN_TEX_FORMAT_BC4_UNORM,
    PEN_TEX_FORMAT_BC5_UNORM
};

enum clear_bits
{
    PEN_CLEAR_COLOUR_BUFFER = 1<<0,
    PEN_CLEAR_DEPTH_BUFFER = 1<<1,
    PEN_CLEAR_STENCIL_BUFFER = 1<<2,
};

enum input_classification
{
    PEN_INPUT_PER_VERTEX,
    PEN_INPUT_PER_INSTANCE
};

enum primitive_topology
{
    PEN_PT_POINTLIST,
    PEN_PT_LINELIST,
    PEN_PT_LINESTRIP,
    PEN_PT_TRIANGLELIST,
    PEN_PT_TRIANGLESTRIP
};

enum vertex_format
{
    PEN_VERTEX_FORMAT_FLOAT1,
    PEN_VERTEX_FORMAT_FLOAT2,
    PEN_VERTEX_FORMAT_FLOAT3,
    PEN_VERTEX_FORMAT_FLOAT4,
    PEN_VERTEX_FORMAT_UNORM4,
    PEN_VERTEX_FORMAT_UNORM2,
    PEN_VERTEX_FORMAT_UNORM1
};

enum index_buffer_format
{
    PEN_FORMAT_R16_UINT,
    PEN_FORMAT_R32_UINT
};

enum usage
{
    PEN_USAGE_DEFAULT,   // gpu read and write, d3d can updatesubresource with usage default
    PEN_USAGE_IMMUTABLE, // gpu read only
    PEN_USAGE_DYNAMIC,   // dynamic
    PEN_USAGE_STAGING    // cpu access
};

enum bind_flags
{
    PEN_BIND_SHADER_RESOURCE = 1 << 0,
    PEN_BIND_VERTEX_BUFFER = 1 << 1,
    PEN_BIND_INDEX_BUFFER = 1 << 2,
    PEN_BIND_CONSTANT_BUFFER = 1 << 3,
    PEN_BIND_RENDER_TARGET = 1 << 5,
    PEN_BIND_DEPTH_STENCIL = 1 << 6,
    PEN_BIND_SHADER_WRITE = 1 << 7,
    PEN_STREAM_OUT_VERTEX_BUFFER = 1 << 8 // needs renaming
};

enum cpu_access_flags
{
    PEN_CPU_ACCESS_WRITE = 1<<0,
    PEN_CPU_ACCESS_READ = 1<<1
};

enum texture_address_mode
{
    PEN_TEXTURE_ADDRESS_WRAP,
    PEN_TEXTURE_ADDRESS_MIRROR,
    PEN_TEXTURE_ADDRESS_CLAMP,
    PEN_TEXTURE_ADDRESS_BORDER,
    PEN_TEXTURE_ADDRESS_MIRROR_ONCE
};

enum filter_mode
{
    PEN_FILTER_MIN_MAG_MIP_LINEAR,
    PEN_FILTER_MIN_MAG_MIP_POINT,
    PEN_FILTER_LINEAR,
    PEN_FILTER_POINT
};

enum comparison
{
    PEN_COMPARISON_NEVER,
    PEN_COMPARISON_LESS,
    PEN_COMPARISON_EQUAL,
    PEN_COMPARISON_LESS_EQUAL,
    PEN_COMPARISON_GREATER,
    PEN_COMPARISON_NOT_EQUAL,
    PEN_COMPARISON_GREATER_EQUAL,
    PEN_COMPARISON_ALWAYS
};

enum blending_factor : s32
{
    PEN_BLEND_ZERO = GL_ZERO,
    PEN_BLEND_ONE = GL_ONE,
    PEN_BLEND_SRC_COLOR = GL_SRC_COLOR,
    PEN_BLEND_INV_SRC_COLOR = GL_ONE_MINUS_SRC_COLOR,
    PEN_BLEND_SRC_ALPHA = GL_SRC_ALPHA,
    PEN_BLEND_INV_SRC_ALPHA = GL_ONE_MINUS_SRC_ALPHA,
    PEN_BLEND_DEST_ALPHA = GL_DST_ALPHA,
    PEN_BLEND_INV_DEST_ALPHA = GL_ONE_MINUS_DST_ALPHA,
    PEN_BLEND_DEST_COLOR = GL_DST_COLOR,
    PEN_BLEND_INV_DEST_COLOR = GL_ONE_MINUS_DST_COLOR,
    PEN_BLEND_SRC_ALPHA_SAT = GL_SRC_ALPHA_SATURATE,
    PEN_BLEND_BLEND_FACTOR = GL_CONSTANT_COLOR,
    PEN_BLEND_INV_BLEND_FACTOR = GL_ONE_MINUS_CONSTANT_COLOR,
    PEN_BLEND_SRC1_COLOR = GL_SRC1_COLOR,
    PEN_BLEND_INV_SRC1_COLOR = GL_ONE_MINUS_SRC1_COLOR,
    PEN_BLEND_SRC1_ALPHA = GL_SRC1_ALPHA,
    PEN_BLEND_INV_SRC1_ALPHA = GL_ONE_MINUS_SRC1_ALPHA
};

enum blend_op : s32
{
    PEN_BLEND_OP_ADD = GL_FUNC_ADD,
    PEN_BLEND_OP_SUBTRACT = GL_FUNC_SUBTRACT,
    PEN_BLEND_OP_REV_SUBTRACT = GL_FUNC_REVERSE_SUBTRACT,
    PEN_BLEND_OP_MIN = GL_MIN,
    PEN_BLEND_OP_MAX = GL_MAX
};

enum stencil_op : s32
{
    PEN_STENCIL_OP_KEEP = GL_KEEP,
    PEN_STENCIL_OP_REPLACE = GL_REPLACE,
    PEN_STENCIL_OP_ZERO = GL_ZERO,
    PEN_STENCIL_OP_INCR_SAT = GL_INCR,
    PEN_STENCIL_OP_DECR_SAT = GL_DECR,
    PEN_STENCIL_OP_INVERT = GL_INVERT,
    PEN_STENCIL_OP_INCR = GL_INCR_WRAP,
    PEN_STENCIL_OP_DECR = GL_DECR_WRAP
};
#endif
