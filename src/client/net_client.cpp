#include "net_client.h"
#include "audio_player.h"
#include "input.h"

#include "../common/lang.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <mstcpip.h>
#include <timeapi.h>

#include <atomic>
#include <cstring>
#include <map>
#include <mutex>
#include <thread>

namespace {

std::mutex g_mutex;          // protege g_status y g_settings
NetStatus g_status;
Settings g_settings;
std::thread g_thread;
std::atomic<bool> g_stop{false};
std::atomic<bool> g_keyframe_wanted{false};

std::mutex g_frames_mutex;
std::vector<VideoFrame> g_frames;
HANDLE g_frame_event = CreateEventW(nullptr, FALSE, FALSE, nullptr);

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
    g_status = NetStatus{};
    g_status.state = state;
    g_status.message = message;
}

const char* reason_text(proto::Reason reason) {
    switch (reason) {
    case proto::Reason::slot_taken: return T("That player is already taken by someone else. Pick another number.", "Ese jugador ya está ocupado por otra persona. Elige otro número.");
    case proto::Reason::bad_version: return T("Your client and the host's are different versions. Update both.", "Tu cliente y el del host son de versiones distintas. Actualicen los dos.");
    case proto::Reason::bad_player: return T("Invalid player number (it must be 2, 3 or 4).", "Número de jugador inválido (tiene que ser 2, 3 o 4).");
    case proto::Reason::kicked: return T("The host kicked you. You can connect again.", "El host te expulsó. Puedes volver a conectarte.");
    case proto::Reason::host_closed: return T("The host closed Co Op Evolved Host.", "El host cerró Co Op Evolved Host.");
    default: return T("Connection refused.", "Conexión rechazada.");
    }
}

// Arma fotogramas a partir de los fragmentos. Como el video no tiene fotogramas completos periódicos,
// si se pierde uno hay que pedir uno completo y descartar hasta que llegue.
class FrameAssembler {
public:
    // Devuelve true si hay que pedir un fotograma completo.
    bool add(const proto::VideoHeader& h, const uint8_t* payload, size_t size, uint64_t now) {
        // Paquetes mal formados (o de alguien que no es el host): no se escribe fuera del búfer ni se reserva de más.
        if (h.fragment_count == 0 || h.fragment_count > k_max_fragments || h.fragment_index >= h.fragment_count ||
            size == 0 || size > proto::k_video_payload)
            return false;
        if (m_has_delivered && int32_t(h.frame_id - m_last_delivered) <= 0) return false;  // viejo o repetido
        Partial& p = m_partials[h.frame_id];
        if (p.received.empty()) {
            p.received.assign(h.fragment_count, false);
            p.data.resize(size_t(h.fragment_count) * proto::k_video_payload);
            p.keyframe = h.keyframe;
            p.width = h.width;
            p.height = h.height;
        }
        if (p.received.size() != h.fragment_count || p.received[h.fragment_index]) return false;
        p.received[h.fragment_index] = true;
        ++p.count;
        memcpy(p.data.data() + size_t(h.fragment_index) * proto::k_video_payload, payload, size);
        if (h.fragment_index == h.fragment_count - 1)
            p.size = size_t(h.fragment_index) * proto::k_video_payload + size;
        if (p.count < h.fragment_count) {
            // Si ya hay dos fotogramas más nuevos en camino, este no se va a completar.
            if (m_partials.size() > 3) return drop_oldest();
            return false;
        }

        bool in_order = !m_has_delivered || h.frame_id == m_last_delivered + 1;
        bool want_keyframe = false;
        if (p.keyframe) {
            m_waiting_keyframe = false;
        } else if (!in_order) {
            ++m_lost;
            m_waiting_keyframe = true;
            want_keyframe = true;
        }
        if (!m_waiting_keyframe) {
            VideoFrame frame;
            p.data.resize(p.size);
            frame.data = std::move(p.data);
            frame.keyframe = p.keyframe;
            frame.width = p.width;
            frame.height = p.height;
            frame.received_us = now;
            {
                std::lock_guard lock(g_frames_mutex);
                // Si la ventana no da abasto (minimizada), mejor empezar de nuevo desde un fotograma completo.
                if (g_frames.size() > 30) {
                    g_frames.clear();
                    m_waiting_keyframe = true;
                    want_keyframe = true;
                } else {
                    g_frames.push_back(std::move(frame));
                    ++m_delivered;
                }
            }
            SetEvent(g_frame_event);
        }
        m_has_delivered = true;
        m_last_delivered = h.frame_id;
        // Todo lo anterior a este ya no sirve.
        for (auto it = m_partials.begin(); it != m_partials.end();)
            it = int32_t(it->first - h.frame_id) <= 0 ? m_partials.erase(it) : std::next(it);
        return want_keyframe;
    }

