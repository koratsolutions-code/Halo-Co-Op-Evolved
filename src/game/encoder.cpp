#include "encoder.h"
#include "log.h"

#include <dxgi1_2.h>

#define FFNV_LOG_FUNC(logctx, msg, ...) log_write("NVENC: " msg, __VA_ARGS__)
#define FFNV_DEBUG_LOG_FUNC(logctx, msg, ...)
#include <ffnvcodec/dynlink_loader.h>

namespace {

NV_ENCODE_API_FUNCTION_LIST& api(void* p) {
    return *static_cast<NV_ENCODE_API_FUNCTION_LIST*>(p);
}

NV_ENC_BUFFER_FORMAT buffer_format(DXGI_FORMAT format) {
    switch (format) {
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: return NV_ENC_BUFFER_FORMAT_ABGR;
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: return NV_ENC_BUFFER_FORMAT_ARGB;
    case DXGI_FORMAT_R10G10B10A2_UNORM: return NV_ENC_BUFFER_FORMAT_ABGR10;
    default: return NV_ENC_BUFFER_FORMAT_UNDEFINED;
    }
}

// Las texturas compartidas no pueden ser sRGB "tipadas" de forma distinta en cada lado: se usa el formato base.
DXGI_FORMAT texture_format(DXGI_FORMAT format) {
    switch (format) {
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: return DXGI_FORMAT_R8G8B8A8_UNORM;
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: return DXGI_FORMAT_B8G8R8A8_UNORM;
    default: return format;
    }
}

template <typename T>
void release(T*& p) {
    if (p) p->Release();
    p = nullptr;
}

}  // namespace

Encoder::~Encoder() {
    close();
}

