// HaloCoopClient: app de los jugadores 2, 3 y 4. Se conecta al host por Radmin VPN y le manda su mando o
// teclado y mouse. (Fase 2: todavía sin video; la pantalla del host se ve aparte, por ejemplo por Discord.)

#include "input.h"
#include "net_client.h"
#include "settings.h"

#include <d3d11.h>
#include <windows.h>

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace {

constexpr wchar_t k_title[] = L"HaloCoop - Cliente";

ID3D11Device* g_device = nullptr;
ID3D11DeviceContext* g_context = nullptr;
IDXGISwapChain* g_swap_chain = nullptr;
ID3D11RenderTargetView* g_target = nullptr;
HWND g_window = nullptr;

Settings g_settings;
char g_host_text[64] = {};
char g_name_text[proto::k_nickname_size] = {};
float g_toast_until = 0;
char g_toast[128] = {};

void create_target() {
    ID3D11Texture2D* back = nullptr;
    g_swap_chain->GetBuffer(0, IID_PPV_ARGS(&back));
    g_device->CreateRenderTargetView(back, nullptr, &g_target);
    back->Release();
}

bool create_device(HWND hwnd) {
    DXGI_SWAP_CHAIN_DESC desc{};
    desc.BufferCount = 2;
    desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.OutputWindow = hwnd;
    desc.SampleDesc.Count = 1;
    desc.Windowed = TRUE;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    if (FAILED(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
                                             D3D11_SDK_VERSION, &desc, &g_swap_chain, &g_device, nullptr,
                                             &g_context)) &&
        FAILED(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0,
                                             D3D11_SDK_VERSION, &desc, &g_swap_chain, &g_device, nullptr,
                                             &g_context)))
        return false;
    create_target();
    return true;
}

void toast(const char* text) {
    snprintf(g_toast, sizeof(g_toast), "%s", text);
    g_toast_until = float(ImGui::GetTime()) + 2.0f;
}

void change_sensitivity(float factor) {
    g_settings.sensitivity = std::clamp(g_settings.sensitivity * factor, k_min_sensitivity, k_max_sensitivity);
    net_update_settings(g_settings);
    settings_save(g_settings);
    char text[64];
    snprintf(text, sizeof(text), "Sensibilidad: %.2fx", g_settings.sensitivity);
    toast(text);
}

LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (msg == WM_INPUT) input_on_raw_input(lparam);
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) return true;
    switch (msg) {
    case WM_SIZE:
        if (g_device && wparam != SIZE_MINIMIZED) {
            if (g_target) g_target->Release();
            g_target = nullptr;
            g_swap_chain->ResizeBuffers(0, LOWORD(lparam), HIWORD(lparam), DXGI_FORMAT_UNKNOWN, 0);
            create_target();
        }
        if (input_captured()) {
            input_set_captured(false);  // se vuelve a capturar con un clic, ya con el tamaño nuevo
        }
        return 0;
    case WM_ACTIVATE:
        if (LOWORD(wparam) == WA_INACTIVE) input_set_captured(false);
        return 0;
    case WM_KEYDOWN:
        if (wparam == VK_ESCAPE && input_captured()) {
            input_set_captured(false);
            return 0;
        }
        if (input_captured() && (wparam == VK_OEM_PLUS || wparam == VK_ADD)) change_sensitivity(1.1f);
        if (input_captured() && (wparam == VK_OEM_MINUS || wparam == VK_SUBTRACT)) change_sensitivity(1 / 1.1f);
        break;
    case WM_SYSCOMMAND:
        if ((wparam & 0xfff0) == SC_KEYMENU) return 0;  // Alt no abre el menú de la ventana
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

void apply_style() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImGui::StyleColorsDark();
    style.WindowRounding = 0;
    style.FrameRounding = 6;
    style.GrabRounding = 6;
    style.FramePadding = ImVec2(10, 7);
    style.ItemSpacing = ImVec2(10, 10);
    ImVec4* c = style.Colors;
    c[ImGuiCol_WindowBg] = ImVec4(0.07f, 0.09f, 0.11f, 1);
    c[ImGuiCol_FrameBg] = ImVec4(0.14f, 0.17f, 0.20f, 1);
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.18f, 0.22f, 0.26f, 1);
    c[ImGuiCol_Button] = ImVec4(0.20f, 0.45f, 0.30f, 1);
    c[ImGuiCol_ButtonHovered] = ImVec4(0.25f, 0.55f, 0.36f, 1);
    c[ImGuiCol_ButtonActive] = ImVec4(0.17f, 0.38f, 0.25f, 1);
    c[ImGuiCol_SliderGrab] = ImVec4(0.35f, 0.70f, 0.45f, 1);
    c[ImGuiCol_CheckMark] = ImVec4(0.35f, 0.70f, 0.45f, 1);
    c[ImGuiCol_Header] = ImVec4(0.20f, 0.45f, 0.30f, 1);
    c[ImGuiCol_HeaderHovered] = ImVec4(0.25f, 0.55f, 0.36f, 1);
}

