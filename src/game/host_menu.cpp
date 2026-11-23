#include <winsock2.h>
#include <ws2tcpip.h>

#include "host_menu.h"
#include "local_players.h"
#include "log.h"
#include "p1_view.h"
#include "skins.h"
#include "video.h"

#include "../common/lang.h"
#include "../common/protocol.h"
#include "../common/ui_armor_color.h"
#include "../common/ui_skin.h"
#include "../common/ui_style.h"

#include <d3d11.h>
#include <xinput.h>

#include <imgui.h>
#include <imgui_impl_dx11.h>

#include <algorithm>
#include <cstring>

namespace {

// ImGui propio del menú (los nombres tienen el suyo: cada uno con su contexto y su estado).
ImGuiContext* g_imgui = nullptr;
ID3D11Device* g_device = nullptr;
ImGuiStyle g_base_style;  // el estilo a 1080p; se escala a la resolución del juego en cada cuadro

bool g_open = false;
bool g_f2_down = false, g_b_down = false, g_back_used = false;
ULONGLONG g_back_since = 0;
bool g_popup_open = false;  // una lista o el selector de color abierto (B cierra eso, no el menú)
bool g_just_opened = false;  // el primer cuadro después de abrir: el menú toma el foco
LARGE_INTEGER g_last_frame{};

// El mando físico del host. XINPUT1_3 está desviado (sus slots son los de los amigos): se lee con xinput1_4.
using XInputGetStateFn = DWORD(WINAPI*)(DWORD, XINPUT_STATE*);
XInputGetStateFn g_pad_get = nullptr;

SOCKET g_socket = INVALID_SOCKET;

// Lo que muestra el menú. Lo que llega del launcher solo se adopta cuando cambia (si no, un VideoConfig viejo que
// llega justo después de tocar algo lo volvería a su valor anterior por un momento).
struct Values {
    uint32_t armor = 0;
    int skin = 0;
    bool p1_subtitles = true, name_tags = true, full_cinematics = true, hide_subtitles = false,
         copy_subtitles = true;
};
Values g_shown, g_last_config;
bool g_have_config = false;

Values from_config(const proto::VideoConfig& config) {
    Values v;
    v.armor = armor_sanitize(config.armor_colors[0]);
    v.skin = config.skins[0] < k_skin_count ? config.skins[0] : 0;
    v.p1_subtitles = config.subtitles_for[0] != 0;
    v.name_tags = config.name_tags != 0;
    v.full_cinematics = config.full_cinematics != 0;
    v.hide_subtitles = config.hide_subtitles != 0;
    v.copy_subtitles = config.copy_subtitles != 0;
    return v;
}

void adopt(const Values& incoming) {
    if (!g_have_config) {
        g_shown = incoming;
    } else {
        auto take = [&](auto field) {
            if (incoming.*field != g_last_config.*field) g_shown.*field = incoming.*field;
        };
        take(&Values::armor);
        take(&Values::skin);
        take(&Values::p1_subtitles);
        take(&Values::name_tags);
        take(&Values::full_cinematics);
        take(&Values::hide_subtitles);
        take(&Values::copy_subtitles);
    }
    g_last_config = incoming;
    g_have_config = true;
}

// Al launcher (el mismo puerto donde recibe el estado del video). Es local, pero por las dudas dos veces.
template <typename T>
void send_to_launcher(const T& message) {
    if (g_socket == INVALID_SOCKET) g_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (g_socket == INVALID_SOCKET) return;
    sockaddr_in launcher{};
    launcher.sin_family = AF_INET;
    launcher.sin_port = htons(proto::k_port);
    launcher.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    for (int i = 0; i < 2; ++i)
        sendto(g_socket, reinterpret_cast<const char*>(&message), sizeof(message), 0,
               reinterpret_cast<sockaddr*>(&launcher), sizeof(launcher));
}

void send_values() {
    proto::HostMenu menu;
    menu.armor_color = g_shown.armor;
    menu.skin = uint8_t(g_shown.skin);
    menu.p1_subtitles = g_shown.p1_subtitles ? 1 : 0;
    menu.name_tags = g_shown.name_tags ? 1 : 0;
    menu.full_cinematics = g_shown.full_cinematics ? 1 : 0;
    menu.hide_subtitles = g_shown.hide_subtitles ? 1 : 0;
    menu.copy_subtitles = g_shown.copy_subtitles ? 1 : 0;
    send_to_launcher(menu);
}

bool game_in_foreground() {
    DWORD pid = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &pid);
    return pid == GetCurrentProcessId();
}

