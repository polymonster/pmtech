#include "input.h"
#include "window.h"

namespace pen
{
	//Keyboard and mouse
	#define KEY_PRESS 0x01
	#define KEY_HELD  0x02
	#define KEY_ARRAY_SIZE 128

	u8 keyboard_state[ KEY_ARRAY_SIZE ];

	mouse_state mouse_state_ = { 0 };

	a_u8 show_cursor;

	void input_internal_update( )
	{
		ShowCursor( show_cursor );
	}

	void input_set_key_down( u32 key_index )
	{
		PEN_ASSERT( key_index < KEY_ARRAY_SIZE );

		keyboard_state[ key_index ]++;
	}

	void input_set_key_up( u32 key_index )
	{
		PEN_ASSERT( key_index < KEY_ARRAY_SIZE );

		keyboard_state[ key_index ] = 0;
	}

	bool input_is_key_pressed( u32 key_index )
	{
		PEN_ASSERT( key_index < KEY_ARRAY_SIZE );

		return keyboard_state[key_index] == KEY_PRESS;
	}

	bool input_is_key_held( u32 key_index )
	{
		PEN_ASSERT( key_index < KEY_ARRAY_SIZE );

		return keyboard_state[key_index] >= KEY_HELD;
	}

	void input_set_mouse_down( u32 button_index )
	{
		mouse_state_.buttons[ button_index ]++;
	}

	void input_set_mouse_up( u32 button_index )
	{
		mouse_state_.buttons[ button_index ] = 0;
	}

	pen::mouse_state* input_get_mouse_state( )
	{
		return &mouse_state_;
	}

	bool input_is_mouse_pressed( u32 button_index )
	{
		return mouse_state_.buttons[ button_index ] == KEY_PRESS;
	}

	bool input_is_mouse_held( u32 button_index )
	{
		return mouse_state_.buttons[ button_index ] == KEY_PRESS;
	}

	void input_set_mouse_pos( u32 x, u32 y )
	{
		mouse_state_.x = x;
		mouse_state_.y = y;
	}

	void input_set_mouse_wheel( s32 wheel )
	{
		mouse_state_.wheel += wheel;
	}

	bool input_is_key_down( u32 key_index )
	{
		return ( input_is_key_held( key_index ) || input_is_key_pressed( key_index ) );
	}

	bool input_is_mouse_down( u32 button_index )
	{
		return ( input_is_mouse_held( button_index ) || input_is_mouse_pressed( button_index ) );
	}

	void input_set_cursor_pos( u32 client_x, u32 client_y )
	{
		HWND hw = (HWND)pen::window_get_primary_display_handle( );
		POINT p = { client_x, client_y };

		ClientToScreen( hw, &p );
		SetCursorPos( p.x, p.y );
	}

	void input_show_cursor( u8 show )
	{
		show_cursor = show;
	}
}
