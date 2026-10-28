#pragma once
#include <d3d11.h>

#include <cstddef>
#include <cstdint>
#include <string>

// Decodificador H.264 por hardware (NVDEC). Escribe cada fotograma en dos texturas D3D11 (luma Y y croma UV)
// que la ventana convierte a color al dibujar. Sin colas: cada fotograma sale en cuanto se decodifica.
class Decoder {
public:
    ~Decoder();
    bool init(ID3D11Device* device, std::string& error);
    // Un fotograma H.264 completo. true si se pudo decodificar (y las texturas tienen el fotograma nuevo).
    bool decode(const uint8_t* data, size_t size);

    ID3D11ShaderResourceView* luma() const { return m_luma_srv; }
    ID3D11ShaderResourceView* chroma() const { return m_chroma_srv; }
    uint32_t width() const { return m_width; }
    uint32_t height() const { return m_height; }
    double last_decode_ms() const { return m_last_decode_ms; }

    // Callbacks del parser de NVDEC (internos).
    int on_sequence(void* format);
    int on_decode(void* picture);
    int on_display(void* info);

private:
    bool create_textures(uint32_t width, uint32_t height);
    void release_textures();

    ID3D11Device* m_device = nullptr;
    void* m_cuda = nullptr;    // CudaFunctions
    void* m_cuvid = nullptr;   // CuvidFunctions
    void* m_context = nullptr; // CUcontext
    void* m_lock = nullptr;    // CUvideoctxlock
    void* m_parser = nullptr;
    void* m_decoder = nullptr;
    uint32_t m_width = 0, m_height = 0, m_surface_height = 0;
    bool m_got_frame = false;
    double m_last_decode_ms = 0;

    ID3D11Texture2D* m_luma = nullptr;
    ID3D11Texture2D* m_chroma = nullptr;
    ID3D11ShaderResourceView* m_luma_srv = nullptr;
    ID3D11ShaderResourceView* m_chroma_srv = nullptr;
    void* m_luma_resource = nullptr;    // CUgraphicsResource
    void* m_chroma_resource = nullptr;
};