bool Encoder::open(ID3D11Device* game_device, const Settings& settings, Output output) {
    m_settings = settings;
    m_output = std::move(output);
    NV_ENC_BUFFER_FORMAT nv_format = buffer_format(settings.format);
    if (nv_format == NV_ENC_BUFFER_FORMAT_UNDEFINED) {
        log_write("Formato de pantalla del juego no soportado: %d", int(settings.format));
        return false;
    }

    // Nuestro dispositivo, en la misma GPU que el juego.
    IDXGIDevice* dxgi_device = nullptr;
    IDXGIAdapter* adapter = nullptr;
    game_device->QueryInterface(IID_PPV_ARGS(&dxgi_device));
    if (dxgi_device) dxgi_device->GetAdapter(&adapter);
    HRESULT hr = D3D11CreateDevice(adapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION,
                                   &m_device, nullptr, &m_context);
    release(adapter);
    release(dxgi_device);
    if (FAILED(hr)) {
        log_write("No se pudo crear el dispositivo D3D11 del codificador (0x%08lX)", hr);
        return false;
    }

    // Texturas compartidas y fence compartida.
    ID3D11Device5* game5 = nullptr;
    ID3D11Device5* mine5 = nullptr;
    game_device->QueryInterface(IID_PPV_ARGS(&game5));
    m_device->QueryInterface(IID_PPV_ARGS(&mine5));
    bool ok = game5 && mine5;
    HANDLE fence_handle = nullptr;
    if (ok) ok = SUCCEEDED(game5->CreateFence(0, D3D11_FENCE_FLAG_SHARED, IID_PPV_ARGS(&m_game_fence)));
    if (ok) ok = SUCCEEDED(m_game_fence->CreateSharedHandle(nullptr, GENERIC_ALL, nullptr, &fence_handle));
    if (ok) ok = SUCCEEDED(mine5->OpenSharedFence(fence_handle, IID_PPV_ARGS(&m_encoder_fence)));
    if (fence_handle) CloseHandle(fence_handle);
    release(game5);
    release(mine5);
    if (!ok) {
        log_write("No se pudo crear la fence compartida (requiere Windows 10 y D3D11.4)");
        return false;
    }
    m_fence_event = CreateEventW(nullptr, FALSE, FALSE, nullptr);

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = settings.width;
    desc.Height = settings.height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = texture_format(settings.format);
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;
    for (int i = 0; i < k_slots; ++i) {
        IDXGIResource* resource = nullptr;
        HANDLE shared = nullptr;
        ok = SUCCEEDED(game_device->CreateTexture2D(&desc, nullptr, &m_game_textures[i])) &&
             SUCCEEDED(m_game_textures[i]->QueryInterface(IID_PPV_ARGS(&resource))) &&
             SUCCEEDED(resource->GetSharedHandle(&shared)) &&
             SUCCEEDED(m_device->OpenSharedResource(shared, IID_PPV_ARGS(&m_encoder_textures[i])));
        release(resource);
        if (!ok) {
            log_write("No se pudo crear la textura compartida %d", i);
            return false;
        }
    }

    // NVENC
    NvencFunctions* loaded = nullptr;
    if (nvenc_load_functions(&loaded, nullptr) != 0) {
        log_write("No se encontró NVENC (nvEncodeAPI64.dll): ¿GPU NVIDIA con drivers actualizados?");
        return false;
    }
    m_loaded = loaded;
    auto* functions = new NV_ENCODE_API_FUNCTION_LIST{NV_ENCODE_API_FUNCTION_LIST_VER};
    m_functions = functions;
    if (loaded->NvEncodeAPICreateInstance(functions) != NV_ENC_SUCCESS) {
        log_write("NvEncodeAPICreateInstance falló: el driver de NVIDIA es demasiado antiguo, actualízalo");
        return false;
    }
    NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS session{NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS_VER};
    session.device = m_device;
    session.deviceType = NV_ENC_DEVICE_TYPE_DIRECTX;
    session.apiVersion = NVENCAPI_VERSION;
    NVENCSTATUS status = functions->nvEncOpenEncodeSessionEx(&session, &m_session);
    if (status != NV_ENC_SUCCESS) {
        log_write("No se pudo abrir la sesión NVENC (%d)", int(status));
        m_session = nullptr;
        return false;
    }

    NV_ENC_PRESET_CONFIG preset{};
    preset.version = NV_ENC_PRESET_CONFIG_VER;
    preset.presetCfg.version = NV_ENC_CONFIG_VER;
    status = functions->nvEncGetEncodePresetConfigEx(m_session, NV_ENC_CODEC_H264_GUID, NV_ENC_PRESET_P1_GUID,
                                                     NV_ENC_TUNING_INFO_ULTRA_LOW_LATENCY, &preset);
    if (status != NV_ENC_SUCCESS) {
        log_write("nvEncGetEncodePresetConfigEx falló (%d): %s", int(status),
                  functions->nvEncGetLastErrorString(m_session));
        return false;
    }
    NV_ENC_CONFIG config = preset.presetCfg;
    config.version = NV_ENC_CONFIG_VER;
    config.gopLength = NVENC_INFINITE_GOPLENGTH;  // fotogramas completos solo cuando un cliente los pide
    config.frameIntervalP = 1;                    // sin fotogramas B
    config.rcParams.rateControlMode = NV_ENC_PARAMS_RC_CBR;
    config.rcParams.averageBitRate = settings.bitrate;
    config.rcParams.maxBitRate = settings.bitrate;
    config.rcParams.vbvBufferSize = settings.bitrate / settings.fps;  // un fotograma: sin colchón = sin retraso
    config.rcParams.vbvInitialDelay = config.rcParams.vbvBufferSize;
    config.encodeCodecConfig.h264Config.idrPeriod = NVENC_INFINITE_GOPLENGTH;
    config.encodeCodecConfig.h264Config.repeatSPSPPS = 1;

    NV_ENC_INITIALIZE_PARAMS init{NV_ENC_INITIALIZE_PARAMS_VER};
    init.encodeGUID = NV_ENC_CODEC_H264_GUID;
    init.presetGUID = NV_ENC_PRESET_P1_GUID;
    init.tuningInfo = NV_ENC_TUNING_INFO_ULTRA_LOW_LATENCY;
    init.encodeWidth = init.darWidth = settings.width;
    init.encodeHeight = init.darHeight = settings.height;
    init.frameRateNum = settings.fps;
    init.frameRateDen = 1;
    init.enablePTD = 1;
    init.encodeConfig = &config;
    status = functions->nvEncInitializeEncoder(m_session, &init);
    if (status != NV_ENC_SUCCESS) {
        log_write("nvEncInitializeEncoder falló (%d): %s", int(status), functions->nvEncGetLastErrorString(m_session));
        return false;
    }

    for (int i = 0; i < k_slots; ++i) {
        NV_ENC_REGISTER_RESOURCE reg{NV_ENC_REGISTER_RESOURCE_VER};
        reg.resourceType = NV_ENC_INPUT_RESOURCE_TYPE_DIRECTX;
        reg.resourceToRegister = m_encoder_textures[i];
        reg.width = settings.width;
        reg.height = settings.height;
        reg.bufferFormat = nv_format;
        reg.bufferUsage = NV_ENC_INPUT_IMAGE;
        status = functions->nvEncRegisterResource(m_session, &reg);
        if (status != NV_ENC_SUCCESS) {
            log_write("nvEncRegisterResource falló (%d)", int(status));
            return false;
        }
        m_registered[i] = reg.registeredResource;
    }
    NV_ENC_CREATE_BITSTREAM_BUFFER bitstream{NV_ENC_CREATE_BITSTREAM_BUFFER_VER};
    if (functions->nvEncCreateBitstreamBuffer(m_session, &bitstream) != NV_ENC_SUCCESS) return false;
    m_bitstream = bitstream.bitstreamBuffer;

    m_thread = std::thread(&Encoder::encode_thread, this);
    log_write("Codificador listo: %ux%u, %u fps, %.1f Mbps", settings.width, settings.height, settings.fps,
              settings.bitrate / 1e6);
    return true;
}

