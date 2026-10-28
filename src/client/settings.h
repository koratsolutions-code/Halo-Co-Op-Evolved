#pragma once
#include "../common/lang.h"

#include <string>

// Preferencias del cliente, guardadas en coopevolved-cliente.ini junto al .exe.
enum class ControlMode { gamepad = 0, keyboard_mouse = 1 };

struct Settings {
    std::string host;      // IP de Radmin del host
    int player = 2;        // 2..4
    std::string nickname;
    ControlMode control = ControlMode::gamepad;
    float sensitivity = 1.0f;  // 0.1..99
    int volume = 100;          // 0..100
    bool muted = false;
    Lang lang = Lang::en;  // idioma de la interfaz
    int armor_color = 0;   // índice en k_armor_colors (0: por defecto)
};

constexpr float k_min_sensitivity = 0.1f;
constexpr float k_max_sensitivity = 99.0f;

Settings settings_load();
void settings_save(const Settings& settings);
