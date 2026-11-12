#pragma once
#include <d3d11.h>

// Dibuja una textura (estirada o reducida, con filtro lineal) sobre un render target, con el dispositivo del juego.
// Deja el estado del pipeline del juego tal como estaba. Cada usuario tiene su propio Blitter.
class Blitter {
public:
    ~Blitter() { release_all(); }
    // Llena todo el render target (width x height). Con source_aspect > 0, la imagen conserva esa
    // proporción (ancho/alto) y sobra negro arriba y abajo o a los lados.
    bool draw(ID3D11Device* device, ID3D11DeviceContext* context, ID3D11ShaderResourceView* source,
              ID3D11RenderTargetView* target, UINT width, UINT height, float source_aspect = 0);

private:
    bool create(ID3D11Device* device);
    void release_all();

    ID3D11Device* m_device = nullptr;
    ID3D11VertexShader* m_vs = nullptr;
    ID3D11PixelShader* m_ps = nullptr;
    ID3D11SamplerState* m_sampler = nullptr;
    ID3D11RasterizerState* m_raster = nullptr;
    ID3D11BlendState* m_blend = nullptr;
    ID3D11DepthStencilState* m_depth = nullptr;
    bool m_failed = false;
};

// Textura propia que se rehace si cambia el tamaño o el formato.
struct OwnTexture {
    ID3D11Texture2D* texture = nullptr;
    ID3D11ShaderResourceView* view = nullptr;
    ID3D11RenderTargetView* target = nullptr;  // solo si se pidió render target
    D3D11_TEXTURE2D_DESC desc{};

    ~OwnTexture() { release(); }
    bool ensure(ID3D11Device* device, UINT width, UINT height, DXGI_FORMAT format, bool render_target);
    void release();
};
