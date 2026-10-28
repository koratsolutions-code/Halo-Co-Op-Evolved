#pragma once
// Skins de los jugadores: cada jugador puede verse como otro humano de la misión (marine, tripulante...) pero
// sigue siendo Master Chief en todo lo demás (hitbox, velocidad, salto, física).
// Cómo: justo antes de que el juego cree el personaje de un jugador (halo1.dll+0xAD4184), se ponen en la
// definición de Master Chief el modelo 3D ('mod2', +0x28) y las animaciones ('antr') de la skin; justo después se
// devuelven las suyas. El juego toma el modelo y las animaciones al crear el personaje y no los vuelve a leer.
// La lista de tags se lee al instante desde halo1.dll+0x1C34FB0 (lista) y +0x2B22F50 (reserva; los punteros de
// 32 bits de los tags valen reserva + (puntero - 0x50000000)). Solo MCC Steam 1.3528: si la función no tiene los
// bytes esperados, no se engancha nada.
//
// Prueba actual: F10 activa o desactiva skins fijas (J2 marine con armadura, J3 marine, J4 tripulante).
void skins_start();
