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
constexpr uint32_t k_magic = 0x50434F48;  // "HOCP"
constexpr uint16_t k_version = 1;
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
};

enum class Reason : uint8_t {
    none = 0,
    slot_taken = 1,   // ese jugador ya está ocupado por otra persona
    bad_version = 2,  // cliente y host de versiones distintas
    bad_player = 3,   // jugador fuera de 2..4
    kicked = 4,
    host_closed = 5,
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
