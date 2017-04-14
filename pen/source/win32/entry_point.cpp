#include <windows.h>

#include "structs.h"
#include "pen.h"
#include "window.h"
#include "threads.h"
#include "timer.h"
#include "audio.h"

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

	//initilaise any generic systems
	pen::timer_system_intialise( );

	renderer_params rp;
	rp.hwnd = (HWND)pen::window_get_primary_display_handle();

	pen::default_thread_info thread_info;
	thread_info.flags = pen::PEN_CREATE_AUDIO_THREAD | pen::PEN_CREATE_RENDER_THREAD;
	thread_info.render_thread_params = &rp;

	pen::threads_create_default_jobs( thread_info );

	// Main message loop
	MSG msg = { 0 };
	while ( 1 )
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		if (WM_QUIT == msg.message)
		{
			pen::threads_terminate_jobs();
			break;
		}

		Sleep( 16 );
	}

	return (INT)msg.wParam;
}