#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>

// Captura solo el sonido de MCC (no el de Discord, el navegador, etc.) con la captura por proceso de
// Windows 10 2004+, lo comprime con Opus en paquetes de 10 ms y se los pasa a `send` (cabecera incluida).
// Espera sola a que MCC se abra y vuelve a empezar si se cierra.
using AudioSend = std::function<void(const uint8_t* packet, size_t size)>;
using FindProcess = std::function<unsigned long()>;  // pid de MCC, 0 si no está abierto

void audio_capture_start(FindProcess find_mcc, AudioSend send);
void audio_capture_stop();
bool audio_capture_active();  // capturando MCC ahora mismo
