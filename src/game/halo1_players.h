#pragma once
// Lectura en vivo de los 4 jugadores locales de Halo 1 (MCC 1.3528), para dibujar sus nombres. Solo lee memoria.
// Estructuras de Halo 1 encontradas con ingeniería inversa:
//   tabla 'players' (data array, elementos de 0xC20): el registro N es el jugador local N; +0x64 = handle del biped.
//   tabla 'object' (elementos de 12 bytes): +8 = desplazamiento de los datos dentro del bloque de objetos,
//   que empieza en cabecera de 'object' + 0x38 + max*12 + 0x34.
//   biped: +0x18 posición (pies) · +0x5C centro del cuerpo · +0x210 vector de la mira (unitario).
// Hay varias copias de las tablas (estado vivo y copias de checkpoint); se usa la que el juego actualiza.

struct Halo1Player {
    bool alive;          // false: sin personaje (muerto, reapareciendo) o tablas no encontradas
    float position[3];   // pies
    float center[3];     // centro del cuerpo
    float look[3];       // dirección de la mira
};

void halo1_players_start();                   // hilo que localiza las tablas tras cargar cada mapa
bool halo1_players_read(Halo1Player out[4]);  // false si todavía no se encontraron las tablas
