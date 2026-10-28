#include "decoder.h"

#include "../common/lang.h"

#include <dxgi.h>

#include <chrono>
#include <cstdio>

#define FFNV_LOG_FUNC(logctx, msg, ...) ((void)0)
#define FFNV_DEBUG_LOG_FUNC(logctx, msg, ...) ((void)0)
#include <ffnvcodec/dynlink_loader.h>

namespace {

CudaFunctions* cu(void* p) {
    return static_cast<CudaFunctions*>(p);
}
CuvidFunctions* cv(void* p) {
    return static_cast<CuvidFunctions*>(p);
}

int CUDAAPI sequence_callback(void* user, CUVIDEOFORMAT* format) {
    return static_cast<Decoder*>(user)->on_sequence(format);
}
int CUDAAPI decode_callback(void* user, CUVIDPICPARAMS* picture) {
    return static_cast<Decoder*>(user)->on_decode(picture);
}
int CUDAAPI display_callback(void* user, CUVIDPARSERDISPINFO* info) {
    return static_cast<Decoder*>(user)->on_display(info);
}

template <typename T>
void release(T*& p) {
    if (p) p->Release();
    p = nullptr;
}

}  // namespace

Decoder::~Decoder() {
    if (m_cuda && m_context) cu(m_cuda)->cuCtxPushCurrent(static_cast<CUcontext>(m_context));
    if (m_parser) cv(m_cuvid)->cuvidDestroyVideoParser(static_cast<CUvideoparser>(m_parser));
    if (m_decoder) cv(m_cuvid)->cuvidDestroyDecoder(static_cast<CUvideodecoder>(m_decoder));
    release_textures();
    if (m_cuda && m_context) {
        CUcontext popped;
        cu(m_cuda)->cuCtxPopCurrent(&popped);
    }
    if (m_lock) cv(m_cuvid)->cuvidCtxLockDestroy(static_cast<CUvideoctxlock>(m_lock));
    if (m_context) cu(m_cuda)->cuCtxDestroy(static_cast<CUcontext>(m_context));
    auto* cuda = cu(m_cuda);
    auto* cuvid = cv(m_cuvid);
    cuvid_free_functions(&cuvid);
    cuda_free_functions(&cuda);
}

bool Decoder::init(ID3D11Device* device, std::string& error) {
    m_device = device;
    CudaFunctions* cuda = nullptr;
    CuvidFunctions* cuvid = nullptr;
    if (cuda_load_functions(&cuda, nullptr) != 0 || cuvid_load_functions(&cuvid, nullptr) != 0) {
        error = T("NVIDIA's video decoder was not found (nvcuda.dll / nvcuvid.dll). You need an NVIDIA GPU with "
                  "up-to-date drivers.",
                  "No se encontró el decodificador de NVIDIA (nvcuda.dll / nvcuvid.dll). Necesitas una GPU NVIDIA "
                  "con drivers actualizados.");
        return false;
    }
    m_cuda = cuda;
    m_cuvid = cuvid;
    if (cuda->cuInit(0) != CUDA_SUCCESS) {
        error = T("Could not start CUDA: update your NVIDIA drivers.", "No se pudo iniciar CUDA: actualiza los drivers de NVIDIA.");
        return false;
    }
    // CUDA en la misma GPU que dibuja la ventana.
    IDXGIDevice* dxgi_device = nullptr;
    IDXGIAdapter* adapter = nullptr;
    device->QueryInterface(IID_PPV_ARGS(&dxgi_device));
    if (dxgi_device) dxgi_device->GetAdapter(&adapter);
    CUdevice cuda_device = 0;
    CUresult result = adapter ? cuda->cuD3D11GetDevice(&cuda_device, adapter) : CUresult(100);  // sin dispositivo
    release(adapter);
    release(dxgi_device);
    if (result != CUDA_SUCCESS) {
        error = T("The screen is not driven by the NVIDIA GPU (laptop with integrated graphics?). In Windows graphics "
                  "settings choose 'High performance' for HaloCoopClient.exe.",
                  "La pantalla no está conectada a la GPU NVIDIA (¿laptop con gráficos integrados?). En Configuración "
                  "de gráficos de Windows elige 'Alto rendimiento' para HaloCoopClient.exe.");
        return false;
    }
    CUcontext context = nullptr;
    if (cuda->cuCtxCreate(&context, 0, cuda_device) != CUDA_SUCCESS) {
        error = T("Could not create the CUDA context.", "No se pudo crear el contexto CUDA.");
        return false;
    }
    m_context = context;
    CUvideoctxlock lock = nullptr;
    cuvid->cuvidCtxLockCreate(&lock, context);
    m_lock = lock;

    CUVIDPARSERPARAMS params{};
    params.CodecType = cudaVideoCodec_H264;
    params.ulMaxNumDecodeSurfaces = 1;  // se ajusta en on_sequence
    params.ulMaxDisplayDelay = 0;       // mostrar en cuanto se decodifica
    params.pUserData = this;
    params.pfnSequenceCallback = sequence_callback;
    params.pfnDecodePicture = decode_callback;
    params.pfnDisplayPicture = display_callback;
    CUvideoparser parser = nullptr;
    if (cuvid->cuvidCreateVideoParser(&parser, &params) != CUDA_SUCCESS) {
        error = T("Could not create NVIDIA's video parser.", "No se pudo crear el lector de video de NVIDIA.");
        return false;
    }
    m_parser = parser;
    CUcontext popped;
    cuda->cuCtxPopCurrent(&popped);  // cuCtxCreate lo dejó activo en este hilo
    return true;
}

