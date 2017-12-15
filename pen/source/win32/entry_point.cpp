#include <windows.h>

#include "structs.h"
#include "pen.h"
#include "entry_point.h"
#include "threads.h"
#include "timer.h"
#include "audio.h"
#include "definitions.h"
#include "input.h"
#include "pen.h"
#include "pen_string.h"

extern a_u8 g_window_resize;
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

extern pen::window_creation_params pen_window;
namespace pen
{
	//--------------------------------------------------------------------------------------
	// Forward Declarations
	//-------------------------------------------------------------------------------------
	LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);

	//--------------------------------------------------------------------------------------
	// Structs
	//--------------------------------------------------------------------------------------
	typedef struct window_params
	{
		HINSTANCE hinstance;
		int		  cmdshow;
	} window_params;

	//--------------------------------------------------------------------------------------
	// Globals
	//--------------------------------------------------------------------------------------
	HWND           g_hwnd = nullptr;
	HINSTANCE	   g_hinstance = nullptr;

	//--------------------------------------------------------------------------------------
	// window init
	//--------------------------------------------------------------------------------------
	u32 window_init(void* params)
	{
		window_params* wp = (window_params*)params;

		// Register class
		WNDCLASSEXA wcex;
		ZeroMemory(&wcex, sizeof(WNDCLASSEXA));
		wcex.cbSize = sizeof(WNDCLASSEXA);
		wcex.style = CS_HREDRAW | CS_VREDRAW;
		wcex.lpfnWndProc = WndProc;
		wcex.cbClsExtra = 0;
		wcex.cbWndExtra = 0;
		wcex.hInstance = wp->hinstance;
		wcex.hIcon = NULL;
		wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
		wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
		wcex.lpszMenuName = nullptr;
		wcex.lpszClassName = pen_window.window_title;
		wcex.hIconSm = NULL;

		if (!RegisterClassExA(&wcex))
			return E_FAIL;

		// Create window
		g_hinstance = wp->hinstance;

		//pass in as params
		RECT rc = { 0, 0, (LONG)pen_window.width, (LONG)pen_window.height };
		AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

		RECT desktop_rect;
		GetClientRect(GetDesktopWindow(), &desktop_rect);

		LONG screen_mid_x = (desktop_rect.right - desktop_rect.left) / 2;
		LONG screen_mid_y = (desktop_rect.bottom - desktop_rect.top) / 2;

		LONG half_window_x = (rc.right - rc.left) / 2;
		LONG half_window_y = (rc.bottom - rc.top) / 2;

		g_hwnd = CreateWindowA(
			pen_window.window_title, pen_window.window_title,
			WS_OVERLAPPEDWINDOW,
			screen_mid_x - half_window_x,
			screen_mid_y - half_window_y,
			rc.right - rc.left,
			rc.bottom - rc.top,
			nullptr, nullptr,
			wp->hinstance,
			nullptr);

		DWORD lasterror = GetLastError();

		if (!g_hwnd)
			return E_FAIL;

		//console
		AllocConsole();
		freopen("CONIN$", "r", stdin);
		freopen("CONOUT$", "w", stdout);
		freopen("CONOUT$", "w", stderr);

		ShowWindow(g_hwnd, wp->cmdshow);

		SetForegroundWindow(g_hwnd);

		return S_OK;
	}


	//--------------------------------------------------------------------------------------
	// Called every time the application receives a message
	//--------------------------------------------------------------------------------------
	void set_unicode_key(u32 key_index, bool down )
	{
		wchar_t buff[10];

		BYTE keyState[256] = { 0 };

		int result = ToUnicodeEx(
			key_index,
			MapVirtualKey(key_index, MAPVK_VK_TO_VSC),
			keyState,
			buff,
			_countof(buff),
			0,
			NULL);

		u32 unicode = buff[0];

		if (unicode > 511)
			return;

		if( down )
			pen::input_set_unicode_key_down(unicode);
		else
			pen::input_set_unicode_key_up(unicode);
	}

	LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		PAINTSTRUCT ps;
		HDC hdc;

		switch (message)
		{
		case WM_PAINT:
			hdc = BeginPaint(hWnd, &ps);
			EndPaint(hWnd, &ps);
			break;

		case WM_DESTROY:
			PostQuitMessage(0);
			break;

		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
			pen::input_set_key_down((u32)wParam);
			set_unicode_key((u32)wParam, true);
			break;

		case WM_KEYUP:
		case WM_SYSKEYUP:
			pen::input_set_key_up((u32)wParam);
			set_unicode_key((u32)wParam, false);
			break;

		case WM_LBUTTONDOWN:
		case WM_MBUTTONDOWN:
		case WM_RBUTTONDOWN:
			pen::input_set_mouse_down((message - 0x201) / 3);
			break;

		case WM_LBUTTONUP:
		case WM_MBUTTONUP:
		case WM_RBUTTONUP:
			pen::input_set_mouse_up((message - 0x202) / 3);
			break;

		case WM_MOUSEMOVE:
			pen::input_set_mouse_pos(LOWORD(lParam), HIWORD(lParam));
			break;

		case WM_MOUSEWHEEL:
		{
			s16 low_w = (s16)LOWORD(wParam);
			s16 hi_w = (s16)HIWORD(wParam);

			s16 low_l = (s16)LOWORD(lParam);
			s16 hi_l = (s16)HIWORD(lParam);

			pen::input_set_mouse_wheel(hi_w / WHEEL_DELTA);
		}
		break;

        case WM_SIZE:
        {
            s16 lo = LOWORD( wParam );

            if (lo == SIZE_MINIMIZED)
                break;

            if (g_window_resize == 0)
            {
                pen_window.width = LOWORD( lParam );
                pen_window.height = HIWORD( lParam );

                g_window_resize = 1;
            }
        }
        break;

		default:
			return DefWindowProcA(hWnd, message, wParam, lParam);
		}

		return 0;
	}

	void* window_get_primary_display_handle()
	{
		return g_hwnd;
	}

	void os_set_cursor_pos(u32 client_x, u32 client_y)
	{
		HWND hw = (HWND)pen::window_get_primary_display_handle();
		POINT p = { (LONG)client_x, (LONG)client_y };

		ClientToScreen(hw, &p);
		SetCursorPos(p.x, p.y);
	}

	void os_show_cursor(bool show)
	{
		ShowCursor(show);
	}
}