bool read_pad(XINPUT_STATE& state) {
    static bool loaded = false;
    if (!loaded) {
        loaded = true;
        if (HMODULE xinput = LoadLibraryW(L"xinput1_4.dll"))
            g_pad_get = reinterpret_cast<XInputGetStateFn>(GetProcAddress(xinput, "XInputGetState"));
    }
    // Solo si los mandos de los amigos pasan por este DLL: si no, el slot 0 podría ser el mando de un amigo.
    if (!g_pad_get || !local_players_own_input()) return false;
    memset(&state, 0, sizeof(state));
    return g_pad_get(0, &state) == ERROR_SUCCESS;
}

void set_open(bool open) {
    if (open == g_open) return;
    g_open = open;
    g_just_opened = open;
    g_popup_open = false;
    local_players_block_host(open);
    log_write("Menú F2 del host: %s", open ? "abierto" : "cerrado");
}

// F2 / View 1 s abren y cierran; B cierra (si no hay una lista abierta). Solo con el juego en primer plano.
void poll_toggle(bool pad_ok, const XINPUT_STATE& pad) {
    bool foreground = game_in_foreground();
    bool f2 = (GetAsyncKeyState(VK_F2) & 0x8000) != 0;
    if (f2 && !g_f2_down && foreground) set_open(!g_open);
    g_f2_down = f2;

    bool back = pad_ok && (pad.Gamepad.wButtons & XINPUT_GAMEPAD_BACK);
    ULONGLONG now = GetTickCount64();
    if (!back) {
        g_back_since = 0;
        g_back_used = false;
    } else if (!g_back_since) {
        g_back_since = now;
    } else if (!g_back_used && now - g_back_since >= 1000 && foreground) {
        g_back_used = true;
        set_open(!g_open);
    }

    bool b = pad_ok && (pad.Gamepad.wButtons & XINPUT_GAMEPAD_B);
    if (b && !g_b_down && g_open && !g_popup_open && foreground) set_open(false);
    g_b_down = b;
}

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
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
    io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
    io.MouseDrawCursor = true;  // el cursor de Windows está escondido dentro del juego
    ui_apply_style();
    g_base_style = ImGui::GetStyle();
    ui_load_font(24);
    g_device = device;
    bool ok = ImGui_ImplDX11_Init(device, context);
    log_write("Menú F2 del host: dibujo %s", ok ? "listo" : "FALLÓ");
    return ok;
}

