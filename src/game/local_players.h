#pragma once
#include "../common/protocol.h"

// Jugadores locales 2-4.
//
// Mandos: MCC lee los mandos solo con XInputGetState/XInputSetState de XINPUT1_3.dll. Los desviamos: los slots
// 0, 1 y 2 son los mandos de J2, J3 y J4 que llegan por red (el launcher los manda a este DLL), siempre
// "conectados"; el slot 3 es el mando físico del slot 0, por si el host juega con mando.
//
// 4 jugadores: MCC en PC solo tiene un usuario local, así que este DLL desvía funciones del gestor de juego de MCC
// (usuarios conectados, controles por jugador, perfil, mapeo, vibración, índice por xuid; ver docs/DEVELOPMENT.md)
// y cambia tres instrucciones de halo1.dll (límite de 2 jugadores, pausa, congelamiento al terminar el nivel). Solo
// MCC Steam 1.3528: se comprueba la marca de tiempo del exe y los bytes antes de tocar nada.
void local_players_start();  // lo antes posible (hilo propio): MCC registra los mandos al arrancar

// Último estado de los mandos de J2-J4 que mandó el launcher.
void local_players_set_pads(const proto::Pads& pads);

// Menú F2 del host: mientras está abierto, el jugador 1 no recibe controles (teclado, mouse ni mando).
void local_players_block_host(bool block);
// true si los controles por jugador pasan por este DLL (los desvíos se instalaron bien): solo así se pueden bloquear.
bool local_players_own_input();

// Entrar con la misión empezada: cuántos amigos conectados no están en la misión (0 si no hay misión que se pueda
// recargar), y la recarga de la misión desde el principio con todos los conectados (la hace el hilo del
// juego; solo si hay alguien esperando: si la cantidad de jugadores no cambia, MCC deja su pantalla de carga encima).
int local_players_waiting();
void local_players_reload_mission();
