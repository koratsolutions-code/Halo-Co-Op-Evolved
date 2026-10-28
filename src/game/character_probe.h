#pragma once
// Diagnóstico temporal (solo lee memoria) para poder jugar con otros personajes. Con el juego activo, F9:
//   - lee qué personaje (tag "bipd") usa el jugador 1: el primer dato de su objeto es el ID del tag;
//   - busca en memoria las referencias a tags de personajes (clase 'bipd' + nombre + ID, como en Halo de PC)
//     y con ellas lista los personajes que trae la misión;
//   - anota dónde se referencia al personaje del jugador (una de esas es la configuración "los jugadores son
//     Master Chief" del tag globals).
// Todo va a coopevolved-juego.log.
void character_probe_start();
