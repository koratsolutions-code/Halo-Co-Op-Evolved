// CoOpEvolvedHost: launcher del host.
// Crea los 3 mandos virtuales ANTES de abrir MCC (si no, MCC no siempre los registra y los jugadores 2-4 no
// se mueven) y muestra el panel del host.

#include "audio_capture.h"
#include "game_install.h"
#include "log.h"
#include "net_server.h"
#include "pads.h"

#include "../common/lang.h"
#include "../common/ui_armor_color.h"
#include "../common/ui_style.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <windows.h>
#include <shellapi.h>
#include <tlhelp32.h>
#include <d3d11.h>

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include <algorithm>
#include <cstdio>
#include <string>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace {

constexpr wchar_t k_title[] = L"Halo: Co Op Evolved - Host";
constexpr wchar_t k_mcc_exe[] = L"MCC-Win64-Shipping.exe";
constexpr wchar_t k_mcc_steam_url[] = L"steam://rungameid/976730";
constexpr wchar_t k_vigembus_url[] = L"https://github.com/nefarius/ViGEmBus/releases/latest";
constexpr wchar_t k_alpharing_url[] = L"https://github.com/wouter51/AlphaRing/releases";

// ---------------------------------------------------------------- configuración (coopevolved-host.ini)

struct HostSettings {
    Lang lang = Lang::en;
    char nickname[proto::k_nickname_size] = {};
    bool name_tags = true;
    int fov = 70;
    int fps = 60;
    int armor_color = 0;  // jugador 1, índice en k_armor_colors
    bool p1_fullscreen = false;
};
HostSettings g_settings;

std::wstring ini_path() {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring folder = path;
    folder = folder.substr(0, folder.find_last_of(L"\\/") + 1);
    std::wstring p = folder + L"coopevolved-host.ini";
    // Antes el proyecto se llamaba HaloCoop: se conservan los datos guardados.
    if (GetFileAttributesW(p.c_str()) == INVALID_FILE_ATTRIBUTES)
        MoveFileW((folder + L"halocoop-host.ini").c_str(), p.c_str());
    return p;
}

void settings_load() {
    std::wstring ini = ini_path();
    g_settings.lang = GetPrivateProfileIntW(L"host", L"lang", 0, ini.c_str()) == 1 ? Lang::es : Lang::en;
    wchar_t name[proto::k_nickname_size] = {};
    GetPrivateProfileStringW(L"host", L"nickname", L"", name, proto::k_nickname_size, ini.c_str());
    WideCharToMultiByte(CP_UTF8, 0, name, -1, g_settings.nickname, sizeof(g_settings.nickname), nullptr, nullptr);
    g_settings.name_tags = GetPrivateProfileIntW(L"host", L"name_tags", 1, ini.c_str()) != 0;
    g_settings.fov = GetPrivateProfileIntW(L"host", L"fov", 70, ini.c_str());
    if (g_settings.fov < 40 || g_settings.fov > 150) g_settings.fov = 70;
    g_settings.fps = GetPrivateProfileIntW(L"host", L"fps", 60, ini.c_str());
    bool valid_fps = false;
    for (int fps : proto::k_fps_options) valid_fps |= fps == g_settings.fps;
    if (!valid_fps) g_settings.fps = 60;
    g_settings.armor_color = GetPrivateProfileIntW(L"host", L"armor_color", 0, ini.c_str());
    if (g_settings.armor_color < 0 || g_settings.armor_color >= k_armor_color_count) g_settings.armor_color = 0;
    g_settings.p1_fullscreen = GetPrivateProfileIntW(L"host", L"p1_fullscreen", 0, ini.c_str()) != 0;
    g_lang = g_settings.lang;
}

void settings_save() {
    std::wstring ini = ini_path();
    wchar_t name[proto::k_nickname_size] = {};
    MultiByteToWideChar(CP_UTF8, 0, g_settings.nickname, -1, name, proto::k_nickname_size);
    WritePrivateProfileStringW(L"host", L"lang", g_settings.lang == Lang::es ? L"1" : L"0", ini.c_str());
    WritePrivateProfileStringW(L"host", L"nickname", name, ini.c_str());
    WritePrivateProfileStringW(L"host", L"name_tags", g_settings.name_tags ? L"1" : L"0", ini.c_str());
    WritePrivateProfileStringW(L"host", L"fov", std::to_wstring(g_settings.fov).c_str(), ini.c_str());
    WritePrivateProfileStringW(L"host", L"fps", std::to_wstring(g_settings.fps).c_str(), ini.c_str());
    WritePrivateProfileStringW(L"host", L"armor_color", std::to_wstring(g_settings.armor_color).c_str(), ini.c_str());
    WritePrivateProfileStringW(L"host", L"p1_fullscreen", g_settings.p1_fullscreen ? L"1" : L"0", ini.c_str());
}

