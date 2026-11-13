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
