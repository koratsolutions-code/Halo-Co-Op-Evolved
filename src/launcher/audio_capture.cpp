#include "audio_capture.h"
#include "log.h"

#include "../common/protocol.h"

#include <windows.h>
#include <audioclient.h>
#include <audioclientactivationparams.h>
#include <mmdeviceapi.h>

#include <opus.h>

#include <atomic>
#include <cstring>
#include <thread>
#include <vector>

namespace {

std::thread g_thread;
std::atomic<bool> g_stop{false};
std::atomic<bool> g_active{false};

// Recibe el resultado de ActivateAudioInterfaceAsync (que siempre es asíncrono).
class Activation : public IActivateAudioInterfaceCompletionHandler, public IAgileObject {
public:
    HANDLE done = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    HRESULT result = E_FAIL;
    IAudioClient* client = nullptr;

    ~Activation() { CloseHandle(done); }

    STDMETHODIMP QueryInterface(REFIID riid, void** out) override {
        if (riid == __uuidof(IUnknown) || riid == __uuidof(IActivateAudioInterfaceCompletionHandler)) {
            *out = static_cast<IActivateAudioInterfaceCompletionHandler*>(this);
        } else if (riid == __uuidof(IAgileObject)) {
            *out = static_cast<IAgileObject*>(this);
        } else {
            *out = nullptr;
            return E_NOINTERFACE;
        }
        AddRef();
        return S_OK;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return ++m_refs; }
    STDMETHODIMP_(ULONG) Release() override {
        ULONG refs = --m_refs;
        if (!refs) delete this;
        return refs;
    }
    STDMETHODIMP ActivateCompleted(IActivateAudioInterfaceAsyncOperation* operation) override {
        IUnknown* unknown = nullptr;
        HRESULT activate = E_FAIL;
        result = operation->GetActivateResult(&activate, &unknown);
        if (SUCCEEDED(result)) result = activate;
        if (SUCCEEDED(result) && unknown) result = unknown->QueryInterface(IID_PPV_ARGS(&client));
        if (unknown) unknown->Release();
        SetEvent(done);
        return S_OK;
    }

private:
    std::atomic<ULONG> m_refs{1};
};

IAudioClient* activate_process_loopback(DWORD pid) {
    AUDIOCLIENT_ACTIVATION_PARAMS params{};
    params.ActivationType = AUDIOCLIENT_ACTIVATION_TYPE_PROCESS_LOOPBACK;
    params.ProcessLoopbackParams.TargetProcessId = pid;
    params.ProcessLoopbackParams.ProcessLoopbackMode = PROCESS_LOOPBACK_MODE_INCLUDE_TARGET_PROCESS_TREE;
    PROPVARIANT variant{};
    variant.vt = VT_BLOB;
    variant.blob.cbSize = sizeof(params);
    variant.blob.pBlobData = reinterpret_cast<BYTE*>(&params);

    auto* activation = new Activation();
    IActivateAudioInterfaceAsyncOperation* operation = nullptr;
    HRESULT hr = ActivateAudioInterfaceAsync(VIRTUAL_AUDIO_DEVICE_PROCESS_LOOPBACK, __uuidof(IAudioClient), &variant,
                                             activation, &operation);
    IAudioClient* client = nullptr;
    if (SUCCEEDED(hr) && WaitForSingleObject(activation->done, 5000) == WAIT_OBJECT_0 && SUCCEEDED(activation->result)) {
        client = activation->client;
        activation->client = nullptr;
    } else {
        log_write("Audio: no se pudo capturar el sonido de MCC (0x%08lX). Requiere Windows 10 2004 o más nuevo.",
                  SUCCEEDED(hr) ? activation->result : hr);
    }
    if (activation->client) activation->client->Release();
    if (operation) operation->Release();
    activation->Release();
    return client;
}

bool process_alive(DWORD pid) {
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) return false;
    DWORD code = 0;
    bool alive = GetExitCodeProcess(process, &code) && code == STILL_ACTIVE;
    CloseHandle(process);
    return alive;
}

