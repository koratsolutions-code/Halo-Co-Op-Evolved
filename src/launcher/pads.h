#pragma once
#include <cstdint>

// Mandos de los jugadores 2, 3 y 4: el estado de cada mando va por UDP al DLL del juego (127.0.0.1), que se lo
// entrega a MCC como si fueran los mandos XInput 0, 1 y 2. Sin drivers, y MCC puede estar abierto antes.

constexpr int k_first_remote_player = 2;
constexpr int k_remote_players = 3;

struct PadState {
    uint16_t buttons = 0;  // XINPUT_GAMEPAD_*
    uint8_t left_trigger = 0, right_trigger = 0;
    int16_t lx = 0, ly = 0, rx = 0, ry = 0;
};

void pads_init();
bool pads_update(int player, const PadState& state);  // player 2..4
void pads_reset(int player);
int pads_slot(int player);  // slot XInput (0..3) de ese mando, -1 si no existe

// Diagnóstico: el personaje camina adelante, atrás, a los lados, gira y salta (~10 s, hilo aparte).
// Devuelve false si ya hay una prueba corriendo para ese jugador.
bool pads_move_test(int player);
