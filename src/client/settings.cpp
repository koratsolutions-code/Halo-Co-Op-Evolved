#include "settings.h"

#include "../common/armor_colors.h"

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>

static std::wstring ini_path() {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring folder = path;
    folder = folder.substr(0, folder.find_last_of(L"\\/") + 1);
    std::wstring result = folder + L"coopevolved-cliente.ini";
    // Antes el proyecto se llamaba HaloCoop: se conservan los datos guardados.
    if (GetFileAttributesW(result.c_str()) == INVALID_FILE_ATTRIBUTES)
        MoveFileW((folder + L"halocoop-cliente.ini").c_str(), result.c_str());
    return result;
}

static std::string read(const wchar_t* key, const wchar_t* fallback) {
    wchar_t value[256];
    GetPrivateProfileStringW(L"cliente", key, fallback, value, 256, ini_path().c_str());
    char utf8[512];
    WideCharToMultiByte(CP_UTF8, 0, value, -1, utf8, sizeof(utf8), nullptr, nullptr);
    return utf8;
}

static void write(const wchar_t* key, const std::string& value) {
    wchar_t wide[256];
    MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, wide, 256);
    WritePrivateProfileStringW(L"cliente", key, wide, ini_path().c_str());
}

Settings settings_load() {
    Settings s;
    s.host = read(L"host", L"");
    s.player = std::clamp(atoi(read(L"jugador", L"2").c_str()), 2, 4);
    s.nickname = read(L"nombre", L"");
    s.control = read(L"control", L"mando") == "teclado" ? ControlMode::keyboard_mouse : ControlMode::gamepad;
    s.sensitivity = std::clamp(float(atof(read(L"sensibilidad", L"1").c_str())), k_min_sensitivity, k_max_sensitivity);
    s.volume = std::clamp(atoi(read(L"volumen", L"100").c_str()), 0, 100);
    s.muted = read(L"silencio", L"no") == "si";
    s.lang = read(L"idioma", L"en") == "es" ? Lang::es : Lang::en;
    s.armor_color = std::clamp(atoi(read(L"color", L"0").c_str()), 0, k_armor_color_count - 1);
    s.direct_aim = read(L"mira_directa", L"no") == "si";
    return s;
}

void settings_save(const Settings& s) {
    char sensitivity[32];
    snprintf(sensitivity, sizeof(sensitivity), "%.2f", s.sensitivity);
    write(L"host", s.host);
    write(L"jugador", std::to_string(s.player));
    write(L"nombre", s.nickname);
    write(L"control", s.control == ControlMode::keyboard_mouse ? "teclado" : "mando");
    write(L"sensibilidad", sensitivity);
    write(L"volumen", std::to_string(s.volume));
    write(L"silencio", s.muted ? "si" : "no");
    write(L"idioma", s.lang == Lang::es ? "es" : "en");
    write(L"color", std::to_string(s.armor_color));
    write(L"mira_directa", s.direct_aim ? "si" : "no");
}
