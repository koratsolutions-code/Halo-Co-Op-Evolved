#pragma once
#include <dxgi.h>

// Pantalla completa del jugador 1: estira su cuadrante (arriba a la izquierda) sobre toda la pantalla del host.
// Se llama en el hook de Present DESPUÉS de capturar el video de J2-J4, así ellos siguen recibiendo su cuadrante.
// F6 lo activa o lo desactiva; la casilla del panel de Co Op Evolved Host elige cómo empieza.
// Solo cambia la imagen del monitor del host: no toca el juego, los mandos ni AlphaRing.
void p1_view_on_frame(IDXGISwapChain* swap_chain);
