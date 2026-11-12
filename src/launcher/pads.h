#pragma once
#include <cstdint>

// Mandos de los jugadores 2, 3 y 4. Dos modos:
// - direct (por defecto): el estado de cada mando va al DLL del juego (127.0.0.1), que se lo entrega a MCC como si
//   fueran los mandos XInput 0, 1 y 2. Sin drivers, y MCC puede estar abierto antes.
// - vigem (el sistema anterior): mandos Xbox 360 virtuales de ViGEmBus. Tienen que existir ANTES de que MCC
//   arranque: si se crean después, MCC no siempre los registra y AlphaRing les entrega input vacío.

constexpr int k_first_remote_player = 2;
constexpr int k_remote_players = 3;

struct PadState {
    uint16_t buttons = 0;  // XUSB_GAMEPAD_*
    uint8_t left_trigger = 0, right_trigger = 0;
    int16_t lx = 0, ly = 0, rx = 0, ry = 0;
};

enum class PadsError { none, driver_missing, create_failed };

PadsError pads_init(bool direct);
bool pads_direct();
void pads_shutdown();
bool pads_update(int player, const PadState& state);  // player 2..4
void pads_reset(int player);
int pads_slot(int player);  // slot XInput (0..3) de ese mando, -1 si no existe
int pads_occupied_slots_before();  // mandos que ya había conectados antes de crear los nuestros (físicos)

// Diagnóstico: el personaje camina adelante, atrás, a los lados, gira y salta (~10 s, hilo aparte).
// Devuelve false si ya hay una prueba corriendo para ese jugador.
bool pads_move_test(int player);
