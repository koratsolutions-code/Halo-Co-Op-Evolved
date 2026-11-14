#include "blit.h"

#include <d3dcompiler.h>

namespace {

// Un triángulo que cubre todo el render target (sin buffers de vértices) y muestrea la textura de origen.
constexpr char k_shader[] = R"(
Texture2D source : register(t0);
SamplerState linear_sampler : register(s0);
struct Output { float4 position : SV_Position; float2 uv : TEXCOORD0; };
Output vs_main(uint id : SV_VertexID) {
    Output o;
    o.uv = float2((id << 1) & 2, id & 2);
    o.position = float4(o.uv * float2(2, -2) + float2(-1, 1), 0, 1);
    return o;
}
float4 ps_main(Output i) : SV_Target { return float4(source.Sample(linear_sampler, i.uv).rgb, 1); }
// Subtítulos de MCC: texto blanco con borde oscuro. Solo pasa lo casi blanco en los tres canales (así no se cuela
// la imagen de otro jugador que haya detrás); lo demás oscurece lo que ya había (alfa BAND_ALPHA, mezcla normal).
#ifndef BAND_ALPHA
#define BAND_ALPHA 0.6
#endif
float4 ps_subtitle(Output i) : SV_Target {
    float3 c = source.Sample(linear_sampler, i.uv).rgb;
    float text = smoothstep(0.72, 0.90, min(c.r, min(c.g, c.b)));
    return float4(c * text, max(text, BAND_ALPHA));
}
float4 ps_dark(Output i) : SV_Target { return float4(0, 0, 0, BAND_ALPHA); }
)";

template <typename T>
void release(T*& p) {
    if (p) p->Release();
    p = nullptr;
}

// Estado del pipeline del juego que tocamos; se devuelve tal cual al terminar.
struct SavedState {
    ID3D11RenderTargetView* rtvs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {};
    ID3D11DepthStencilView* dsv = nullptr;
    UINT viewport_count = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
    D3D11_VIEWPORT viewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE] = {};
    ID3D11RasterizerState* raster = nullptr;
    ID3D11BlendState* blend = nullptr;
    FLOAT blend_factor[4] = {};
    UINT sample_mask = 0;
    ID3D11DepthStencilState* depth = nullptr;
    UINT stencil_ref = 0;
    D3D11_PRIMITIVE_TOPOLOGY topology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
    ID3D11InputLayout* layout = nullptr;
    ID3D11VertexShader* vs = nullptr;
    ID3D11PixelShader* ps = nullptr;
    ID3D11GeometryShader* gs = nullptr;
    ID3D11HullShader* hs = nullptr;
    ID3D11DomainShader* ds = nullptr;
    ID3D11ClassInstance* vs_instances[256] = {};
    ID3D11ClassInstance* ps_instances[256] = {};
    UINT vs_instance_count = 256, ps_instance_count = 256;
    ID3D11ShaderResourceView* ps_srv = nullptr;
    ID3D11SamplerState* ps_sampler = nullptr;

    void save(ID3D11DeviceContext* c) {
        c->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, rtvs, &dsv);
        c->RSGetViewports(&viewport_count, viewports);
        c->RSGetState(&raster);
        c->OMGetBlendState(&blend, blend_factor, &sample_mask);
        c->OMGetDepthStencilState(&depth, &stencil_ref);
        c->IAGetPrimitiveTopology(&topology);
        c->IAGetInputLayout(&layout);
        c->VSGetShader(&vs, vs_instances, &vs_instance_count);
        c->PSGetShader(&ps, ps_instances, &ps_instance_count);
        c->GSGetShader(&gs, nullptr, nullptr);
        c->HSGetShader(&hs, nullptr, nullptr);
        c->DSGetShader(&ds, nullptr, nullptr);
        c->PSGetShaderResources(0, 1, &ps_srv);
        c->PSGetSamplers(0, 1, &ps_sampler);
    }

    void restore(ID3D11DeviceContext* c) {
        c->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, rtvs, dsv);
        c->RSSetViewports(viewport_count, viewports);
        c->RSSetState(raster);
        c->OMSetBlendState(blend, blend_factor, sample_mask);
        c->OMSetDepthStencilState(depth, stencil_ref);
        c->IASetPrimitiveTopology(topology);
        c->IASetInputLayout(layout);
        c->VSSetShader(vs, vs_instances, vs_instance_count);
        c->PSSetShader(ps, ps_instances, ps_instance_count);
        c->GSSetShader(gs, nullptr, 0);
        c->HSSetShader(hs, nullptr, 0);
        c->DSSetShader(ds, nullptr, 0);
        c->PSSetShaderResources(0, 1, &ps_srv);
        c->PSSetSamplers(0, 1, &ps_sampler);
        for (auto* p : rtvs) release(p);
        release(dsv);
        release(raster);
        release(blend);
        release(depth);
        release(layout);
        release(vs);
        release(ps);
        release(gs);
        release(hs);
        release(ds);
        for (UINT i = 0; i < vs_instance_count; ++i) release(vs_instances[i]);
        for (UINT i = 0; i < ps_instance_count; ++i) release(ps_instances[i]);
        release(ps_srv);
        release(ps_sampler);
    }
};

}  // namespace

