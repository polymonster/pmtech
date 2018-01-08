#include "input.h"
#include "entry_point.h"
#include <atomic>

namespace pen
{
	//Keyboard and mouse
	#define KEY_PRESS 0x01
	#define KEY_HELD  0x02

	u8 keyboard_state[ PENK_ARRAY_SIZE ];
    u8 ascii_state[ PENK_ARRAY_SIZE ];

	mouse_state mouse_state_ = { 0 };

    std::atomic<bool> show_cursor = { true };

    void input_set_unicode_key_down( u32 key_index )
    {
        ascii_state[key_index] = 1;
    }
    
    void input_set_unicode_key_up( u32 key_index )
    {
        ascii_state[key_index] = 0;
    }
    
    bool input_get_unicode_key( u32 key_index )
    {
        return ascii_state[ key_index ] == 1;
    }
	
    void input_set_key_down( u32 key_index )
	{
		keyboard_state[ key_index ]++;
	}

	void input_set_key_up( u32 key_index )
	{
		keyboard_state[ key_index ] = 0;
	}

	bool input_is_key_pressed( u32 key_index )
	{
		return keyboard_state[key_index] == KEY_PRESS;
	}

	bool input_is_key_held( u32 key_index )
	{
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

	const pen::mouse_state& input_get_mouse_state( )
	{
		return mouse_state_;
	}
    
	bool input_is_mouse_pressed( u32 button_index )
	{
		return mouse_state_.buttons[ button_index ] == KEY_PRESS;
	}

	bool input_is_mouse_held( u32 button_index )
	{
		return mouse_state_.buttons[ button_index ] == KEY_PRESS;
	}

	void input_set_mouse_pos( f32 x, f32 y )
	{
		mouse_state_.x = x;
		mouse_state_.y = y;
	}

	void input_set_mouse_wheel( f32 wheel )
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
        os_set_cursor_pos( client_x, client_y );
	}

	void input_show_cursor( bool show )
	{
		show_cursor = show;
	}
    
    const c8*         input_get_key_str( u32 key_index )
    {
        switch (key_index)
        {
            case PENK_LBUTTON:      return "L button";
            case PENK_RBUTTON:      return "R button";
            case PENK_CANCEL:       return "cancel";
            case PENK_MBUTTON:      return "M button";
            case PENK_BACK:         return "back";
            case PENK_TAB:          return "tab";
            case PENK_CLEAR:        return "clear";
            case PENK_RETURN:       return "return";
            case PENK_SHIFT:        return "shift";
            case PENK_CONTROL:      return "ctrl";
            case PENK_MENU:         return "menu";
            case PENK_PAUSE:        return "pause";
            case PENK_CAPITAL:      return "caps lock";
            case PENK_ESCAPE:       return "escape";
            case PENK_SPACE:        return "space";
            case PENK_PRIOR:        return "page down";
            case PENK_NEXT:         return "page up";
            case PENK_HOME:         return "home";
            case PENK_LEFT:         return "left";
            case PENK_UP:           return "up";
            case PENK_RIGHT:        return "right";
            case PENK_DOWN:         return "down";
            case PENK_SELECT:       return "select";
            case PENK_EXECUTE:      return "execute";
            case PENK_INSERT:       return "insert";
            case PENK_SNAPSHOT:     return "screen shot";
            case PENK_DELETE:       return "delete";
            case PENK_HELP:         return "help";
            case PENK_F1:           return "f1";
            case PENK_F2:           return "f2";
            case PENK_F3:           return "f3";
            case PENK_F4:           return "f4";
            case PENK_F5:           return "f5";
            case PENK_F6:           return "f6";
            case PENK_F7:           return "f7";
            case PENK_F8:           return "f8";
            case PENK_F9:           return "f9";
            case PENK_F10:          return "f10";
            case PENK_F11:          return "f11";
            case PENK_F12:          return "f12";
            case PENK_NUMLOCK:      return "num lock";
            case PENK_SCROLL:       return "scroll";
            case PENK_COMMAND:      return "command";
        }
        
        static c8 key_char[512] = { 0 };
        if( key_char[ 2 ] == 0 )
        {
            for( s32 i = 0; i < 256; ++i )
            {
                key_char[i*2] = (c8)i;
            }
        }
        
        if( key_index < 256 )
        {
            return &key_char[ key_index*2 ];
        }
        
        return "unknown";
    }
}