// Captura MCC hasta que se cierre o se pida parar.
void capture(DWORD pid, OpusEncoder* encoder, const AudioSend& send) {
    IAudioClient* client = activate_process_loopback(pid);
    if (!client) return;
    WAVEFORMATEX format{WAVE_FORMAT_PCM, proto::k_audio_channels, proto::k_audio_rate,
                        proto::k_audio_rate * proto::k_audio_channels * 2, proto::k_audio_channels * 2, 16, 0};
    HANDLE ready = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    IAudioCaptureClient* capture_client = nullptr;
    HRESULT hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                    AUDCLNT_STREAMFLAGS_LOOPBACK | AUDCLNT_STREAMFLAGS_EVENTCALLBACK |
                                        AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM,
                                    200000 /* 20 ms */, 0, &format, nullptr);
    if (SUCCEEDED(hr)) hr = client->SetEventHandle(ready);
    if (SUCCEEDED(hr)) hr = client->GetService(IID_PPV_ARGS(&capture_client));
    if (SUCCEEDED(hr)) hr = client->Start();
    if (FAILED(hr)) {
        log_write("Audio: no se pudo iniciar la captura (0x%08lX)", hr);
    } else {
        log_write("Audio: capturando el sonido de MCC");
        g_active = true;
        std::vector<int16_t> pcm;
        uint8_t packet[sizeof(proto::AudioHeader) + 1500];
        proto::AudioHeader header;
        ULONGLONG last_check = GetTickCount64();
        while (!g_stop) {
            WaitForSingleObject(ready, 100);
            UINT32 frames = 0;
            while (SUCCEEDED(capture_client->GetNextPacketSize(&frames)) && frames > 0) {
                BYTE* data = nullptr;
                DWORD flags = 0;
                if (FAILED(capture_client->GetBuffer(&data, &frames, &flags, nullptr, nullptr))) break;
                size_t samples = size_t(frames) * proto::k_audio_channels;
                if (flags & AUDCLNT_BUFFERFLAGS_SILENT)
                    pcm.insert(pcm.end(), samples, 0);
                else
                    pcm.insert(pcm.end(), reinterpret_cast<int16_t*>(data), reinterpret_cast<int16_t*>(data) + samples);
                capture_client->ReleaseBuffer(frames);
            }
            // Paquetes de 10 ms.
            const size_t frame_samples = size_t(proto::k_audio_frame) * proto::k_audio_channels;
            size_t offset = 0;
            while (pcm.size() - offset >= frame_samples) {
                int size = opus_encode(encoder, pcm.data() + offset, proto::k_audio_frame,
                                       packet + sizeof(header), 1500);
                offset += frame_samples;
                if (size <= 0) continue;
                ++header.sequence;
                memcpy(packet, &header, sizeof(header));
                send(packet, sizeof(header) + size);
            }
            pcm.erase(pcm.begin(), pcm.begin() + offset);
            if (GetTickCount64() - last_check > 1000) {
                last_check = GetTickCount64();
                if (!process_alive(pid)) break;
            }
        }
        client->Stop();
        g_active = false;
        log_write("Audio: captura detenida");
    }
    if (capture_client) capture_client->Release();
    client->Release();
    CloseHandle(ready);
}

}  // namespace

void audio_capture_start(FindProcess find_mcc, AudioSend send) {
    g_stop = false;
    g_thread = std::thread([find_mcc, send] {
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        int error = 0;
        OpusEncoder* encoder =
            opus_encoder_create(proto::k_audio_rate, proto::k_audio_channels, OPUS_APPLICATION_RESTRICTED_LOWDELAY, &error);
        if (!encoder) {
            log_write("Audio: no se pudo crear el codificador Opus (%d)", error);
            return;
        }
        opus_encoder_ctl(encoder, OPUS_SET_BITRATE(96000));
        opus_encoder_ctl(encoder, OPUS_SET_COMPLEXITY(5));
        while (!g_stop) {
            DWORD pid = find_mcc();
            if (pid) capture(pid, encoder, send);
            for (int i = 0; i < 20 && !g_stop; ++i) Sleep(100);  // reintentar cada 2 s
        }
        opus_encoder_destroy(encoder);
        CoUninitialize();
    });
}

void audio_capture_stop() {
    g_stop = true;
    if (g_thread.joinable()) g_thread.join();
}

bool audio_capture_active() {
    return g_active;
}
