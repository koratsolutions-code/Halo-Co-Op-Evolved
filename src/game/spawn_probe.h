#pragma once
// Diagnóstico temporal (no cambia el juego): busca la función de halo1.dll que hace aparecer a un jugador.
// Con el juego activo, F11 pone un punto de vigilancia de hardware (registros de depuración del procesador) sobre
// el dato "personaje que controla el jugador 2" (registro del jugador +0x64). Cada vez que el juego escribe ahí
// (al morir lo borra, al reaparecer pone el nuevo), anota en coopevolved-juego.log la instrucción que escribió,
// el valor nuevo y la cadena de llamadas (halo1.dll+dirección). F11 otra vez lo quita; se quita solo tras 20 avisos.
void spawn_probe_start();
