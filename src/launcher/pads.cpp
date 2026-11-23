#include "pads.h"
#include "log.h"

#include "../common/protocol.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <xinput.h>

#include <atomic>
#include <mutex>
#include <thread>

static int g_slots[k_remote_players] = {-1, -1, -1};
static std::mutex g_mutex;

// Último estado de cada mando, que se manda al DLL del juego al cambiar y cada 50 ms (por si el DLL arrancó después
// o se perdió un paquete).
static SOCKET g_socket = INVALID_SOCKET;
static proto::Pads g_direct_pads;

static void send_direct_pads() {
    sockaddr_in game{};
    game.sin_family = AF_INET;
    game.sin_port = htons(proto::k_video_control_port);
    game.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    sendto(g_socket, reinterpret_cast<const char*>(&g_direct_pads), sizeof(g_direct_pads), 0,
           reinterpret_cast<const sockaddr*>(&game), sizeof(game));
}

static int index_of(int player) {
    int index = player - k_first_remote_player;
    return index >= 0 && index < k_remote_players ? index : -1;
}

void pads_init() {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    g_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    for (int i = 0; i < k_remote_players; ++i) g_slots[i] = i;
    std::thread([] {
        while (true) {
            {
                std::lock_guard lock(g_mutex);
                send_direct_pads();
            }
            Sleep(50);
        }
    }).detach();
    log_write("Mandos de los jugadores 2-4: directo al juego");
}

bool pads_update(int player, const PadState& state) {
    int index = index_of(player);
    if (index < 0) return false;
    std::lock_guard lock(g_mutex);
    proto::GamepadState& pad = g_direct_pads.pads[index];
    pad.buttons = state.buttons;
    pad.left_trigger = state.left_trigger;
    pad.right_trigger = state.right_trigger;
    pad.lx = state.lx;
    pad.ly = state.ly;
    pad.rx = state.rx;
    pad.ry = state.ry;
    send_direct_pads();
    return g_socket != INVALID_SOCKET;
}

void pads_reset(int player) {
    pads_update(player, PadState{});
}

int pads_slot(int player) {
    int index = index_of(player);
    return index < 0 ? -1 : g_slots[index];
}

bool pads_move_test(int player) {
    static std::atomic<bool> running[k_remote_players]{};
    int index = index_of(player);
    if (index < 0 || running[index].exchange(true)) return false;
    std::thread([player, index] {
        struct Step {
            const char* name;
            int16_t lx, ly, rx, ry;
            uint16_t buttons;
            int ms;
        };
        static constexpr Step k_steps[] = {
            {"camina hacia adelante", 0, 32767, 0, 0, 0, 1500},
            {"camina hacia atrás", 0, -32767, 0, 0, 0, 1500},
            {"camina a la izquierda", -32767, 0, 0, 0, 0, 1500},
            {"camina a la derecha", 32767, 0, 0, 0, 0, 1500},
            {"gira a la derecha", 0, 0, 24000, 0, 0, 1500},
            {"gira a la izquierda", 0, 0, -24000, 0, 0, 1500},
            {"salta (A)", 0, 0, 0, 0, XINPUT_GAMEPAD_A, 300},
            {"suelta", 0, 0, 0, 0, 0, 400},
            {"salta (A)", 0, 0, 0, 0, XINPUT_GAMEPAD_A, 300},
        };
        log_write("Prueba de movimiento del jugador %d (slot %d) empieza", player, pads_slot(player));
        int failures = 0;
        for (const Step& step : k_steps) {
            PadState state;
            state.lx = step.lx;
            state.ly = step.ly;
            state.rx = step.rx;
            state.ry = step.ry;
            state.buttons = step.buttons;
            for (int t = 0; t < step.ms; t += 50) {
                if (!pads_update(player, state)) ++failures;
                Sleep(50);
            }
            log_write("  jugador %d: %s", player, step.name);
        }
        pads_reset(player);
        log_write("Prueba de movimiento del jugador %d terminada (%d fallos)", player, failures);
        running[index] = false;
    }).detach();
    return true;
}
