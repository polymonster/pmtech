#include "pen.h"
#include "threads.h"
#include "memory.h"
#include "renderer.h"
#include "timer.h"
#include "file_system.h"
#include "pen_string.h"
#include "loader.h"
#include "pmfx.h"
#include "dev_ui.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

using namespace put;

pen::window_creation_params pen_window
{
    1280,					//width
    720,					//height
    4,						//MSAA samples
    "texture_formats"		//window title / process name
};

struct typed_texture
{
	const c8*	fromat;
	u32			handle;
};
const c8**		k_texture_formats;
typed_texture*	k_textures = nullptr;
u32				k_num_textures = 0;
void load_textures()
{
	static typed_texture textures[] =
	{
		{ "rgb8", put::load_texture("data/textures/formats/texfmt_rgb8.dds") },
		{ "rgba8", put::load_texture("data/textures/formats/texfmt_rgba8.dds") },
        
		{ "bc1", put::load_texture("data/textures/formats/texfmt_bc1.dds") },
		{ "bc2", put::load_texture("data/textures/formats/texfmt_bc2.dds") },
		{ "bc3", put::load_texture("data/textures/formats/texfmt_bc3.dds") },
		
        //unsupported on gl but work on d3d - todo renderer caps
        //{ "bc4", put::load_texture("data/textures/formats/texfmt_bc4.dds") },
		//{ "bc5", put::load_texture("data/textures/formats/texfmt_bc5.dds") },

        //incorrect in d3d and visual studio (nvcompress issue?)
		//{ "bc6", put::load_texture("data/textures/formats/texfmt_bc6.dds") },
		//{ "bc7", put::load_texture("data/textures/formats/texfmt_bc7.dds") },
		//{ "luminance", put::load_texture("data/textures/formats/texfmt_lumi.dds") },

		{ "bc1n", put::load_texture("data/textures/formats/texfmt_bc1n.dds") },
		{ "bc3n", put::load_texture("data/textures/formats/texfmt_bc3n.dds") }
	};

	k_textures = &textures[0];
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

	texture_info info;
	get_texture_info(k_textures[current_type].handle, info);
	ImGui::Image((void*)&k_textures[current_type].handle, ImVec2(info.width, info.height) );

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

	ImGui::End();
}

PEN_TRV pen::user_entry( void* params )
{
    //unpack the params passed to the thread and signal to the engine it ok to proceed
    pen::job_thread_params* job_params = (pen::job_thread_params*)params;
    pen::job* p_thread_info = job_params->job_info;
    pen::thread_semaphore_signal(p_thread_info->p_sem_continue, 1);
    
	dev_ui::init();

    //create 2 clear states one for the render target and one for the main screen, so we can see the difference
    static pen::clear_state cs =
    {
        0.5f, 0.5f, 0.5f, 1.0f, 1.0f, 0x00, PEN_CLEAR_COLOUR_BUFFER | PEN_CLEAR_DEPTH_BUFFER,
    };

    u32 clear_state = pen::renderer_create_clear_state( cs );

    //viewport
    pen::viewport vp =
    {
        0.0f, 0.0f,
        1280.0f, 720.0f,
        0.0f, 1.0f
    };

	load_textures();

    while( 1 )
    {
		//bind back buffer and clear
		pen::renderer_set_viewport(vp);
        pen::renderer_set_scissor_rect( rect{ vp.x, vp.y, vp.width, vp.height} );
		pen::renderer_set_targets(PEN_BACK_BUFFER_COLOUR, PEN_BACK_BUFFER_DEPTH);
		pen::renderer_clear(clear_state);

		dev_ui::new_frame();

		texture_formats_ui();
 
		put::dev_ui::render();

        //present 
        pen::renderer_present();

        pen::renderer_consume_cmd_buffer();

        //msg from the engine we want to terminate
        if( pen::thread_semaphore_try_wait( p_thread_info->p_sem_exit ) )
        {
            break;
        }
    }
    
    //clean up mem here
    pen::renderer_consume_cmd_buffer();
    
    //signal to the engine the thread has finished
    pen::thread_semaphore_signal( p_thread_info->p_sem_terminated, 1);
    
    return PEN_THREAD_OK;
}
