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

	//initilaise anygeneric systems
	pen::timer_system_intialise( );

	//initialise thread sync primitives, and kick off render thread
    pen::renderer_thread_init( );
	pen::thread* p_renderer_thread = pen::threads_create( pen::renderer_init, 1024*1024, &rp, 0 );
	pen::renderer_wait_init( );
	
	//after renderer is initialised kick of the game thread
	pen::thread* p_game_thread	   = pen::threads_create( pen::game_entry, 1024*1024, NULL, 0 );

	// Main message loop
	MSG msg = { 0 };
	while ( WM_QUIT != msg.message )
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		pen::input_internal_update();

		Sleep( 2 );
	}

	pen::threads_suspend( p_renderer_thread );
	pen::threads_suspend( p_game_thread );

	//pen::renderer_destroy();

	return (INT)msg.wParam;
}