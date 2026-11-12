// CoOpEvolved.exe: un solo programa para el host (jugador 1, el PC con MCC) y para los amigos (jugadores 2-4).
// Abre en el último modo usado; arriba a la derecha de cada pantalla se cambia entre "Crear partida" y "Unirse"
// (src/common/ui_mode.h). También se puede elegir con --host o --join (por ejemplo en un acceso directo).

#include "../common/ui_mode.h"

#include <windows.h>
#include <shellapi.h>

#include <string>

int host_main(HINSTANCE instance);
int client_main(HINSTANCE instance);

namespace {

int g_next_mode = -1;  // -1: salir; 0: host; 1: unirse

std::wstring exe_path() {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    return path;
}

std::wstring exe_folder() {
    std::wstring path = exe_path();
    return path.substr(0, path.find_last_of(L"\\/") + 1);
}

bool exists(const std::wstring& path) {
    return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

std::wstring app_ini() {
    return exe_folder() + L"coopevolved.ini";
}

// Antes eran dos programas en carpetas host\ y cliente\: si al lado hay una de esas carpetas con datos guardados
// (nombre, IP del host, colores...), se copian la primera vez.
void bring_old_settings(const std::wstring& folder) {
    const wchar_t* files[][2] = {{L"host\\", L"coopevolved-host.ini"}, {L"cliente\\", L"coopevolved-cliente.ini"}};
    std::wstring parent = folder.substr(0, folder.find_last_of(L"\\/", folder.size() - 2) + 1);
    for (auto& file : files) {
        std::wstring target = folder + file[1];
        std::wstring old = parent + file[0] + file[1];
        if (!exists(target) && exists(old)) CopyFileW(old.c_str(), target.c_str(), TRUE);
    }
}

// Último modo usado. La primera vez: host si solo hay datos del host, si no unirse (lo que usan los amigos).
bool start_as_host(const std::wstring& folder) {
    wchar_t mode[8] = {};
    GetPrivateProfileStringW(L"app", L"mode", L"", mode, 8, app_ini().c_str());
    if (mode[0]) return _wcsicmp(mode, L"host") == 0;
    return exists(folder + L"coopevolved-host.ini") && !exists(folder + L"coopevolved-cliente.ini");
}

void save_mode(bool host) {
    WritePrivateProfileStringW(L"app", L"mode", host ? L"host" : L"join", app_ini().c_str());
}

// El otro modo arranca en un proceso nuevo: así cada modo empieza limpio (hilos, sockets, audio).
void relaunch(bool host) {
    std::wstring command = L"\"" + exe_path() + L"\" " + (host ? L"--host" : L"--join");
    STARTUPINFOW startup{sizeof(startup)};
    PROCESS_INFORMATION process{};
    if (CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, 0, nullptr, exe_folder().c_str(), &startup,
                       &process)) {
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
    }
}

}  // namespace

void app_request_mode(bool host) {
    g_next_mode = host ? 0 : 1;
    save_mode(host);
    PostQuitMessage(0);  // la ventana actual se cierra como si el usuario la cerrara
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    std::wstring folder = exe_folder();
    bring_old_settings(folder);
    bool host = start_as_host(folder);
    bool chosen = false;
    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    for (int i = 1; argv && i < argc; ++i) {
        if (_wcsicmp(argv[i], L"--host") == 0) host = chosen = true;
        if (_wcsicmp(argv[i], L"--join") == 0) host = false, chosen = true;
    }
    if (argv) LocalFree(argv);
    // Ya hay un host abierto en este PC (por ejemplo para probar con un cliente al lado): esta ventana se une, sin
    // cambiar el modo guardado. Si se pasa a "Crear partida" a mano, el host sale inactivo (ver host_main).
    bool host_already_open = false;
    if (HANDLE other = OpenMutexW(SYNCHRONIZE, FALSE, L"CoOpEvolvedHost.SingleInstance")) {
        CloseHandle(other);
        host_already_open = true;
    }
    if (!chosen && host && host_already_open)
        host = false;
    else
        save_mode(host);
    int result = host ? host_main(instance) : client_main(instance);
    if (g_next_mode >= 0) relaunch(g_next_mode == 0);
    return result;
}
