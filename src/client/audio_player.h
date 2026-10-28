#pragma once
#include <cstddef>
#include <cstdint>

// Reproduce el audio del juego que manda el host (Opus, 10 ms por paquete) con poco retraso (~30-40 ms).
// Si se pierde un paquete, Opus lo disimula; si el colchón crece de más, se descarta lo viejo.

void audio_start();
void audio_stop();
void audio_on_packet(uint32_t sequence, const uint8_t* data, size_t size);  // desde el hilo de red
void audio_reset();  // al desconectarse: vaciar lo pendiente
void audio_set_volume(float volume);  // 0..1

struct AudioStats {
    unsigned packets = 0;      // recibidos desde la última consulta (100 por segundo con sonido)
    unsigned concealed = 0;    // perdidos y rellenados por Opus
    unsigned buffered_ms = 0;  // retraso del colchón ahora
};
AudioStats audio_take_stats();
void audio_set_muted(bool muted);
