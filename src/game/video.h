#pragma once
#include "../common/protocol.h"

#include <dxgi.h>

// Video de los jugadores 2, 3 y 4: cada uno recibe su cuadrante de la pantalla dividida de AlphaRing.
// Co Op Evolved Host le dice al DLL (por 127.0.0.1) a qué dirección mandar cada cuadrante y a cuántos FPS.
//   Jugador 2 = arriba a la derecha, 3 = abajo a la izquierda, 4 = abajo a la derecha.

void video_start();                              // hilo de control (configuración y estado)
void video_on_frame(IDXGISwapChain* swap_chain);  // desde el hook de Present, hilo de render del juego
// Última configuración del launcher; false si Co Op Evolved Host no la manda hace más de 1,5 s.
bool video_config(proto::VideoConfig& out);
