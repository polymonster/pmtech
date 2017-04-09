#ifndef _input_h
#define _input_h

#include "definitions.h"
#include "input_definitions.h"

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

#endif