void apply_host_options() {
    HostOptions options;
    options.enabled = g_settings.name_tags;
    options.fov = g_settings.fov;
    memcpy(options.host_name, g_settings.nickname, sizeof(options.host_name));
    options.host_armor_color = g_settings.armor_color;
    options.p1_fullscreen = g_settings.p1_fullscreen;
    net_server_set_host_options(options);
}

// ---------------------------------------------------------------- MCC, Radmin y el mod

std::wstring g_binaries_dir;  // carpeta mcc\binaries\win64 del juego

std::string find_radmin_ip() {
    ULONG size = 32 * 1024;
    std::string buffer(size, '\0');
    auto* adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());
    if (GetAdaptersAddresses(AF_INET, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
                             nullptr, adapters, &size) != NO_ERROR)
        return "";
    for (auto* a = adapters; a; a = a->Next) {
        if (!wcsstr(a->Description, L"Radmin") || a->OperStatus != IfOperStatusUp || !a->FirstUnicastAddress)
            continue;
        auto* sin = reinterpret_cast<sockaddr_in*>(a->FirstUnicastAddress->Address.lpSockaddr);
        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &sin->sin_addr, ip, INET_ADDRSTRLEN);
        return ip;
    }
    return "";
}

DWORD find_mcc_pid() {
    DWORD pid = 0;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W entry{sizeof(entry)};
    for (BOOL ok = Process32FirstW(snapshot, &entry); ok; ok = Process32NextW(snapshot, &entry)) {
        if (_wcsicmp(entry.szExeFile, k_mcc_exe) != 0) continue;
        // Un MCC colgado que ya no tiene ventana no cuenta como abierto.
        HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ProcessID);
        DWORD code = 0;
        bool alive = process && GetExitCodeProcess(process, &code) && code == STILL_ACTIVE;
        if (process) CloseHandle(process);
        if (alive) {
            pid = entry.th32ProcessID;
            break;
        }
    }
    CloseHandle(snapshot);
    return pid;
}

struct WindowSearch {
    DWORD pid;
    HWND best;
    long best_area;
};

BOOL CALLBACK find_largest_window(HWND hwnd, LPARAM param) {
    auto* search = reinterpret_cast<WindowSearch*>(param);
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid != search->pid || !IsWindowVisible(hwnd) || GetWindow(hwnd, GW_OWNER)) return TRUE;
    RECT rect;
    GetWindowRect(hwnd, &rect);
    long area = (rect.right - rect.left) * (rect.bottom - rect.top);
    if (area > search->best_area) {
        search->best = hwnd;
        search->best_area = area;
    }
    return TRUE;
}

HWND find_mcc_window() {
    WindowSearch search{find_mcc_pid(), nullptr, 0};
    if (!search.pid) return nullptr;
    EnumWindows(find_largest_window, reinterpret_cast<LPARAM>(&search));
    return search.best;
}

// MCC ignora los mandos cuando su ventana no está en primer plano.
void focus_mcc() {
    if (HWND mcc = find_mcc_window()) {
        if (IsIconic(mcc)) ShowWindow(mcc, SW_RESTORE);
        SetForegroundWindow(mcc);
    }
}

// Estado que se revisa una vez por segundo (buscar procesos y adaptadores es lento para hacerlo en cada cuadro).
struct Status {
    bool mcc_open = false;
    ModState mod = ModState::game_not_found;
    bool alpharing = false;
    std::string radmin_ip;
    bool video_ok = false;
    proto::VideoStatus video;
    bool audio = false;
    PlayerInfo players[k_remote_players];
};
Status g_status;

