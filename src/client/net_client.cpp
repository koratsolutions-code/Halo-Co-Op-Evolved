#include "net_client.h"
#include "input.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <mstcpip.h>
#include <timeapi.h>

#include <atomic>
#include <cstring>
#include <mutex>
#include <thread>

namespace {

std::mutex g_mutex;          // protege g_status y g_settings
NetStatus g_status;
Settings g_settings;
std::thread g_thread;
std::atomic<bool> g_stop{false};

uint64_t now_us() {
    static LARGE_INTEGER frequency = [] {
        LARGE_INTEGER f;
        QueryPerformanceFrequency(&f);
        return f;
    }();
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return uint64_t(counter.QuadPart) * 1000000 / uint64_t(frequency.QuadPart);
}

void set_status(NetState state, const std::string& message = {}) {
    std::lock_guard lock(g_mutex);
    g_status.state = state;
    g_status.message = message;
    if (state != NetState::connected) {
        g_status.ping_ms = -1;
        g_status.sent_per_second = 0;
    }
}

const char* reason_text(proto::Reason reason) {
    switch (reason) {
    case proto::Reason::slot_taken: return "Ese jugador ya está ocupado por otra persona. Elige otro número.";
    case proto::Reason::bad_version: return "Tu cliente y el del host son de versiones distintas. Actualicen los dos.";
    case proto::Reason::bad_player: return "Número de jugador inválido (tiene que ser 2, 3 o 4).";
    case proto::Reason::kicked: return "El host te expulsó. Puedes volver a conectarte.";
    case proto::Reason::host_closed: return "El host cerró HaloCoop Host.";
    default: return "Conexión rechazada.";
    }
}

template <typename T>
void send(SOCKET s, const T& message) {
    ::send(s, reinterpret_cast<const char*>(&message), sizeof(T), 0);
}

void run(Settings settings) {
    // Resolver la IP del host.
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    addrinfo* result = nullptr;
    if (getaddrinfo(settings.host.c_str(), std::to_string(proto::k_port).c_str(), &hints, &result) != 0 || !result) {
        set_status(NetState::failed, "No se reconoce la IP del host. Revisa que esté bien escrita.");
        return;
    }
    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    BOOL off = FALSE;
    DWORD returned = 0;
    WSAIoctl(s, SIO_UDP_CONNRESET, &off, sizeof(off), nullptr, 0, &returned, nullptr, nullptr);
    connect(s, result->ai_addr, int(result->ai_addrlen));
    freeaddrinfo(result);
    u_long non_blocking = 1;
    ioctlsocket(s, FIONBIO, &non_blocking);

    proto::Hello hello;
    hello.player = uint8_t(settings.player);
    strncpy_s(hello.nickname, settings.nickname.c_str(), _TRUNCATE);

    timeBeginPeriod(1);  // Sleep(1) de verdad: el input sale cada 4 ms
    set_status(NetState::connecting);
    uint64_t start = now_us(), last_hello = 0, last_ping = 0, last_heard = now_us(), next_send = 0, last_rate = now_us();
    uint32_t sequence = 0;
    unsigned sent = 0;
    bool connected = false;
    std::string end_message;

    while (!g_stop) {
        uint64_t now = now_us();
        char buffer[512];
        int size;
        while ((size = recv(s, buffer, sizeof(buffer), 0)) > 0) {
            const proto::Header* header = proto::header_of(buffer, size);
            if (!header) continue;
            last_heard = now;
            if (header->type == proto::Type::welcome) {
                auto* welcome = proto::as<proto::Welcome>(buffer, size);
                if (!welcome) continue;
                if (!welcome->accepted) {
                    end_message = reason_text(welcome->reason);
                    g_stop = true;
                } else if (!connected) {
                    connected = true;
                    set_status(NetState::connected);
                }
            } else if (header->type == proto::Type::pong) {
                if (auto* pong = proto::as<proto::Pong>(buffer, size)) {
                    std::lock_guard lock(g_mutex);
                    g_status.ping_ms = int((now - pong->client_time_us + 500) / 1000);
                }
            } else if (header->type == proto::Type::kicked) {
                if (auto* kicked = proto::as<proto::Kicked>(buffer, size)) end_message = reason_text(kicked->reason);
                g_stop = true;
            }
        }
        if (g_stop) break;

        if (!connected) {
            if (now - last_hello > 500000) {
                send(s, hello);
                last_hello = now;
            }
            if (now - start > 6000000) {
                end_message = "El host no responde. Revisa la IP, que Radmin VPN esté conectado y que el host tenga "
                              "HaloCoop Host abierto.";
                break;
            }
        } else {
            if (now - last_heard > uint64_t(proto::k_timeout_ms) * 1000) {
                end_message = "Se perdió la conexión con el host.";
                break;
            }
            if (now >= next_send) {
                ControlMode mode;
                float sensitivity;
                {
                    std::lock_guard lock(g_mutex);
                    mode = g_settings.control;
                    sensitivity = g_settings.sensitivity;
                }
                proto::Input input;
                input.sequence = ++sequence;
                input.pad = input_read(mode, sensitivity);
                send(s, input);
                ++sent;
                next_send = now + k_send_interval_ms * 1000;
            }
            if (now - last_ping > 500000) {
                proto::Ping ping;
                ping.client_time_us = now;
                send(s, ping);
                last_ping = now;
            }
            if (now - last_rate >= 1000000) {
                std::lock_guard lock(g_mutex);
                g_status.sent_per_second = sent;
                sent = 0;
                last_rate = now;
            }
        }
        Sleep(1);
    }

    if (connected && end_message.empty()) send(s, proto::Bye{});  // desconexión voluntaria
    closesocket(s);
    timeEndPeriod(1);
    set_status(end_message.empty() ? NetState::idle : NetState::failed, end_message);
}

}  // namespace

void net_connect(const Settings& settings) {
    net_disconnect();
    static bool wsa_ready = [] {
        WSADATA wsa;
        return WSAStartup(MAKEWORD(2, 2), &wsa) == 0;
    }();
    (void)wsa_ready;
    {
        std::lock_guard lock(g_mutex);
        g_settings = settings;
    }
    g_stop = false;
    set_status(NetState::connecting);
    g_thread = std::thread(run, settings);
}

void net_update_settings(const Settings& settings) {
    std::lock_guard lock(g_mutex);
    g_settings = settings;
}

void net_disconnect() {
    g_stop = true;
    if (g_thread.joinable()) g_thread.join();
}

NetStatus net_status() {
    std::lock_guard lock(g_mutex);
    return g_status;
}
