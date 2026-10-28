#include "renderer.h"

#include <d3dcompiler.h>

#include <algorithm>
#include <cstring>

namespace {

// Un triángulo que cubre la pantalla, sin buffers de vértices.
const char k_shader[] = R"(
Texture2D luma : register(t0);
Texture2D chroma : register(t1);
SamplerState linear_sampler : register(s0);

struct VsOut { float4 position : SV_Position; float2 uv : TEXCOORD0; };

VsOut vs_main(uint id : SV_VertexID) {
    VsOut o;
    o.uv = float2((id << 1) & 2, id & 2);
    o.position = float4(o.uv * float2(2, -2) + float2(-1, 1), 0, 1);
    return o;
}

float4 ps_main(VsOut i) : SV_Target {
    float y = (luma.Sample(linear_sampler, i.uv).r - 16.0 / 255.0) * 1.164;
    float2 uv = chroma.Sample(linear_sampler, i.uv).rg - 0.5;
    float3 rgb = float3(y + 1.596 * uv.y, y - 0.392 * uv.x - 0.813 * uv.y, y + 2.017 * uv.x);
    return float4(saturate(rgb), 1);
}
)";

template <typename T>
void release(T*& p) {
    if (p) p->Release();
    p = nullptr;
}

}  // namespace

VideoRenderer::~VideoRenderer() {
    release(m_vs);
    release(m_ps);
    release(m_sampler);
}

bool VideoRenderer::init(ID3D11Device* device) {
    ID3DBlob* vs = nullptr;
    ID3DBlob* ps = nullptr;
    bool ok = SUCCEEDED(D3DCompile(k_shader, strlen(k_shader), nullptr, nullptr, nullptr, "vs_main", "vs_4_0", 0, 0,
                                   &vs, nullptr)) &&
              SUCCEEDED(D3DCompile(k_shader, strlen(k_shader), nullptr, nullptr, nullptr, "ps_main", "ps_4_0", 0, 0,
                                   &ps, nullptr)) &&
              SUCCEEDED(device->CreateVertexShader(vs->GetBufferPointer(), vs->GetBufferSize(), nullptr, &m_vs)) &&
              SUCCEEDED(device->CreatePixelShader(ps->GetBufferPointer(), ps->GetBufferSize(), nullptr, &m_ps));
    release(vs);
    release(ps);
    D3D11_SAMPLER_DESC sampler{};
    sampler.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    return ok && SUCCEEDED(device->CreateSamplerState(&sampler, &m_sampler));
}

void VideoRenderer::draw(ID3D11DeviceContext* context, ID3D11ShaderResourceView* luma,
                         ID3D11ShaderResourceView* chroma, unsigned video_width, unsigned video_height,
                         unsigned target_width, unsigned target_height) {
    if (!luma || !chroma || !video_width || !video_height || !target_width || !target_height) return;
    float scale = std::min(float(target_width) / video_width, float(target_height) / video_height);
    D3D11_VIEWPORT viewport{};
    viewport.Width = video_width * scale;
    viewport.Height = video_height * scale;
    viewport.TopLeftX = (target_width - viewport.Width) / 2;
    viewport.TopLeftY = (target_height - viewport.Height) / 2;
    viewport.MaxDepth = 1;
    context->RSSetViewports(1, &viewport);
    context->IASetInputLayout(nullptr);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(m_vs, nullptr, 0);
    context->PSSetShader(m_ps, nullptr, 0);
    ID3D11ShaderResourceView* views[2] = {luma, chroma};
    context->PSSetShaderResources(0, 2, views);
    context->PSSetSamplers(0, 1, &m_sampler);
    context->OMSetBlendState(nullptr, nullptr, 0xffffffff);
    context->Draw(3, 0);
    ID3D11ShaderResourceView* none[2] = {};
    context->PSSetShaderResources(0, 2, none);
}