void refresh_status() {
    static ULONGLONG last = 0, last_ip = 0;
    ULONGLONG now = GetTickCount64();
    if (last && now - last < 1000) return;
    last = now;
    g_status.mcc_open = find_mcc_window() != nullptr;
    g_status.mod = game_mod_state(g_binaries_dir);
    g_status.alpharing = game_alpharing_present(g_binaries_dir);
    g_status.video_ok = net_server_video_status(g_status.video);
    g_status.audio = audio_capture_active();
    for (int i = 0; i < k_remote_players; ++i) g_status.players[i] = net_server_player(i + k_first_remote_player);
    if (!last_ip || now - last_ip > 5000) {
        last_ip = now;
        g_status.radmin_ip = find_radmin_ip();
    }
}

// ---------------------------------------------------------------- panel

ID3D11Device* g_device = nullptr;
ID3D11DeviceContext* g_context = nullptr;
IDXGISwapChain* g_swap_chain = nullptr;
ID3D11RenderTargetView* g_target = nullptr;
HWND g_window = nullptr;
ImFont* g_title_font = nullptr;
ImFont* g_big_font = nullptr;

void create_target() {
    ID3D11Texture2D* back = nullptr;
    g_swap_chain->GetBuffer(0, IID_PPV_ARGS(&back));
    g_device->CreateRenderTargetView(back, nullptr, &g_target);
    back->Release();
}

bool create_device(HWND hwnd) {
    // GPU por defecto (en laptops, la integrada): el panel no le quita nada a la GPU del juego.
    DXGI_SWAP_CHAIN_DESC desc{};
    desc.BufferCount = 2;
    desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.OutputWindow = hwnd;
    desc.SampleDesc.Count = 1;
    desc.Windowed = TRUE;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
                                               D3D11_SDK_VERSION, &desc, &g_swap_chain, &g_device, nullptr, &g_context);
    if (FAILED(hr))  // sin GPU utilizable: render por software
        hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION,
                                           &desc, &g_swap_chain, &g_device, nullptr, &g_context);
    if (FAILED(hr)) return false;
    create_target();
    return true;
}

// Punto de color + texto, para los estados.
void status_dot(const ImVec4& color, const char* text) {
    ImVec2 pos = ImGui::GetCursorScreenPos();
    float h = ImGui::GetTextLineHeight();
    ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(pos.x + 5, pos.y + h / 2), 5, ImGui::GetColorU32(color));
    ImGui::Dummy(ImVec2(14, h));
    ImGui::SameLine(0, 4);
    ImGui::TextUnformatted(text);
}

void colored_button_style(const ImVec4& base) {
    ImGui::PushStyleColor(ImGuiCol_Button, base);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(base.x * 1.2f, base.y * 1.2f, base.z * 1.2f, 1));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(base.x * 0.85f, base.y * 0.85f, base.z * 0.85f, 1));
}

void draw_header() {
    ImGui::PushFont(g_title_font);
    ImGui::TextColored(k_green, "HALO:");
    ImGui::SameLine();
    ImGui::TextUnformatted("CO OP EVOLVED  ·  HOST");
    ImGui::PopFont();

    // Idioma, arriba a la derecha.
    const char* languages[] = {"English", "Español"};
    float width = 120;
    ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - width);
    ImGui::SetNextItemWidth(width);
    int lang = int(g_settings.lang);
    if (ImGui::Combo("##lang", &lang, languages, 2)) {
        g_settings.lang = Lang(lang);
        g_lang = g_settings.lang;
        settings_save();
    }
}