bool Decoder::decode(const uint8_t* data, size_t size) {
    auto start = std::chrono::steady_clock::now();
    m_got_frame = false;
    cu(m_cuda)->cuCtxPushCurrent(static_cast<CUcontext>(m_context));
    CUVIDSOURCEDATAPACKET packet{};
    packet.flags = CUVID_PKT_ENDOFPICTURE;
    packet.payload = data;
    packet.payload_size = static_cast<unsigned long>(size);
    CUresult result = cv(m_cuvid)->cuvidParseVideoData(static_cast<CUvideoparser>(m_parser), &packet);
    CUcontext popped;
    cu(m_cuda)->cuCtxPopCurrent(&popped);
    m_last_decode_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    return result == CUDA_SUCCESS && m_got_frame;
}

int Decoder::on_sequence(void* format_ptr) {
    auto* format = static_cast<CUVIDEOFORMAT*>(format_ptr);
    uint32_t width = format->display_area.right - format->display_area.left;
    uint32_t height = format->display_area.bottom - format->display_area.top;
    int surfaces = format->min_num_decode_surfaces + 2;
    if (m_decoder && width == m_width && height == m_height) return surfaces;  // misma resolución
    if (m_decoder) cv(m_cuvid)->cuvidDestroyDecoder(static_cast<CUvideodecoder>(m_decoder));
    m_decoder = nullptr;

    CUVIDDECODECREATEINFO info{};
    info.CodecType = format->codec;
    info.ChromaFormat = format->chroma_format;
    info.OutputFormat = cudaVideoSurfaceFormat_NV12;
    info.bitDepthMinus8 = format->bit_depth_luma_minus8;
    info.DeinterlaceMode = cudaVideoDeinterlaceMode_Weave;
    info.ulWidth = format->coded_width;
    info.ulHeight = format->coded_height;
    info.ulMaxWidth = format->coded_width;
    info.ulMaxHeight = format->coded_height;
    info.ulTargetWidth = width;
    info.ulTargetHeight = height;
    info.display_area.left = short(format->display_area.left);
    info.display_area.top = short(format->display_area.top);
    info.display_area.right = short(format->display_area.right);
    info.display_area.bottom = short(format->display_area.bottom);
    info.ulNumDecodeSurfaces = surfaces;
    info.ulNumOutputSurfaces = 2;
    info.ulCreationFlags = cudaVideoCreate_PreferCUVID;
    info.vidLock = static_cast<CUvideoctxlock>(m_lock);
    CUvideodecoder decoder = nullptr;
    if (cv(m_cuvid)->cuvidCreateDecoder(&decoder, &info) != CUDA_SUCCESS) return 0;
    m_decoder = decoder;
    m_width = width;
    m_height = height;
    m_surface_height = height;
    if (!create_textures(width, height)) return 0;
    return surfaces;
}

int Decoder::on_decode(void* picture) {
    if (!m_decoder) return 0;
    return cv(m_cuvid)->cuvidDecodePicture(static_cast<CUvideodecoder>(m_decoder),
                                           static_cast<CUVIDPICPARAMS*>(picture)) == CUDA_SUCCESS;
}

