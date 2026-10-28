#pragma once
// Protocolo UDP entre HaloCoopClient (jugadores 2-4) y HaloCoopHost. Todos los enteros en little endian.
#include <cstdint>

// Ioctl de Winsock para que un ICMP "puerto inalcanzable" no haga fallar el siguiente recvfrom.
// (mstcpip.h no siempre lo declara.)
#ifndef SIO_UDP_CONNRESET
#define SIO_UDP_CONNRESET 0x9800000C
#endif

namespace proto {

constexpr uint16_t k_port = 27960;
// Puerto local (solo 127.0.0.1) del DLL del juego: ahí el launcher le dice a quién mandar el video.
constexpr uint16_t k_video_control_port = 27961;
constexpr uint32_t k_magic = 0x50434F48;  // "HOCP"
constexpr uint16_t k_version = 3;
constexpr int k_nickname_size = 24;       // UTF-8 con '\0'

// Si el host no recibe nada de un cliente en este tiempo, libera su lugar y suelta su mando.
constexpr int k_timeout_ms = 3000;

enum class Type : uint8_t {
    hello = 1,    // cliente -> host: quiero ser el jugador N
    welcome = 2,  // host -> cliente: respuesta al hello
    input = 3,    // cliente -> host: estado del mando
    ping = 4,     // cliente -> host
    pong = 5,     // host -> cliente (devuelve el mismo tiempo)
    bye = 6,      // cliente -> host: me desconecto
    kicked = 7,   // host -> cliente: el host te expulsó / cerró
    keyframe_request = 8,  // cliente -> host: perdí video, mándame un fotograma completo
    video = 9,             // DLL del juego -> cliente: un fragmento de un fotograma H.264
    video_config = 10,     // launcher -> DLL (local): a quién mandar video, FPS y bitrate
    video_status = 11,     // DLL -> launcher (local): estado del video para el panel
    audio = 12,            // host -> cliente: 10 ms de audio del juego en Opus
};

// Audio: 48 kHz estéreo, paquetes de 10 ms (480 muestras por canal) comprimidos con Opus.
constexpr int k_audio_rate = 48000;
constexpr int k_audio_channels = 2;
constexpr int k_audio_frame = 480;

constexpr int k_video_payload = 1180;  // bytes de H.264 por paquete (cabe en el MTU de Radmin)
constexpr int k_fps_options[] = {30, 60, 120};

enum class Reason : uint8_t {
    none = 0,
    slot_taken = 1,   // ese jugador ya está ocupado por otra persona
    bad_version = 2,  // cliente y host de versiones distintas
    bad_player = 3,   // jugador fuera de 2..4
    kicked = 4,
    host_closed = 5,
};

enum class VideoError : uint8_t {
    none = 0,
    nvenc_failed = 1,  // no se pudo iniciar NVENC
    no_d3d11_4 = 2,    // el juego no usa D3D11.4
    msaa = 3,          // el juego usa MSAA
};

#pragma pack(push, 1)
struct Header {
    uint32_t magic = k_magic;
    Type type;
};

struct Hello {
    Header header{k_magic, Type::hello};
    uint16_t version = k_version;
    uint8_t player = 0;  // 2..4
    char nickname[k_nickname_size] = {};
};

struct Welcome {
    Header header{k_magic, Type::welcome};
    uint8_t accepted = 0;
    Reason reason = Reason::none;
    uint8_t player = 0;
};

// Mismo formato que XINPUT_GAMEPAD.
struct GamepadState {
    uint16_t buttons = 0;
    uint8_t left_trigger = 0, right_trigger = 0;
    int16_t lx = 0, ly = 0, rx = 0, ry = 0;
};

struct Input {
    Header header{k_magic, Type::input};
    uint32_t sequence = 0;  // creciente: el host ignora paquetes viejos que lleguen desordenados
    GamepadState pad;
};

struct Ping {
    Header header{k_magic, Type::ping};
    uint64_t client_time_us = 0;
};

struct Pong {
    Header header{k_magic, Type::pong};
    uint64_t client_time_us = 0;
};

struct Bye {
    Header header{k_magic, Type::bye};
};

struct Kicked {
    Header header{k_magic, Type::kicked};
    Reason reason = Reason::kicked;
};

// Seguido del paquete Opus.
struct AudioHeader {
    Header header{k_magic, Type::audio};
    uint32_t sequence = 0;  // creciente: el cliente detecta paquetes perdidos y los rellena
};

struct KeyframeRequest {
    Header header{k_magic, Type::keyframe_request};
};

// Seguido de hasta k_video_payload bytes del fotograma.
struct VideoHeader {
    Header header{k_magic, Type::video};
    uint32_t frame_id = 0;
    uint16_t fragment_index = 0;
    uint16_t fragment_count = 0;
    uint8_t keyframe = 0;
    uint16_t width = 0, height = 0;
    uint64_t capture_us = 0;  // reloj del host, para estadísticas
};

struct VideoTarget {
    uint8_t active = 0;
    uint32_t ip = 0;          // orden de red, como sin_addr
    uint16_t port = 0;        // orden de red, como sin_port
    uint32_t keyframe_requests = 0;  // contador: si sube, el DLL manda un fotograma completo
};

// El launcher lo manda al DLL cada 250 ms. Si el DLL deja de recibirlo, deja de mandar video.
struct VideoConfig {
    Header header{k_magic, Type::video_config};
    uint8_t fps = 60;
    VideoTarget targets[3];  // jugadores 2, 3, 4
    // Nombres sobre los jugadores (los dibuja el DLL en la pantalla del juego).
    uint8_t name_tags = 1;    // 0: apagados
    uint8_t name_fov = 70;    // FOV horizontal del juego en grados, para proyectar los nombres
    char names[4][k_nickname_size] = {};  // jugadores 1..4 (vacío: "Player N")
};

struct VideoStatus {
    Header header{k_magic, Type::video_status};
    uint8_t encoder_ok = 0;
    uint16_t game_width = 0, game_height = 0;
    uint16_t fps_sent[3] = {};       // por jugador 2, 3, 4
    uint32_t kbps_sent[3] = {};
    VideoError error = VideoError::none;  // el panel lo muestra en el idioma elegido
};
#pragma pack(pop)

template <typename T>
inline const T* as(const char* data, int size) {
    if (size < int(sizeof(T))) return nullptr;
    auto* message = reinterpret_cast<const T*>(data);
    return message->header.magic == k_magic ? message : nullptr;
}

inline const Header* header_of(const char* data, int size) {
    if (size < int(sizeof(Header))) return nullptr;
    auto* header = reinterpret_cast<const Header*>(data);
    return header->magic == k_magic ? header : nullptr;
}

}  // namespace proto
