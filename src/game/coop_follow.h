#pragma once
// J3 y J4 en la campaña de Halo 1: los guiones de las misiones solo conocen a dos jugadores (player0 y player1). Este
// módulo hace que J3 siga a J1 y J4 siga a J2, sin tocar los mapas:
// - Al empezar cada misión, J3 aparece al lado de J1 y J4 al lado de J2 (o en el asiento de al lado si van en un
//   vehículo, como el Pelican del principio de The Silent Cartographer).
// - Cuando un guion mueve a J1/J2 a un punto de cinemática (object_teleport), J3/J4 van al mismo punto.
// - Cuando un guion congela o suelta a J1/J2 (unit_suspended), J3/J4 también.
// - Cuando un guion sube a J1/J2 a un vehículo (unit_enter_vehicle), J3/J4 suben al asiento de al lado.

#include <cstdint>

void coop_follow_start();      // después de MH_Initialize
void coop_follow_game_tick();  // en cada cuadro, desde el hilo del juego (local_players.cpp)
