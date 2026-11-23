#pragma once
// Vehículos extra para 3 y 4 jugadores, sin tocar los mapas: copias de los vehículos que pone la misión.
// - Halo (a30): cuando el guion suelta el primer Warthog ("jeep") de la nave, con 4 jugadores aparece otro a su lado.
// - Two Betrayals (c40): cuando aparece el segundo Banshee de la cooperativa ("fly_away_2"), uno más por cada jugador
//   extra (1 con 3 jugadores, 2 con 4), en la misma fila que los dos originales.
#include <cstdint>

void extra_vehicles_game_tick(uintptr_t halo1, int players);  // hilo del juego, en cada cuadro
void extra_vehicles_on_script_exit(uintptr_t halo1, uint32_t unit);  // hilo del juego: un guion bajó a una unidad
