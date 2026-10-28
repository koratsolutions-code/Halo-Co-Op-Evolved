#pragma once
#include "../common/protocol.h"

// Servidor UDP del host: recibe el input de los jugadores 2, 3 y 4 y lo aplica a sus mandos virtuales.

struct PlayerInfo {
    bool connected = false;
    char nickname[proto::k_nickname_size] = {};
    char address[64] = {};
    unsigned ms_since_packet = 0;
    unsigned inputs_per_second = 0;
    int armor_color = 0;  // índice en k_armor_colors
    bool direct_aim = false;  // usa la mira directa con mouse
};

bool net_server_start();   // false si el puerto está ocupado (¿otro Co Op Evolved Host abierto?)
void net_server_stop();    // avisa a los clientes que el host se cerró
PlayerInfo net_server_player(int player);  // player 2..4
void net_server_kick(int player);

// Audio del juego: el mismo paquete para todos los jugadores conectados.
void net_server_send_audio(const uint8_t* packet, size_t size);

// Video: el DLL del juego manda los cuadrantes; el launcher le dice a quién y a cuántos FPS.
void net_server_set_fps(int fps);
int net_server_fps();
// false si el DLL del juego no reporta (juego cerrado o sin el DLL instalado).
bool net_server_video_status(proto::VideoStatus& status);

// Lo que el host elige para el juego. Nombres y colores de J2-J4 los manda cada cliente.
struct HostOptions {
    bool enabled = true;  // nombres sobre los jugadores
    int fov = 70;         // FOV horizontal del juego (grados), para los nombres
    char host_name[proto::k_nickname_size] = {};  // jugador 1
    int host_armor_color = 0;                     // jugador 1, índice en k_armor_colors
    bool p1_fullscreen = false;                   // jugador 1 a pantalla completa al abrir el juego
};
void net_server_set_host_options(const HostOptions& options);