void sensitivity_slider() {
    ImGui::SetNextItemWidth(-1);
    if (ImGui::SliderFloat("##sens", &g_settings.sensitivity, k_min_sensitivity, k_max_sensitivity,
                           "Sensibilidad del mouse: %.2fx", ImGuiSliderFlags_Logarithmic)) {
        net_update_settings(g_settings);
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) settings_save(g_settings);
}

void draw_menu(const NetStatus& status) {
    ImGui::TextColored(ImVec4(0.45f, 0.80f, 0.55f, 1), "HALO CE · COOP 4 JUGADORES");
    ImGui::TextDisabled("Conéctate al host por Radmin VPN.");
    ImGui::Separator();

    ImGui::Text("IP de Radmin del host");
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##host", "Te la da el host (sale en su HaloCoop Host)", g_host_text, sizeof(g_host_text),
                             ImGuiInputTextFlags_CharsNoBlank);
    ImGui::Text("Tu nombre");
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##name", "Cómo te verán los demás", g_name_text, sizeof(g_name_text));

    ImGui::Text("Jugador");
    for (int p = 2; p <= 4; ++p) {
        if (p > 2) ImGui::SameLine();
        char label[32];
        snprintf(label, sizeof(label), "Jugador %d", p);
        if (ImGui::RadioButton(label, g_settings.player == p)) g_settings.player = p;
    }
    ImGui::TextDisabled("J2 = arriba a la derecha · J3 = abajo a la izquierda · J4 = abajo a la derecha");

    ImGui::Text("Control");
    if (ImGui::RadioButton("Mando", g_settings.control == ControlMode::gamepad)) g_settings.control = ControlMode::gamepad;
    ImGui::SameLine();
    if (ImGui::RadioButton("Teclado y mouse", g_settings.control == ControlMode::keyboard_mouse))
        g_settings.control = ControlMode::keyboard_mouse;
    if (g_settings.control == ControlMode::gamepad && !input_gamepad_connected())
        ImGui::TextColored(ImVec4(0.95f, 0.70f, 0.30f, 1), "No se detecta ningún mando conectado.");
    if (g_settings.control == ControlMode::keyboard_mouse) sensitivity_slider();

    if (status.state == NetState::failed && !status.message.empty())
        ImGui::TextColored(ImVec4(0.95f, 0.45f, 0.40f, 1), "%s", status.message.c_str());

    ImGui::Spacing();
    bool can_connect = g_host_text[0] != '\0';
    ImGui::BeginDisabled(!can_connect);
    if (ImGui::Button("Conectar", ImVec2(-1, 44))) {
        g_settings.host = g_host_text;
        g_settings.nickname = g_name_text;
        settings_save(g_settings);
        net_connect(g_settings);
    }
    ImGui::EndDisabled();
}

void draw_connecting() {
    ImGui::TextColored(ImVec4(0.45f, 0.80f, 0.55f, 1), "Conectando...");
    ImGui::Text("Host %s  ·  Jugador %d", g_settings.host.c_str(), g_settings.player);
    ImGui::Spacing();
    if (ImGui::Button("Cancelar", ImVec2(-1, 40))) net_disconnect();
}

