#pragma once
// Cómo reparte Halo CE la pantalla según cuántos jugadores locales hay (visto en MCC 1.3528):
//   1: toda la pantalla
//   2: mitades a todo el ancho (J1 arriba, J2 abajo)
//   3: J1 arriba a todo el ancho; J2 abajo a la izquierda; J3 abajo a la derecha
//   4: cuartos (J1 arriba izq., J2 arriba der., J3 abajo izq., J4 abajo der.)
#include <windows.h>

#include <cstdint>

struct ViewRect {
    uint32_t x = 0, y = 0, width = 0, height = 0;
};

// Jugadores locales de la partida en curso (halo1.dll+0x1B7B910), o 4 si no hay partida o no se puede leer.
inline int split_player_count() {
    HMODULE halo1 = GetModuleHandleW(L"halo1.dll");
    if (!halo1) return 4;
    int16_t count = *reinterpret_cast<const volatile int16_t*>(reinterpret_cast<uintptr_t>(halo1) + 0x1B7B910);
    return count >= 1 && count <= 4 ? count : 4;
}

// Rectángulo del jugador 0..3 en una pantalla de width x height con count jugadores (medidas pares, para el
// codificador). false si ese jugador no está en la partida.
inline bool split_view(int player, int count, uint32_t width, uint32_t height, ViewRect& out) {
    if (player < 0 || player >= count) return false;
    uint32_t half_w = (width / 2) & ~1u, half_h = (height / 2) & ~1u;
    uint32_t full_w = width & ~1u;
    bool top = count == 4 ? player < 2 : player == 0;
    bool full_row = count == 1 || count == 2 || (count == 3 && player == 0);
    out.width = full_row ? full_w : half_w;
    out.height = count == 1 ? (height & ~1u) : half_h;
    out.y = top ? 0 : height - out.height;
    bool right = count == 4 ? (player % 2 == 1) : (count == 3 && player == 2);
    out.x = full_row ? 0 : (right ? width - out.width : 0);
    return true;
}
