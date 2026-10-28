#include "name_tags.h"
#include "halo1_players.h"
#include "log.h"
#include "video.h"

#include <d3d11.h>

#include <imgui.h>
#include <imgui_impl_dx11.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <string>

namespace {

// Altura de los ojos sobre los pies (cámara en primera persona de Master Chief, de pie) y del nombre sobre
// el centro del cuerpo, en unidades de Halo (1 unidad ≈ 3 m).
constexpr float k_eye_height = 0.62f;
constexpr float k_label_height = 0.42f;

// ImGui propio (AlphaRing tiene el suyo en su DLL; cada uno usa su contexto).
ImGuiContext* g_imgui = nullptr;
ID3D11Device* g_device = nullptr;
ImFont* g_font = nullptr;

bool ensure_imgui(ID3D11Device* device, ID3D11DeviceContext* context) {
    if (g_imgui && g_device == device) return true;
    if (g_imgui) {  // el juego recreó el dispositivo
        ImGui::SetCurrentContext(g_imgui);
        ImGui_ImplDX11_Shutdown();
        ImGui::DestroyContext(g_imgui);
        g_imgui = nullptr;
    }
    g_imgui = ImGui::CreateContext();
    ImGui::SetCurrentContext(g_imgui);
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    static const ImWchar ranges[] = {0x0020, 0x00FF, 0};
    char font[MAX_PATH] = "";
    GetWindowsDirectoryA(font, MAX_PATH);
    strcat_s(font, "\\Fonts\\segoeuib.ttf");
    g_font = io.Fonts->AddFontFromFileTTF(font, 30.0f, nullptr, ranges);
    if (!g_font) g_font = io.Fonts->AddFontDefault();
    g_device = device;
    bool ok = ImGui_ImplDX11_Init(device, context);
    log_write("Nombres: dibujo %s", ok ? "listo" : "FALLÓ");
    return ok;
}

struct Vec3 {
    float x, y, z;
};
Vec3 sub(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
Vec3 cross(Vec3 a, Vec3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
Vec3 normalize(Vec3 v) {
    float l = std::sqrt(dot(v, v));
    return l > 1e-6f ? Vec3{v.x / l, v.y / l, v.z / l} : Vec3{0, 0, 0};
}

// Texto con borde oscuro para que se lea sobre cualquier fondo.
void outlined_text(ImDrawList* draw, ImVec2 center, const char* text, float alpha, float scale) {
    float size = g_font->FontSize * scale;
    ImVec2 extent = g_font->CalcTextSizeA(size, FLT_MAX, 0, text);
    ImVec2 pos(center.x - extent.x / 2, center.y - extent.y);
    ImU32 shadow = IM_COL32(0, 0, 0, int(200 * alpha));
    for (int dx = -2; dx <= 2; dx += 2)
        for (int dy = -2; dy <= 2; dy += 2)
            if (dx || dy) draw->AddText(g_font, size, ImVec2(pos.x + dx, pos.y + dy), shadow, text);
    draw->AddText(g_font, size, pos, IM_COL32(150, 220, 255, int(255 * alpha)), text);
}

}  // namespace

void name_tags_start() {
    halo1_players_start();
}

void name_tags_draw(IDXGISwapChain* swap_chain) {
    proto::VideoConfig config;
    if (!video_config(config) || !config.name_tags) return;
    Halo1Player players[4];
    if (!halo1_players_read(players)) return;
    int alive = 0;
    for (const auto& p : players) alive += p.alive ? 1 : 0;
    if (alive < 2) return;  // nadie a quien ponerle nombre

    ID3D11Device* device = nullptr;
    if (FAILED(swap_chain->GetDevice(IID_PPV_ARGS(&device)))) return;
    ID3D11DeviceContext* context = nullptr;
    device->GetImmediateContext(&context);
    ID3D11Texture2D* backbuffer = nullptr;
    ID3D11RenderTargetView* rtv = nullptr;
    if (ensure_imgui(device, context) && SUCCEEDED(swap_chain->GetBuffer(0, IID_PPV_ARGS(&backbuffer))) &&
        SUCCEEDED(device->CreateRenderTargetView(backbuffer, nullptr, &rtv))) {
        D3D11_TEXTURE2D_DESC desc;
        backbuffer->GetDesc(&desc);
        ImGui::SetCurrentContext(g_imgui);
        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2(float(desc.Width), float(desc.Height));
        io.DeltaTime = 1.0f / 60.0f;
        ImGui_ImplDX11_NewFrame();
        ImGui::NewFrame();
        ImDrawList* draw = ImGui::GetForegroundDrawList();

        const float qw = float(desc.Width) / 2, qh = float(desc.Height) / 2;
        const float fov = std::clamp<float>(config.name_fov, 40, 150);
        const float tan_half = std::tan(fov * 3.14159265f / 360.0f);
        const float aspect = qw / qh;
        const Vec3 world_up{0, 0, 1};
        std::string names[4];
        for (int p = 0; p < 4; ++p) {
            names[p].assign(config.names[p], strnlen(config.names[p], proto::k_nickname_size));
            if (names[p].empty()) names[p] = "Player " + std::to_string(p + 1);
        }

        for (int viewer = 0; viewer < 4; ++viewer) {
            const Halo1Player& v = players[viewer];
            if (!v.alive) continue;
            // Cámara en primera persona del jugador que mira (cuadrícula 2x2 de AlphaRing).
            Vec3 eye{v.position[0], v.position[1], v.position[2] + k_eye_height};
            Vec3 forward = normalize({v.look[0], v.look[1], v.look[2]});
            Vec3 right = normalize(cross(forward, world_up));
            Vec3 up = cross(right, forward);
            float qx = float(viewer % 2) * qw, qy = float(viewer / 2) * qh;
            draw->PushClipRect(ImVec2(qx, qy), ImVec2(qx + qw, qy + qh), false);
            for (int target = 0; target < 4; ++target) {
                const Halo1Player& t = players[target];
                if (target == viewer || !t.alive) continue;
                Vec3 anchor{t.center[0], t.center[1], t.center[2] + k_label_height};
                Vec3 d = sub(anchor, eye);
                float depth = dot(d, forward);
                if (depth < 0.2f) continue;  // detrás de la cámara o encima de ella
                float sx = dot(d, right) / (depth * tan_half);
                float sy = dot(d, up) / (depth * tan_half / aspect);
                if (std::fabs(sx) > 1.2f || std::fabs(sy) > 1.2f) continue;
                ImVec2 screen(qx + (sx * 0.5f + 0.5f) * qw, qy + (0.5f - sy * 0.5f) * qh);
                // Más chico y tenue a la distancia (siempre visible, también a través de paredes).
                float distance = std::sqrt(dot(d, d));
                float scale = std::clamp(1.2f / std::sqrt(std::max(distance, 0.3f)), 0.55f, 1.0f);
                float alpha = std::clamp(1.3f - distance / 40.0f, 0.55f, 1.0f);
                outlined_text(draw, screen, names[target].c_str(), alpha, scale * qh / 720.0f);
            }
            draw->PopClipRect();
        }

        ImGui::Render();
        // Guardamos y restauramos los render targets del juego (el backend de ImGui restaura el resto del estado).
        ID3D11RenderTargetView* old_rtvs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {};
        ID3D11DepthStencilView* old_dsv = nullptr;
        context->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, old_rtvs, &old_dsv);
        context->OMSetRenderTargets(1, &rtv, nullptr);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        context->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, old_rtvs, old_dsv);
        for (auto* old : old_rtvs)
            if (old) old->Release();
        if (old_dsv) old_dsv->Release();
    }
    if (rtv) rtv->Release();
    if (backbuffer) backbuffer->Release();
    context->Release();
    device->Release();
}