// Teclado, mouse y mando del host hacia ImGui (sin desviar la ventana del juego: se leen directo).
void feed_input(IDXGISwapChain* swap_chain, const D3D11_TEXTURE2D_DESC& desc, bool pad_ok, const XINPUT_STATE& pad) {
    ImGuiIO& io = ImGui::GetIO();
    bool foreground = game_in_foreground();
    DXGI_SWAP_CHAIN_DESC sc{};
    swap_chain->GetDesc(&sc);
    POINT cursor;
    RECT client;
    if (foreground && sc.OutputWindow && GetCursorPos(&cursor) && ScreenToClient(sc.OutputWindow, &cursor) &&
        GetClientRect(sc.OutputWindow, &client) && client.right > 0 && client.bottom > 0) {
        io.AddMousePosEvent(float(cursor.x) * desc.Width / client.right, float(cursor.y) * desc.Height / client.bottom);
    }
    io.AddMouseButtonEvent(0, foreground && (GetAsyncKeyState(VK_LBUTTON) & 0x8000));

    // Esc no: abre la pausa de MCC. Retroceso hace de "volver" (cierra una lista abierta).
    static const struct {
        int vk;
        ImGuiKey key;
    } k_keys[] = {{VK_UP, ImGuiKey_UpArrow},     {VK_DOWN, ImGuiKey_DownArrow}, {VK_LEFT, ImGuiKey_LeftArrow},
                  {VK_RIGHT, ImGuiKey_RightArrow}, {VK_RETURN, ImGuiKey_Enter},  {VK_SPACE, ImGuiKey_Space},
                  {VK_TAB, ImGuiKey_Tab},         {VK_BACK, ImGuiKey_Escape},    {VK_SHIFT, ImGuiMod_Shift}};
    for (const auto& k : k_keys) io.AddKeyEvent(k.key, foreground && (GetAsyncKeyState(k.vk) & 0x8000));

    WORD buttons = pad_ok && foreground ? pad.Gamepad.wButtons : 0;
    io.AddKeyEvent(ImGuiKey_GamepadDpadUp, buttons & XINPUT_GAMEPAD_DPAD_UP);
    io.AddKeyEvent(ImGuiKey_GamepadDpadDown, buttons & XINPUT_GAMEPAD_DPAD_DOWN);
    io.AddKeyEvent(ImGuiKey_GamepadDpadLeft, buttons & XINPUT_GAMEPAD_DPAD_LEFT);
    io.AddKeyEvent(ImGuiKey_GamepadDpadRight, buttons & XINPUT_GAMEPAD_DPAD_RIGHT);
    io.AddKeyEvent(ImGuiKey_GamepadFaceDown, buttons & XINPUT_GAMEPAD_A);
    io.AddKeyEvent(ImGuiKey_GamepadFaceRight, buttons & XINPUT_GAMEPAD_B);
    float lx = pad_ok && foreground ? pad.Gamepad.sThumbLX / 32767.0f : 0;
    float ly = pad_ok && foreground ? pad.Gamepad.sThumbLY / 32767.0f : 0;
    auto stick = [&](ImGuiKey key, float v) {
        float amount = std::clamp((v - 0.35f) / 0.65f, 0.0f, 1.0f);
        io.AddKeyAnalogEvent(key, amount > 0.1f, amount);
    };
    stick(ImGuiKey_GamepadLStickUp, ly);
    stick(ImGuiKey_GamepadLStickDown, -ly);
    stick(ImGuiKey_GamepadLStickLeft, -lx);
    stick(ImGuiKey_GamepadLStickRight, lx);
}

void section(const char* text) {
    ImGui::Spacing();
    ImGui::TextColored(k_cyan, "%s", text);
    ImGui::Separator();
}