void Blitter::release_all() {
    release(m_blend_alpha);
    release(m_ps_subtitle);
    release(m_ps_subtitle_cover);
    release(m_ps_dark);
    release(m_depth);
    release(m_blend);
    release(m_raster);
    release(m_sampler);
    release(m_ps);
    release(m_vs);
    m_device = nullptr;
}

bool Blitter::create(ID3D11Device* device) {
    ID3DBlob* vs = nullptr;
    ID3DBlob* ps = nullptr;
    ID3DBlob* ps_subtitle = nullptr;
    ID3DBlob* ps_cover = nullptr;
    ID3DBlob* ps_dark = nullptr;
    const D3D_SHADER_MACRO cover_alpha[] = {{"BAND_ALPHA", "0.98"}, {nullptr, nullptr}};
    bool ok = SUCCEEDED(D3DCompile(k_shader, sizeof(k_shader) - 1, nullptr, nullptr, nullptr, "vs_main", "vs_4_0", 0, 0,
                                   &vs, nullptr)) &&
              SUCCEEDED(D3DCompile(k_shader, sizeof(k_shader) - 1, nullptr, nullptr, nullptr, "ps_main", "ps_4_0", 0, 0,
                                   &ps, nullptr)) &&
              SUCCEEDED(D3DCompile(k_shader, sizeof(k_shader) - 1, nullptr, nullptr, nullptr, "ps_subtitle", "ps_4_0",
                                   0, 0, &ps_subtitle, nullptr)) &&
              SUCCEEDED(D3DCompile(k_shader, sizeof(k_shader) - 1, nullptr, cover_alpha, nullptr, "ps_subtitle",
                                   "ps_4_0", 0, 0, &ps_cover, nullptr)) &&
              SUCCEEDED(D3DCompile(k_shader, sizeof(k_shader) - 1, nullptr, cover_alpha, nullptr, "ps_dark", "ps_4_0",
                                   0, 0, &ps_dark, nullptr)) &&
              SUCCEEDED(device->CreateVertexShader(vs->GetBufferPointer(), vs->GetBufferSize(), nullptr, &m_vs)) &&
              SUCCEEDED(device->CreatePixelShader(ps->GetBufferPointer(), ps->GetBufferSize(), nullptr, &m_ps)) &&
              SUCCEEDED(device->CreatePixelShader(ps_subtitle->GetBufferPointer(), ps_subtitle->GetBufferSize(),
                                                  nullptr, &m_ps_subtitle)) &&
              SUCCEEDED(device->CreatePixelShader(ps_cover->GetBufferPointer(), ps_cover->GetBufferSize(), nullptr,
                                                  &m_ps_subtitle_cover)) &&
              SUCCEEDED(device->CreatePixelShader(ps_dark->GetBufferPointer(), ps_dark->GetBufferSize(), nullptr,
                                                  &m_ps_dark));
    if (vs) vs->Release();
    if (ps) ps->Release();
    if (ps_subtitle) ps_subtitle->Release();
    if (ps_cover) ps_cover->Release();
    if (ps_dark) ps_dark->Release();
    if (!ok) return false;

    D3D11_SAMPLER_DESC sampler{};
    sampler.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler.MaxLOD = D3D11_FLOAT32_MAX;
    D3D11_RASTERIZER_DESC raster{};
    raster.FillMode = D3D11_FILL_SOLID;
    raster.CullMode = D3D11_CULL_NONE;
    raster.DepthClipEnable = TRUE;
    D3D11_BLEND_DESC blend{};
    blend.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    D3D11_BLEND_DESC alpha{};
    alpha.RenderTarget[0].BlendEnable = TRUE;
    alpha.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    alpha.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    alpha.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    alpha.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
    alpha.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
    alpha.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    alpha.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    D3D11_DEPTH_STENCIL_DESC depth{};
    return SUCCEEDED(device->CreateSamplerState(&sampler, &m_sampler)) &&
           SUCCEEDED(device->CreateRasterizerState(&raster, &m_raster)) &&
           SUCCEEDED(device->CreateBlendState(&blend, &m_blend)) &&
           SUCCEEDED(device->CreateBlendState(&alpha, &m_blend_alpha)) &&
           SUCCEEDED(device->CreateDepthStencilState(&depth, &m_depth));
}

