#include "input.h"
#include "os.h"
#include <atomic>

namespace pen
{
// Keyboard and mouse
#define KEY_PRESS 0x01
#define KEY_HELD 0x02

    u8 keyboard_state[PK_ARRAY_SIZE];
    u8 ascii_state[PK_ARRAY_SIZE];

    mouse_state mouse_state_ = {0};

    std::atomic<bool> show_cursor = {true};

    void input_set_unicode_key_down(u32 key_index)
    {
        ascii_state[key_index] = 1;
    }

    void input_set_unicode_key_up(u32 key_index)
    {
        ascii_state[key_index] = 0;
    }

    bool input_get_unicode_key(u32 key_index)
    {
        return ascii_state[key_index] == 1;
    }

    void input_set_key_down(u32 key_index)
    {
        keyboard_state[key_index]++;
    }

    void input_set_key_up(u32 key_index)
    {
        keyboard_state[key_index] = 0;
    }

    bool input_is_key_pressed(u32 key_index)
    {
        return keyboard_state[key_index] == KEY_PRESS;
    }

    bool input_is_key_held(u32 key_index)
    {
        return keyboard_state[key_index] >= KEY_HELD;
    }

    void input_set_mouse_down(u32 button_index)
    {
        mouse_state_.buttons[button_index]++;
    }

    void input_set_mouse_up(u32 button_index)
    {
        mouse_state_.buttons[button_index] = 0;
    }

    const pen::mouse_state& input_get_mouse_state()
    {
        return mouse_state_;
    }

    bool input_is_mouse_pressed(u32 button_index)
    {
        return mouse_state_.buttons[button_index] == KEY_PRESS;
    }

    bool input_is_mouse_held(u32 button_index)
    {
        return mouse_state_.buttons[button_index] == KEY_PRESS;
    }

    void input_set_mouse_pos(f32 x, f32 y)
    {
        mouse_state_.x = x;
        mouse_state_.y = y;
    }

    void input_set_mouse_wheel(f32 wheel)
    {
        mouse_state_.wheel += wheel;
    }

    bool input_is_key_down(u32 key_index)
    {
        return (input_is_key_held(key_index) || input_is_key_pressed(key_index));
    }

    bool input_is_mouse_down(u32 button_index)
    {
        return (input_is_mouse_held(button_index) || input_is_mouse_pressed(button_index));
    }

    void input_set_cursor_pos(u32 client_x, u32 client_y)
    {
        os_set_cursor_pos(client_x, client_y);
    }

    void input_show_cursor(bool show)
    {
        show_cursor = show;
    }

    const c8* input_get_key_str(u32 key_index)
    {
        switch (key_index)
        {
            case PK_LBUTTON:
                return "L button";
            case PK_RBUTTON:
                return "R button";
            case PK_CANCEL:
                return "cancel";
            case PK_MBUTTON:
                return "M button";
            case PK_BACK:
                return "back";
            case PK_TAB:
                return "tab";
            case PK_CLEAR:
                return "clear";
            case PK_RETURN:
                return "return";
            case PK_SHIFT:
                return "shift";
            case PK_CONTROL:
                return "ctrl";
            case PK_MENU:
                return "menu";
            case PK_PAUSE:
                return "pause";
            case PK_CAPITAL:
                return "caps lock";
            case PK_ESCAPE:
                return "escape";
            case PK_SPACE:
                return "space";
            case PK_PRIOR:
                return "page down";
            case PK_NEXT:
                return "page up";
            case PK_HOME:
                return "home";
            case PK_LEFT:
                return "left";
            case PK_UP:
                return "up";
            case PK_RIGHT:
                return "right";
            case PK_DOWN:
                return "down";
            case PK_SELECT:
                return "select";
            case PK_EXECUTE:
                return "execute";
            case PK_INSERT:
                return "insert";
            case PK_SNAPSHOT:
                return "screen shot";
            case PK_DELETE:
                return "delete";
            case PK_HELP:
                return "help";
            case PK_F1:
                return "f1";
            case PK_F2:
                return "f2";
            case PK_F3:
                return "f3";
            case PK_F4:
                return "f4";
            case PK_F5:
                return "f5";
            case PK_F6:
                return "f6";
            case PK_F7:
                return "f7";
            case PK_F8:
                return "f8";
            case PK_F9:
                return "f9";
            case PK_F10:
                return "f10";
            case PK_F11:
                return "f11";
            case PK_F12:
                return "f12";
            case PK_NUMLOCK:
                return "num lock";
            case PK_SCROLL:
                return "scroll";
            case PK_COMMAND:
                return "command";
        }

        static c8 key_char[512] = {0};
        if (key_char[2] == 0)
        {
            for (s32 i = 0; i < 256; ++i)
            {
                key_char[i * 2] = (c8)i;
            }
        }

        if (key_index < 256)
        {
            return &key_char[key_index * 2];
        }

        return "unknown";
    }
} // namespace pen

