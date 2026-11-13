// CoOpEvolvedClient: app de los jugadores 2, 3 y 4. Se conecta al host por Radmin VPN, muestra su cuadrante del
// juego a pantalla completa y le manda su mando o teclado y mouse.

#include "audio_player.h"
#include "decoder.h"
#include "input.h"
#include "log.h"
#include "net_client.h"
#include "renderer.h"
#include "settings.h"

#include "../common/lang.h"
#include "../common/ui_armor_color.h"
#include "../common/ui_halo.h"
#include "../common/ui_mode.h"
#include "../common/ui_skin.h"
#include "../common/ui_style.h"

#include <d3d11.h>
#include <dxgi1_5.h>
#include <windows.h>

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace {


ID3D11Device* g_device = nullptr;
ID3D11DeviceContext* g_context = nullptr;
IDXGISwapChain* g_swap_chain = nullptr;
ID3D11RenderTargetView* g_target = nullptr;
HWND g_window = nullptr;
bool g_tearing = false;  // presentar sin esperar al refresco del monitor (menos retraso)
UINT g_swap_flags = 0;
UINT g_width = 0, g_height = 0;

Settings g_settings;
char g_host_text[64] = {};
char g_name_text[proto::k_nickname_size] = {};
float g_toast_until = 0;
char g_toast[128] = {};

std::unique_ptr<Decoder> g_decoder;
std::string g_decoder_error;
VideoRenderer g_renderer;
bool g_has_video = false;
bool g_show_stats = false;
bool g_show_menu = false;
bool g_fullscreen = false;
WINDOWPLACEMENT g_windowed_placement{sizeof(WINDOWPLACEMENT)};

void create_target() {
    ID3D11Texture2D* back = nullptr;
    g_swap_chain->GetBuffer(0, IID_PPV_ARGS(&back));
    g_device->CreateRenderTargetView(back, nullptr, &g_target);
    back->Release();
}

bool create_device(HWND hwnd) {
    // ¿El sistema permite "tearing"? (Windows 10 con drivers recientes) Y en laptops con dos GPU, la NVIDIA:
    // el decodificador NVDEC tiene que estar en la misma GPU que dibuja.
    IDXGIFactory5* factory = nullptr;
    IDXGIAdapter1* nvidia = nullptr;
    if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) {
        BOOL allow = FALSE;
        g_tearing = SUCCEEDED(factory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allow, sizeof(allow))) &&
                    allow;
        for (UINT i = 0; !nvidia; ++i) {
            IDXGIAdapter1* adapter = nullptr;
            if (factory->EnumAdapters1(i, &adapter) == DXGI_ERROR_NOT_FOUND) break;
            DXGI_ADAPTER_DESC1 adapter_desc;
            adapter->GetDesc1(&adapter_desc);
            if (adapter_desc.VendorId == 0x10DE) {
                nvidia = adapter;
                char name[128];
                WideCharToMultiByte(CP_UTF8, 0, adapter_desc.Description, -1, name, sizeof(name), nullptr, nullptr);
                log_write("GPU: %s", name);
            } else {
                adapter->Release();
            }
        }
        factory->Release();
    }
    if (!nvidia) log_write("No se encontró una GPU NVIDIA");
    g_swap_flags = g_tearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

    DXGI_SWAP_CHAIN_DESC desc{};
    desc.BufferCount = 2;
    desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.OutputWindow = hwnd;
    desc.SampleDesc.Count = 1;
    desc.Windowed = TRUE;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.Flags = g_swap_flags;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(nvidia, nvidia ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE,
                                               nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &desc, &g_swap_chain,
                                               &g_device, nullptr, &g_context);
    if (nvidia) nvidia->Release();
    if (FAILED(hr)) return false;
    // Alt+Enter de DXGI no: la pantalla completa la maneja F11 (sin bordes, sin cambio de modo).
    IDXGIFactory1* parent = nullptr;
    if (SUCCEEDED(g_swap_chain->GetParent(IID_PPV_ARGS(&parent)))) {
        parent->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
        parent->Release();
    }
    create_target();
    return true;
}

