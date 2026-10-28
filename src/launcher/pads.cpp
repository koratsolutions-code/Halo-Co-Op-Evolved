#include "pads.h"
#include "log.h"

#include <windows.h>
#include <xinput.h>

#include <ViGEm/Client.h>

#include <atomic>
#include <mutex>
#include <thread>

static PVIGEM_CLIENT g_client = nullptr;
static PVIGEM_TARGET g_pads[k_remote_players]{};
static int g_slots[k_remote_players] = {-1, -1, -1};
static int g_occupied_before = 0;
static std::mutex g_mutex;

static int index_of(int player) {
    int index = player - k_first_remote_player;
    return index >= 0 && index < k_remote_players ? index : -1;
}

static int count_connected_slots() {
    int count = 0;
    for (DWORD slot = 0; slot < XUSER_MAX_COUNT; ++slot) {
        XINPUT_STATE state{};
        if (XInputGetState(slot, &state) == ERROR_SUCCESS) ++count;
    }
    return count;
}

PadsError pads_init() {
    g_occupied_before = count_connected_slots();
    log_write("Mandos ya conectados antes de crear los virtuales: %d", g_occupied_before);

    g_client = vigem_alloc();
    if (!g_client) return PadsError::create_failed;
    VIGEM_ERROR error = vigem_connect(g_client);
    if (!VIGEM_SUCCESS(error)) {
        log_write("ViGEmBus no responde (0x%08X): el driver no está instalado", error);
        vigem_free(g_client);
        g_client = nullptr;
        return PadsError::driver_missing;
    }
    // Se crean en orden para que reciban slots consecutivos: jugador 2, 3, 4 -> slots 0, 1, 2
    // (lo que AlphaRing asigna por defecto) si no hay mandos físicos conectados.
    for (int i = 0; i < k_remote_players; ++i) {
        g_pads[i] = vigem_target_x360_alloc();
        error = vigem_target_add(g_client, g_pads[i]);
        if (!VIGEM_SUCCESS(error)) {
            log_write("No se pudo crear el mando del jugador %d (0x%08X)", i + k_first_remote_player, error);
            pads_shutdown();
            return PadsError::create_failed;
        }
        ULONG user_index = 0xFF;
        vigem_target_x360_get_user_index(g_client, g_pads[i], &user_index);
        g_slots[i] = user_index < XUSER_MAX_COUNT ? int(user_index) : -1;
        log_write("Mando del jugador %d creado: slot XInput %d ('Controller %d' en AlphaRing)",
                  i + k_first_remote_player, g_slots[i], g_slots[i] + 1);
    }
    // Estado neutro explícito: sin esto los mandos reportan ruido en los sticks hasta la primera actualización.
    // Un reporte todo en cero igual al inicial no llega a XInput, así que primero se manda uno distinto.
    PadState nudge;
    nudge.lx = 1;
    for (int i = 0; i < k_remote_players; ++i) pads_update(i + k_first_remote_player, nudge);
    Sleep(50);
    for (int i = 0; i < k_remote_players; ++i) pads_reset(i + k_first_remote_player);
    return PadsError::none;
}

void pads_shutdown() {
    std::lock_guard lock(g_mutex);
    for (auto& pad : g_pads) {
        if (!pad) continue;
        vigem_target_remove(g_client, pad);
        vigem_target_free(pad);
        pad = nullptr;
    }
    if (g_client) {
        vigem_disconnect(g_client);
        vigem_free(g_client);
        g_client = nullptr;
    }
}

bool pads_update(int player, const PadState& state) {
    int index = index_of(player);
    if (index < 0 || !g_client || !g_pads[index]) return false;
    XUSB_REPORT report{};
    report.wButtons = state.buttons;
    report.bLeftTrigger = state.left_trigger;
    report.bRightTrigger = state.right_trigger;
    report.sThumbLX = state.lx;
    report.sThumbLY = state.ly;
    report.sThumbRX = state.rx;
    report.sThumbRY = state.ry;
    std::lock_guard lock(g_mutex);
    return VIGEM_SUCCESS(vigem_target_x360_update(g_client, g_pads[index], report));
}

void pads_reset(int player) {
    pads_update(player, PadState{});
}

int pads_slot(int player) {
    int index = index_of(player);
    return index < 0 ? -1 : g_slots[index];
}

int pads_occupied_slots_before() {
    return g_occupied_before;
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
            {"salta (A)", 0, 0, 0, 0, XUSB_GAMEPAD_A, 300},
            {"suelta", 0, 0, 0, 0, 0, 400},
            {"salta (A)", 0, 0, 0, 0, XUSB_GAMEPAD_A, 300},
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