    bool waiting_keyframe() const { return m_waiting_keyframe; }
    void set_waiting_keyframe() { m_waiting_keyframe = true; }
    unsigned lost() const { return m_lost; }
    unsigned delivered() const { return m_delivered; }

private:
    static constexpr uint16_t k_max_fragments = 4096;  // ~4.8 MB: más que cualquier fotograma completo real

    struct Partial {
        std::vector<uint8_t> data;
        std::vector<bool> received;
        unsigned count = 0;
        size_t size = 0;
        bool keyframe = false;
        uint16_t width = 0, height = 0;
    };

    bool drop_oldest() {
        auto oldest = m_partials.begin();
        for (auto it = m_partials.begin(); it != m_partials.end(); ++it)
            if (int32_t(it->first - oldest->first) < 0) oldest = it;
        m_last_delivered = oldest->first;  // se da por perdido
        m_has_delivered = true;
        m_partials.erase(oldest);
        ++m_lost;
        m_waiting_keyframe = true;
        return true;
    }

    std::map<uint32_t, Partial> m_partials;
    bool m_has_delivered = false;
    uint32_t m_last_delivered = 0;
    bool m_waiting_keyframe = true;  // el primero que se muestra tiene que ser completo
    unsigned m_lost = 0;
    unsigned m_delivered = 0;
};