bool Encoder::submit(ID3D11DeviceContext4* context, ID3D11Texture2D* source, const D3D11_BOX& box,
                     bool force_keyframe, uint64_t capture_us) {
    int slot = -1;
    {
        std::lock_guard lock(m_mutex);
        if (m_has_job) return false;  // todavía hay uno esperando: descartar este
        for (int i = 0; i < k_slots; ++i)
            if (!m_busy[i]) {
                slot = i;
                break;
            }
        if (slot < 0) return false;
        m_busy[slot] = true;
    }
    context->CopySubresourceRegion(m_game_textures[slot], 0, 0, 0, 0, source, 0, &box);
    uint64_t value = ++m_fence_value;
    context->Signal(m_game_fence, value);
    {
        std::lock_guard lock(m_mutex);
        m_job = {slot, value, force_keyframe, capture_us};
        m_has_job = true;
    }
    m_wake.notify_one();
    return true;
}

void Encoder::encode_thread() {
    auto& fn = api(m_functions);
    while (true) {
        Job job;
        {
            std::unique_lock lock(m_mutex);
            m_wake.wait(lock, [&] { return m_stop || m_has_job; });
            if (m_stop) return;
            job = m_job;
            m_has_job = false;
        }
        // Esperar a que la GPU termine de copiar el cuadrante.
        m_encoder_fence->SetEventOnCompletion(job.fence_value, m_fence_event);
        if (WaitForSingleObject(m_fence_event, 200) != WAIT_OBJECT_0) {
            std::lock_guard lock(m_mutex);
            m_busy[job.slot] = false;
            continue;
        }

        NV_ENC_MAP_INPUT_RESOURCE map{NV_ENC_MAP_INPUT_RESOURCE_VER};
        map.registeredResource = m_registered[job.slot];
        if (fn.nvEncMapInputResource(m_session, &map) == NV_ENC_SUCCESS) {
            NV_ENC_PIC_PARAMS pic{NV_ENC_PIC_PARAMS_VER};
            pic.inputBuffer = map.mappedResource;
            pic.bufferFmt = map.mappedBufferFmt;
            pic.inputWidth = m_settings.width;
            pic.inputHeight = m_settings.height;
            pic.outputBitstream = m_bitstream;
            pic.pictureStruct = NV_ENC_PIC_STRUCT_FRAME;
            if (job.force_keyframe) pic.encodePicFlags = NV_ENC_PIC_FLAG_FORCEIDR | NV_ENC_PIC_FLAG_OUTPUT_SPSPPS;
            if (fn.nvEncEncodePicture(m_session, &pic) == NV_ENC_SUCCESS) {
                NV_ENC_LOCK_BITSTREAM lock_bs{NV_ENC_LOCK_BITSTREAM_VER};
                lock_bs.outputBitstream = m_bitstream;
                if (fn.nvEncLockBitstream(m_session, &lock_bs) == NV_ENC_SUCCESS) {
                    bool keyframe = lock_bs.pictureType == NV_ENC_PIC_TYPE_IDR || lock_bs.pictureType == NV_ENC_PIC_TYPE_I;
                    m_output(static_cast<const uint8_t*>(lock_bs.bitstreamBufferPtr), lock_bs.bitstreamSizeInBytes,
                             keyframe, job.capture_us);
                    fn.nvEncUnlockBitstream(m_session, m_bitstream);
                }
            }
            fn.nvEncUnmapInputResource(m_session, map.mappedResource);
        }
        std::lock_guard lock(m_mutex);
        m_busy[job.slot] = false;
    }
}

void Encoder::close() {
    {
        std::lock_guard lock(m_mutex);
        m_stop = true;
    }
    m_wake.notify_all();
    if (m_thread.joinable()) m_thread.join();
    if (m_session) {
        auto& fn = api(m_functions);
        if (m_bitstream) fn.nvEncDestroyBitstreamBuffer(m_session, m_bitstream);
        for (void*& r : m_registered)
            if (r) fn.nvEncUnregisterResource(m_session, r), r = nullptr;
        fn.nvEncDestroyEncoder(m_session);
        m_session = nullptr;
    }
    delete static_cast<NV_ENCODE_API_FUNCTION_LIST*>(m_functions);
    m_functions = nullptr;
    if (m_loaded) {
        auto* loaded = static_cast<NvencFunctions*>(m_loaded);
        nvenc_free_functions(&loaded);
        m_loaded = nullptr;
    }
    for (int i = 0; i < k_slots; ++i) {
        release(m_encoder_textures[i]);
        release(m_game_textures[i]);
    }
    release(m_encoder_fence);
    release(m_game_fence);
    if (m_fence_event) CloseHandle(m_fence_event);
    m_fence_event = nullptr;
    release(m_context);
    release(m_device);
}
