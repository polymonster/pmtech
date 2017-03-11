#include <windows.h>

#include "structs.h"
#include "window.h"
#include "input.h"

extern u32 pen_window_width;
extern u32 pen_window_height;

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
		WNDCLASSEX wcex;
		ZeroMemory(&wcex, sizeof(WNDCLASSEX));
		wcex.cbSize = sizeof(WNDCLASSEX);
		wcex.style = CS_HREDRAW | CS_VREDRAW;
		wcex.lpfnWndProc = WndProc;
		wcex.cbClsExtra = 0;
		wcex.cbWndExtra = 0;
		wcex.hInstance = wp->hinstance;
		wcex.hIcon = NULL;
		wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
		wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
		wcex.lpszMenuName = nullptr;
		wcex.lpszClassName = "PEN";
		wcex.hIconSm = NULL;

		if (!RegisterClassEx(&wcex))
			return E_FAIL;

		// Create window
		g_hinstance = wp->hinstance;

		//pass in as params
		RECT rc = { 0, 0, pen_window_width, pen_window_height };
		AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

		g_hwnd = CreateWindow(
			"PEN", "PEN RENDERER",
			WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT, 
			CW_USEDEFAULT, 
			rc.right - rc.left, rc.bottom - rc.top, 
			nullptr, nullptr, 
			wp->hinstance,
			nullptr);

		DWORD lasterror = GetLastError();

		if (!g_hwnd)
			return E_FAIL;

		ShowWindow(g_hwnd, wp->cmdshow);

		return S_OK;
	}


	//--------------------------------------------------------------------------------------
	// Called every time the application receives a message
	//--------------------------------------------------------------------------------------
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
			pen::input_set_key_down( (u32)wParam );
			break;

		case WM_KEYUP:
		case WM_SYSKEYUP:
			pen::input_set_key_up( (u32)wParam );
			break;

		case WM_LBUTTONDOWN:
		case WM_MBUTTONDOWN:
		case WM_RBUTTONDOWN:
			pen::input_set_mouse_down( (message - 0x201) / 3 );
			break;

		case WM_LBUTTONUP:
		case WM_MBUTTONUP:
		case WM_RBUTTONUP:
			pen::input_set_mouse_up( (message - 0x202) / 3 );
			break;

		case WM_MOUSEMOVE:
			pen::input_set_mouse_pos( LOWORD( lParam ) , HIWORD( lParam ) );
			break;

		case WM_MOUSEWHEEL:
		{
			s16 low_w = (s16)LOWORD( wParam ); 
			s16 hi_w = (s16)HIWORD( wParam ); 

			s16 low_l = (s16)LOWORD( lParam ); 
			s16 hi_l = (s16)HIWORD( lParam ); 

			pen::input_set_mouse_wheel( hi_w/WHEEL_DELTA );
		}
			break;

		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}

		return 0;
	}

	void* window_get_primary_display_handle()
	{
		return g_hwnd;
	}
}
