#pragma once
#include <d3d11_4.h>

#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <thread>

// Codificador H.264 NVENC de baja latencia para un cuadrante.
// Usa su propio dispositivo D3D11 (compartir el del juego con NVENC causaba bloqueos): el cuadrante se copia
// en el dispositivo del juego a una textura compartida y una ID3D11Fence compartida avisa cuando está lista.
class Encoder {
public:
    // data/size: un fotograma H.264 completo (Annex B). Se llama desde el hilo del codificador.
    using Output = std::function<void(const uint8_t* data, size_t size, bool keyframe, uint64_t capture_us)>;

    struct Settings {
        uint32_t width = 0, height = 0;
        DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
        uint32_t fps = 60;
        uint32_t bitrate = 10'000'000;
    };

    ~Encoder();
    bool open(ID3D11Device* game_device, const Settings& settings, Output output);
    const Settings& settings() const { return m_settings; }

    // Hilo de render del juego. Copia `box` de `source` y lo encola. Si el codificador va atrasado,
    // descarta el fotograma (mejor saltar uno que acumular retraso) y devuelve false.
    bool submit(ID3D11DeviceContext4* context, ID3D11Texture2D* source, const D3D11_BOX& box, bool force_keyframe,
                uint64_t capture_us);

private:
    static constexpr int k_slots = 2;
    struct Job {
        int slot;
        uint64_t fence_value;
        bool force_keyframe;
        uint64_t capture_us;
    };

    void encode_thread();
    void close();

    Settings m_settings;
    Output m_output;
    ID3D11Device* m_device = nullptr;  // propio
    ID3D11DeviceContext* m_context = nullptr;
    ID3D11Fence* m_game_fence = nullptr;
    ID3D11Fence* m_encoder_fence = nullptr;
    HANDLE m_fence_event = nullptr;
    uint64_t m_fence_value = 0;
    ID3D11Texture2D* m_game_textures[k_slots]{};     // lado del juego
    ID3D11Texture2D* m_encoder_textures[k_slots]{};  // mismas texturas, abiertas en nuestro dispositivo
    void* m_registered[k_slots]{};
    bool m_busy[k_slots]{};

    void* m_session = nullptr;
    void* m_bitstream = nullptr;
    void* m_functions = nullptr;  // NV_ENCODE_API_FUNCTION_LIST
    void* m_loaded = nullptr;     // NvencFunctions (nvEncodeAPI64.dll)

    std::thread m_thread;
    std::mutex m_mutex;
    std::condition_variable m_wake;
    bool m_stop = false;
    bool m_has_job = false;
    Job m_job{};
};
