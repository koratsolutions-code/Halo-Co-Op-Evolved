#include "audio_player.h"
#include "log.h"

#include "../common/protocol.h"

#include <windows.h>
#include <audioclient.h>
#include <mmdeviceapi.h>

#include <opus.h>

#include <algorithm>
#include <atomic>
#include <deque>
#include <mutex>
#include <thread>

namespace {

constexpr size_t k_channels = proto::k_audio_channels;
constexpr size_t k_samples_per_ms = proto::k_audio_rate / 1000 * k_channels;
constexpr size_t k_start_level = 30 * k_samples_per_ms;  // colchón antes de empezar a sonar
constexpr size_t k_max_level = 100 * k_samples_per_ms;   // si pasa esto, el retraso creció: recortar
constexpr size_t k_trim_level = 40 * k_samples_per_ms;

std::mutex g_mutex;
std::deque<int16_t> g_buffer;
bool g_playing = false;  // false mientras se llena el colchón (al empezar o tras quedarse sin audio)

OpusDecoder* g_decoder = nullptr;  // solo el hilo de red
uint32_t g_last_sequence = 0;
std::atomic<bool> g_has_sequence{false};

std::atomic<unsigned> g_packets{0};
std::atomic<unsigned> g_concealed{0};
std::atomic<float> g_volume{1.0f};
std::atomic<bool> g_muted{false};
std::atomic<bool> g_stop{false};
std::thread g_thread;

void push(const int16_t* pcm, size_t samples) {
    std::lock_guard lock(g_mutex);
    g_buffer.insert(g_buffer.end(), pcm, pcm + samples);
    if (g_buffer.size() > k_max_level) g_buffer.erase(g_buffer.begin(), g_buffer.end() - k_trim_level);
}

// Llena `out` con `frames` del colchón (o silencio si no hay).
void pull(int16_t* out, size_t frames) {
    size_t samples = frames * k_channels;
    float volume = g_muted ? 0.0f : g_volume.load();
    std::lock_guard lock(g_mutex);
    if (!g_playing && g_buffer.size() >= k_start_level) g_playing = true;
    size_t available = g_playing ? std::min(samples, g_buffer.size()) : 0;
    for (size_t i = 0; i < available; ++i) out[i] = int16_t(g_buffer[i] * volume);
    g_buffer.erase(g_buffer.begin(), g_buffer.begin() + available);
    if (available < samples) {
        std::fill(out + available, out + samples, int16_t(0));
        if (g_playing) g_playing = false;  // se vació: volver a juntar colchón
    }
}

// Reproduce hasta que falle el dispositivo (p. ej. se desconectaron los audífonos) o se pida parar.
void play_once() {
    IMMDeviceEnumerator* enumerator = nullptr;
    IMMDevice* device = nullptr;
    IAudioClient* client = nullptr;
    IAudioRenderClient* render = nullptr;
    HANDLE ready = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    WAVEFORMATEX format{WAVE_FORMAT_PCM, WORD(k_channels), proto::k_audio_rate,
                        DWORD(proto::k_audio_rate * k_channels * 2), WORD(k_channels * 2), 16, 0};
    UINT32 buffer_frames = 0;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&enumerator));
    if (SUCCEEDED(hr)) hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
    if (SUCCEEDED(hr)) hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&client));
    if (SUCCEEDED(hr))
        hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM |
                                    AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,
                                200000 /* 20 ms */, 0, &format, nullptr);
    if (SUCCEEDED(hr)) hr = client->SetEventHandle(ready);
    if (SUCCEEDED(hr)) hr = client->GetBufferSize(&buffer_frames);
    if (SUCCEEDED(hr)) hr = client->GetService(IID_PPV_ARGS(&render));
    if (SUCCEEDED(hr)) hr = client->Start();
    if (FAILED(hr)) {
        log_write("Audio: no se pudo abrir la salida de sonido (0x%08lX)", hr);
    } else {
        log_write("Audio: reproduciendo");
        while (!g_stop) {
            WaitForSingleObject(ready, 100);
            UINT32 padding = 0;
            if (FAILED(client->GetCurrentPadding(&padding))) break;  // dispositivo desconectado
            UINT32 frames = buffer_frames - padding;
            if (!frames) continue;
            BYTE* data = nullptr;
            if (FAILED(render->GetBuffer(frames, &data))) break;
            pull(reinterpret_cast<int16_t*>(data), frames);
            render->ReleaseBuffer(frames, 0);
        }
        client->Stop();
    }
    if (render) render->Release();
    if (client) client->Release();
    if (device) device->Release();
    if (enumerator) enumerator->Release();
    CloseHandle(ready);
}

}  // namespace

void audio_start() {
    int error = 0;
    g_decoder = opus_decoder_create(proto::k_audio_rate, int(k_channels), &error);
    if (!g_decoder) log_write("Audio: no se pudo crear el decodificador Opus (%d)", error);
    g_stop = false;
    g_thread = std::thread([] {
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        while (!g_stop) {
            play_once();
            for (int i = 0; i < 10 && !g_stop; ++i) Sleep(100);  // reintentar (p. ej. al cambiar de audífonos)
        }
        CoUninitialize();
    });
}

void audio_stop() {
    g_stop = true;
    if (g_thread.joinable()) g_thread.join();
    if (g_decoder) opus_decoder_destroy(g_decoder);
    g_decoder = nullptr;
}

void audio_on_packet(uint32_t sequence, const uint8_t* data, size_t size) {
    if (!g_decoder) return;
    int16_t pcm[proto::k_audio_frame * k_channels];
    if (g_has_sequence) {
        int32_t gap = int32_t(sequence - g_last_sequence);
        if (gap <= 0) return;  // repetido o desordenado
        // Paquetes perdidos (pocos): que Opus invente un relleno suave en vez de un corte.
        if (gap > 1 && gap <= 4) {
            for (int32_t i = 1; i < gap; ++i) {
                ++g_concealed;
                int samples = opus_decode(g_decoder, nullptr, 0, pcm, proto::k_audio_frame, 0);
                if (samples > 0) push(pcm, size_t(samples) * k_channels);
            }
        }
    }
    g_has_sequence = true;
    g_last_sequence = sequence;
    ++g_packets;
    int samples = opus_decode(g_decoder, data, opus_int32(size), pcm, proto::k_audio_frame, 0);
    if (samples > 0) push(pcm, size_t(samples) * k_channels);
}

void audio_reset() {
    std::lock_guard lock(g_mutex);
    g_buffer.clear();
    g_playing = false;
    g_has_sequence = false;
}

AudioStats audio_take_stats() {
    AudioStats stats;
    stats.packets = g_packets.exchange(0);
    stats.concealed = g_concealed.exchange(0);
    std::lock_guard lock(g_mutex);
    stats.buffered_ms = unsigned(g_buffer.size() / k_samples_per_ms);
    return stats;
}

void audio_set_volume(float volume) {
    g_volume = std::clamp(volume, 0.0f, 1.0f);
}

void audio_set_muted(bool muted) {
    g_muted = muted;
}
