#include "net_server.h"
#include "log.h"
#include "pads.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <mstcpip.h>

#include <atomic>
#include <cstring>
#include <mutex>
#include <thread>

namespace {

struct Slot {
    bool connected = false;
    sockaddr_in address{};
    char nickname[proto::k_nickname_size] = {};
    ULONGLONG last_seen = 0;
    uint32_t last_sequence = 0;
    unsigned inputs = 0;             // contador de la ventana actual
    unsigned inputs_per_second = 0;  // último valor medido
    uint32_t keyframe_requests = 0;
};

SOCKET g_socket = INVALID_SOCKET;
std::thread g_thread;
std::atomic<bool> g_running{false};
std::mutex g_mutex;
Slot g_slots[k_remote_players];
std::atomic<int> g_fps{60};
proto::VideoStatus g_video_status;
ULONGLONG g_video_status_time = 0;

// Con g_mutex tomado. Le dice al DLL del juego a quién mandar cada cuadrante.
void send_video_config() {
    proto::VideoConfig config;
    config.fps = uint8_t(g_fps.load());
    for (int i = 0; i < k_remote_players; ++i) {
        config.targets[i].active = g_slots[i].connected ? 1 : 0;
        config.targets[i].ip = g_slots[i].address.sin_addr.s_addr;
        config.targets[i].port = g_slots[i].address.sin_port;
        config.targets[i].keyframe_requests = g_slots[i].keyframe_requests;
    }
    sockaddr_in game{};
    game.sin_family = AF_INET;
    game.sin_port = htons(proto::k_video_control_port);
    game.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    sendto(g_socket, reinterpret_cast<const char*>(&config), sizeof(config), 0, reinterpret_cast<sockaddr*>(&game),
           sizeof(game));
}

bool same_address(const sockaddr_in& a, const sockaddr_in& b) {
    return a.sin_addr.s_addr == b.sin_addr.s_addr && a.sin_port == b.sin_port;
}

void format_address(const sockaddr_in& address, char* out, size_t size) {
    char ip[INET_ADDRSTRLEN] = {};
    inet_ntop(AF_INET, &address.sin_addr, ip, sizeof(ip));
    snprintf(out, size, "%s", ip);
}

template <typename T>
void send_to(const sockaddr_in& to, const T& message) {
    sendto(g_socket, reinterpret_cast<const char*>(&message), sizeof(T), 0, reinterpret_cast<const sockaddr*>(&to),
           sizeof(to));
}

// Con g_mutex tomado.
void release_slot(int index, const char* why) {
    Slot& slot = g_slots[index];
    if (!slot.connected) return;
    log_write("Jugador %d (%s) desconectado: %s", index + k_first_remote_player, slot.nickname, why);
    slot = Slot{};
    pads_reset(index + k_first_remote_player);
}

int slot_of(const sockaddr_in& from) {
    for (int i = 0; i < k_remote_players; ++i)
        if (g_slots[i].connected && same_address(g_slots[i].address, from)) return i;
    return -1;
}

void handle_hello(const proto::Hello& hello, const sockaddr_in& from) {
    proto::Welcome welcome;
    welcome.player = hello.player;
    int index = hello.player - k_first_remote_player;
    char ip[64];
    format_address(from, ip, sizeof(ip));

    if (hello.version != proto::k_version) {
        welcome.reason = proto::Reason::bad_version;
    } else if (index < 0 || index >= k_remote_players) {
        welcome.reason = proto::Reason::bad_player;
    } else {
        Slot& slot = g_slots[index];
        if (slot.connected && !same_address(slot.address, from)) {
            welcome.reason = proto::Reason::slot_taken;
        } else {
            // Si esta misma persona estaba en otro jugador, lo suelta.
            int previous = slot_of(from);
            if (previous >= 0 && previous != index) release_slot(previous, "cambió de jugador");
            bool is_new = !slot.connected;
            slot.connected = true;
            slot.address = from;
            memcpy(slot.nickname, hello.nickname, sizeof(slot.nickname));
            slot.nickname[sizeof(slot.nickname) - 1] = '\0';
            slot.last_seen = GetTickCount64();
            slot.last_sequence = 0;
            welcome.accepted = 1;
            if (is_new) log_write("Jugador %d conectado: \"%s\" desde %s", hello.player, slot.nickname, ip);
        }
    }
    if (!welcome.accepted)
        log_write("Rechazado %s como jugador %d (motivo %d)", ip, hello.player, int(welcome.reason));
    send_to(from, welcome);
}

void handle_input(const proto::Input& input, const sockaddr_in& from) {
    int index = slot_of(from);
    if (index < 0) return;
    Slot& slot = g_slots[index];
    slot.last_seen = GetTickCount64();
    // Paquetes que llegan desordenados: el estado viejo no debe pisar al nuevo.
    if (input.sequence <= slot.last_sequence && slot.last_sequence - input.sequence < 1000) return;
    slot.last_sequence = input.sequence;
    ++slot.inputs;
    PadState state;
    state.buttons = input.pad.buttons;
    state.left_trigger = input.pad.left_trigger;
    state.right_trigger = input.pad.right_trigger;
    state.lx = input.pad.lx;
    state.ly = input.pad.ly;
    state.rx = input.pad.rx;
    state.ry = input.pad.ry;
    pads_update(index + k_first_remote_player, state);
}

void server_thread() {
    char buffer[1500];
    ULONGLONG last_rate = GetTickCount64();
    ULONGLONG last_video_config = 0;
    while (g_running) {
        sockaddr_in from{};
        int from_size = sizeof(from);
        int size = recvfrom(g_socket, buffer, sizeof(buffer), 0, reinterpret_cast<sockaddr*>(&from), &from_size);
        std::lock_guard lock(g_mutex);
        if (size > 0) {
            if (const proto::Header* header = proto::header_of(buffer, size)) {
                switch (header->type) {
                case proto::Type::hello:
                    if (auto* m = proto::as<proto::Hello>(buffer, size)) handle_hello(*m, from);
                    break;
                case proto::Type::input:
                    if (auto* m = proto::as<proto::Input>(buffer, size)) handle_input(*m, from);
                    break;
                case proto::Type::ping:
                    if (auto* m = proto::as<proto::Ping>(buffer, size)) {
                        int index = slot_of(from);
                        if (index >= 0) g_slots[index].last_seen = GetTickCount64();
                        proto::Pong pong;
                        pong.client_time_us = m->client_time_us;
                        send_to(from, pong);
                    }
                    break;
                case proto::Type::bye: {
                    int index = slot_of(from);
                    if (index >= 0) release_slot(index, "salió");
                    break;
                }
                case proto::Type::keyframe_request: {
                    int index = slot_of(from);
                    if (index >= 0) {
                        ++g_slots[index].keyframe_requests;
                        g_slots[index].last_seen = GetTickCount64();
                        send_video_config();  // que llegue ya, no en el próximo envío periódico
                    }
                    break;
                }
                case proto::Type::video_status:
                    if (auto* m = proto::as<proto::VideoStatus>(buffer, size);
                        m && from.sin_addr.s_addr == htonl(INADDR_LOOPBACK)) {
                        g_video_status = *m;
                        g_video_status_time = GetTickCount64();
                    }
                    break;
                default:
                    break;
                }
            }
        }
        ULONGLONG now = GetTickCount64();
        for (int i = 0; i < k_remote_players; ++i)
            if (g_slots[i].connected && now - g_slots[i].last_seen > proto::k_timeout_ms)
                release_slot(i, "sin respuesta");
        if (now - last_video_config >= 250) {
            send_video_config();
            last_video_config = now;
        }
        if (now - last_rate >= 1000) {
            for (Slot& slot : g_slots) {
                slot.inputs_per_second = slot.inputs;
                slot.inputs = 0;
            }
            last_rate = now;
        }
    }
}

}  // namespace