void draw_connection_card() {
    ImGui::BeginChild("##connection", ImVec2(0, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
    ImGui::TextColored(k_muted, "%s", T("YOUR RADMIN IP  ·  give it to your friends", "TU IP DE RADMIN  ·  pásasela a tus amigos"));
    if (g_status.radmin_ip.empty()) {
        ImGui::TextColored(k_red, "%s", T("Radmin VPN is not connected: your friends will not be able to join.",
                                          "Radmin VPN no está conectado: tus amigos no podrán entrar."));
    } else {
        ImGui::PushFont(g_big_font);
        ImGui::TextUnformatted(g_status.radmin_ip.c_str());
        ImGui::PopFont();
        ImGui::SameLine();
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6);
        if (ImGui::Button(T("Copy", "Copiar"))) ImGui::SetClipboardText(g_status.radmin_ip.c_str());
        ImGui::SameLine();
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6);
        ImGui::TextColored(k_muted, T("UDP port %u", "puerto UDP %u"), proto::k_port);
    }

    ImGui::Separator();
    // Estados en una línea.
    switch (g_status.mod) {
    case ModState::installed: status_dot(k_green, T("Game mod", "Mod del juego")); break;
    case ModState::outdated: status_dot(k_orange, T("Game mod: update it", "Mod del juego: actualízalo")); break;
    case ModState::not_installed: status_dot(k_red, T("Game mod: NOT installed", "Mod del juego: NO instalado")); break;
    case ModState::game_not_found: status_dot(k_red, T("MCC not found in Steam", "No se encontró MCC en Steam")); break;
    }
    ImGui::SameLine(0, 18);
    status_dot(g_status.alpharing ? k_green : k_red, g_status.alpharing ? "AlphaRing" : T("AlphaRing: missing", "AlphaRing: falta"));
    ImGui::SameLine(0, 18);
    status_dot(g_status.mcc_open ? k_green : k_muted, g_status.mcc_open ? T("MCC open", "MCC abierto") : T("MCC closed", "MCC cerrado"));
    ImGui::SameLine(0, 18);
    status_dot(g_status.audio ? k_green : k_muted, g_status.audio ? T("Audio active", "Audio activo") : T("Audio: waiting for MCC", "Audio: esperando a MCC"));

    // Video.
    char video[200];
    ImVec4 video_color = k_muted;
    if (!g_status.video_ok) {
        snprintf(video, sizeof(video), "%s",
                 g_status.mcc_open ? T("Video: the game is not responding (starts ~10 s after MCC opens)",
                                       "Video: el juego no responde (se activa ~10 s después de abrir MCC)")
                                   : T("Video: starts when MCC opens", "Video: se activa al abrir MCC"));
        if (g_status.mcc_open) video_color = k_orange;
    } else if (g_status.video.error != proto::VideoError::none) {
        const char* error = "";
        switch (g_status.video.error) {
        case proto::VideoError::nvenc_failed: error = T("could not start NVENC (see coopevolved-juego.log)", "no se pudo iniciar NVENC (revisa coopevolved-juego.log)"); break;
        case proto::VideoError::no_d3d11_4: error = T("the game does not use D3D11.4", "el juego no usa D3D11.4"); break;
        case proto::VideoError::msaa: error = T("the game uses MSAA antialiasing", "el juego usa antialiasing MSAA"); break;
        default: break;
        }
        snprintf(video, sizeof(video), "Video: %s", error);
        video_color = k_red;
    } else {
        snprintf(video, sizeof(video), T("Video ready  ·  game at %ux%u", "Video listo  ·  juego a %ux%u"),
                 g_status.video.game_width, g_status.video.game_height);
        video_color = k_green;
    }
    status_dot(video_color, video);

    ImGui::Separator();
    ImGui::BeginDisabled(g_status.mcc_open);
    if (ImGui::Button(T("Open MCC", "Abrir MCC"), ImVec2(150, 0))) {
        log_write("Abriendo MCC desde Steam");
        ShellExecuteW(nullptr, L"open", k_mcc_steam_url, nullptr, nullptr, SW_SHOWNORMAL);
    }
    ImGui::EndDisabled();
    if (!g_status.mcc_open && (g_status.mod == ModState::not_installed || g_status.mod == ModState::outdated)) {
        ImGui::SameLine();
        if (ImGui::Button(T("Install mod in the game", "Instalar en el juego"))) {
            std::wstring error;
            if (game_install_mod(g_binaries_dir, error))
                MessageBoxW(g_window, T(L"Mod installed in the game.", L"Mod instalado en el juego."), k_title,
                            MB_ICONINFORMATION);
            else
                MessageBoxW(g_window, error.c_str(), k_title, MB_ICONERROR);
        }
    }
    ImGui::SameLine();
    ImGui::TextColored(k_muted, "%s", T("Choose \"without anti-cheat\"", "Elige \"sin anticheat\""));

    float combo_width = 170;
    ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - combo_width);
    ImGui::SetNextItemWidth(combo_width);
    char preview[32];
    snprintf(preview, sizeof(preview), T("Video at %d FPS", "Video a %d FPS"), g_settings.fps);
    if (ImGui::BeginCombo("##fps", preview)) {
        for (int fps : proto::k_fps_options) {
            char option[32];
            snprintf(option, sizeof(option), T("Video at %d FPS", "Video a %d FPS"), fps);
            if (ImGui::Selectable(option, fps == g_settings.fps)) {
                g_settings.fps = fps;
                net_server_set_fps(fps);
                settings_save();
            }
        }
        ImGui::EndCombo();
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", T("More FPS = less delay, but more upload (about 10 Mbps per friend at 60 FPS).",
                                  "Más FPS = menos retraso, pero más subida (unos 10 Mbps por amigo a 60 FPS)."));
    ImGui::EndChild();
}

