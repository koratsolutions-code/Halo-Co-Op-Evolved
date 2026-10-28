#pragma once
#include "../common/protocol.h"
#include "settings.h"

#include <string>

// Conexión con el host en un hilo propio: saludo, envío del input a ritmo fijo y ping.

enum class NetState { idle, connecting, connected, failed };

struct NetStatus {
    NetState state = NetState::idle;
    std::string message;    // motivo del fallo / desconexión, para mostrar
    int ping_ms = -1;
    unsigned sent_per_second = 0;
};

void net_connect(const Settings& settings);  // host, jugador, nombre, control y sensibilidad
void net_update_settings(const Settings& settings);  // control/sensibilidad cambiados en partida
void net_disconnect();
NetStatus net_status();
