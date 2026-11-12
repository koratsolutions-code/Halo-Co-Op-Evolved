#include "p1_view.h"
#include "log.h"
#include "video.h"

#include <d3d11.h>

#include "blit.h"
#include "cinematic.h"

namespace {

bool g_enabled = false;
bool g_f6_down = false;
int g_panel_value = -1;  // última casilla del panel que vimos (-1: todavía ninguna)

Blitter g_blitter;
OwnTexture g_copy;  // copia del cuadrante 1: el back buffer no se puede leer y escribir en el mismo dibujo

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
    if (cinematic_now()) return;  // las cinemáticas ocupan toda la pantalla: se ven tal cual

    ID3D11Texture2D* back_buffer = nullptr;
    if (FAILED(swap_chain->GetBuffer(0, IID_PPV_ARGS(&back_buffer)))) return;
    ID3D11Device* device = nullptr;
    back_buffer->GetDevice(&device);
    D3D11_TEXTURE2D_DESC desc;
    back_buffer->GetDesc(&desc);
    UINT quad_w = desc.Width / 2, quad_h = desc.Height / 2;
    // El render target se crea en cada fotograma y se suelta enseguida: si guardáramos una referencia al back
    // buffer, el juego no podría cambiar de resolución (ResizeBuffers fallaría).
    ID3D11RenderTargetView* rtv = nullptr;
    bool drawn = false;
    if (desc.SampleDesc.Count == 1 && quad_w && quad_h && g_copy.ensure(device, quad_w, quad_h, desc.Format, false) &&
        SUCCEEDED(device->CreateRenderTargetView(back_buffer, nullptr, &rtv))) {
        ID3D11DeviceContext* context = nullptr;
        device->GetImmediateContext(&context);
        D3D11_BOX box{0, 0, 0, quad_w, quad_h, 1};
        context->CopySubresourceRegion(g_copy.texture, 0, 0, 0, 0, back_buffer, 0, &box);
        drawn = g_blitter.draw(device, context, g_copy.view, rtv, desc.Width, desc.Height);
        context->Release();
    }
    if (!drawn) {
        static bool logged = false;
        if (!logged) log_write("Pantalla del jugador 1: no se pudo dibujar (formato %d)", int(desc.Format));
        logged = true;
    }
    if (rtv) rtv->Release();
    device->Release();
    back_buffer->Release();
}