void draw_name_tags_card() {
    ImGui::BeginChild("##names", ImVec2(0, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
    if (ImGui::Checkbox(T("Show names over players", "Mostrar nombres sobre los jugadores"), &g_settings.name_tags)) {
        apply_host_options();
        settings_save();
    }
    ImGui::SameLine(0, 24);
    ImGui::BeginDisabled(!g_settings.name_tags);
    ImGui::SetNextItemWidth(-1);
    if (ImGui::SliderInt("##fov", &g_settings.fov, 40, 150, T("Game FOV: %d°", "FOV del juego: %d°"))) apply_host_options();
    if (ImGui::IsItemDeactivatedAfterEdit()) settings_save();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", T("If the names do not sit right over the players, match this to the game's FOV.",
                                  "Si los nombres no quedan justo sobre los jugadores, ajústalo al FOV del juego."));
    ImGui::EndDisabled();
    if (ImGui::Checkbox(T("Player 1 full screen (F6 in game)", "Jugador 1 a pantalla completa (F6 en el juego)"),
                        &g_settings.p1_fullscreen)) {
        apply_host_options();
        settings_save();
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", T("You see only your part of the screen, stretched to your whole monitor. Your friends\n"
                                  "still get their own part. F6 switches it on and off while playing (menus are\n"
                                  "easier to use in split screen). Tip: a higher game resolution looks sharper.",
                                  "Ves solo tu parte de la pantalla, estirada a todo tu monitor. Tus amigos siguen\n"
                                  "recibiendo la suya. F6 lo activa y lo desactiva mientras juegas (los menús se usan\n"
                                  "mejor en pantalla dividida). Consejo: con más resolución en el juego se ve más nítido."));
    ImGui::EndChild();
}

// Tarjeta de un jugador. player 1 es el host; 2-4 son los clientes.
void draw_player_card(int player, float width, float height) {
    char id[16];
    snprintf(id, sizeof(id), "##player%d", player);
    ImGui::BeginChild(id, ImVec2(width, height), ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    static const char* k_quadrants_en[] = {"top left", "top right", "bottom left", "bottom right"};
    static const char* k_quadrants_es[] = {"arriba izq.", "arriba der.", "abajo izq.", "abajo der."};
    ImGui::PushFont(g_big_font);
    ImGui::TextColored(k_green, T("P%d", "J%d"), player);
    ImGui::PopFont();
    ImGui::SameLine();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6);
    ImGui::TextColored(k_muted, "%s", T(k_quadrants_en[player - 1], k_quadrants_es[player - 1]));

    if (player == 1) {
        ImGui::SameLine();
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6);
        ImGui::TextColored(k_muted, "·  %s", T("you, on this PC", "tú, en este PC"));
        // Las dos casillas alineadas a la misma columna.
        const char* armor_label = T("Armor", "Armadura");
        float column = std::max(ImGui::CalcTextSize("Nickname").x, ImGui::CalcTextSize(armor_label).x) +
                       ImGui::GetStyle().ItemSpacing.x * 2;
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Nickname");
        ImGui::SameLine(column);
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputTextWithHint("##nickname", T("Your name over your character", "Tu nombre sobre tu personaje"),
                                     g_settings.nickname, sizeof(g_settings.nickname)))
            apply_host_options();
        if (ImGui::IsItemDeactivatedAfterEdit()) settings_save();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(armor_label);
        ImGui::SameLine(column);
        if (ui_armor_color_combo("##armor", &g_settings.armor_color, -1)) {
            apply_host_options();
            settings_save();
        }
        ImGui::EndChild();
        return;
    }

    int index = player - k_first_remote_player;
    const PlayerInfo& info = g_status.players[index];
    ImGui::SameLine();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6);
    ImGui::TextColored(k_muted, T("·  Controller %d", "·  Controller %d"), pads_slot(player) + 1);

    if (info.connected) {
        status_dot(k_green, info.nickname[0] ? info.nickname : T("(no name)", "(sin nombre)"));
        ImGui::SameLine();
        ImGui::TextColored(k_muted, "·  %s  ·", info.address);
        ImGui::SameLine();
        ui_armor_swatch(info.armor_color);
        ImGui::SameLine();
        ImGui::TextColored(k_muted, "%s", T(k_armor_colors[info.armor_color].name_en, k_armor_colors[info.armor_color].name_es));
        if (g_status.video_ok)
            ImGui::TextColored(k_muted, T("%u inputs/s  ·  video %u fps  ·  %.1f Mbps", "%u controles/s  ·  video %u fps  ·  %.1f Mbps"),
                               info.inputs_per_second, g_status.video.fps_sent[index],
                               g_status.video.kbps_sent[index] / 1000.0);
        else
            ImGui::TextColored(k_muted, T("%u inputs/s  ·  no video yet", "%u controles/s  ·  sin video todavía"),
                               info.inputs_per_second);
        if (info.direct_aim) {
            ImGui::SameLine();
            ImGui::TextColored(k_green, "%s", T("·  mouse aim", "·  mira con mouse"));
        }
    } else {
        status_dot(k_muted, T("Free", "Libre"));
        ImGui::TextColored(k_muted, "%s", T("Waiting for a friend to connect", "Esperando que se conecte un amigo"));
    }

    float button = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) / 2;
    if (ImGui::Button(T("Move", "Mover"), ImVec2(button, 0))) {
        pads_move_test(player);
        focus_mcc();
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", T("Walks, turns and jumps for ~10 s to test this controller.",
                                  "Camina, gira y salta ~10 s para probar este mando."));
    ImGui::SameLine();
    ImGui::BeginDisabled(!info.connected);
    colored_button_style(ImVec4(0.50f, 0.22f, 0.20f, 1));
    if (ImGui::Button(T("Kick", "Expulsar"), ImVec2(button, 0))) {
        net_server_kick(player);  // puede volver a conectarse
        g_status.players[index] = PlayerInfo{};
        focus_mcc();
    }
    ImGui::PopStyleColor(3);
    ImGui::EndDisabled();
    ImGui::EndChild();
}