void toast(const char* text) {
    snprintf(g_toast, sizeof(g_toast), "%s", text);
    g_toast_until = float(ImGui::GetTime()) + 2.5f;
}

void set_fullscreen(bool fullscreen) {
    if (fullscreen == g_fullscreen) return;
    g_fullscreen = fullscreen;
    DWORD style = GetWindowLongW(g_window, GWL_STYLE);
    if (fullscreen) {
        GetWindowPlacement(g_window, &g_windowed_placement);
        MONITORINFO monitor{sizeof(monitor)};
        GetMonitorInfoW(MonitorFromWindow(g_window, MONITOR_DEFAULTTONEAREST), &monitor);
        SetWindowLongW(g_window, GWL_STYLE, style & ~WS_OVERLAPPEDWINDOW);
        const RECT& r = monitor.rcMonitor;
        SetWindowPos(g_window, HWND_TOP, r.left, r.top, r.right - r.left, r.bottom - r.top,
                     SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    } else {
        SetWindowLongW(g_window, GWL_STYLE, style | WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(g_window, &g_windowed_placement);
        SetWindowPos(g_window, nullptr, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }
}

void apply_audio_settings() {
    audio_set_volume(g_settings.volume / 100.0f);
    audio_set_muted(g_settings.muted);
}

void toggle_mute() {
    g_settings.muted = !g_settings.muted;
    apply_audio_settings();
    settings_save(g_settings);
    toast(g_settings.muted ? T("Sound muted (F3)", "Sonido silenciado (F3)") : T("Sound on (F3)", "Sonido activado (F3)"));
}

void change_sensitivity(float factor) {
    g_settings.sensitivity = std::clamp(g_settings.sensitivity * factor, k_min_sensitivity, k_max_sensitivity);
    net_update_settings(g_settings);
    settings_save(g_settings);
    char text[64];
    snprintf(text, sizeof(text), T("Sensitivity: %.2fx", "Sensibilidad: %.2fx"), g_settings.sensitivity);
    toast(text);
}

LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (msg == WM_INPUT) input_on_raw_input(lparam);
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) return true;
    switch (msg) {
    case WM_SIZE:
        g_width = LOWORD(lparam);
        g_height = HIWORD(lparam);
        if (g_device && wparam != SIZE_MINIMIZED) {
            if (g_target) g_target->Release();
            g_target = nullptr;
            g_swap_chain->ResizeBuffers(0, g_width, g_height, DXGI_FORMAT_UNKNOWN, g_swap_flags);
            create_target();
        }
        input_set_captured(false);  // se vuelve a capturar con un clic, ya con el tamaño nuevo
        return 0;
    case WM_ACTIVATE:
        if (LOWORD(wparam) == WA_INACTIVE) input_set_captured(false);
        return 0;
    case WM_KEYDOWN:
        if (wparam == VK_ESCAPE && input_captured()) {
            input_set_captured(false);
            return 0;
        }
        if (wparam == VK_F11) set_fullscreen(!g_fullscreen);
        if (wparam == VK_F1) g_show_stats = !g_show_stats;
        if (wparam == VK_F3) toggle_mute();
        if (wparam == VK_F2 && net_status().state == NetState::connected) {
            g_show_menu = !g_show_menu;
            if (g_show_menu) input_set_captured(false);
        }
        if (input_captured() && (wparam == VK_OEM_PLUS || wparam == VK_ADD)) change_sensitivity(1.1f);
        if (input_captured() && (wparam == VK_OEM_MINUS || wparam == VK_SUBTRACT)) change_sensitivity(1 / 1.1f);
        break;
    case WM_SYSKEYDOWN:
        if (wparam == VK_RETURN) {  // Alt+Enter: pantalla completa
            set_fullscreen(!g_fullscreen);
            return 0;
        }
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

void sensitivity_slider() {
    ImGui::SetNextItemWidth(-1);
    if (ImGui::SliderFloat("##sens", &g_settings.sensitivity, k_min_sensitivity, k_max_sensitivity,
                           T("Mouse sensitivity: %.2fx", "Sensibilidad del mouse: %.2fx"), ImGuiSliderFlags_Logarithmic))
        net_update_settings(g_settings);
    if (ImGui::IsItemDeactivatedAfterEdit()) settings_save(g_settings);
}

// Mira directa con mouse (beta): solo para teclado y mouse. Si el host no la puede usar, sigue el stick.
void direct_aim_control(const NetStatus& status) {
    if (ImGui::Checkbox(T("Direct mouse aim (beta)", "Mira directa con mouse (beta)"), &g_settings.direct_aim)) {
        net_update_settings(g_settings);
        settings_save(g_settings);
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", T("The mouse turns your camera directly, like Halo on PC, instead of moving the\n"
                                  "controller's right stick. If the host's game cannot do it, the stick is used.",
                                  "El mouse gira tu cámara directo, como Halo en PC, en vez de mover el stick derecho\n"
                                  "del mando. Si el juego del host no puede, se usa el stick."));
    if (!g_settings.direct_aim || status.state != NetState::connected) return;
    ImGui::SameLine();
    if (status.direct_aim)
        ImGui::TextColored(k_green, "%s", T("active", "activa"));
    else
        ImGui::TextColored(k_orange, "%s", T("not available yet: using the stick", "todavía no disponible: se usa el stick"));
}

void volume_controls() {
    if (ImGui::Checkbox(T("Mute (F3)", "Silenciar (F3)"), &g_settings.muted)) {
        apply_audio_settings();
        settings_save(g_settings);
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1);
    ImGui::BeginDisabled(g_settings.muted);
    if (ImGui::SliderInt("##volume", &g_settings.volume, 0, 100, T("Game volume: %d%%", "Volumen del juego: %d%%"))) apply_audio_settings();
    if (ImGui::IsItemDeactivatedAfterEdit()) settings_save(g_settings);
    ImGui::EndDisabled();
}

// Etiqueta con la fuente de Halo, centrada en la altura de la casilla que sigue en la misma línea.
void field_label(const char* key) {
    constexpr float k_label = 22;
    float y = ImGui::GetCursorPosY();
    ImGui::SetCursorPosY(y + (ImGui::GetFrameHeight() - k_label) / 2);
    ui_halo_text(key, k_label, k_text);
    // Las casillas de color y de skin empiezan en la misma columna.
    float column = std::max(ui_halo_text_width("armor_color", k_label), ui_halo_text_width("skin", k_label)) +
                   ImGui::GetStyle().ItemSpacing.x * 2;
    ImGui::SameLine(column + ImGui::GetStyle().WindowPadding.x);
    ImGui::SetCursorPosY(y);
}

// Color de armadura: se guarda y, si ya está conectado, le llega al host en menos de un segundo.
void armor_color_control() {
    field_label("armor_color");
    if (ui_armor_color_combo("##armor", &g_settings.armor_color, -1)) {
        net_update_settings(g_settings);
        settings_save(g_settings);
    }
}

// Skin: se guarda y, si ya está conectado, le llega al host en menos de un segundo.
void skin_control(const NetStatus& status) {
    field_label("skin");
    if (ui_skin_combo("##skin", &g_settings.skin, -1, status.skins_available)) {
        net_update_settings(g_settings);
        settings_save(g_settings);
    }
}

void draw_menu(const NetStatus& status) {
    float y = ImGui::GetCursorPosY();
    ImGui::SetCursorPosY(y + (k_tab_h - 30) / 2);
    ui_halo_logo(30, k_text);
    // Modo (crear partida / unirse) e idioma, arriba a la derecha.
    ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - ui_mode_tabs_width());
    ImGui::SetCursorPosY(y);
    if (ui_mode_tabs(false, &g_settings.lang)) settings_save(g_settings);
    // El formulario en una columna centrada (la ventana suele estar maximizada).
    float width = std::min(760.0f, ImGui::GetContentRegionAvail().x);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (ImGui::GetContentRegionAvail().x - width) / 2);
    ui_halo_panel_begin("##join", "join_title", ImVec2(width, 0));
    ImGui::TextDisabled("%s", T("Connect to the host through Radmin VPN.", "Conéctate al host por Radmin VPN."));

    ui_halo_text("host_ip", 22, k_text);
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##host", T("The host gives it to you (shown at the top of their Co Op Evolved)",
                                         "Te la da el host (sale arriba en su Co Op Evolved)"),
                             g_host_text, sizeof(g_host_text), ImGuiInputTextFlags_CharsNoBlank);
    ui_halo_text("your_name", 22, k_text);
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##name", T("How the others will see you", "Cómo te verán los demás"), g_name_text,
                             sizeof(g_name_text));
    armor_color_control();
    skin_control(status);

    ui_halo_text("player", 22, k_text);
    for (int p = 2; p <= 4; ++p) {
        if (p > 2) ImGui::SameLine();
        char label[32];
        snprintf(label, sizeof(label), T("Player %d", "Jugador %d"), p);
        if (ImGui::RadioButton(label, g_settings.player == p)) g_settings.player = p;
    }
    ImGui::TextDisabled("%s", T("P2 = top right · P3 = bottom left · P4 = bottom right",
                                "J2 = arriba a la derecha · J3 = abajo a la izquierda · J4 = abajo a la derecha"));

    ui_halo_text("control", 22, k_text);
    if (ImGui::RadioButton(T("Controller", "Mando"), g_settings.control == ControlMode::gamepad))
        g_settings.control = ControlMode::gamepad;
    ImGui::SameLine();
    if (ImGui::RadioButton(T("Keyboard and mouse", "Teclado y mouse"), g_settings.control == ControlMode::keyboard_mouse))
        g_settings.control = ControlMode::keyboard_mouse;
    if (g_settings.control == ControlMode::gamepad && !input_gamepad_connected())
        ImGui::TextColored(k_orange, "%s", T("No controller detected.", "No se detecta ningún mando conectado."));
    if (g_settings.control == ControlMode::keyboard_mouse) {
        sensitivity_slider();
        direct_aim_control(status);
    }
    volume_controls();

    if (status.state == NetState::failed && !status.message.empty()) ImGui::TextColored(k_red, "%s", status.message.c_str());

    ImGui::Spacing();
    ImGui::BeginDisabled(g_host_text[0] == '\0');
    if (ui_halo_button("connect", ImVec2(ImGui::GetContentRegionAvail().x, 48), k_blue)) {
        g_settings.host = g_host_text;
        g_settings.nickname = g_name_text;
        settings_save(g_settings);
        net_connect(g_settings);
    }
    ImGui::EndDisabled();
    ui_halo_panel_end();
}

