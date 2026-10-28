#include "input.h"

#include <xinput.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdlib>

static HWND g_window = nullptr;
static std::atomic<bool> g_captured{false};
static std::atomic<long> g_mouse_dx{0}, g_mouse_dy{0};
static std::atomic<bool> g_left_button{false}, g_right_button{false}, g_middle_button{false};
static std::atomic<int> g_wheel{0};  // muescas pendientes: cada una es un toque de Y (cambiar arma)

void input_init(HWND window) {
    g_window = window;
    RAWINPUTDEVICE mouse{0x01, 0x02, 0, window};  // Generic Desktop / Mouse
    RegisterRawInputDevices(&mouse, 1, sizeof(mouse));
}

void input_on_raw_input(LPARAM lparam) {
    RAWINPUT raw;
    UINT size = sizeof(raw);
    if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lparam), RID_INPUT, &raw, &size, sizeof(RAWINPUTHEADER)) ==
            UINT(-1) ||
        raw.header.dwType != RIM_TYPEMOUSE || !g_captured)
        return;
    const RAWMOUSE& m = raw.data.mouse;
    if (!(m.usFlags & MOUSE_MOVE_ABSOLUTE)) {
        g_mouse_dx += m.lLastX;
        g_mouse_dy += m.lLastY;
    }
    USHORT f = m.usButtonFlags;
    if (f & RI_MOUSE_LEFT_BUTTON_DOWN) g_left_button = true;
    if (f & RI_MOUSE_LEFT_BUTTON_UP) g_left_button = false;
    if (f & RI_MOUSE_RIGHT_BUTTON_DOWN) g_right_button = true;
    if (f & RI_MOUSE_RIGHT_BUTTON_UP) g_right_button = false;
    if (f & RI_MOUSE_MIDDLE_BUTTON_DOWN) g_middle_button = true;
    if (f & RI_MOUSE_MIDDLE_BUTTON_UP) g_middle_button = false;
    if (f & RI_MOUSE_WHEEL) g_wheel += 1;
}

void input_set_captured(bool captured) {
    if (captured == g_captured) return;
    g_captured = captured;
    if (captured) {
        RECT rect;
        GetClientRect(g_window, &rect);
        MapWindowPoints(g_window, nullptr, reinterpret_cast<POINT*>(&rect), 2);
        ClipCursor(&rect);
        while (ShowCursor(FALSE) >= 0) {}
    } else {
        ClipCursor(nullptr);
        while (ShowCursor(TRUE) < 0) {}
        g_left_button = g_right_button = g_middle_button = false;
        g_mouse_dx = g_mouse_dy = 0;
        g_wheel = 0;
    }
}

bool input_captured() {
    return g_captured;
}

static bool read_gamepad(XINPUT_STATE& state) {
    for (DWORD slot = 0; slot < XUSER_MAX_COUNT; ++slot)
        if (XInputGetState(slot, &state) == ERROR_SUCCESS) return true;
    return false;
}

bool input_gamepad_connected() {
    XINPUT_STATE state{};
    return read_gamepad(state);
}

static bool key(int vk) {
    return (GetAsyncKeyState(vk) & 0x8000) != 0;
}

// Movimiento del mouse en este intervalo -> stick derecho. Se promedian los últimos intervalos para que un
// mouse de 125 Hz (que no manda movimiento en todos los intervalos de 4 ms) no haga temblar la mira.
static int16_t mouse_to_stick(long delta, float sensitivity, long history[3]) {
    history[2] = history[1];
    history[1] = history[0];
    history[0] = delta;
    float average = (history[0] + history[1] + history[2]) / 3.0f;
    if (std::fabs(average) < 0.01f) return 0;
    // El juego ignora el stick dentro de su zona muerta: se arranca justo por encima de ella.
    constexpr float k_dead_zone = 7000.0f;
    constexpr float k_gain = 900.0f;  // unidades de stick por cuenta del mouse con sensibilidad 1
    float value = k_dead_zone + std::fabs(average) * k_gain * sensitivity;
    value = std::min(value, 32767.0f);
    return int16_t(average > 0 ? value : -value);
}

