#include "data_struct.h"
#include "console.h"
#include "timer.h"
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

// Gamepad ------------------------------------------------------------------------------------------------------------------

extern "C" {
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

#define TRIGGER_X360 1024

namespace
{
    struct device_mapping
    {
        u32 vendor_id;
        u32 product_id;
        u32 button_map[PGP_MAX_BUTTONS];
        u32 axes_map[PGP_MAX_AXIS];
    };
    
    pen::raw_gamepad_state  s_raw_gamepads[PGP_MAX_GAMEPADS] = {};
    pen::gamepad_state      s_gamepads[PGP_MAX_GAMEPADS] = {};
    device_mapping*         s_device_maps = nullptr;
    
    void init_map(device_mapping& map)
    {
        for(u32 b = 0; b < PGP_MAX_BUTTONS; ++b)
            map.button_map[b] = PEN_INVALID_HANDLE;
        
        for(u32 a = 0; a < PGP_MAX_AXIS; ++a)
            map.axes_map[a] = PEN_INVALID_HANDLE;
    }
    
    void init_gamepad_values(pen::gamepad_state& gs)
    {
        for(u32 b = 0; b < PGP_BUTTON_NUM; ++b)
            gs.button[b] = 0;
        
        for(u32 a = 0; a < PGP_AXIS_NUM; ++a)
            gs.axis[a] = 0.0f;
        
        gs.axis[PGP_AXIS_LTRIGGER] = -1.0f;
        gs.axis[PGP_AXIS_RTRIGGER] = -1.0f;
    }
    
    void init_gamepad_mappings()
    {
        // No Mapping
        device_mapping no;
        no.vendor_id = 0;
        no.product_id = 0;
        init_map(no);
        for(u32 i = 0; i < PGP_BUTTON_NUM; ++i)
            no.button_map[i] = i;
        
        for(u32 i = 0; i < PGP_AXIS_NUM; ++i)
            no.axes_map[i] = i;
        
        sb_push(s_device_maps, no);
        
        // DS4
        device_mapping ps4;
        init_map(ps4);
        ps4.vendor_id = 1356;
        ps4.product_id = 1476;
        ps4.button_map[0] = PGP_BUTTON_X; // SQUARE
        ps4.button_map[1] = PGP_BUTTON_A; // X
        ps4.button_map[2] = PGP_BUTTON_B; // CIRCLE
        ps4.button_map[3] = PGP_BUTTON_Y; // TRIANGLE
        ps4.button_map[4] = PGP_BUTTON_L1;
        ps4.button_map[5] = PGP_BUTTON_R1;
        ps4.button_map[8] = PGP_BUTTON_BACK;
        ps4.button_map[9] = PGP_BUTTON_START;
        ps4.button_map[10] = PGP_BUTTON_L3;
        ps4.button_map[11] = PGP_BUTTON_R3;
        ps4.button_map[12] = PGP_BUTTON_PLATFORM;
        ps4.button_map[13] = PGP_BUTTON_TOUCH_PAD;
        ps4.axes_map[0] = PGP_AXIS_LEFT_STICK_X;
        ps4.axes_map[1] = PGP_AXIS_LEFT_STICK_Y;
        ps4.axes_map[2] = PGP_AXIS_RIGHT_STICK_X;
        ps4.axes_map[3] = PGP_AXIS_RIGHT_STICK_Y;
        ps4.axes_map[4] = PGP_DPAD_X;
        ps4.axes_map[5] = PGP_DPAD_Y;
        ps4.axes_map[7] = PGP_AXIS_LTRIGGER;
        ps4.axes_map[8] = PGP_AXIS_RTRIGGER;
        sb_push(s_device_maps, ps4);
        
        // Xbox 360
        device_mapping x360;
        init_map(x360);
        x360.vendor_id = 1118;
        x360.product_id = 654;
        x360.button_map[0] = PGP_BUTTON_A;
        x360.button_map[1] = PGP_BUTTON_B;
        x360.button_map[2] = PGP_BUTTON_X;
        x360.button_map[3] = PGP_BUTTON_Y;
        x360.button_map[4] = PGP_BUTTON_L1;
        x360.button_map[5] = PGP_BUTTON_R1;
        x360.button_map[6] = PGP_BUTTON_BACK;
        x360.button_map[7] = PGP_BUTTON_START;
        x360.button_map[8] = PGP_BUTTON_L3;
        x360.button_map[9] = PGP_BUTTON_R3;
        x360.axes_map[0] = PGP_AXIS_LEFT_STICK_X;
        x360.axes_map[1] = PGP_AXIS_LEFT_STICK_Y;
        x360.axes_map[2] = TRIGGER_X360;
        x360.axes_map[3] = PGP_AXIS_RIGHT_STICK_X;
        x360.axes_map[4] = PGP_AXIS_RIGHT_STICK_Y;
        x360.axes_map[5] = PGP_DPAD_X;
        x360.axes_map[6] = PGP_DPAD_Y;
        sb_push(s_device_maps, x360);
    }

    void map_button(u32 gamepad, u32 button)
    {
        u32 gi = gamepad;
        u32 mapping = s_raw_gamepads[gi].mapping;
        
        u32 rb = s_device_maps[mapping].button_map[button];
        if(rb == PEN_INVALID_HANDLE)
            return;
        
        s_gamepads[gi].button[rb] = s_raw_gamepads[gi].button[button];
    }
    
    void map_axis(u32 gamepad, u32 axis)
    {
        u32 gi = gamepad;
        u32 mapping = s_raw_gamepads[gi].mapping;
        
        u32 ra = s_device_maps[mapping].axes_map[axis];
        if(ra == PEN_INVALID_HANDLE)
            return;
        
        if(ra == TRIGGER_X360)
        {
            f32 raw = s_raw_gamepads[gi].axis[axis];
            if(raw < 0.0f)
            {
                s_gamepads[gi].axis[PGP_AXIS_LTRIGGER] = fabs(raw) * 2.0f - 1.0f;
            }
            else
            {
                s_gamepads[gi].axis[PGP_AXIS_RTRIGGER] = fabs(raw) * 2.0f - 1.0f;
            }
        }
        else
        {
            s_gamepads[gi].axis[ra] = s_raw_gamepads[gi].axis[axis];
        }
    }
    
    void update_gamepad(Gamepad_device* device, u32 axis, u32 button)
    {
        u32 gi = device->deviceID;
        
        if (gi >= PGP_MAX_GAMEPADS)
            return;
        
        s_raw_gamepads[gi].device_id = device->deviceID;
        s_raw_gamepads[gi].vendor_id = device->vendorID;
        s_raw_gamepads[gi].product_id = device->productID;
        
        if (button < PGP_MAX_BUTTONS)
            s_raw_gamepads[gi].button[button] = device->buttonStates[button];
        
        if (axis <= PGP_MAX_AXIS)
            s_raw_gamepads[gi].axis[axis] = device->axisStates[axis];
    }
}
namespace pen
{
    void gamepad_attach_func(struct Gamepad_device* device, void* context)
    {
        update_gamepad(device, -1, -1);
        
        // find mapping for buttons and axes
        u32 gi = device->deviceID;
        
        // init vals
        init_gamepad_values(s_gamepads[gi]);
        
        u32 num_maps = sb_count(s_device_maps);
        for(u32 i = 0; i < num_maps; ++i)
        {
            if(s_raw_gamepads[gi].vendor_id == s_device_maps[i].vendor_id)
            {
                if(s_raw_gamepads[gi].product_id == s_device_maps[i].product_id)
                {
                    s_raw_gamepads[gi].mapping = i;
                    break;
                }
            }
        }
    }

    void gamepad_remove_func(struct Gamepad_device* device, void* context)
    {
        PEN_LOG("Device Remove\n");
    }

    void gamepad_button_down_func(struct Gamepad_device* device, u32 button_id, f64 timestamp, void* context)
    {
        update_gamepad(device, -1, button_id);
        map_button(device->deviceID, button_id);
    }

    void gamepad_button_up_func(struct Gamepad_device* device, u32 button_id, f64 timestamp, void* context)
    {
        update_gamepad(device, -1, button_id);
        map_button(device->deviceID, button_id);
    }

    void gamepad_axis_move_func(struct Gamepad_device* device, u32 axis_id, f32 value, f32 last_value, f64 timestamp,
                                void* context)
    {
        update_gamepad(device, axis_id, -1);
        map_axis(device->deviceID, axis_id);
    }

    void input_gamepad_shutdown()
    {
        Gamepad_shutdown();
    }

    void input_gamepad_update()
    {
        Gamepad_processEvents();

        // detect devices
        static u32       htimer = timer_create("gamepad_detect");
        static const f32 detect_time = 1000.0f;
        static f32       detect_timer = detect_time;
        if (detect_timer <= 0)
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
        
        init_gamepad_mappings();

        Gamepad_detectDevices();
    }

    void input_get_gamepad_state(u32 device_index, gamepad_state& gs)
    {
        gs = s_gamepads[device_index];
    }
    
    void input_get_raw_gamepad_state(u32 device_index, raw_gamepad_state& gs)
    {
        gs = s_raw_gamepads[device_index];
    }
} // namespace pen
