// Subtítulos para cada jugador. MCC dibuja la línea una sola vez, centrada en la pantalla entera (medido en MCC
// 1.3528 a 2560x1440 con 1 y con 4 jugadores: del 26 % al 74 % del ancho y del 84,5 % al 87 % del alto). Con la
// pantalla dividida queda cortada entre las partes de abajo, y las de arriba no la ven. Mientras halo1.dll tiene una
// línea en pantalla, se copia esa franja dentro de la parte de cada jugador que no la contiene entera.
#include "subtitles.h"
#include "blit.h"
#include "cinematic.h"
#include "log.h"
#include "split_layout.h"
#include "video.h"

#include <d3d11.h>
#include <dxgi.h>

#include <algorithm>

namespace {

// Franja que se copia, en fracciones de la pantalla entera. Las líneas largas pasan del 80 % del ancho y MCC las
// parte en dos renglones hacia arriba: hay margen para ambas cosas.
constexpr float k_band_left = 0.10f, k_band_right = 0.90f;
constexpr float k_band_top = 0.80f, k_band_bottom = 0.92f;
// Centro de la franja dentro de la parte de cada jugador: donde cae en las partes de abajo, para tapar el pedazo
// cortado que ya está ahí.
constexpr float k_band_center = 0.72f;

Blitter g_blitter;
OwnTexture g_band;  // copia de la franja: el back buffer no se puede leer y escribir en el mismo dibujo
bool g_was_drawing = false;

}  // namespace

void subtitles_copy_draw(IDXGISwapChain* swap_chain) {
    proto::VideoConfig config;
    bool wanted = video_config(config) && config.copy_subtitles && !config.hide_subtitles && subtitles_showing() &&
                  !(config.full_cinematics && cinematic_now());
    const int count = wanted ? split_player_count() : 1;
    if (count < 2) wanted = false;
    if (wanted != g_was_drawing) {
        g_was_drawing = wanted;
        static bool logged = false;
        if (wanted && !logged) log_write("Subtítulos: copiando la línea a cada jugador");
        logged |= wanted;
    }
    if (!wanted) return;

    ID3D11Texture2D* back_buffer = nullptr;
    if (FAILED(swap_chain->GetBuffer(0, IID_PPV_ARGS(&back_buffer)))) return;
    ID3D11Device* device = nullptr;
    back_buffer->GetDevice(&device);
    D3D11_TEXTURE2D_DESC desc;
    back_buffer->GetDesc(&desc);

    UINT band_x = UINT(desc.Width * k_band_left), band_y = UINT(desc.Height * k_band_top);
    UINT band_w = UINT(desc.Width * k_band_right) - band_x, band_h = UINT(desc.Height * k_band_bottom) - band_y;
    // El render target se crea en cada fotograma y se suelta enseguida (si no, el juego no podría cambiar de
    // resolución).
    ID3D11RenderTargetView* rtv = nullptr;
    if (desc.SampleDesc.Count == 1 && band_w && band_h &&
        g_band.ensure(device, band_w, band_h, desc.Format, false) &&
        SUCCEEDED(device->CreateRenderTargetView(back_buffer, nullptr, &rtv))) {
        ID3D11DeviceContext* context = nullptr;
        device->GetImmediateContext(&context);
        D3D11_BOX box{band_x, band_y, 0, band_x + band_w, band_y + band_h, 1};
        context->CopySubresourceRegion(g_band.texture, 0, 0, 0, 0, back_buffer, 0, &box);
        for (int player = 0; player < count; ++player) {
            ViewRect view;
            if (!split_view(player, count, desc.Width, desc.Height, view)) continue;
            if (!config.subtitles_for[player]) continue;  // ese jugador la desactivó en su menú (F2)
            bool contains = view.x <= band_x && view.x + view.width >= band_x + band_w && view.y <= band_y &&
                            view.y + view.height >= band_y + band_h;
            if (contains) continue;  // esa parte ya la ve entera (p. ej. J2 con 2 jugadores)
            // Las partes donde cae un pedazo de la línea original (las de abajo): la copia no coincide con él, así
            // que la banda va casi opaca y, además, una franja oscura a todo el ancho tapa todos sus renglones.
            bool overlaps = view.x < band_x + band_w && band_x < view.x + view.width &&
                            view.y < band_y + band_h && band_y < view.y + view.height;
            if (overlaps) {
                UINT top = std::max(view.y, band_y), bottom = std::min(view.y + view.height, band_y + band_h);
                D3D11_VIEWPORT cover{float(view.x), float(top), float(view.width), float(bottom - top), 0, 1};
                g_blitter.draw_dark(device, context, rtv, cover);
            }
            float scale = std::min(1.0f, float(view.width) / float(band_w));
            D3D11_VIEWPORT area{};
            area.Width = band_w * scale;
            area.Height = band_h * scale;
            area.TopLeftX = view.x + (view.width - area.Width) / 2;
            area.TopLeftY = view.y + view.height * k_band_center - area.Height / 2;
            area.MaxDepth = 1;
            g_blitter.draw_subtitle(device, context, g_band.view, rtv, area, overlaps);
        }
        context->Release();
    }
    if (rtv) rtv->Release();
    device->Release();
    back_buffer->Release();
}
