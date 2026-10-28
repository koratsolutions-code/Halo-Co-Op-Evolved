#pragma once
// Color de armadura de cada jugador en la campaña de Halo 1 (la personalización de MCC solo vale en multijugador).
// Escribe el color elegido en los "change colors" que el juego usa para dibujar el biped (+0x174, 4 colores RGB;
// equivale a 0x1B8 del Halo de PC) y lo vuelve a aplicar si el personaje cambia (reaparecer, otro nivel).
// Solo toca esos 48 bytes de cada biped; nada de mandos ni perfiles.
void armor_colors_start();
