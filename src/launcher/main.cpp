// HaloCoopHost: launcher del host.
// Crea los 3 mandos virtuales ANTES de abrir MCC (si no, MCC no siempre los registra y los jugadores 2-4 no
// se mueven) y muestra el panel del host.

#include "log.h"
#include "pads.h"

#include <windows.h>
#include <shellapi.h>
#include <tlhelp32.h>

#include <cstdio>

static constexpr wchar_t k_title[] = L"HaloCoop Host";
static constexpr wchar_t k_mcc_exe[] = L"MCC-Win64-Shipping.exe";
static constexpr wchar_t k_mcc_steam_url[] = L"steam://rungameid/976730";
static constexpr wchar_t k_vigembus_url[] = L"https://github.com/nefarius/ViGEmBus/releases/latest";

static constexpr UINT_PTR k_refresh_timer = 1;
static constexpr int k_id_launch = 100;
static constexpr int k_id_move = 200;  // +0, +1, +2 para jugadores 2, 3, 4

struct Ui {
    HWND window = nullptr;
    HWND pads_label = nullptr;
    HWND mcc_label = nullptr;
    HWND launch_button = nullptr;
};
static Ui g_ui;

static DWORD find_mcc_pid() {
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

static BOOL CALLBACK find_largest_window(HWND hwnd, LPARAM param) {
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

static HWND find_mcc_window() {
    WindowSearch search{find_mcc_pid(), nullptr, 0};
    if (!search.pid) return nullptr;
    EnumWindows(find_largest_window, reinterpret_cast<LPARAM>(&search));
    return search.best;
}

// MCC ignora los mandos cuando su ventana no está en primer plano.
static void focus_mcc() {
    if (HWND mcc = find_mcc_window()) {
        if (IsIconic(mcc)) ShowWindow(mcc, SW_RESTORE);
        SetForegroundWindow(mcc);
    }
}

static void refresh() {
    bool mcc_open = find_mcc_window() != nullptr;
    SetWindowTextW(g_ui.mcc_label, mcc_open
        ? L"MCC: abierto. Mantén la ventana del juego activa mientras juegan."
        : L"MCC: cerrado. Ábrelo con el botón y elige \"sin anticheat\".");
    EnableWindow(g_ui.launch_button, !mcc_open);
}

static LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
    case WM_TIMER:
        refresh();
        return 0;
    case WM_COMMAND: {
        int id = LOWORD(wparam);
        if (id == k_id_launch) {
            log_write("Abriendo MCC desde Steam");
            ShellExecuteW(nullptr, L"open", k_mcc_steam_url, nullptr, nullptr, SW_SHOWNORMAL);
        } else if (id >= k_id_move && id < k_id_move + k_remote_players) {
            pads_move_test(id - k_id_move + k_first_remote_player);
            focus_mcc();
        }
        return 0;
    }
    case WM_CLOSE:
        if (find_mcc_pid() &&
            MessageBoxW(hwnd, L"MCC sigue abierto. Si cierras HaloCoop Host, los jugadores 2, 3 y 4 pierden sus "
                              L"mandos.\n\n¿Cerrar de todas formas?",
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

static void create_ui(HINSTANCE instance) {
    WNDCLASSW wc{0, window_proc, 0, 0, instance, LoadIconW(nullptr, IDI_APPLICATION),
                 LoadCursorW(nullptr, IDC_ARROW), reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1), nullptr,
                 L"HaloCoopHost"};
    RegisterClassW(&wc);

    const int width = 560, margin = 12, row_h = 32;
    RECT rect{0, 0, width, margin * 2 + row_h * (k_remote_players + 4) + 20};
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    AdjustWindowRect(&rect, style, FALSE);
    g_ui.window = CreateWindowW(L"HaloCoopHost", k_title, style, CW_USEDEFAULT, CW_USEDEFAULT,
                                rect.right - rect.left, rect.bottom - rect.top, nullptr, nullptr, instance, nullptr);

    HFONT font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    auto make = [&](const wchar_t* cls, const wchar_t* text, DWORD extra, int x, int y, int w, int h, int id) {
        HWND control = CreateWindowW(cls, text, WS_CHILD | WS_VISIBLE | extra, x, y, w, h, g_ui.window,
                                     reinterpret_cast<HMENU>(INT_PTR(id)), instance, nullptr);
        SendMessageW(control, WM_SETFONT, WPARAM(font), TRUE);
        return control;
    };

    int y = margin;
    wchar_t text[256];
    swprintf_s(text, L"Mandos virtuales listos:  J2 = Controller %d  ·  J3 = Controller %d  ·  J4 = Controller %d",
               pads_slot(2) + 1, pads_slot(3) + 1, pads_slot(4) + 1);
    g_ui.pads_label = make(L"STATIC", text, SS_LEFT, margin, y + 4, width - 2 * margin, 20, 0);
    y += row_h;
    g_ui.mcc_label = make(L"STATIC", L"", SS_LEFT, margin, y + 6, width - 2 * margin - 140, 20, 0);
    g_ui.launch_button = make(L"BUTTON", L"Abrir MCC", BS_PUSHBUTTON, width - margin - 130, y, 130, 26, k_id_launch);
    y += row_h + 8;
    make(L"STATIC", L"En AlphaRing (F4): Players = 4, sin cambiar nada más. Ciérralo con F4 y usa gráficos clásicos (Tab).",
         SS_LEFT, margin, y, width - 2 * margin, 20, 0);
    y += row_h;
    for (int i = 0; i < k_remote_players; ++i, y += row_h) {
        int player = i + k_first_remote_player;
        swprintf_s(text, L"Jugador %d  (Controller %d)", player, pads_slot(player) + 1);
        make(L"STATIC", text, SS_LEFT, margin, y + 6, width - 2 * margin - 140, 20, 0);
        make(L"BUTTON", L"Mover", BS_PUSHBUTTON, width - margin - 130, y, 130, 26, k_id_move + i);
    }

    refresh();
    SetTimer(g_ui.window, k_refresh_timer, 1000, nullptr);
    ShowWindow(g_ui.window, SW_SHOW);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    // Un solo launcher: dos crearían 6 mandos y desordenarían los slots.
    HANDLE single = CreateMutexW(nullptr, TRUE, L"HaloCoopHost.SingleInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBoxW(nullptr, L"HaloCoop Host ya está abierto.", k_title, MB_ICONINFORMATION);
        return 0;
    }
    log_init();
    log_write("HaloCoop Host iniciado");

    // Los mandos tienen que existir antes que MCC.
    while (find_mcc_pid()) {
        log_write("MCC ya estaba abierto al iniciar");
        if (MessageBoxW(nullptr, L"MCC ya está abierto.\n\nCiérralo y presiona Reintentar: los mandos de los "
                                 L"jugadores 2, 3 y 4 tienen que crearse antes de abrir el juego.",
                        k_title, MB_RETRYCANCEL | MB_ICONWARNING) != IDRETRY)
            return 0;
    }

    switch (pads_init()) {
    case PadsError::none:
        break;
    case PadsError::driver_missing:
        if (MessageBoxW(nullptr, L"Falta el driver ViGEmBus (mandos virtuales).\n\n¿Abrir la página de descarga? "
                                 L"Instala el archivo ViGEmBus_..._x64_x86_arm64.exe, reinicia el PC y vuelve a "
                                 L"abrir HaloCoop Host.",
                        k_title, MB_YESNO | MB_ICONERROR) == IDYES)
            ShellExecuteW(nullptr, L"open", k_vigembus_url, nullptr, nullptr, SW_SHOWNORMAL);
        return 1;
    case PadsError::create_failed:
        MessageBoxW(nullptr, L"No se pudieron crear los mandos virtuales. Revisa halocoop-host.log.", k_title,
                    MB_ICONERROR);
        return 1;
    }

    if (pads_occupied_slots_before() > 0 || pads_slot(2) != 0 || pads_slot(3) != 1 || pads_slot(4) != 2) {
        MessageBoxW(nullptr, L"Hay un mando físico conectado, así que los mandos virtuales no quedaron en "
                             L"Controller 1, 2 y 3.\n\nLo más fácil: desconecta el mando físico y vuelve a abrir "
                             L"HaloCoop Host. Si no, en AlphaRing asigna a cada jugador el Controller que muestra "
                             L"el panel.",
                    k_title, MB_ICONWARNING);
    }

    create_ui(instance);
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    pads_shutdown();
    log_write("HaloCoop Host cerrado");
    if (single) CloseHandle(single);
    return 0;
}
