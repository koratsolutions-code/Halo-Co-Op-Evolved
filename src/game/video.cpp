#include "video.h"
#include "local_players.h"
#include "aim.h"
#include "cinematic.h"
#include "encoder.h"
#include "log.h"
#include "skins.h"
#include "split_layout.h"

#include "../common/protocol.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <d3d11_4.h>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace {

constexpr int k_players = 3;  // 2, 3, 4

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

// Configuración que manda el launcher.
std::mutex g_config_mutex;
proto::VideoConfig g_config;
uint64_t g_config_time = 0;

SOCKET g_send_socket = INVALID_SOCKET;

// Estadísticas para el panel del host.
std::atomic<uint32_t> g_frames[k_players];
std::atomic<uint64_t> g_bytes[k_players];
std::atomic<uint16_t> g_game_width{0}, g_game_height{0};
std::atomic<bool> g_encoder_ok{false};
std::atomic<proto::VideoError> g_error{proto::VideoError::none};

void set_error(proto::VideoError error) {
    g_error = error;
}

struct PlayerStream {
    std::unique_ptr<Encoder> encoder;
    uint32_t frame_id = 0;
    uint32_t seen_keyframe_requests = 0;
    bool force_keyframe = true;
    sockaddr_in target{};
    std::mutex target_mutex;  // target lo lee el hilo del codificador
};
PlayerStream g_streams[k_players];

void send_frame(int index, const uint8_t* data, size_t size, bool keyframe, uint64_t capture_us, uint16_t width,
                uint16_t height) {
    PlayerStream& stream = g_streams[index];
    sockaddr_in target;
    {
        std::lock_guard lock(stream.target_mutex);
        target = stream.target;
    }
    if (!target.sin_port) return;
    uint32_t frame_id = ++stream.frame_id;
    uint16_t count = uint16_t((size + proto::k_video_payload - 1) / proto::k_video_payload);
    char packet[sizeof(proto::VideoHeader) + proto::k_video_payload];
    for (uint16_t i = 0; i < count; ++i) {
        proto::VideoHeader header;
        header.frame_id = frame_id;
        header.fragment_index = i;
        header.fragment_count = count;
        header.keyframe = keyframe ? 1 : 0;
        header.width = width;
        header.height = height;
        header.capture_us = capture_us;
        size_t offset = size_t(i) * proto::k_video_payload;
        size_t chunk = std::min<size_t>(proto::k_video_payload, size - offset);
        memcpy(packet, &header, sizeof(header));
        memcpy(packet + sizeof(header), data + offset, chunk);
        sendto(g_send_socket, packet, int(sizeof(header) + chunk), 0, reinterpret_cast<sockaddr*>(&target),
               sizeof(target));
    }
    ++g_frames[index];
    g_bytes[index] += size;
}

// Libera un codificador sin frenar el hilo de render del juego.
void destroy_later(std::unique_ptr<Encoder> encoder) {
    if (encoder) std::thread([e = encoder.release()] { delete e; }).detach();
}

uint32_t bitrate_for(uint32_t width, uint32_t height, uint32_t fps) {
    // ~0.18 bits por píxel: 1280x720 a 60 FPS ≈ 10 Mbps.
    double bps = double(width) * height * fps * 0.18;
    return uint32_t(std::clamp(bps, 3e6, 30e6));
}