extern "C"{
#include "gamepad/Gamepad.h"
#include "gamepad/Gamepad_private.c"

#ifdef __linux__
#include "gamepad/Gamepad_linux.c"
#elif _WIN32
#include "gamepad/Gamepad_windows_mm.c"
#else //macos
#include "gamepad/Gamepad_macosx.c"
#endif
}

#include "console.h"
#include "timer.h"

namespace pen
{
    static const u32 k_max_gamepads = 4;
    gamepad_state s_gamepads[k_max_gamepads] = { };
    
    void update_gamepad(Gamepad_device * device, u32 axis, u32 button)
    {
        u32 gi = device->deviceID;
        
        if(gi >= 4)
            return;
        
        s_gamepads[gi].device_id = device->deviceID;
        s_gamepads[gi].vendor_id = device->vendorID;
        s_gamepads[gi].product_id = device->productID;
        
        if(button < 16)
            s_gamepads[gi].button[button] = device->buttonStates[button];
        
        if(axis <= 64)
            s_gamepads[gi].axis[axis] = device->axisStates[axis];
    }
    
    void gamepad_attach_func(struct Gamepad_device * device, void * context)
    {
        update_gamepad(device, -1, -1);
    }
    
    void gamepad_remove_func(struct Gamepad_device * device, void * context)
    {
        PEN_LOG("Device Remove\n");
    }
    
    void gamepad_button_down_func(struct Gamepad_device * device, u32 button_id, f64 timestamp, void * context)
    {
        update_gamepad(device, -1, button_id);
    }
    
    void gamepad_button_up_func(struct Gamepad_device * device, u32 button_id, f64 timestamp, void * context)
    {
        update_gamepad(device, -1, button_id);
    }
    
    void gamepad_axis_move_func(struct Gamepad_device * device,
                                u32 axis_id, f32 value, f32 last_value, f64 timestamp, void * context)
    {
        update_gamepad(device, axis_id, -1);
    }
    
    void input_gamepad_shutdown()
    {
        Gamepad_shutdown();
    }
    
    void input_gamepad_update()
    {
        Gamepad_processEvents();
        
        // detect devices
        static u32 htimer = timer_create("gamepad_detect");
        static const f32 detect_time = 1000.0f;
        static f32 detect_timer = detect_time;
        if(detect_timer <= 0)
        {
            Gamepad_detectDevices();
            detect_timer = detect_time;
        }
        
        detect_timer -= timer_elapsed_ms(htimer);
        timer_start(htimer);
    }

    u32 input_get_num_gamepads()
    {
        return Gamepad_numDevices();
    }
    
    void input_gamepad_init()
    {
        Gamepad_init();
        
        // register call backs
        Gamepad_deviceAttachFunc(gamepad_attach_func, nullptr);
        Gamepad_deviceRemoveFunc(gamepad_remove_func, nullptr);
        Gamepad_buttonDownFunc(gamepad_button_down_func, nullptr);
        Gamepad_buttonUpFunc(gamepad_button_up_func, nullptr);
        Gamepad_axisMoveFunc(gamepad_axis_move_func, nullptr);
        
        Gamepad_detectDevices();
    }
    
    void input_get_gamepad_state(u32 device_index, gamepad_state& gs)
    {
        gs = s_gamepads[device_index];
    }
}