// true si algo cambió (hay que avisar al launcher).
bool draw_menu(const proto::VideoConfig& config, bool have_config, float scale) {
    bool changed = false;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->Size.x / 2, viewport->Size.y / 2), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(620 * scale, 0), ImGuiCond_Always);
    if (g_just_opened) ImGui::SetNextWindowFocus();
    g_just_opened = false;
    ImGui::Begin("##host_menu", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::TextColored(k_cyan, "CO OP EVOLVED");
    ImGui::SameLine();
    ImGui::TextColored(k_muted, "%s", T("·  Player 1", "·  Jugador 1"));
    ImGui::TextColored(k_muted, "%s",
                       T("F2 or B: close  ·  arrows / D-pad and Enter / A to move",
                         "F2 o B: cerrar  ·  flechas / cruceta y Enter / A para moverte"));

    section(T("You", "Tú"));
    float label_w = 150 * scale;
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(T("Armor color", "Color de armadura"));
    ImGui::SameLine(label_w);
    if (ui_armor_color_combo("##armor", &g_shown.armor, -1)) changed = true;
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(T("Skin", "Skin"));
    ImGui::SameLine(label_w);
    if (ui_skin_combo("##skin", &g_shown.skin, -1, skins_available())) changed = true;

    section(T("Screen", "Pantalla"));
    bool full = p1_view_enabled();
    if (ImGui::Checkbox(T("Player 1 full screen (F6)", "Jugador 1 a pantalla completa (F6)"), &full)) p1_view_set(full);
    changed |= ImGui::Checkbox(T("Show names over players", "Mostrar nombres sobre los jugadores"), &g_shown.name_tags);
    changed |= ImGui::Checkbox(T("Full-screen cutscenes for friends", "Cinemáticas completas para los amigos"),
                               &g_shown.full_cinematics);

    section(T("Subtitles", "Subtítulos"));
    changed |= ImGui::Checkbox(T("Hide MCC subtitles (everyone)", "Ocultar los subtítulos de MCC (todos)"),
                               &g_shown.hide_subtitles);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", T("Applies when a mission starts.", "Vale al empezar una misión."));
    ImGui::BeginDisabled(g_shown.hide_subtitles);
    changed |= ImGui::Checkbox(T("Subtitles for each player", "Subtítulos para cada jugador"), &g_shown.copy_subtitles);
    ImGui::BeginDisabled(!g_shown.copy_subtitles);
    changed |= ImGui::Checkbox(T("Also in my part (full screen F6)", "También en mi parte (pantalla completa F6)"),
                               &g_shown.p1_subtitles);
    ImGui::EndDisabled();
    ImGui::EndDisabled();

    section(T("Players", "Jugadores"));
    static int s_kick = 0;  // jugador a expulsar (2..4) mientras se pregunta
    static char s_kick_name[proto::k_nickname_size + 1] = {};
    for (int i = 0; i < 3; ++i) {
        bool connected = have_config && config.targets[i].active;
        char name[proto::k_nickname_size + 1] = {};
        memcpy(name, config.names[i + 1], proto::k_nickname_size);
        ImGui::AlignTextToFramePadding();
        ImGui::Text("P%d", i + 2);
        ImGui::SameLine(60 * scale);
        if (connected) {
            ImGui::TextColored(k_green, "%s", name[0] ? name : T("connected", "conectado"));
            ImGui::SameLine(ImGui::GetContentRegionMax().x - 130 * scale);
            ImGui::PushID(i);
            ImGui::PushStyleColor(ImGuiCol_Button, k_kick);
            if (ImGui::Button(T("Kick", "Expulsar"), ImVec2(130 * scale, 0))) {
                s_kick = i + 2;
                memcpy(s_kick_name, name, sizeof(s_kick_name));
            }
            ImGui::PopStyleColor();
            ImGui::PopID();
        } else {
            ImGui::TextColored(k_muted, "%s", T("not connected", "sin conectar"));
        }
    }
    if (s_kick && !ImGui::IsPopupOpen("##kick")) ImGui::OpenPopup("##kick");
    if (ImGui::BeginPopup("##kick")) {
        ImGui::Text(T("Kick P%d (%s)? They can connect again.", "¿Expulsar a P%d (%s)? Puede volver a conectarse."),
                    s_kick, s_kick_name[0] ? s_kick_name : "?");
        if (ImGui::Button(T("Kick", "Expulsar"), ImVec2(140 * scale, 0))) {
            proto::HostKick kick;
            kick.player = uint8_t(s_kick);
            send_to_launcher(kick);
            log_write("Menú F2 del host: expulsar a J%d", s_kick);
            s_kick = 0;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(T("Cancel", "Cancelar"), ImVec2(140 * scale, 0))) {
            s_kick = 0;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    } else {
        s_kick = 0;  // se cerró con B o haciendo clic afuera
    }
    // Recargar la misión: así entran los amigos que se conectaron con la misión empezada.
    static bool s_reload = false;
    ImGui::Spacing();
    if (int waiting = local_players_waiting())
        ImGui::TextColored(k_orange, T("%d friend(s) waiting to join the mission.", "%d amigo(s) esperando entrar a la misión."),
                           waiting);
    // Solo con alguien esperando: si la cantidad de jugadores no cambia, MCC deja su pantalla de carga encima.
    bool can_reload = local_players_waiting() > 0;
    ImGui::BeginDisabled(!can_reload);
    if (ImGui::Button(T("Reload Mission", "Recargar misión"), ImVec2(-1, 0))) s_reload = true;
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("%s", can_reload ? T("Starts the mission again with everyone connected.",
                                               "Empieza la misión otra vez con todos los conectados.")
                                           : T("Available during a mission when a friend is waiting to join.\n"
                                               "To restart without new friends, use Restart in MCC's pause menu.",
                                               "Disponible en una misión cuando un amigo espera entrar.\n"
                                               "Para reiniciar sin amigos nuevos, usa Reiniciar en la pausa de MCC."));
    if (!can_reload) s_reload = false;
    if (s_reload && !ImGui::IsPopupOpen("##reload")) ImGui::OpenPopup("##reload");
    if (ImGui::BeginPopup("##reload")) {
        ImGui::TextUnformatted(T("The mission starts again from the beginning with everyone connected.",
                                 "La misión empieza otra vez desde el principio con todos los conectados."));
        ImGui::TextColored(k_muted, "%s",
                           T("Progress since the start of the mission is lost.",
                             "Se pierde lo avanzado desde el inicio de la misión."));
        if (ImGui::Button(T("Reload", "Recargar"), ImVec2(140 * scale, 0))) {
            local_players_reload_mission();
            log_write("Menú F2 del host: recargar la misión");
            s_reload = false;
            ImGui::CloseCurrentPopup();
            set_open(false);
        }
        ImGui::SameLine();
        if (ImGui::Button(T("Cancel", "Cancelar"), ImVec2(140 * scale, 0))) {
            s_reload = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    } else {
        s_reload = false;
    }
    if (!have_config)
        ImGui::TextColored(k_orange, "%s",
                           T("Co Op Evolved is closed: changes are not saved.",
                             "Co Op Evolved está cerrado: los cambios no se guardan."));

    g_popup_open = ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
    ImGui::End();
    return changed;
}

// Con el menú cerrado: aviso arriba al centro si hay amigos esperando entrar (solo lo ve el host).
void draw_waiting_notice(int waiting, float scale) {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->Size.x / 2, 40 * scale), ImGuiCond_Always, ImVec2(0.5f, 0));
    ImGui::SetNextWindowBgAlpha(0.75f);
    ImGui::Begin("##waiting", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize |
                     ImGuiWindowFlags_NoFocusOnAppearing);
    ImGui::TextColored(k_orange,
                       T("%d friend(s) waiting to join  ·  F2 > Reload Mission",
                         "%d amigo(s) esperando entrar  ·  F2 > Recargar misión"),
                       waiting);
    ImGui::End();
}

}  // namespace

