#pragma once
#include "../common/protocol.h"

// Jugadores locales 2-4 sin AlphaRing ni ViGEmBus (modo "direct").
//
// Mandos: MCC lee los mandos solo con XInputGetState/XInputSetState de XINPUT1_3.dll. Los desviamos: los slots
// 0, 1 y 2 son los mandos de J2, J3 y J4 que llegan por red (el launcher los manda a este DLL), siempre
// "conectados"; el slot 3 es el mando físico del slot 0, por si el host juega con mando.
//
// 4 jugadores: si AlphaRing (WTSAPI32.dll en la carpeta del juego) no está, este DLL hace lo mismo que él, con la
// misma idea (AlphaRing es MIT, © 2024 WinterSquire; ver docs/DEVELOPMENT.md): desvía funciones del gestor de juego
// de MCC (usuarios conectados, controles por jugador, perfil, mapeo, vibración, índice por xuid) y cambia tres
// instrucciones de halo1.dll (límite de 2 jugadores, pausa, congelamiento al terminar el nivel). Solo MCC Steam
// 1.3528: se comprueba la marca de tiempo del exe y los bytes antes de tocar nada.
//
// El launcher elige el modo en coopevolved-juego.ini (junto a este DLL): mandos=direct (por defecto) o vigem (el
// sistema anterior: mandos virtuales de ViGEmBus + AlphaRing). En vigem este módulo no toca nada.
void local_players_start();  // lo antes posible (hilo propio): MCC registra los mandos al arrancar

// Último estado de los mandos de J2-J4 que mandó el launcher.
void local_players_set_pads(const proto::Pads& pads);
