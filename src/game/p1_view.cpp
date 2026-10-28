#include "p1_view.h"
#include "log.h"
#include "video.h"

#include <d3d11.h>
#include <d3dcompiler.h>

namespace {

// Un triángulo que cubre toda la pantalla (sin buffers de vértices) y muestrea la copia del cuadrante 1.
constexpr char k_shader[] = R"(
Texture2D quadrant : register(t0);
SamplerState linear_sampler : register(s0);
struct Output { float4 position : SV_Position; float2 uv : TEXCOORD0; };
Output vs_main(uint id : SV_VertexID) {
    Output o;
    o.uv = float2((id << 1) & 2, id & 2);
    o.position = float4(o.uv * float2(2, -2) + float2(-1, 1), 0, 1);
    return o;
}
float4 ps_main(Output i) : SV_Target { return float4(quadrant.Sample(linear_sampler, i.uv).rgb, 1); }
)";

bool g_enabled = false;
bool g_f6_down = false;
int g_panel_value = -1;  // última casilla del panel que vimos (-1: todavía ninguna)

// Recursos propios; se rehacen si el juego cambia de dispositivo, tamaño o formato.
ID3D11Device* g_device = nullptr;
ID3D11VertexShader* g_vs = nullptr;
ID3D11PixelShader* g_ps = nullptr;
ID3D11SamplerState* g_sampler = nullptr;
ID3D11RasterizerState* g_raster = nullptr;
ID3D11BlendState* g_blend = nullptr;
ID3D11DepthStencilState* g_depth = nullptr;
ID3D11Texture2D* g_copy = nullptr;
ID3D11ShaderResourceView* g_copy_view = nullptr;
D3D11_TEXTURE2D_DESC g_copy_desc{};
bool g_failed = false;  // no reintentar en cada fotograma

template <typename T>
void release(T*& p) {
    if (p) p->Release();
    p = nullptr;
}

void release_all() {
    release(g_copy_view);
    release(g_copy);
    release(g_depth);
    release(g_blend);
    release(g_raster);
    release(g_sampler);
    release(g_ps);
    release(g_vs);
    g_device = nullptr;
    g_copy_desc = {};
}

bool create_pipeline(ID3D11Device* device) {
    ID3DBlob* vs = nullptr;
    ID3DBlob* ps = nullptr;
    bool ok = SUCCEEDED(D3DCompile(k_shader, sizeof(k_shader) - 1, nullptr, nullptr, nullptr, "vs_main", "vs_4_0", 0, 0,
                                   &vs, nullptr)) &&
              SUCCEEDED(D3DCompile(k_shader, sizeof(k_shader) - 1, nullptr, nullptr, nullptr, "ps_main", "ps_4_0", 0, 0,
                                   &ps, nullptr)) &&
              SUCCEEDED(device->CreateVertexShader(vs->GetBufferPointer(), vs->GetBufferSize(), nullptr, &g_vs)) &&
              SUCCEEDED(device->CreatePixelShader(ps->GetBufferPointer(), ps->GetBufferSize(), nullptr, &g_ps));
    if (vs) vs->Release();
    if (ps) ps->Release();
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
    D3D11_DEPTH_STENCIL_DESC depth{};
    return SUCCEEDED(device->CreateSamplerState(&sampler, &g_sampler)) &&
           SUCCEEDED(device->CreateRasterizerState(&raster, &g_raster)) &&
           SUCCEEDED(device->CreateBlendState(&blend, &g_blend)) &&
           SUCCEEDED(device->CreateDepthStencilState(&depth, &g_depth));
}

bool ensure_copy(ID3D11Device* device, UINT width, UINT height, DXGI_FORMAT format) {
    if (g_copy && g_copy_desc.Width == width && g_copy_desc.Height == height && g_copy_desc.Format == format)
        return true;
    release(g_copy_view);
    release(g_copy);
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    if (FAILED(device->CreateTexture2D(&desc, nullptr, &g_copy)) ||
        FAILED(device->CreateShaderResourceView(g_copy, nullptr, &g_copy_view))) {
        release(g_copy_view);
        release(g_copy);
        return false;
    }
    g_copy_desc = desc;
    return true;
}