void control_thread() {
    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    sockaddr_in local{};
    local.sin_family = AF_INET;
    local.sin_port = htons(proto::k_video_control_port);
    inet_pton(AF_INET, "127.0.0.1", &local.sin_addr);
    if (bind(s, reinterpret_cast<sockaddr*>(&local), sizeof(local)) != 0) {
        log_write("No se pudo abrir el puerto local %u (error %d): sin video", proto::k_video_control_port,
                  WSAGetLastError());
        return;
    }
    DWORD timeout_ms = 250;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout_ms), sizeof(timeout_ms));
    sockaddr_in launcher{};
    launcher.sin_family = AF_INET;
    launcher.sin_port = htons(proto::k_port);
    inet_pton(AF_INET, "127.0.0.1", &launcher.sin_addr);
    log_write("Esperando a Co Op Evolved Host en 127.0.0.1:%u", proto::k_video_control_port);

    uint64_t last_status = now_us();
    bool had_launcher = false;
    while (true) {
        char buffer[512];
        sockaddr_in from{};
        int from_size = sizeof(from);
        int size = recvfrom(s, buffer, sizeof(buffer), 0, reinterpret_cast<sockaddr*>(&from), &from_size);
        if (size > 0 && from.sin_addr.s_addr == htonl(INADDR_LOOPBACK)) {
            if (auto* config = proto::as<proto::VideoConfig>(buffer, size);
                config && config->header.type == proto::Type::video_config) {
                std::lock_guard lock(g_config_mutex);
                g_config = *config;
                g_config_time = now_us();
                if (!had_launcher) log_write("Co Op Evolved Host conectado");
                had_launcher = true;
            } else if (auto* aim = proto::as<proto::Aim>(buffer, size); aim && aim->header.type == proto::Type::aim) {
                for (int i = 0; i < k_players; ++i)
                    if (aim->yaw[i] != 0 || aim->pitch[i] != 0) aim_apply(i + 2, aim->yaw[i], aim->pitch[i]);
            } else if (auto* pads = proto::as<proto::Pads>(buffer, size);
                       pads && pads->header.type == proto::Type::pads) {
                local_players_set_pads(*pads);
            }
        }
        uint64_t now = now_us();
        if (now - last_status >= 1000000) {
            double seconds = (now - last_status) / 1e6;
            last_status = now;
            proto::VideoStatus status;
            status.encoder_ok = g_encoder_ok ? 1 : 0;
            status.game_width = g_game_width;
            status.game_height = g_game_height;
            for (int i = 0; i < k_players; ++i) {
                status.fps_sent[i] = uint16_t(g_frames[i].exchange(0) / seconds + 0.5);
                status.kbps_sent[i] = uint32_t(g_bytes[i].exchange(0) * 8 / 1000 / seconds);
            }
            status.error = g_error;
            status.aim_ready = aim_ready() ? 1 : 0;
            status.skins_available = skins_available();
            status.local_players = GetModuleHandleW(L"halo1.dll") ? uint8_t(split_player_count()) : 0;
            sendto(s, reinterpret_cast<const char*>(&status), sizeof(status), 0,
                   reinterpret_cast<sockaddr*>(&launcher), sizeof(launcher));
        }
    }
}

}  // namespace

void video_start() {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    g_send_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    int buffer_size = 4 * 1024 * 1024;  // ráfagas de un fotograma completo sin descartar paquetes
    setsockopt(g_send_socket, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<const char*>(&buffer_size), sizeof(buffer_size));
    std::thread(control_thread).detach();
}

bool video_config(proto::VideoConfig& out) {
    std::lock_guard lock(g_config_mutex);
    out = g_config;
    return g_config_time && now_us() - g_config_time < 1500000;
}

