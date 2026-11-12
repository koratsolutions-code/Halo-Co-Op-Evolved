#include "cinematic.h"
#include "blit.h"
#include "log.h"

#include <windows.h>

#include <cstdint>

namespace {

constexpr uint32_t k_halo1_timestamp = 0x68A0E4CF;  // cabecera PE de halo1.dll de MCC 1.3528
constexpr uintptr_t k_flag_rva = 0x1B87A20;

Blitter g_blitter;
OwnTexture g_full;   // copia de la pantalla entera (el back buffer no siempre se puede leer en un shader)
OwnTexture g_small;  // la pantalla entera reducida al tamaño de un cuarto

}  // namespace

bool cinematic_now() {
    static uintptr_t checked_module = 0;
    static bool valid = false;
    uintptr_t halo1 = reinterpret_cast<uintptr_t>(GetModuleHandleW(L"halo1.dll"));
    if (!halo1) return false;
    if (halo1 != checked_module) {
        checked_module = halo1;
        auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(halo1);
        auto* nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(halo1 + dos->e_lfanew);
        valid = nt->FileHeader.TimeDateStamp == k_halo1_timestamp;
        if (!valid) log_write("Cinemáticas: esta versión de halo1.dll no es la esperada: se ven por cuartos");
    }
    if (!valid) return false;
    const auto* flag = reinterpret_cast<const volatile uint8_t*>(halo1 + k_flag_rva);
    return flag[0] == 1 && flag[1] == 0;
}

ID3D11Texture2D* cinematic_frame(ID3D11Device* device, ID3D11DeviceContext* context, ID3D11Texture2D* back_buffer,
                                 UINT width, UINT height) {
    D3D11_TEXTURE2D_DESC desc;
    back_buffer->GetDesc(&desc);
    if (desc.SampleDesc.Count != 1 || !g_full.ensure(device, desc.Width, desc.Height, desc.Format, false) ||
        !g_small.ensure(device, width, height, desc.Format, true)) {
        static bool logged = false;
        if (!logged) log_write("Cinemáticas: formato de pantalla no compatible (%d)", int(desc.Format));
        logged = true;
        return nullptr;
    }
    context->CopyResource(g_full.texture, back_buffer);
    if (!g_blitter.draw(device, context, g_full.view, g_small.target, width, height)) return nullptr;
    return g_small.texture;
}
