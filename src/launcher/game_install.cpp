#include "game_install.h"
#include "log.h"

#include "../common/lang.h"

#include <windows.h>

#include <fstream>
#include <sstream>
#include <vector>

static const wchar_t k_game_folder[] = L"\\steamapps\\common\\Halo The Master Chief Collection\\mcc\\binaries\\win64";

static std::wstring utf8_to_wide(const std::string& text) {
    int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    std::wstring result(size > 0 ? size - 1 : 0, L'\0');
    if (size > 1) MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, result.data(), size);
    return result;
}

static bool exists(const std::wstring& path) {
    return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

static std::wstring steam_dir() {
    wchar_t path[MAX_PATH] = {};
    DWORD size = sizeof(path);
    if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", L"SteamPath", RRF_RT_REG_SZ, nullptr, path,
                     &size) == ERROR_SUCCESS)
        return path;
    size = sizeof(path);
    if (RegGetValueW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\WOW6432Node\\Valve\\Steam", L"InstallPath", RRF_RT_REG_SZ,
                     nullptr, path, &size) == ERROR_SUCCESS)
        return path;
    return L"";
}

// Bibliotecas de Steam: la carpeta principal más cada "path" de libraryfolders.vdf.
static std::vector<std::wstring> steam_libraries() {
    std::vector<std::wstring> libraries;
    std::wstring steam = steam_dir();
    if (steam.empty()) return libraries;
    libraries.push_back(steam);
    std::ifstream file(steam + L"\\steamapps\\libraryfolders.vdf");
    std::string line;
    while (std::getline(file, line)) {
        size_t key = line.find("\"path\"");
        if (key == std::string::npos) continue;
        size_t open = line.find('"', key + 6);
        size_t close = line.rfind('"');
        if (open == std::string::npos || close <= open) continue;
        std::string value = line.substr(open + 1, close - open - 1);
        std::string unescaped;
        for (size_t i = 0; i < value.size(); ++i) {
            if (value[i] == '\\' && i + 1 < value.size() && value[i + 1] == '\\') ++i;
            unescaped += value[i];
        }
        libraries.push_back(utf8_to_wide(unescaped));
    }
    return libraries;
}

std::wstring game_binaries_dir() {
    for (const std::wstring& library : steam_libraries()) {
        std::wstring dir = library + k_game_folder;
        if (exists(dir + L"\\MCC-Win64-Shipping.exe")) return dir;
    }
    return L"";
}

static std::wstring bundled_dll() {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring dir = path;
    return dir.substr(0, dir.find_last_of(L"\\/") + 1) + L"VERSION.dll";
}

static std::vector<char> read_file(const std::wstring& path) {
    std::ifstream file(path, std::ios::binary);
    return std::vector<char>(std::istreambuf_iterator<char>(file), {});
}

ModState game_mod_state(const std::wstring& binaries_dir) {
    if (binaries_dir.empty()) return ModState::game_not_found;
    std::wstring installed = binaries_dir + L"\\VERSION.dll";
    if (!exists(installed)) return ModState::not_installed;
    return read_file(installed) == read_file(bundled_dll()) ? ModState::installed : ModState::outdated;
}

bool game_alpharing_present(const std::wstring& binaries_dir) {
    return !binaries_dir.empty() && exists(binaries_dir + L"\\WTSAPI32.dll");
}

bool game_install_mod(const std::wstring& binaries_dir, std::wstring& error) {
    std::wstring source = bundled_dll();
    if (!exists(source)) {
        error = T(L"VERSION.dll is missing next to HaloCoopHost.exe. Download the whole host folder again.",
                  L"Falta VERSION.dll junto a HaloCoopHost.exe. Descarga de nuevo la carpeta host completa.");
        return false;
    }
    std::wstring target = binaries_dir + L"\\VERSION.dll";
    if (!CopyFileW(source.c_str(), target.c_str(), FALSE)) {
        DWORD code = GetLastError();
        error = code == ERROR_SHARING_VIOLATION || code == ERROR_LOCK_VIOLATION
            ? T(L"MCC is open: close it and try again.", L"MCC está abierto: ciérralo y vuelve a intentar.")
            : code == ERROR_ACCESS_DENIED
            ? std::wstring(T(L"Windows does not allow writing to the game folder. Copy VERSION.dll by hand to:\n",
                             L"Windows no deja escribir en la carpeta del juego. Copia VERSION.dll a mano en:\n")) +
                  binaries_dir
            : std::wstring(T(L"Could not copy it (error ", L"No se pudo copiar (error ")) + std::to_wstring(code) + L").";
        log_write("No se pudo instalar el mod en el juego (error %lu)", code);
        return false;
    }
    log_write("Mod instalado en la carpeta del juego");
    return true;
}