void host_menu_draw(IDXGISwapChain* swap_chain) {
    XINPUT_STATE pad{};
    bool pad_ok = read_pad(pad);
    poll_toggle(pad_ok, pad);

    proto::VideoConfig config;
    bool have_config = video_config(config);
    if (have_config) {
        adopt(from_config(config));
        g_lang = config.lang == 1 ? Lang::es : Lang::en;
    }
    int waiting = g_open ? 0 : local_players_waiting();
    if (!g_open && !waiting) return;

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
        LARGE_INTEGER now, frequency;
        QueryPerformanceCounter(&now);
        QueryPerformanceFrequency(&frequency);
        float dt = g_last_frame.QuadPart ? float(now.QuadPart - g_last_frame.QuadPart) / float(frequency.QuadPart) : 0;
        g_last_frame = now;
        io.DeltaTime = std::clamp(dt, 0.001f, 0.1f);
        float scale = std::clamp(float(desc.Height) / 1080.0f, 0.7f, 2.0f);
        ImGui::GetStyle() = g_base_style;
        ImGui::GetStyle().ScaleAllSizes(scale);
        io.FontGlobalScale = scale;
        io.MouseDrawCursor = g_open;  // el aviso no usa el mouse
        if (g_open) feed_input(swap_chain, desc, pad_ok, pad);

        ImGui_ImplDX11_NewFrame();
        ImGui::NewFrame();
        if (!g_open)
            draw_waiting_notice(waiting, scale);
        else if (draw_menu(config, have_config, scale))
            send_values();
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
