#pragma once
#include <dxgi.h>

// Menú F2 del host dentro del juego: color de armadura, skin, subtítulos, pantalla completa (F6), las opciones del
// host y expulsar a un jugador, sin salir de MCC. Se abre y se cierra con F2 (con el juego en primer plano) o
// manteniendo View/Back del mando 1 segundo; B también lo cierra. Se dibuja al final del hook de Present: solo en el monitor del host, nunca en
// el video de los amigos. Mientras está abierto el jugador 1 no recibe controles. Cada cambio va al panel de
// Co Op Evolved (mensaje local host_menu), que lo guarda y lo vuelve a mandar en VideoConfig.
void host_menu_draw(IDXGISwapChain* swap_chain);