void run(Settings settings) {
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    addrinfo* result = nullptr;
    if (getaddrinfo(settings.host.c_str(), std::to_string(proto::k_port).c_str(), &hints, &result) != 0 || !result) {
        set_status(NetState::failed, T("The host IP is not valid. Check that it is typed correctly.", "No se reconoce la IP del host. Revisa que esté bien escrita."));
        return;
    }
    sockaddr_in host = *reinterpret_cast<sockaddr_in*>(result->ai_addr);
    freeaddrinfo(result);

    // Socket sin "connect": el video llega desde otro puerto del host (el del juego).
    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    BOOL off = FALSE;
    DWORD returned = 0;
    WSAIoctl(s, SIO_UDP_CONNRESET, &off, sizeof(off), nullptr, 0, &returned, nullptr, nullptr);
    int buffer_size = 4 * 1024 * 1024;
    setsockopt(s, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&buffer_size), sizeof(buffer_size));
    sockaddr_in any{};
    any.sin_family = AF_INET;
    bind(s, reinterpret_cast<sockaddr*>(&any), sizeof(any));
    // Evento que se activa al llegar datos: esperar en él respeta timeBeginPeriod(1) (WSAPoll no).
    WSAEVENT readable = WSACreateEvent();
    WSAEventSelect(s, readable, FD_READ);  // deja el socket en modo no bloqueante
    auto send_message = [&](const auto& message) {
        sendto(s, reinterpret_cast<const char*>(&message), sizeof(message), 0, reinterpret_cast<sockaddr*>(&host),
               sizeof(host));
    };

    proto::Hello hello;
    hello.player = uint8_t(settings.player);
    strncpy_s(hello.nickname, settings.nickname.c_str(), _TRUNCATE);

    timeBeginPeriod(1);
    set_status(NetState::connecting);
    {
        std::lock_guard lock(g_frames_mutex);
        g_frames.clear();
    }
    FrameAssembler assembler;
    uint64_t start = now_us(), last_hello = 0, last_ping = 0, last_heard = now_us(), next_send = 0,
             last_rate = now_us(), last_keyframe_request = 0, last_video = 0;
    uint32_t sequence = 0;
    unsigned sent = 0, video_bytes = 0, frames_completed = 0;
    bool connected = false;
    std::string end_message;
    g_keyframe_wanted = false;

    while (!g_stop) {
        // Esperar datos como mucho 1 ms: el input tiene que salir cada 4 ms.
        WaitForSingleObject(readable, 1);
        WSAResetEvent(readable);
        uint64_t now = now_us();
        char buffer[1600];
        sockaddr_in from{};
        int from_size = sizeof(from);
        int size;
        while ((size = recvfrom(s, buffer, sizeof(buffer), 0, reinterpret_cast<sockaddr*>(&from), &from_size)) > 0) {
            from_size = sizeof(from);
            if (from.sin_addr.s_addr != host.sin_addr.s_addr) continue;  // solo del host
            const proto::Header* header = proto::header_of(buffer, size);
            if (!header) continue;
            last_heard = now;
            switch (header->type) {
            case proto::Type::welcome:
                if (auto* welcome = proto::as<proto::Welcome>(buffer, size)) {
                    if (!welcome->accepted) {
                        end_message = reason_text(welcome->reason);
                        g_stop = true;
                    } else if (!connected) {
                        connected = true;
                        set_status(NetState::connected);
                    }
                }
                break;
            case proto::Type::pong:
                if (auto* pong = proto::as<proto::Pong>(buffer, size)) {
                    std::lock_guard lock(g_mutex);
                    g_status.ping_ms = int((now - pong->client_time_us + 500) / 1000);
                }
                break;
            case proto::Type::kicked:
                if (auto* kicked = proto::as<proto::Kicked>(buffer, size)) end_message = reason_text(kicked->reason);
                g_stop = true;
                break;
            case proto::Type::audio:
                if (connected && size > int(sizeof(proto::AudioHeader))) {
                    auto* h = reinterpret_cast<const proto::AudioHeader*>(buffer);
                    audio_on_packet(h->sequence, reinterpret_cast<const uint8_t*>(buffer) + sizeof(proto::AudioHeader),
                                    size - sizeof(proto::AudioHeader));
                }
                break;
            case proto::Type::video:
                if (connected && size > int(sizeof(proto::VideoHeader))) {
                    auto* h = reinterpret_cast<const proto::VideoHeader*>(buffer);
                    video_bytes += unsigned(size);
                    last_video = now;
                    if (assembler.add(*h, reinterpret_cast<const uint8_t*>(buffer) + sizeof(proto::VideoHeader),
                                      size - sizeof(proto::VideoHeader), now))
                        g_keyframe_wanted = true;
                }
                break;
            default:
                break;
            }
            if (g_stop) break;
        }
        if (g_stop) break;

        if (!connected) {
            if (now - last_hello > 500000) {
                send_message(hello);
                last_hello = now;
            }
            if (now - start > 6000000) {
                end_message = T("The host is not responding. Check the IP, that Radmin VPN is connected and that the host "
                                "has Co Op Evolved Host open.",
                                "El host no responde. Revisa la IP, que Radmin VPN esté conectado y que el host tenga "
                                "Co Op Evolved Host abierto.");
                break;
            }
            continue;
        }
        if (now - last_heard > uint64_t(proto::k_timeout_ms) * 1000) {
            end_message = T("Lost the connection to the host.", "Se perdió la conexión con el host.");
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
            send_message(input);
            ++sent;
            // Ritmo fijo (no "ahora + 4 ms"): si no, el retraso de cada espera se acumula.
            next_send += k_send_interval_ms * 1000;
            if (next_send < now) next_send = now + k_send_interval_ms * 1000;
        }
        // Fotograma completo: si se perdió algo (con reintento si no llega), o si el decodificador lo pide.
        if (g_keyframe_wanted.exchange(false)) assembler.set_waiting_keyframe();
        if (assembler.waiting_keyframe() && last_video && now - last_keyframe_request > 300000) {
            send_message(proto::KeyframeRequest{});
            last_keyframe_request = now;
        }
        if (now - last_ping > 500000) {
            proto::Ping ping;
            ping.client_time_us = now;
            send_message(ping);
            proto::PlayerOptions options;  // se repite: si se pierde uno, llega el siguiente
            {
                std::lock_guard lock(g_mutex);
                options.armor_color = uint8_t(g_settings.armor_color);
            }
            send_message(options);
            last_ping = now;
        }
        if (now - last_rate >= 1000000) {
            double seconds = (now - last_rate) / 1e6;
            std::lock_guard lock(g_mutex);
            g_status.sent_per_second = sent;
            g_status.video_fps = unsigned((assembler.delivered() - frames_completed) / seconds + 0.5);
            frames_completed = assembler.delivered();
            g_status.video_kbps = unsigned(video_bytes * 8 / 1000 / seconds);
            g_status.frames_lost = assembler.lost();
            sent = 0;
            video_bytes = 0;
            last_rate = now;
        }
    }

    if (connected && end_message.empty()) send_message(proto::Bye{});  // desconexión voluntaria
    closesocket(s);
    WSACloseEvent(readable);
    audio_reset();
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

HANDLE net_frame_event() {
    return g_frame_event;
}

void net_take_frames(std::vector<VideoFrame>& out) {
    std::lock_guard lock(g_frames_mutex);
    out.swap(g_frames);
    g_frames.clear();
}

void net_request_keyframe() {
    g_keyframe_wanted = true;
}
