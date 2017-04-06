#include "pen.h"
#include "renderer.h"
#include "timer.h"
#include "file_system.h"
#include "pen_string.h"
#include "loader.h"
#include "debug_render.h"
#include "audio.h"
#include "dev_ui.h"


pen::window_creation_params pen_window
{
    1280,					//width
    720,					//height
    4,						//MSAA samples
    "imgui"		            //window title / process name
};

u32 clear_state_grey;
u32 raster_state_cull_back;

pen::viewport vp =
{
    0.0f, 0.0f,
    1280.0f, 720.0f,
    0.0f, 1.0f
};

void renderer_state_init( )
{
    //initialise the debug render system
    dbg::initialise();

    //create 2 clear states one for the render target and one for the main screen, so we can see the difference
    static pen::clear_state cs =
    {
        0.5f, 0.5f, 0.5f, 0.5f, 1.0f, PEN_CLEAR_COLOUR_BUFFER | PEN_CLEAR_DEPTH_BUFFER,
    };

    clear_state_grey = pen::renderer_create_clear_state( cs );

    //raster state
    pen::rasteriser_state_creation_params rcp;
    pen::memory_zero( &rcp, sizeof( pen::rasteriser_state_creation_params ) );
    rcp.fill_mode = PEN_FILL_SOLID;
    rcp.cull_mode = PEN_CULL_BACK;
    rcp.depth_bias_clamp = 0.0f;
    rcp.sloped_scale_depth_bias = 0.0f;
    rcp.depth_clip_enable = true;

    raster_state_cull_back = pen::defer::renderer_create_rasterizer_state( rcp );
}

PEN_THREAD_RETURN pen::game_entry( void* params )
{
    renderer_state_init();

    dev_ui::init();

    u32 sound_index = pen::audio_create_sound("data/audio/singing.wav");
    u32 channel_index = pen::audio_create_channel_for_sound( sound_index );
    u32 group_index = pen::audio_create_channel_group();
    
    pen::audio_add_channel_to_group( channel_index, group_index );

    pen::audio_group_set_pitch( group_index, 0.5f );
    
    pen::audio_group_set_volume( group_index, 1.0f );

    while( 1 )
    {
        dev_ui::new_frame();

        ImGui::Text("Hello World");

        pen::defer::renderer_set_rasterizer_state( raster_state_cull_back );

		static f32 renderer_time = 0.0f;

        //bind back buffer and clear
        pen::defer::renderer_set_viewport( vp );
        pen::defer::renderer_set_targets( PEN_DEFAULT_RT, PEN_DEFAULT_DS );
        pen::defer::renderer_clear( clear_state_grey );

        //dbg::print_text( 10.0f, 10.0f, vp, vec4f( 0.0f, 1.0f, 0.0f, 1.0f ), "%s", "Debug Text" );

        dbg::render_text();

		bool show_test_window = true;
		bool show_another_window = false;
		ImVec4 clear_col = ImColor(114, 144, 154);

		// 1. Show a simple window
		// Tip: if we don't call ImGui::Begin()/ImGui::End() the widgets appears in a window automatically called "Debug"
		{
			static float f = 0.0f;
			ImGui::Text("Hello, world!");
			ImGui::SliderFloat("float", &f, 0.0f, 1.0f);
			ImGui::ColorEdit3("clear color", (float*)&clear_col);
			if (ImGui::Button("Test Window")) show_test_window ^= 1;
			if (ImGui::Button("Another Window")) show_another_window ^= 1;
			ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

			ImGui::Text("Imgui render implementation time : %f", renderer_time);
		}

		// 2. Show another simple window, this time using an explicit Begin/End pair
		if (show_another_window)
		{
			ImGui::SetNextWindowSize(ImVec2(200, 100), ImGuiSetCond_FirstUseEver);
			ImGui::Begin("Another Window", &show_another_window);
			ImGui::Text("Hello");
			ImGui::End();
		}

		// 3. Show the ImGui test window. Most of the sample code is in ImGui::ShowTestWindow()
		if (show_test_window)
		{
			ImGui::SetNextWindowPos(ImVec2(650, 20), ImGuiSetCond_FirstUseEver);     // Normally user code doesn't need/want to call it because positions are saved in .ini file anyway. Here we just want to make the demo initial state a bit more friendly!
			ImGui::ShowTestWindow(&show_test_window);
		}

		f32 start_time = pen::timer_get_time();
		static f32 end_time = start_time;

        ImGui::Render();

		end_time = pen::timer_get_time();
		renderer_time = end_time - start_time;

        //present 
        pen::defer::renderer_present();

        pen::defer::renderer_consume_cmd_buffer();
        
        pen::audio_consume_command_buffer();

    }

    return PEN_THREAD_OK;
}