proto::GamepadState input_read(ControlMode mode, float sensitivity) {
    proto::GamepadState pad{};
    if (mode == ControlMode::gamepad) {
        XINPUT_STATE state{};
        if (read_gamepad(state)) {
            const XINPUT_GAMEPAD& g = state.Gamepad;
            pad.buttons = g.wButtons;
            pad.left_trigger = g.bLeftTrigger;
            pad.right_trigger = g.bRightTrigger;
            pad.lx = g.sThumbLX;
            pad.ly = g.sThumbLY;
            pad.rx = g.sThumbRX;
            pad.ry = g.sThumbRY;
        }
        return pad;
    }

    static long history_x[3] = {}, history_y[3] = {};
    if (!g_captured) {
        std::fill(history_x, history_x + 3, 0);
        std::fill(history_y, history_y + 3, 0);
        return pad;
    }
    // Movimiento: WASD
    int x = (key('D') ? 1 : 0) - (key('A') ? 1 : 0);
    int y = (key('W') ? 1 : 0) - (key('S') ? 1 : 0);
    pad.lx = int16_t(x * 32767);
    pad.ly = int16_t(y * 32767);
    // Mira: mouse (eje Y del mouse invertido respecto al stick)
    pad.rx = mouse_to_stick(g_mouse_dx.exchange(0), sensitivity, history_x);
    pad.ry = int16_t(-mouse_to_stick(g_mouse_dy.exchange(0), sensitivity, history_y));
    // Gatillos: clic izquierdo dispara, clic derecho lanza granada
    pad.right_trigger = g_left_button ? 255 : 0;
    pad.left_trigger = g_right_button ? 255 : 0;

    uint16_t b = 0;
    if (key(VK_SPACE)) b |= XINPUT_GAMEPAD_A;                              // saltar
    if (key('F') || key('V')) b |= XINPUT_GAMEPAD_B;                      // golpe cuerpo a cuerpo
    if (key('E') || key('R')) b |= XINPUT_GAMEPAD_X;                      // acción / recargar
    if (key('Q')) b |= XINPUT_GAMEPAD_Y;                                  // cambiar arma
    if (key('G')) b |= XINPUT_GAMEPAD_LEFT_SHOULDER;                      // cambiar granada
    if (key(VK_LCONTROL) || key('C')) b |= XINPUT_GAMEPAD_LEFT_THUMB;     // agacharse
    if (key('Z') || g_middle_button) b |= XINPUT_GAMEPAD_RIGHT_THUMB;     // zoom
    if (key(VK_RETURN)) b |= XINPUT_GAMEPAD_START;                        // pausa
    if (key(VK_TAB)) b |= XINPUT_GAMEPAD_BACK;                            // marcador
    if (key('1') || key(VK_UP)) b |= XINPUT_GAMEPAD_DPAD_UP;
    if (key('2') || key(VK_DOWN)) b |= XINPUT_GAMEPAD_DPAD_DOWN;
    if (key('3') || key(VK_LEFT)) b |= XINPUT_GAMEPAD_DPAD_LEFT;
    if (key('4') || key(VK_RIGHT)) b |= XINPUT_GAMEPAD_DPAD_RIGHT;
    // Rueda del mouse: un toque corto de Y por muesca (se mantiene ~2 envíos para que el juego lo vea).
    static int wheel_hold = 0;
    if (wheel_hold == 0 && g_wheel > 0) {
        --g_wheel;
        wheel_hold = 12;  // ~48 ms
    }
    if (wheel_hold > 0) {
        if (wheel_hold > 6) b |= XINPUT_GAMEPAD_Y;  // presionado la mitad del tiempo, suelto la otra mitad
        --wheel_hold;
    }
    pad.buttons = b;
    return pad;
}