bool net_server_start() {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return false;
    g_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (g_socket == INVALID_SOCKET) return false;

    // Sin esto, un ICMP "puerto inalcanzable" (cliente que se cerró) hace fallar el próximo recvfrom.
    BOOL off = FALSE;
    DWORD returned = 0;
    WSAIoctl(g_socket, SIO_UDP_CONNRESET, &off, sizeof(off), nullptr, 0, &returned, nullptr, nullptr);
    DWORD timeout_ms = 100;  // para revisar desconexiones aunque no llegue nada
    setsockopt(g_socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout_ms), sizeof(timeout_ms));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(proto::k_port);
    address.sin_addr.s_addr = INADDR_ANY;
    if (bind(g_socket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        log_write("No se pudo abrir el puerto UDP %u (error %d)", proto::k_port, WSAGetLastError());
        closesocket(g_socket);
        g_socket = INVALID_SOCKET;
        return false;
    }
    g_running = true;
    g_thread = std::thread(server_thread);
    log_write("Servidor escuchando en UDP %u", proto::k_port);
    return true;
}

void net_server_stop() {
    if (!g_running) return;
    {
        std::lock_guard lock(g_mutex);
        proto::Kicked bye;
        bye.reason = proto::Reason::host_closed;
        for (int i = 0; i < k_remote_players; ++i) {
            if (g_slots[i].connected) send_to(g_slots[i].address, bye);
            release_slot(i, "el host se cerró");
        }
    }
    g_running = false;
    if (g_thread.joinable()) g_thread.join();
    closesocket(g_socket);
    g_socket = INVALID_SOCKET;
    WSACleanup();
}

PlayerInfo net_server_player(int player) {
    PlayerInfo info;
    int index = player - k_first_remote_player;
    if (index < 0 || index >= k_remote_players) return info;
    std::lock_guard lock(g_mutex);
    const Slot& slot = g_slots[index];
    info.connected = slot.connected;
    if (!slot.connected) return info;
    memcpy(info.nickname, slot.nickname, sizeof(info.nickname));
    format_address(slot.address, info.address, sizeof(info.address));
    info.ms_since_packet = unsigned(GetTickCount64() - slot.last_seen);
    info.inputs_per_second = slot.inputs_per_second;
    return info;
}

void net_server_set_fps(int fps) {
    g_fps = fps;
    log_write("Video a %d FPS", fps);
}

int net_server_fps() {
    return g_fps;
}

bool net_server_video_status(proto::VideoStatus& status) {
    std::lock_guard lock(g_mutex);
    if (!g_video_status_time || GetTickCount64() - g_video_status_time > 3000) return false;
    status = g_video_status;
    return true;
}

void net_server_kick(int player) {
    int index = player - k_first_remote_player;
    if (index < 0 || index >= k_remote_players) return;
    std::lock_guard lock(g_mutex);
    if (!g_slots[index].connected) return;
    proto::Kicked kicked;
    send_to(g_slots[index].address, kicked);
    release_slot(index, "expulsado por el host");
}
