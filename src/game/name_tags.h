#pragma once
#include <dxgi.h>

// Dibuja el nombre de cada compañero sobre su personaje en cada cuadrante de la pantalla dividida.
// Se llama en el hook de Present ANTES de capturar los cuadrantes: los nombres salen en la pantalla del host y
// en el video de cada cliente. Los nombres, el FOV y si están activos los manda HaloCoop Host.
void name_tags_start();
void name_tags_draw(IDXGISwapChain* swap_chain);