void draw_connecting() {
    ImGui::TextColored(k_green, "%s", T("Connecting...", "Conectando..."));
    ImGui::Text(T("Host %s  ·  Player %d", "Host %s  ·  Jugador %d"), g_settings.host.c_str(), g_settings.player);
    ImGui::Spacing();
    if (ImGui::Button(T("Cancel", "Cancelar"), ImVec2(-1, 40))) net_disconnect();
}

// Panel de partida (F2, o mientras no llega el video).
void draw_ingame_panel(const NetStatus& status) {
    ImGui::TextColored(k_green, T("CONNECTED  ·  PLAYER %d", "CONECTADO  ·  JUGADOR %d"), g_settings.player);
    if (status.ping_ms >= 0)
        ImGui::Text("Ping %d ms  ·  video %u fps", status.ping_ms, status.video_fps);
    else
        ImGui::TextUnformatted(T("Measuring ping...", "Midiendo ping..."));
    if (!g_has_video) {
        if (!g_decoder_error.empty())
            ImGui::TextColored(k_red, "%s", g_decoder_error.c_str());
        else
            ImGui::TextColored(k_orange, "%s", T("Waiting for video... The host must have MCC open with Co Op Evolved.",
                                                 "Esperando el video... El host tiene que tener MCC abierto con Co Op Evolved."));
    }
    ImGui::Separator();
    armor_color_control();
    skin_control(status);
    volume_controls();
    if (g_settings.control == ControlMode::gamepad) {
        if (input_gamepad_connected())
            ImGui::TextUnformatted(T("Using your controller.", "Usando tu mando."));
        else
            ImGui::TextColored(k_orange, "%s", T("No controller detected: plug it in.", "No se detecta ningún mando: conéctalo."));
    } else {
        ImGui::TextDisabled("%s", T("Click the video to capture the mouse · Esc releases it · + / - sensitivity",
                                    "Clic en el video para capturar el mouse · Esc lo suelta · + / - sensibilidad"));
        sensitivity_slider();
        direct_aim_control(status);
        if (ImGui::CollapsingHeader(T("Keyboard controls", "Controles del teclado"))) {
            ImGui::TextDisabled("%s", T(
                "WASD move · Mouse aim · Left click shoot · Right click grenade\n"
                "Space jump · E action (vehicles, pick up) · R reload · F/V melee · Q or wheel switch weapon\n"
                "G switch grenade · Ctrl/C crouch · Z or middle click zoom\n"
                "1-4 or arrows d-pad (flashlight, etc.) · Enter pause · Tab scoreboard",
                "WASD moverse · Mouse apuntar · Clic izq. disparar · Clic der. granada\n"
                "Espacio saltar · E acción (vehículos, recoger) · R recargar · F/V golpe · Q o rueda cambiar arma\n"
                "G cambiar granada · Ctrl/C agacharse · Z o clic central zoom\n"
                "1-4 o flechas cruceta (linterna, etc.) · Enter pausa · Tab marcador"));
        }
    }
    ImGui::TextDisabled("%s", T("F1 stats · F2 this menu · F3 mute · F11 full screen",
                                "F1 estadísticas · F2 este menú · F3 silenciar · F11 pantalla completa"));
    ImGui::Spacing();
    if (ImGui::Button(T("Disconnect", "Desconectar"), ImVec2(-1, 40))) {
        input_set_captured(false);
        net_disconnect();
    }
}

