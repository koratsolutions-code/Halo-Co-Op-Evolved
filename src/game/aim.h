#pragma once
// Mira directa con mouse para los jugadores 2, 3 y 4: suma el movimiento del mouse de cada cliente al giro y a
// la altura de su cámara en Halo 1, sin pasar por el stick del mando virtual.
// Las cámaras están en el estado del juego, una por jugador cada 0x58 bytes: float giro (radianes, [0, 2π),
// crece hacia la izquierda) y float altura (+ hacia arriba). Se encontraron con un diagnóstico que escribió +30°
// y vio girar la cámara; el bloque parecido de cada 0x30 bytes es solo una copia que el juego rehace.
// Se ubican comparando con la mira de cada jugador y se vuelven a buscar si dejan de coincidir (otro nivel).
// Solo se escriben esos 8 bytes de J2-J4: nada de mandos, perfiles ni AlphaRing.

void aim_start();  // hilo que ubica las cámaras
bool aim_ready();  // true si las encontró
// player 2..4. Radianes: giro + a la izquierda, altura + hacia arriba.
void aim_apply(int player, float yaw, float pitch);
