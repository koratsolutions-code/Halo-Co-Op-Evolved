#pragma once
#include "../common/protocol.h"
#include "settings.h"

#include <windows.h>

// Lee el mando (XInput) o el teclado y mouse y lo convierte en un estado de mando Xbox.
// El mando funciona aunque la ventana no esté activa; el teclado y mouse solo con el mouse capturado.

void input_init(HWND window);               // registra el mouse en Raw Input
void input_on_raw_input(LPARAM lparam);     // WM_INPUT (hilo de la ventana)
void input_set_captured(bool captured);     // mouse oculto y encerrado en la ventana
bool input_captured();
bool input_gamepad_connected();
// Seguro desde cualquier hilo. Se llama a ritmo fijo (cada k_send_interval_ms).
proto::GamepadState input_read(ControlMode mode, float sensitivity);

constexpr int k_send_interval_ms = 4;  // 250 envíos por segundo
