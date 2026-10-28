#include "settings.h"

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>

static std::wstring ini_path() {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring result = path;
    return result.substr(0, result.find_last_of(L"\\/") + 1) + L"halocoop-cliente.ini";
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
}