void video_on_frame(IDXGISwapChain* swap_chain) {
    proto::VideoConfig config;
    bool fresh = video_config(config);  // launcher cerrado -> sin video
    bool any_active = false;
    for (int i = 0; i < k_players; ++i) {
        if (!fresh || !config.targets[i].active) {
            destroy_later(std::move(g_streams[i].encoder));
            std::lock_guard lock(g_streams[i].target_mutex);
            g_streams[i].target = {};
        } else {
            any_active = true;
        }
    }
    if (!any_active) return;

    // Ritmo: como mucho los FPS elegidos en el panel del host. Margen del 20 %: los fotogramas del juego no
    // llegan exactos (a 60 FPS, uno de 15 ms no se debe saltar), pero a 120 FPS se toma uno de cada dos.
    static uint64_t last_capture = 0;
    uint32_t fps = std::clamp<uint32_t>(config.fps, 15, 240);
    uint64_t now = now_us();
    if (now - last_capture < 800000 / fps) return;
    last_capture = now;

    ID3D11Texture2D* back_buffer = nullptr;
    if (FAILED(swap_chain->GetBuffer(0, IID_PPV_ARGS(&back_buffer)))) return;
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    ID3D11DeviceContext4* context4 = nullptr;
    back_buffer->GetDevice(&device);
    device->GetImmediateContext(&context);
    context->QueryInterface(IID_PPV_ARGS(&context4));

    D3D11_TEXTURE2D_DESC desc;
    back_buffer->GetDesc(&desc);
    g_game_width = uint16_t(desc.Width);
    g_game_height = uint16_t(desc.Height);
    uint32_t quad_w = (desc.Width / 2) & ~1u;
    uint32_t quad_h = (desc.Height / 2) & ~1u;

    // Reparto de la pantalla: depende de cuántos jugadores locales tiene la partida (2, 3 o 4).
    const int count = split_player_count();

    // Pantalla entera reducida a un cuarto: para todos durante las cinemáticas (el juego las dibuja una vez sobre
    // toda la pantalla) y para quien se conectó con la misión ya empezada (entra al recargarla: mientras, mira).
    // Al entrar y al salir de una cinemática, un fotograma completo para que el cambio de imagen se vea limpio.
    static bool was_cinematic = false;
    bool cinematic = config.full_cinematics && context4 && desc.SampleDesc.Count == 1 && cinematic_now();
    bool spectators = false;
    for (int i = 0; i < k_players; ++i) spectators |= config.targets[i].active && i + 1 >= count;
    ID3D11Texture2D* whole_screen = nullptr;
    if ((cinematic || spectators) && context4 && desc.SampleDesc.Count == 1)
        whole_screen = cinematic_frame(device, context, back_buffer, quad_w, quad_h);
    if (!whole_screen) cinematic = false;
    if (cinematic != was_cinematic) {
        was_cinematic = cinematic;
        for (auto& stream : g_streams) stream.force_keyframe = true;
        log_write("Video: %s", cinematic ? "cinemática, pantalla entera para todos" : "cada uno con su parte");
    }

    if (context4 && desc.SampleDesc.Count == 1) {
        for (int i = 0; i < k_players; ++i) {
            if (!config.targets[i].active) continue;
            PlayerStream& stream = g_streams[i];
            ViewRect view;
            bool in_game = split_view(i + 1, count, desc.Width, desc.Height, view);
            bool whole = cinematic || !in_game;
            if (whole && !whole_screen) continue;
            if (whole) view = {0, 0, quad_w, quad_h};
            {
                std::lock_guard lock(stream.target_mutex);
                stream.target.sin_family = AF_INET;
                stream.target.sin_addr.s_addr = config.targets[i].ip;
                stream.target.sin_port = config.targets[i].port;
            }
            Encoder::Settings wanted{view.width, view.height, desc.Format, fps,
                                     bitrate_for(view.width, view.height, fps)};
            if (stream.encoder) {
                const auto& current = stream.encoder->settings();
                if (current.width != wanted.width || current.height != wanted.height ||
                    current.format != wanted.format || current.fps != wanted.fps)
                    destroy_later(std::move(stream.encoder));
            }
            if (!stream.encoder) {
                static uint64_t last_failure[k_players] = {};
                if (now - last_failure[i] < 5000000) continue;  // no reintentar en cada fotograma
                auto encoder = std::make_unique<Encoder>();
                auto output = [i, w = uint16_t(view.width), h = uint16_t(view.height)](
                                  const uint8_t* data, size_t size, bool keyframe, uint64_t capture_us) {
                    send_frame(i, data, size, keyframe, capture_us, w, h);
                };
                if (!encoder->open(device, wanted, output)) {
                    last_failure[i] = now;
                    g_encoder_ok = false;
                    set_error(proto::VideoError::nvenc_failed);
                    destroy_later(std::move(encoder));
                    continue;
                }
                g_encoder_ok = true;
                set_error(proto::VideoError::none);
                stream.encoder = std::move(encoder);
                stream.force_keyframe = true;
            }
            if (config.targets[i].keyframe_requests != stream.seen_keyframe_requests) {
                stream.seen_keyframe_requests = config.targets[i].keyframe_requests;
                stream.force_keyframe = true;
            }
            D3D11_BOX box{view.x, view.y, 0, view.x + view.width, view.y + view.height, 1};
            if (stream.encoder->submit(context4, whole ? whole_screen : back_buffer, box, stream.force_keyframe, now))
                stream.force_keyframe = false;
        }
    } else if (!context4) {
        set_error(proto::VideoError::no_d3d11_4);
    } else {
        set_error(proto::VideoError::msaa);
    }

    if (context4) context4->Release();
    context->Release();
    device->Release();
    back_buffer->Release();
}