void draw_panel() {
    refresh_status();
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("##panel", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoBringToFrontOnFocus);
    draw_header();
    draw_connection_card();
    draw_name_tags_card();

    // Jugadores en cuadrícula 2x2, como la pantalla dividida. Alto fijo: título, dos líneas y los botones.
    const ImGuiStyle& style = ImGui::GetStyle();
    float spacing = style.ItemSpacing.x;
    float card_w = (ImGui::GetContentRegionAvail().x - spacing) / 2;
    float card_h = style.WindowPadding.y * 2 + g_big_font->FontSize + ImGui::GetTextLineHeight() * 2 +
                   ImGui::GetFrameHeight() + style.ItemSpacing.y * 3 + 12;
    draw_player_card(1, card_w, card_h);
    ImGui::SameLine();
    draw_player_card(2, card_w, card_h);
    draw_player_card(3, card_w, card_h);
    ImGui::SameLine();
    draw_player_card(4, card_w, card_h);

    ImGui::PushStyleColor(ImGuiCol_Text, k_muted);
    ImGui::TextWrapped("%s", T("In AlphaRing (F4): Players = 4, change nothing else, close it with F4. Use classic graphics "
                               "(Tab) and keep the game window active while you play.",
                               "En AlphaRing (F4): Players = 4, sin cambiar nada más, ciérralo con F4. Usa gráficos "
                               "clásicos (Tab) y deja la ventana del juego activa mientras juegan."));
    ImGui::PopStyleColor();
    ImGui::End();
}

LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) return true;
    switch (msg) {
    case WM_SIZE:
        if (g_device && wparam != SIZE_MINIMIZED) {
            if (g_target) g_target->Release();
            g_target = nullptr;
            g_swap_chain->ResizeBuffers(0, LOWORD(lparam), HIWORD(lparam), DXGI_FORMAT_UNKNOWN, 0);
            create_target();
        }
        return 0;
    case WM_GETMINMAXINFO: {
        auto* info = reinterpret_cast<MINMAXINFO*>(lparam);
        info->ptMinTrackSize = POINT{820, 820};
        return 0;
    }
    case WM_SYSCOMMAND:
        if ((wparam & 0xfff0) == SC_KEYMENU) return 0;  // Alt no abre el menú de la ventana
        break;
    case WM_CLOSE:
        if (find_mcc_pid() &&
            MessageBoxW(hwnd, T(L"MCC is still open. If you close Co Op Evolved Host, players 2, 3 and 4 disconnect and "
                                L"lose their controllers.\n\nClose anyway?",
                                L"MCC sigue abierto. Si cierras Co Op Evolved Host, los jugadores 2, 3 y 4 se "
                                L"desconectan y pierden sus mandos.\n\n¿Cerrar de todas formas?"),
                        k_title, MB_YESNO | MB_ICONWARNING) != IDYES)
            return 0;
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

bool run_panel(HINSTANCE instance) {
    WNDCLASSEXW wc{sizeof(wc), CS_CLASSDC, window_proc, 0, 0, instance, LoadIconW(nullptr, IDI_APPLICATION),
                   LoadCursorW(nullptr, IDC_ARROW), nullptr, nullptr, L"CoOpEvolvedHost", nullptr};
    RegisterClassExW(&wc);
    g_window = CreateWindowW(wc.lpszClassName, k_title, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 900, 860,
                             nullptr, nullptr, instance, nullptr);
    if (!create_device(g_window)) {
        MessageBoxW(nullptr, T(L"Could not start Direct3D 11.", L"No se pudo iniciar Direct3D 11."), k_title,
                    MB_ICONERROR);
        return false;
    }
    ShowWindow(g_window, SW_SHOWDEFAULT);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    ui_load_font(19.0f);
    g_big_font = ui_load_font(26.0f, true);
    g_title_font = ui_load_font(22.0f, true);
    ui_apply_style();
    ImGui_ImplWin32_Init(g_window);
    ImGui_ImplDX11_Init(g_device, g_context);

    bool running = true;
    while (running) {
        // Con el panel en primer plano ~30 cuadros/s; si no, pocos (el host está jugando en otra ventana).
        bool foreground = GetForegroundWindow() == g_window;
        MsgWaitForMultipleObjects(0, nullptr, FALSE, foreground ? 33 : 250, QS_ALLINPUT);
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (msg.message == WM_QUIT) running = false;
        }
        if (!running) break;
        if (IsIconic(g_window)) continue;

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        draw_panel();
        ImGui::Render();
        g_context->OMSetRenderTargets(1, &g_target, nullptr);
        g_context->ClearRenderTargetView(g_target, k_background);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_swap_chain->Present(0, 0);
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    if (g_target) g_target->Release();
    g_swap_chain->Release();
    g_context->Release();
    g_device->Release();
    return true;
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    settings_load();
    // Un solo launcher: dos crearían 6 mandos y desordenarían los slots.
    HANDLE single = CreateMutexW(nullptr, TRUE, L"CoOpEvolvedHost.SingleInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBoxW(nullptr, T(L"Co Op Evolved Host is already open.", L"Co Op Evolved Host ya está abierto."), k_title,
                    MB_ICONINFORMATION);
        return 0;
    }
    log_init();
    log_write("Co Op Evolved Host iniciado");

    // Los mandos tienen que existir antes que MCC.
    while (find_mcc_pid()) {
        log_write("MCC ya estaba abierto al iniciar");
        if (MessageBoxW(nullptr,
                        T(L"MCC is already open.\n\nClose it and press Retry: the controllers for players 2, 3 and 4 "
                          L"must be created before the game opens.",
                          L"MCC ya está abierto.\n\nCiérralo y presiona Reintentar: los mandos de los jugadores 2, 3 "
                          L"y 4 tienen que crearse antes de abrir el juego."),
                        k_title, MB_RETRYCANCEL | MB_ICONWARNING) != IDRETRY)
            return 0;
    }

    switch (pads_init()) {
    case PadsError::none:
        break;
    case PadsError::driver_missing:
        if (MessageBoxW(nullptr,
                        T(L"The ViGEmBus driver (virtual controllers) is missing.\n\nOpen the download page? Install "
                          L"ViGEmBus_..._x64_x86_arm64.exe, restart the PC and open Co Op Evolved Host again.",
                          L"Falta el driver ViGEmBus (mandos virtuales).\n\n¿Abrir la página de descarga? Instala el "
                          L"archivo ViGEmBus_..._x64_x86_arm64.exe, reinicia el PC y vuelve a abrir Co Op Evolved Host."),
                        k_title, MB_YESNO | MB_ICONERROR) == IDYES)
            ShellExecuteW(nullptr, L"open", k_vigembus_url, nullptr, nullptr, SW_SHOWNORMAL);
        return 1;
    case PadsError::create_failed:
        MessageBoxW(nullptr,
                    T(L"Could not create the virtual controllers. See coopevolved-host.log.",
                      L"No se pudieron crear los mandos virtuales. Revisa coopevolved-host.log."),
                    k_title, MB_ICONERROR);
        return 1;
    }

    if (pads_occupied_slots_before() > 0 || pads_slot(2) != 0 || pads_slot(3) != 1 || pads_slot(4) != 2) {
        MessageBoxW(nullptr,
                    T(L"A physical controller is plugged in, so the virtual controllers did not get Controller 1, 2 "
                      L"and 3.\n\nEasiest fix: unplug the physical controller and open Co Op Evolved Host again. "
                      L"Otherwise, in AlphaRing assign each player the Controller shown in the panel.",
                      L"Hay un mando físico conectado, así que los mandos virtuales no quedaron en Controller 1, 2 y "
                      L"3.\n\nLo más fácil: desconecta el mando físico y vuelve a abrir Co Op Evolved Host. Si no, en "
                      L"AlphaRing asigna a cada jugador el Controller que muestra el panel."),
                    k_title, MB_ICONWARNING);
    }

    // El DLL del juego (video y nombres): instalarlo o actualizarlo ahora que MCC está cerrado.
    g_binaries_dir = game_binaries_dir();
    log_write("Carpeta del juego: %s", g_binaries_dir.empty() ? "no encontrada" : "encontrada");
    ModState mod = game_mod_state(g_binaries_dir);
    bool auto_install = !GetEnvironmentVariableW(L"COOPEVOLVED_NO_INSTALL", nullptr, 0);  // pruebas: no tocar el juego
    if (auto_install && (mod == ModState::not_installed || mod == ModState::outdated)) {
        std::wstring error;
        if (!game_install_mod(g_binaries_dir, error))
            MessageBoxW(nullptr,
                        (std::wstring(T(L"Could not install the game mod (video):\n\n",
                                        L"No se pudo instalar el mod del juego (video):\n\n")) +
                         error)
                            .c_str(),
                        k_title, MB_ICONWARNING);
    }
    if (!g_binaries_dir.empty() && !game_alpharing_present(g_binaries_dir) &&
        MessageBoxW(nullptr,
                    T(L"AlphaRing (4-player split screen) is missing from the game folder.\n\nOpen its download page? "
                      L"Copy WTSAPI32.dll from Release1.0.zip into mcc\\binaries\\win64.",
                      L"Falta AlphaRing (pantalla dividida de 4 jugadores) en la carpeta del juego.\n\n¿Abrir su "
                      L"página de descarga? Copia WTSAPI32.dll de Release1.0.zip en mcc\\binaries\\win64."),
                    k_title, MB_YESNO | MB_ICONWARNING) == IDYES)
        ShellExecuteW(nullptr, L"open", k_alpharing_url, nullptr, nullptr, SW_SHOWNORMAL);

    net_server_set_fps(g_settings.fps);
    apply_host_options();
    if (!net_server_start()) {
        MessageBoxW(nullptr,
                    T(L"Could not open UDP port 27960: another program is using it.\n\nThe controllers work, but your "
                      L"friends will not be able to connect.",
                      L"No se pudo abrir el puerto UDP 27960: otro programa lo está usando.\n\nLos mandos "
                      L"funcionan, pero tus amigos no podrán conectarse."),
                    k_title, MB_ICONWARNING);
    }

    // Audio de MCC para los clientes. El pid se toma de la ventana del juego: un MCC colgado sin ventana no sirve.
    audio_capture_start(
        [] {
            DWORD pid = 0;
            if (HWND mcc = find_mcc_window()) GetWindowThreadProcessId(mcc, &pid);
            // Pruebas: COOPEVOLVED_AUDIO_PID=n captura otro proceso en lugar de MCC.
            wchar_t test_pid[16];
            if (GetEnvironmentVariableW(L"COOPEVOLVED_AUDIO_PID", test_pid, 16)) pid = DWORD(_wtoi(test_pid));
            return static_cast<unsigned long>(pid);
        },
        [](const uint8_t* packet, size_t size) { net_server_send_audio(packet, size); });

    run_panel(instance);
    audio_capture_stop();
    net_server_stop();
    pads_shutdown();
    log_write("Co Op Evolved Host cerrado");
    if (single) CloseHandle(single);
    return 0;
}
