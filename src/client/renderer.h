#pragma once
#include <d3d11.h>

// Dibuja el fotograma (texturas Y y UV de NVDEC) a pantalla completa, conservando la proporción (barras negras
// si la ventana no es 16:9) y convirtiendo YUV BT.601 de rango limitado (lo que produce NVENC) a RGB.
class VideoRenderer {
public:
    ~VideoRenderer();
    bool init(ID3D11Device* device);
    void draw(ID3D11DeviceContext* context, ID3D11ShaderResourceView* luma, ID3D11ShaderResourceView* chroma,
              unsigned video_width, unsigned video_height, unsigned target_width, unsigned target_height);

private:
    ID3D11VertexShader* m_vs = nullptr;
    ID3D11PixelShader* m_ps = nullptr;
    ID3D11SamplerState* m_sampler = nullptr;
};
