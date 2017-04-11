#ifndef _input_h
#define _input_h

#include "definitions.h"

#define PENK_ARRAY_SIZE     512

namespace pen
{
	struct mouse_state
	{
		u32 x;
		u32 y;
		s32 wheel;
		u8 buttons[ 3 ];

	};

	void				input_set_key_down( u32 key_index );
	void				input_set_key_up( u32 key_index );

	bool				input_is_key_pressed( u32 key_index );
	bool				input_is_key_held( u32 key_index );
	bool				input_is_key_down( u32 key_index );

	void				input_set_mouse_down( u32 button_index );
	void				input_set_mouse_up( u32 button_index );
	void				input_set_mouse_pos( u32 x, u32 y );
	void				input_set_mouse_wheel( s32 wheel );

	const mouse_state&	input_get_mouse_state( );
	bool				input_is_mouse_pressed( u32 button_index );
	bool				input_is_mouse_held( u32 button_index );
	bool				input_is_mouse_down( u32 button_index );

	void				input_set_cursor_pos( u32 client_x, u32 client_y );
	void				input_show_cursor( bool show );
    
    const c8*           input_get_key_str( u32 key_index );

	//Gamepads
}

#define INPUT_PKEY_PRESS( key_index ) pen::input_is_key_pressed( key_index )
#define INPUT_PKEY( key_index ) pen::input_is_key_pressed( key_index ) || pen::input_is_key_held( key_index )
#define INPUT_PMOUSE( key_index ) pen::input_is_mouse_pressed( key_index ) || pen::input_is_mouse_held( key_index )

enum mouse_button
{
	PEN_MOUSE_L = 0,
	PEN_MOUSE_R = 1,
	PEN_MOUSE_M = 2
};

enum virtual_key
{
	PENK_LBUTTON = 0x01,
	PENK_RBUTTON = 0x02,
	PENK_CANCEL = 0x03,
	PENK_MBUTTON = 0x04,
	PENK_BACK = 0x08,
	PENK_TAB = 0x09,
	PENK_CLEAR = 0x0C,
	PENK_RETURN = 0x0D,
	PENK_SHIFT = 0x10,
	PENK_CONTROL = 0x11,
	PENK_MENU = 0x12,
	PENK_PAUSE = 0x13,
	PENK_CAPITAL = 0x14,
	PENK_ESCAPE = 0x1B,
	PENK_SPACE = 0x20,
	PENK_PRIOR = 0x21,
	PENK_NEXT = 0x22,
	PENK_END = 0x23,
	PENK_HOME = 0x24,
	PENK_LEFT = 0x25,
	PENK_UP = 0x26,
	PENK_RIGHT = 0x27,
	PENK_DOWN = 0x28,
	PENK_SELECT = 0x29,
	PENK_EXECUTE = 0x2B,
	PENK_SNAPSHOT = 0x2C,
	PENK_INSERT = 0x2D,
	PENK_DELETE = 0x2E,
	PENK_HELP = 0x2F,
	PENK_0 = 0x30,
	PENK_1 = 0x31,
	PENK_2 = 0x32,
	PENK_3 = 0x33,
	PENK_4 = 0x34,
	PENK_5 = 0x35,
	PENK_6 = 0x36,
	PENK_7 = 0x37,
	PENK_8 = 0x38,
	PENK_9 = 0x39,
	PENK_A = 0x41,
	PENK_B = 0x42,
	PENK_C = 0x43,
	PENK_D = 0x44,
	PENK_E = 0x45,
	PENK_F = 0x46,
	PENK_G = 0x47,
	PENK_H = 0x48,
	PENK_I = 0x49,
	PENK_J = 0x4A,
	PENK_K = 0x4B,
	PENK_L = 0x4C,
	PENK_M = 0x4D,
	PENK_N = 0x4E,
	PENK_O = 0x4F,
	PENK_P = 0x50,
	PENK_Q = 0x51,
	PENK_R = 0x52,
	PENK_S = 0x53,
	PENK_T = 0x54,
	PENK_U = 0x55,
	PENK_V = 0x56,
	PENK_W = 0x57,
	PENK_X = 0x58,
	PENK_Y = 0x59,
	PENK_Z = 0x5A,
	PENK_NUMPAD0 = 0x60,
	PENK_NUMPAD1 = 0x61,
	PENK_NUMPAD2 = 0x62,
	PENK_NUMPAD3 = 0x63,
	PENK_NUMPAD4 = 0x64,
	PENK_NUMPAD5 = 0x65,
	PENK_NUMPAD6 = 0x66,
	PENK_NUMPAD7 = 0x67,
	PENK_NUMPAD8 = 0x68,
	PENK_NUMPAD9 = 0x69,
	PENK_MULTIPLY = 0x6A,
	PENK_ADD = 0x6B,
	PENK_SEPARATOR = 0x6C,
	PENK_SUBTRACT = 0x6D,
	PENK_DECIMAL = 0x6E,
	PENK_DIVIDE = 0x6F,
	PENK_F1 = 0x70,
	PENK_F2 = 0x71,
	PENK_F3 = 0x72,
	PENK_F4 = 0x73,
	PENK_F5 = 0x74,
	PENK_F6 = 0x75,
	PENK_F7 = 0x76,
	PENK_F8 = 0x77,
	PENK_F9 = 0x78,
	PENK_F10 = 0x79,
	PENK_F11 = 0x7A,
	PENK_F12 = 0x7B,
	PENK_NUMLOCK = 0x90,
	PENK_SCROLL = 0x91,
	PENK_COMMAND = 0x92
};


#endif