int Decoder::on_display(void* info_ptr) {
    auto* info = static_cast<CUVIDPARSERDISPINFO*>(info_ptr);
    CUVIDPROCPARAMS proc{};
    proc.progressive_frame = info->progressive_frame;
    proc.top_field_first = info->top_field_first;
    CUdeviceptr frame = 0;
    unsigned int pitch = 0;
    auto* cuvid = cv(m_cuvid);
    auto* cuda = cu(m_cuda);
    if (cuvid->cuvidMapVideoFrame(static_cast<CUvideodecoder>(m_decoder), info->picture_index, &frame, &pitch,
                                  &proc) != CUDA_SUCCESS)
        return 0;

    CUgraphicsResource resources[2] = {static_cast<CUgraphicsResource>(m_luma_resource),
                                       static_cast<CUgraphicsResource>(m_chroma_resource)};
    if (cuda->cuGraphicsMapResources(2, resources, nullptr) == CUDA_SUCCESS) {
        CUarray luma = nullptr, chroma = nullptr;
        cuda->cuGraphicsSubResourceGetMappedArray(&luma, resources[0], 0, 0);
        cuda->cuGraphicsSubResourceGetMappedArray(&chroma, resources[1], 0, 0);

        CUDA_MEMCPY2D copy{};
        copy.srcMemoryType = CU_MEMORYTYPE_DEVICE;
        copy.srcDevice = frame;
        copy.srcPitch = pitch;
        copy.dstMemoryType = CU_MEMORYTYPE_ARRAY;
        copy.dstArray = luma;
        copy.WidthInBytes = m_width;
        copy.Height = m_height;
        cuda->cuMemcpy2D(&copy);

        copy.srcDevice = frame + CUdeviceptr(pitch) * ((m_surface_height + 1) & ~1u);
        copy.dstArray = chroma;
        copy.WidthInBytes = (m_width + 1) & ~1u;  // pares U,V: ancho/2 texels de 2 bytes
        copy.Height = (m_height + 1) / 2;
        cuda->cuMemcpy2D(&copy);

        cuda->cuGraphicsUnmapResources(2, resources, nullptr);
        m_got_frame = true;
    }
    cuvid->cuvidUnmapVideoFrame(static_cast<CUvideodecoder>(m_decoder), frame);
    return 1;
}

bool Decoder::create_textures(uint32_t width, uint32_t height) {
    release_textures();
    D3D11_TEXTURE2D_DESC desc{};
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.Width = width;
    desc.Height = height;
    desc.Format = DXGI_FORMAT_R8_UNORM;
    if (FAILED(m_device->CreateTexture2D(&desc, nullptr, &m_luma))) return false;
    desc.Width = (width + 1) / 2;
    desc.Height = (height + 1) / 2;
    desc.Format = DXGI_FORMAT_R8G8_UNORM;
    if (FAILED(m_device->CreateTexture2D(&desc, nullptr, &m_chroma))) return false;
    m_device->CreateShaderResourceView(m_luma, nullptr, &m_luma_srv);
    m_device->CreateShaderResourceView(m_chroma, nullptr, &m_chroma_srv);

    CUgraphicsResource luma = nullptr, chroma = nullptr;
    auto* cuda = cu(m_cuda);
    if (cuda->cuGraphicsD3D11RegisterResource(&luma, m_luma, CU_GRAPHICS_REGISTER_FLAGS_NONE) != CUDA_SUCCESS ||
        cuda->cuGraphicsD3D11RegisterResource(&chroma, m_chroma, CU_GRAPHICS_REGISTER_FLAGS_NONE) != CUDA_SUCCESS)
        return false;
    m_luma_resource = luma;
    m_chroma_resource = chroma;
    return true;
}

void Decoder::release_textures() {
    if (m_cuda) {
        if (m_luma_resource) cu(m_cuda)->cuGraphicsUnregisterResource(static_cast<CUgraphicsResource>(m_luma_resource));
        if (m_chroma_resource)
            cu(m_cuda)->cuGraphicsUnregisterResource(static_cast<CUgraphicsResource>(m_chroma_resource));
    }
    m_luma_resource = m_chroma_resource = nullptr;
    release(m_luma_srv);
    release(m_chroma_srv);
    release(m_luma);
    release(m_chroma);
}
