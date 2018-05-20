#include "dev_ui.h"
#include "file_system.h"
#include "loader.h"
#include "memory.h"
#include "pen.h"
#include "pen_string.h"
#include "pmfx.h"
#include "renderer.h"
#include "threads.h"
#include "timer.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

using namespace put;

pen::window_creation_params pen_window{
    1280,             // width
    720,              // height
    4,                // MSAA samples
    "texture_formats" // window title / process name
};

struct typed_texture
{
    const c8* fromat;
    u32       handle;
};
const c8**     k_texture_formats;
typed_texture* k_textures     = nullptr;
u32            k_num_textures = 0;
void           load_textures()
{
    const pen::renderer_info& ri = pen::renderer_get_info();
    
    bool bc[7];
    bc[0] = ri.caps & PEN_CAPS_TEX_FORMAT_BC1;
    bc[1] = ri.caps & PEN_CAPS_TEX_FORMAT_BC2;
    bc[2] = ri.caps & PEN_CAPS_TEX_FORMAT_BC3;
    bc[3] = ri.caps & PEN_CAPS_TEX_FORMAT_BC4;
    bc[4] = ri.caps & PEN_CAPS_TEX_FORMAT_BC5;
    bc[5] = ri.caps & PEN_CAPS_TEX_FORMAT_BC6;
    bc[6] = ri.caps & PEN_CAPS_TEX_FORMAT_BC7;

    static typed_texture textures[] = {{"rgb8", put::load_texture("data/textures/formats/texfmt_rgb8.dds") },
                                       {"rgba8", put::load_texture("data/textures/formats/texfmt_rgba8.dds") },

                                       {"bc1", bc[0] ? put::load_texture("data/textures/formats/texfmt_bc1.dds") : 0 },
                                       {"bc2", bc[1] ? put::load_texture("data/textures/formats/texfmt_bc2.dds") : 0 },
                                       {"bc3", bc[2] ? put::load_texture("data/textures/formats/texfmt_bc3.dds") : 0 },

                                       { "bc4", bc[3] ? put::load_texture("data/textures/formats/texfmt_bc4.dds") : 0 },
                                       { "bc5", bc[4] ? put::load_texture("data/textures/formats/texfmt_bc5.dds") : 0 },

                                       // incorrect in d3d and visual studio (nvcompress issue?)
                                       { "bc6", bc[5] ? put::load_texture("data/textures/formats/texfmt_bc6.dds") : 0 },
                                       { "bc7", bc[6] ? put::load_texture("data/textures/formats/texfmt_bc7.dds") : 0 },

                                       {"bc1n", bc[0] ? put::load_texture("data/textures/formats/texfmt_bc1n.dds") : 0 },
                                       {"bc3n", bc[2] ? put::load_texture("data/textures/formats/texfmt_bc3n.dds") : 0 }};

    k_textures     = &textures[0];
    k_num_textures = PEN_ARRAY_SIZE(textures);

    k_texture_formats = new const c8*[k_num_textures];
    for (int i = 0; i < k_num_textures; ++i)
        k_texture_formats[i] = k_textures[i].fromat;
}

void texture_formats_ui()
{
    bool opened = true;
    ImGui::Begin("Texture Formats", &opened, ImGuiWindowFlags_AlwaysAutoResize);

    static s32 current_type = 0;
    ImGui::Combo("Format", &current_type, k_texture_formats, k_num_textures);

    if(k_textures[current_type].handle)
    {
        texture_info info;
        get_texture_info(k_textures[current_type].handle, info);
        ImGui::Image((void*)&k_textures[current_type].handle, ImVec2(info.width, info.height));
        
        ImVec2 mip_size = ImVec2(info.width, info.height);
        for (u32 i = 0; i < info.num_mips; ++i)
        {
            mip_size.x *= 0.5f;
            mip_size.y *= 0.5f;
            
            mip_size.x = std::max<f32>(mip_size.x, 1.0f);
            mip_size.y = std::max<f32>(mip_size.y, 1.0f);
            
            ImGui::SameLine();
            ImGui::Image((void*)&k_textures[current_type].handle, mip_size);
        }
    }
    else
    {
        ImGui::Text("Unsupported on this platform");
    }

    ImGui::End();
}

PEN_TRV pen::user_entry(void* params)
{
    // unpack the params passed to the thread and signal to the engine it ok to proceed
    pen::job_thread_params* job_params    = (pen::job_thread_params*)params;
    pen::job*               p_thread_info = job_params->job_info;
    pen::thread_semaphore_signal(p_thread_info->p_sem_continue, 1);

    dev_ui::init();

    // create 2 clear states one for the render target and one for the main screen, so we can see the difference
    static pen::clear_state cs = {
        0.5f, 0.5f, 0.5f, 1.0f, 1.0f, 0x00, PEN_CLEAR_COLOUR_BUFFER | PEN_CLEAR_DEPTH_BUFFER,
    };

    u32 clear_state = pen::renderer_create_clear_state(cs);

    // viewport
    pen::viewport vp = {0.0f, 0.0f, 1280.0f, 720.0f, 0.0f, 1.0f};

    load_textures();

    while (1)
    {
        // bind back buffer and clear
        pen::renderer_set_viewport(vp);
        pen::renderer_set_scissor_rect(rect{vp.x, vp.y, vp.width, vp.height});
        pen::renderer_set_targets(PEN_BACK_BUFFER_COLOUR, PEN_BACK_BUFFER_DEPTH);
        pen::renderer_clear(clear_state);

        dev_ui::new_frame();

        texture_formats_ui();

        put::dev_ui::render();

        // present
        pen::renderer_present();

        pen::renderer_consume_cmd_buffer();

        // msg from the engine we want to terminate
        if (pen::thread_semaphore_try_wait(p_thread_info->p_sem_exit))
        {
            break;
        }
    }

    // clean up mem here
    pen::renderer_consume_cmd_buffer();

    // signal to the engine the thread has finished
    pen::thread_semaphore_signal(p_thread_info->p_sem_terminated, 1);

    return PEN_THREAD_OK;
}