// F6 solo cuenta con el juego en primer plano (no al escribir en otra ventana).
bool f6_pressed() {
    bool down = (GetAsyncKeyState(VK_F6) & 0x8000) != 0;
    bool pressed = down && !g_f6_down;
    g_f6_down = down;
    if (!pressed) return false;
    DWORD pid = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &pid);
    return pid == GetCurrentProcessId();
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

void p1_view_on_frame(IDXGISwapChain* swap_chain) {
    proto::VideoConfig config;
    if (video_config(config) && config.p1_fullscreen != g_panel_value) {
        g_panel_value = config.p1_fullscreen;  // la casilla del panel cambió (o es la primera vez)
        g_enabled = g_panel_value != 0;
    }
    if (f6_pressed()) {
        g_enabled = !g_enabled;
        log_write("Pantalla del jugador 1: %s (F6)", g_enabled ? "completa" : "dividida");
    }
    if (!g_enabled) return;

    ID3D11Texture2D* back_buffer = nullptr;
    if (FAILED(swap_chain->GetBuffer(0, IID_PPV_ARGS(&back_buffer)))) return;
    ID3D11Device* device = nullptr;
    back_buffer->GetDevice(&device);
    D3D11_TEXTURE2D_DESC desc;
    back_buffer->GetDesc(&desc);
    UINT quad_w = desc.Width / 2, quad_h = desc.Height / 2;

    if (device != g_device) {
        release_all();
        g_failed = !create_pipeline(device);
        g_device = device;
        if (g_failed) log_write("Pantalla del jugador 1: no se pudo preparar el dibujo");
    }
    // Una copia por cuarto de pantalla: el back buffer no se puede leer y escribir en el mismo dibujo.
    // El render target se crea en cada fotograma y se suelta enseguida: si guardáramos una referencia al
    // back buffer, el juego no podría cambiar de resolución (ResizeBuffers fallaría).
    ID3D11RenderTargetView* rtv = nullptr;
    if (!g_failed && desc.SampleDesc.Count == 1 && quad_w && quad_h &&
        ensure_copy(device, quad_w, quad_h, desc.Format) &&
        SUCCEEDED(device->CreateRenderTargetView(back_buffer, nullptr, &rtv))) {
        ID3D11DeviceContext* context = nullptr;
        device->GetImmediateContext(&context);
        D3D11_BOX box{0, 0, 0, quad_w, quad_h, 1};
        context->CopySubresourceRegion(g_copy, 0, 0, 0, 0, back_buffer, 0, &box);

        SavedState saved;
        saved.save(context);
        D3D11_VIEWPORT viewport{0, 0, float(desc.Width), float(desc.Height), 0, 1};
        context->OMSetRenderTargets(1, &rtv, nullptr);
        context->RSSetViewports(1, &viewport);
        context->RSSetState(g_raster);
        const FLOAT factor[4] = {};
        context->OMSetBlendState(g_blend, factor, 0xFFFFFFFF);
        context->OMSetDepthStencilState(g_depth, 0);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context->IASetInputLayout(nullptr);
        context->VSSetShader(g_vs, nullptr, 0);
        context->PSSetShader(g_ps, nullptr, 0);
        context->GSSetShader(nullptr, nullptr, 0);
        context->HSSetShader(nullptr, nullptr, 0);
        context->DSSetShader(nullptr, nullptr, 0);
        context->PSSetShaderResources(0, 1, &g_copy_view);
        context->PSSetSamplers(0, 1, &g_sampler);
        context->Draw(3, 0);
        saved.restore(context);
        context->Release();
    } else if (!g_failed) {
        static bool logged = false;
        if (!logged) log_write("Pantalla del jugador 1: formato de pantalla no compatible (%d)", int(desc.Format));
        logged = true;
    }
    if (rtv) rtv->Release();
    device->Release();
    back_buffer->Release();
}
