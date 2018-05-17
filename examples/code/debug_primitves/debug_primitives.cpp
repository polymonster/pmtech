#include "file_system.h"
#include "loader.h"
#include "pen.h"
#include "pen_string.h"
#include "renderer.h"
#include "timer.h"

pen::window_creation_params pen_window{
    1280,      // width
    720,       // height
    4,         // MSAA samples
    "textures" // window title / process name
};

typedef struct vertex
{
    float x, y, z, w;
} vertex;

typedef struct textured_vertex
{
    float x, y, z, w;
    float u, v;
} textured_vertex;

PEN_THREAD_RETURN pen::game_entry(void* params)
{
    f32 prev_time = pen::timer_get_time();

    // create 2 clear states one for the render target and one for the main screen, so we can see the difference
    static pen::clear_state cs = {
        0.0f, 0.0, 1.0f, 1.0f, 1.0f, PEN_CLEAR_COLOUR_BUFFER | PEN_CLEAR_DEPTH_BUFFER,
    };

    u32 clear_state = pen::renderer_create_clear_state(cs);

    // raster state
    pen::rasteriser_state_creation_params rcp;
    pen::memory_zero(&rcp, sizeof(rasteriser_state_creation_params));
    rcp.fill_mode               = PEN_FILL_SOLID;
    rcp.cull_mode               = PEN_CULL_NONE;
    rcp.depth_bias_clamp        = 0.0f;
    rcp.sloped_scale_depth_bias = 0.0f;

    u32 raster_state = pen::defer::renderer_create_rasterizer_state(rcp);

    // viewport
    pen::viewport vp = {0.0f, 0.0f, 1280.0f, 720.0f, 0.0f, 1.0f};

    // load shaders now requiring dependency on put to make loading simpler.

    put::shader_program textured_shader = put::loader_load_shader_program("textured");

    pen::texture_creation_params* tex_params   = put::loader_load_texture("data\\textures\\test_normal.dds");
    u32                           test_texture = pen::defer::renderer_create_texture2d(*tex_params);

    put::loader_free_texture(&tex_params);

    // create vertex buffer for a quad
    textured_vertex quad_vertices[] = {
        -0.5f, -0.5f, 0.5f, 1.0f, // p1
        0.0f,  0.0f,              // uv1

        -0.5f, 0.5f,  0.5f, 1.0f, // p2
        0.0f,  1.0f,              // uv2

        0.5f,  0.5f,  0.5f, 1.0f, // p3
        1.0f,  1.0f,              // uv3

        0.5f,  -0.5f, 0.5f, 1.0f, // p4
        1.0f,  0.0f,              // uv4
    };

    pen::buffer_creation_params bcp;
    bcp.usage_flags      = PEN_USAGE_DEFAULT;
    bcp.bind_flags       = PEN_BIND_VERTEX_BUFFER;
    bcp.cpu_access_flags = 0;

    bcp.buffer_size = sizeof(textured_vertex) * 4;
    bcp.data        = (void*)&quad_vertices[0];

    u32 quad_vertex_buffer = pen::defer::renderer_create_buffer(bcp);

    // create index buffer
    u16 indices[] = {0, 1, 2, 2, 3, 0};

    bcp.usage_flags      = PEN_USAGE_DEFAULT;
    bcp.bind_flags       = PEN_BIND_INDEX_BUFFER;
    bcp.cpu_access_flags = 0;
    bcp.buffer_size      = sizeof(u16) * 6;
    bcp.data             = (void*)&indices[0];

    u32 quad_index_buffer = pen::defer::renderer_create_buffer(bcp);

    // create a sampler object so we can sample a texture
    pen::sampler_creation_params scp;
    pen::memory_zero(&scp, sizeof(pen::sampler_creation_params));
    scp.filter          = PEN_FILTER_MIN_MAG_MIP_LINEAR;
    scp.address_u       = PEN_TEXTURE_ADDRESS_CLAMP;
    scp.address_v       = PEN_TEXTURE_ADDRESS_CLAMP;
    scp.address_w       = PEN_TEXTURE_ADDRESS_CLAMP;
    scp.comparison_func = PEN_COMPARISON_ALWAYS;
    scp.min_lod         = 0.0f;
    scp.max_lod         = 4.0f;

    u32 render_target_texture_sampler = defer::renderer_create_sampler(scp);

    while (1)
    {
        pen::defer::renderer_set_rasterizer_state(raster_state);

        // bind back buffer and clear
        pen::defer::renderer_set_viewport(vp);
        pen::defer::renderer_set_targets(PEN_DEFAULT_RT, PEN_DEFAULT_DS);
        pen::defer::renderer_clear(clear_state);

        // draw quad
        {
            // bind vertex layout
            pen::defer::renderer_set_input_layout(textured_shader.input_layout);

            // bind vertex buffer
            u32 stride = sizeof(textured_vertex);
            u32 offset = 0;
            pen::defer::renderer_set_vertex_buffer(quad_vertex_buffer, 0, 1, &stride, &offset);
            pen::defer::renderer_set_index_buffer(quad_index_buffer, PEN_FORMAT_R16_UINT, 0);

            // bind shaders
            pen::defer::renderer_set_shader(textured_shader.vertex_shader, PEN_SHADER_TYPE_VS);
            pen::defer::renderer_set_shader(textured_shader.pixel_shader, PEN_SHADER_TYPE_PS);

            // bind render target as texture on sampler 0
            pen::defer::renderer_set_texture(test_texture, render_target_texture_sampler, 0, PEN_SHADER_TYPE_PS);

            // draw
            pen::defer::renderer_draw_indexed(6, 0, 0, PEN_PT_TRIANGLELIST);
        }

        // present
        pen::defer::renderer_present();

        pen::defer::renderer_consume_cmd_buffer();
    }

    while (1)
    {
    }

    return PEN_THREAD_OK;
}
