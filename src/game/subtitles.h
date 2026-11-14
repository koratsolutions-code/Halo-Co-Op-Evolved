#pragma once
// Ocultar los subtítulos de MCC (opción del host). MCC los dibuja sobre toda la pantalla dividida, así que los amigos
// los ven cortados.
//
// halo1.dll lee la opción "GameOpt.Subtitles" con halo1.dll+0x47B180 y la guarda en +0x50 de su objeto de opciones;
// quien la llama, si quedó en 0, le manda "HideAll" a la interfaz de subtítulos (+0x47B230). Desviamos +0x47B180:
// después de leerla, si el host pide ocultarlos, el valor queda en 0. La opción de MCC no se toca (MCC la guarda en su
// perfil y la vuelve a escribir en user.cfg). halo1.dll se recarga en cada misión: el desvío se vuelve a poner cuando
// la función tiene otra vez sus bytes originales. Solo halo1.dll de MCC 1.3528 (se comprueban los bytes).
void subtitles_start();  // después de MH_Initialize

// true mientras MCC muestra al menos una línea de subtítulos (se siguen "ShowText" y "HideText" de halo1.dll).
bool subtitles_showing();

// Copia la línea de subtítulos (que MCC dibuja centrada en la pantalla entera) dentro de la parte de cada jugador
// que no la ve completa, sobre una banda oscura. Va en el hook de Present ANTES de capturar: así sale en el video de
// cada amigo y en la pantalla completa del jugador 1 (F6). Opción "Subtítulos para cada jugador" del panel.
struct IDXGISwapChain;
void subtitles_copy_draw(IDXGISwapChain* swap_chain);
