#pragma once
#include <cstdint>

// Skins de los jugadores: cada jugador puede verse como otro personaje de la misión (marine, tripulante, Elite...)
// pero sigue siendo Master Chief en todo lo demás (hitbox, velocidad, salto, física). La skin de cada jugador llega
// del host en VideoConfig::skins; si no está en la misión, ese jugador se ve como Master Chief.
//
// Cómo: en cada frame el juego busca la definición de cada cuerpo con halo1.dll+0xA9B648 (índice = 16 bits bajos
// del ID de definición, que está en el objeto +0). Hacemos una copia de la definición de Master Chief por skin con
// el modelo y las animaciones de esa skin, añadimos esas copias al final de una copia de la lista de tags (puntero
// en halo1.dll+0x1C34FB0) y un pequeño desvío al principio de la búsqueda devuelve la copia para esos índices.
// El cuerpo tiene que NACER con la skin (el juego reserva el espacio de sus huesos al crearlo): mientras se crea el
// cuerpo de un jugador (halo1.dll+0xAD4184) Master Chief da la copia de su skin, y después el cuerpo queda con el
// ID de esa copia. Por eso una skin elegida en plena partida se ve al reaparecer, y los cuerpos que crea una
// cinemática al empezar una misión salen como Master Chief. Solo MCC Steam 1.3528: si el código no tiene los bytes
// esperados, no se toca nada. Detalles en docs/DEVELOPMENT.md.
void skins_start();

// Bit i: la skin i de k_skins está en la misión cargada (0: ninguna misión).
uint16_t skins_available();