bool Blitter::ready(ID3D11Device* device) {
    if (device != m_device) {
        release_all();
        m_failed = !create(device);
        m_device = device;
    }
    return !m_failed;
}

bool Blitter::draw(ID3D11Device* device, ID3D11DeviceContext* context, ID3D11ShaderResourceView* source,
                   ID3D11RenderTargetView* target, UINT width, UINT height, float source_aspect) {
    if (!ready(device)) return false;
    D3D11_VIEWPORT viewport{0, 0, float(width), float(height), 0, 1};
    if (source_aspect > 0) {
        float target_aspect = float(width) / float(height);
        if (source_aspect > target_aspect) {
            viewport.Height = float(width) / source_aspect;
            viewport.TopLeftY = (float(height) - viewport.Height) / 2;
        } else {
            viewport.Width = float(height) * source_aspect;
            viewport.TopLeftX = (float(width) - viewport.Width) / 2;
        }
    }
    draw_with(context, m_ps, m_blend, source, target, viewport, source_aspect > 0);
    return true;
}

bool Blitter::draw_subtitle(ID3D11Device* device, ID3D11DeviceContext* context, ID3D11ShaderResourceView* source,
                            ID3D11RenderTargetView* target, const D3D11_VIEWPORT& area, bool cover) {
    if (!ready(device)) return false;
    draw_with(context, cover ? m_ps_subtitle_cover : m_ps_subtitle, m_blend_alpha, source, target, area, false);
    return true;
}

bool Blitter::draw_dark(ID3D11Device* device, ID3D11DeviceContext* context, ID3D11RenderTargetView* target,
                        const D3D11_VIEWPORT& area) {
    if (!ready(device)) return false;
    draw_with(context, m_ps_dark, m_blend_alpha, nullptr, target, area, false);
    return true;
}

void Blitter::draw_with(ID3D11DeviceContext* context, ID3D11PixelShader* shader, ID3D11BlendState* blend,
                        ID3D11ShaderResourceView* source, ID3D11RenderTargetView* target,
                        const D3D11_VIEWPORT& viewport, bool clear) {
    SavedState saved;
    saved.save(context);
    if (clear) {
        const FLOAT black[4] = {0, 0, 0, 1};
        context->ClearRenderTargetView(target, black);
    }
    context->OMSetRenderTargets(1, &target, nullptr);
    context->RSSetViewports(1, &viewport);
    context->RSSetState(m_raster);
    const FLOAT factor[4] = {};
    context->OMSetBlendState(blend, factor, 0xFFFFFFFF);
    context->OMSetDepthStencilState(m_depth, 0);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->IASetInputLayout(nullptr);
    context->VSSetShader(m_vs, nullptr, 0);
    context->PSSetShader(shader, nullptr, 0);
    context->GSSetShader(nullptr, nullptr, 0);
    context->HSSetShader(nullptr, nullptr, 0);
    context->DSSetShader(nullptr, nullptr, 0);
    context->PSSetShaderResources(0, 1, &source);
    context->PSSetSamplers(0, 1, &m_sampler);
    context->Draw(3, 0);
    saved.restore(context);
}

void OwnTexture::release() {
    ::release(target);
    ::release(view);
    ::release(texture);
    desc = {};
}

bool OwnTexture::ensure(ID3D11Device* device, UINT width, UINT height, DXGI_FORMAT format, bool render_target) {
    if (texture && desc.Width == width && desc.Height == height && desc.Format == format &&
        (target != nullptr) == render_target) {
        ID3D11Device* owner = nullptr;
        texture->GetDevice(&owner);
        if (owner) owner->Release();
        if (owner == device) return true;
    }
    release();
    D3D11_TEXTURE2D_DESC d{};
    d.Width = width;
    d.Height = height;
    d.MipLevels = 1;
    d.ArraySize = 1;
    d.Format = format;
    d.SampleDesc.Count = 1;
    d.Usage = D3D11_USAGE_DEFAULT;
    d.BindFlags = D3D11_BIND_SHADER_RESOURCE | (render_target ? D3D11_BIND_RENDER_TARGET : 0);
    if (FAILED(device->CreateTexture2D(&d, nullptr, &texture)) ||
        FAILED(device->CreateShaderResourceView(texture, nullptr, &view)) ||
        (render_target && FAILED(device->CreateRenderTargetView(texture, nullptr, &target)))) {
        release();
        return false;
    }
    desc = d;
    return true;
}
