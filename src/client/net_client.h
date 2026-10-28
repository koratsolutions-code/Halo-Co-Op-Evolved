#pragma once
#include "../common/protocol.h"
#include "settings.h"

#include <windows.h>

#include <cstdint>
#include <string>
#include <vector>

// Conexión con el host en un hilo propio: saludo, envío del input a ritmo fijo, ping y recepción del video.

enum class NetState { idle, connecting, connected, failed };

struct NetStatus {
    NetState state = NetState::idle;
    std::string message;    // motivo del fallo / desconexión, para mostrar
    int ping_ms = -1;
    unsigned sent_per_second = 0;
    unsigned video_fps = 0;        // fotogramas completos recibidos por segundo
    unsigned video_kbps = 0;
    unsigned frames_lost = 0;      // total desde que se conectó
    bool host_aim_ready = false;   // el host puede mover la cámara directo (mira directa con mouse)
    bool direct_aim = false;       // la mira directa está en uso ahora
};

struct VideoFrame {
    std::vector<uint8_t> data;  // H.264 Annex B
    bool keyframe = false;
    uint16_t width = 0, height = 0;
    uint64_t received_us = 0;
};

void net_connect(const Settings& settings);  // host, jugador, nombre, control y sensibilidad
void net_update_settings(const Settings& settings);  // control/sensibilidad cambiados en partida
void net_disconnect();
NetStatus net_status();

// Video: el hilo de red encola fotogramas completos y en orden; la ventana los saca y los decodifica todos.
HANDLE net_frame_event();                         // se activa cuando hay fotogramas nuevos
void net_take_frames(std::vector<VideoFrame>& out);
void net_request_keyframe();                      // el decodificador falló: pedir un fotograma completo