void draw_stats(const NetStatus& status) {
    ImGui::SetNextWindowPos(ImVec2(12, 12));
    ImGui::SetNextWindowBgAlpha(0.6f);
    ImGui::Begin("##stats", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoInputs |
                     ImGuiWindowFlags_NoSavedSettings);
    ImGui::Text("Ping %d ms", status.ping_ms);
    ImGui::Text("Video %u fps · %.1f Mbps", status.video_fps, status.video_kbps / 1000.0);
    if (g_decoder) ImGui::Text(T("Decode %.1f ms · %ux%u", "Decodificar %.1f ms · %ux%u"), g_decoder->last_decode_ms(), g_decoder->width(), g_decoder->height());
    ImGui::Text(T("Lost frames %u", "Fotogramas perdidos %u"), status.frames_lost);
    ImGui::Text(T("Inputs %u/s", "Controles %u/s"), status.sent_per_second);
    ImGui::End();
}

void decode_pending_frames() {
    static std::vector<VideoFrame> frames;
    net_take_frames(frames);
    if (frames.empty()) return;
    if (!g_decoder && g_decoder_error.empty()) {
        auto decoder = std::make_unique<Decoder>();
        if (decoder->init(g_device, g_decoder_error)) {
            g_decoder = std::move(decoder);
            log_write("Decodificador NVDEC listo");
        } else {
            log_write("Decodificador: %s", g_decoder_error.c_str());
        }
    }
    if (!g_decoder) return;
    // Hay que decodificarlos todos y en orden (cada uno depende del anterior); solo se ve el último.
    for (VideoFrame& frame : frames) {
        if (g_decoder->decode(frame.data.data(), frame.data.size())) {
            if (!g_has_video) {
                log_write("Primer fotograma de video: %ux%u", g_decoder->width(), g_decoder->height());
                g_has_video = true;
                set_fullscreen(true);
                g_show_menu = false;
                toast(T("F2 menu · F1 stats · F3 mute · F11 full screen", "F2 menú · F1 estadísticas · F3 silenciar · F11 pantalla completa"));
            }
        } else if (frame.keyframe || g_has_video) {
            net_request_keyframe();
        }
    }
    frames.clear();
}

}  // namespace

