#include <windows.h>

#include "structs.h"
#include "pen.h"
#include "window.h"
#include "threads.h"
#include "timer.h"

namespace pen
{
	extern void input_internal_update();
}

//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing 
// loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
INT WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
	PSTR lpCmdLine, INT nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	window_params wp;
	wp.cmdshow = nCmdShow;
	wp.hinstance = hInstance;

	if ( pen::window_init( ( (void*)&wp ) ) )
		return 0;

	renderer_params rp;
	rp.hwnd = ( HWND )pen::window_get_primary_display_handle( );

	//initilaise any generic systems
	pen::timer_system_intialise( );

	//initialise thread sync primitives, and kick off render thread
    static bool dedicated_render_thread = true;
    pen::thread* p_renderer_thread = nullptr;
    if( dedicated_render_thread )
    {
        pen::renderer_thread_init();
        p_renderer_thread = pen::threads_create( pen::renderer_init_thread, 1024 * 1024, &rp, pen::THREAD_START_DETACHED );
        pen::renderer_wait_init();
    }
    else
    {
        pen::renderer_init(&rp);
    }

	//after renderer is initialised kick of the game thread
	pen::thread* p_game_thread	   = pen::threads_create( pen::game_entry, 1024*1024, nullptr, pen::THREAD_START_DETACHED );

	// Main message loop
	MSG msg = { 0 };
	while ( WM_QUIT != msg.message )
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

        if(!dedicated_render_thread)
        {
            pen::renderer_poll_for_jobs();
        }

		pen::input_internal_update();

		Sleep( 2 );
	}

    //todo
	//pen::renderer_destroy();

	return (INT)msg.wParam;
}