void draw_connected(const NetStatus& status) {
    ImGui::TextColored(ImVec4(0.45f, 0.80f, 0.55f, 1), "CONECTADO  ·  JUGADOR %d", g_settings.player);
    if (status.ping_ms >= 0)
        ImGui::Text("Ping %d ms  ·  %u envíos/s", status.ping_ms, status.sent_per_second);
    else
        ImGui::Text("Midiendo ping...");
    ImGui::Separator();

    if (g_settings.control == ControlMode::gamepad) {
        if (input_gamepad_connected())
            ImGui::Text("Usando tu mando. Funciona aunque esta ventana no esté activa.");
        else
            ImGui::TextColored(ImVec4(0.95f, 0.70f, 0.30f, 1), "No se detecta ningún mando: conéctalo.");
    } else {
        if (input_captured()) {
            ImGui::TextColored(ImVec4(0.45f, 0.80f, 0.55f, 1), "Mouse y teclado capturados.");
            ImGui::TextDisabled("Esc para soltar el mouse  ·  + / - cambia la sensibilidad");
        } else {
            ImGui::TextColored(ImVec4(0.95f, 0.70f, 0.30f, 1), "Haz clic en esta ventana para jugar con teclado y mouse.");
        }
        sensitivity_slider();
        if (ImGui::CollapsingHeader("Controles del teclado")) {
            ImGui::TextDisabled(
                "WASD moverse · Mouse apuntar · Clic izq. disparar · Clic der. granada\n"
                "Espacio saltar · E/R acción y recargar · F/V golpe · Q o rueda cambiar arma\n"
                "G cambiar granada · Ctrl/C agacharse · Z o clic central zoom\n"
                "1-4 o flechas cruceta (linterna, etc.) · Enter pausa · Tab marcador");
        }
    }
    ImGui::Spacing();
    if (ImGui::Button("Desconectar", ImVec2(-1, 40))) {
        input_set_captured(false);
        net_disconnect();
    }
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    g_settings = settings_load();
    strncpy_s(g_host_text, g_settings.host.c_str(), _TRUNCATE);
    strncpy_s(g_name_text, g_settings.nickname.c_str(), _TRUNCATE);

    WNDCLASSEXW wc{sizeof(wc), CS_CLASSDC, window_proc, 0, 0, instance, LoadIconW(nullptr, IDI_APPLICATION),
                   LoadCursorW(nullptr, IDC_ARROW), nullptr, nullptr, L"HaloCoopClient", nullptr};
    RegisterClassExW(&wc);
    g_window = CreateWindowW(wc.lpszClassName, k_title, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 600, 620,
                             nullptr, nullptr, instance, nullptr);
    if (!create_device(g_window)) {
        MessageBoxW(nullptr, L"No se pudo iniciar Direct3D 11.", k_title, MB_ICONERROR);
        return 1;
    }
    ShowWindow(g_window, SW_SHOWDEFAULT);
    input_init(g_window);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    // Segoe UI de Windows: tiene tildes y ñ.
    ImFontConfig font_config;
    static const ImWchar k_ranges[] = {0x0020, 0x00FF, 0x2010, 0x2027, 0};
    if (!io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 19.0f, &font_config, k_ranges))
        io.Fonts->AddFontDefault();
    apply_style();
    ImGui_ImplWin32_Init(g_window);
    ImGui_ImplDX11_Init(g_device, g_context);

    // Pruebas automáticas: HALOCOOP_AUTOCONNECT=1 conecta al abrir con lo guardado en el .ini.
    if (GetEnvironmentVariableW(L"HALOCOOP_AUTOCONNECT", nullptr, 0) && !g_settings.host.empty())
        net_connect(g_settings);

    bool running = true;
    while (running) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (msg.message == WM_QUIT) running = false;
        }
        if (!running) break;

        NetStatus status = net_status();
        if (status.state != NetState::connected && input_captured()) input_set_captured(false);

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // Clic en la ventana con teclado y mouse: capturar.
        if (status.state == NetState::connected && g_settings.control == ControlMode::keyboard_mouse &&
            !input_captured() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemHovered() &&
            !ImGui::IsAnyItemActive())
            input_set_captured(true);

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::Begin("##main", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_NoBringToFrontOnFocus);
        ImGui::PushItemWidth(-1);
        switch (status.state) {
        case NetState::idle:
        case NetState::failed: draw_menu(status); break;
        case NetState::connecting: draw_connecting(); break;
        case NetState::connected: draw_connected(status); break;
        }
        ImGui::PopItemWidth();
        if (ImGui::GetTime() < g_toast_until) {
            ImGui::SetCursorPosY(io.DisplaySize.y - 40);
            ImGui::TextColored(ImVec4(1, 1, 1, 0.9f), "%s", g_toast);
        }
        ImGui::End();

        ImGui::Render();
        const float clear[4] = {0.07f, 0.09f, 0.11f, 1};
        g_context->OMSetRenderTargets(1, &g_target, nullptr);
        g_context->ClearRenderTargetView(g_target, clear);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_swap_chain->Present(1, 0);
        if (IsIconic(g_window)) Sleep(50);
    }

    input_set_captured(false);
    net_disconnect();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    if (g_target) g_target->Release();
    g_swap_chain->Release();
    g_context->Release();
    g_device->Release();
    return 0;
}