int client_main(HINSTANCE instance) {
    log_init(L"coopevolved-cliente.log");
    log_write("Co Op Evolved Cliente iniciado (protocolo %u)", proto::k_version);
    // Windows 11 ignora timeBeginPeriod en apps tapadas o minimizadas: el control saldría cada ~16 ms en vez
    // de cada 4. Esto le pide que lo respete siempre.
    PROCESS_POWER_THROTTLING_STATE throttling{PROCESS_POWER_THROTTLING_CURRENT_VERSION,
                                              PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION, 0};
    SetProcessInformation(GetCurrentProcess(), ProcessPowerThrottling, &throttling, sizeof(throttling));
    g_settings = settings_load();
    g_lang = g_settings.lang;
    audio_start();
    apply_audio_settings();
    strncpy_s(g_host_text, g_settings.host.c_str(), _TRUNCATE);
    strncpy_s(g_name_text, g_settings.nickname.c_str(), _TRUNCATE);

    WNDCLASSEXW wc{sizeof(wc), CS_CLASSDC, window_proc, 0, 0, instance, LoadIconW(instance, MAKEINTRESOURCEW(1)),
                   LoadCursorW(nullptr, IDC_ARROW), nullptr, nullptr, L"CoOpEvolvedClient", nullptr};
    RegisterClassExW(&wc);
    g_window = CreateWindowW(wc.lpszClassName, T(L"Halo: Co Op Evolved - Client", L"Halo: Co Op Evolved - Cliente"), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 640, 740,
                             nullptr, nullptr, instance, nullptr);
    if (!create_device(g_window)) {
        MessageBoxW(nullptr, T(L"Could not start Direct3D 11.", L"No se pudo iniciar Direct3D 11."), L"Halo: Co Op Evolved", MB_ICONERROR);
        return 1;
    }
    g_renderer.init(g_device);
    ShowWindow(g_window, SW_SHOWMAXIMIZED);  // siempre maximizada
    input_init(g_window);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    ui_load_font(19.0f);
    ui_halo_set_tab_font(ui_load_tab_font(20.0f));
    ui_apply_style();
    ImGui_ImplWin32_Init(g_window);
    ImGui_ImplDX11_Init(g_device, g_context);
    ui_halo_init(g_device);

    // Pruebas automáticas: COOPEVOLVED_AUTOCONNECT=1 conecta al abrir con lo guardado en el .ini.
    if (GetEnvironmentVariableW(L"COOPEVOLVED_AUTOCONNECT", nullptr, 0) && !g_settings.host.empty())
        net_connect(g_settings);

    NetState previous_state = NetState::idle;
    bool running = true;
    while (running) {
        // Conectado: despertar en cuanto llega un fotograma. En el menú, ~60 veces por segundo.
        NetStatus status = net_status();
        bool connected = status.state == NetState::connected;
        HANDLE frame_event = net_frame_event();
        MsgWaitForMultipleObjects(1, &frame_event, FALSE, connected ? 8 : 16, QS_ALLINPUT);
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (msg.message == WM_QUIT) running = false;
        }
        if (!running) break;

        status = net_status();
        connected = status.state == NetState::connected;
        if (status.state != previous_state) {
            static const char* k_names[] = {"desconectado", "conectando", "conectado", "error"};
            log_write("Estado: %s%s%s", k_names[int(status.state)], status.message.empty() ? "" : " - ",
                      status.message.c_str());
        }
        static ULONGLONG last_stats = 0;
        if (connected && GetTickCount64() - last_stats > 5000) {
            last_stats = GetTickCount64();
            AudioStats audio = audio_take_stats();
            log_write("Ping %d ms · video %u fps %.1f Mbps · perdidos %u · decodificar %.1f ms · controles %u/s · "
                      "audio %u paquetes en 5 s, %u rellenados, colchón %u ms",
                      status.ping_ms, status.video_fps, status.video_kbps / 1000.0, status.frames_lost,
                      g_decoder ? g_decoder->last_decode_ms() : 0.0, status.sent_per_second, audio.packets,
                      audio.concealed, audio.buffered_ms);
        }
        if (!connected && previous_state == NetState::connected) {
            // Fin de la partida: volver al menú en ventana.
            input_set_captured(false);
            set_fullscreen(false);
            g_decoder.reset();
            g_decoder_error.clear();
            g_has_video = false;
            g_show_menu = false;
        }
        previous_state = status.state;
        if (connected) decode_pending_frames();

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        if (!connected || !g_has_video) {
            ui_halo_background();
            ImGui::SetNextWindowPos(ImVec2(0, 0));
            ImGui::SetNextWindowSize(io.DisplaySize);
            ImGui::SetNextWindowBgAlpha(0);
            ImGui::Begin("##main", nullptr,
                         ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoBringToFrontOnFocus);
            switch (status.state) {
            case NetState::idle:
            case NetState::failed: draw_menu(status); break;
            case NetState::connecting: draw_connecting(); break;
            case NetState::connected: draw_ingame_panel(status); break;
            }
            ImGui::End();
        } else if (g_show_menu) {
            ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x / 2, io.DisplaySize.y / 2), ImGuiCond_Always,
                                    ImVec2(0.5f, 0.5f));
            ImGui::SetNextWindowSize(ImVec2(std::min(560.0f, io.DisplaySize.x - 40), 0));
            ImGui::Begin(T("Menu###menu", "Menú###menu"), &g_show_menu, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);
            draw_ingame_panel(status);
            ImGui::End();
        }
        if (connected && g_show_stats) draw_stats(status);
        if (connected && g_has_video && status.watching) {
            ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x / 2, 20), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
            ImGui::SetNextWindowBgAlpha(0.75f);
            ImGui::Begin("##watching", nullptr,
                         ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoInputs |
                             ImGuiWindowFlags_NoSavedSettings);
            ImGui::TextColored(k_orange, "%s",
                               T("You joined with the mission already started: you are watching. You will play when "
                                 "the host starts the mission again from the MCC menu.",
                                 "Entraste con la misión ya empezada: estás mirando. Jugarás cuando el host vuelva a "
                                 "empezar la misión desde el menú de MCC."));
            ImGui::End();
        }
        if (ImGui::GetTime() < g_toast_until) {
            ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x / 2, io.DisplaySize.y - 30), ImGuiCond_Always,
                                    ImVec2(0.5f, 1.0f));
            ImGui::SetNextWindowBgAlpha(0.7f);
            ImGui::Begin("##toast", nullptr,
                         ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoInputs |
                             ImGuiWindowFlags_NoSavedSettings);
            ImGui::TextUnformatted(g_toast);
            ImGui::End();
        }

        // Clic en el video con teclado y mouse: capturar.
        if (connected && g_has_video && !g_show_menu && g_settings.control == ControlMode::keyboard_mouse &&
            !input_captured() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !io.WantCaptureMouse)
            input_set_captured(true);

        ImGui::Render();
        const float clear[4] = {0.0f, 0.0f, 0.0f, 1};
        g_context->OMSetRenderTargets(1, &g_target, nullptr);
        g_context->ClearRenderTargetView(g_target, g_has_video ? clear : k_background);
        if (g_has_video && g_decoder)
            g_renderer.draw(g_context, g_decoder->luma(), g_decoder->chroma(), g_decoder->width(),
                            g_decoder->height(), g_width, g_height);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        // Con video: presentar ya (sin esperar al refresco del monitor). En el menú: sincronizado.
        if (g_has_video)
            g_swap_chain->Present(0, g_tearing ? DXGI_PRESENT_ALLOW_TEARING : 0);
        else
            g_swap_chain->Present(1, 0);
        if (IsIconic(g_window)) Sleep(30);
    }

    input_set_captured(false);
    net_disconnect();
    audio_stop();
    g_decoder.reset();
    ui_halo_release();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    if (g_target) g_target->Release();
    g_swap_chain->Release();
    g_context->Release();
    g_device->Release();
    return 0;
}
