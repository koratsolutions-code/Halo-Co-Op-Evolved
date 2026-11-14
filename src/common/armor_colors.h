#pragma once
// Colores de armadura: los 18 de Halo CE más "por defecto" (el verde oliva de la campaña, sin cambiar nada).
// Lo que se guarda y se manda es un ArmorRgb: 0 = por defecto; si no, 0x01RRGGBB (cualquier color, con
// "Más colores"). Los de la lista son atajos a su RGB. Un valor especial: el arcoíris animado ("Gaylo").
// Halo CE pinta toda la armadura con un solo color de cambio: no se pueden hacer franjas de colores.

#include <cmath>
#include <cstdint>

struct ArmorColor {
    const char* name_en;
    const char* name_es;
    float r, g, b;
};

inline constexpr ArmorColor k_armor_colors[] = {
    {"Default", "Por defecto", 0.341f, 0.404f, 0.145f},
    {"White", "Blanco", 0.95f, 0.95f, 0.95f},
    {"Black", "Negro", 0.08f, 0.08f, 0.08f},
    {"Red", "Rojo", 0.80f, 0.05f, 0.05f},
    {"Blue", "Azul", 0.10f, 0.20f, 0.85f},
    {"Gray", "Gris", 0.45f, 0.45f, 0.45f},
    {"Yellow", "Amarillo", 0.95f, 0.85f, 0.10f},
    {"Green", "Verde", 0.10f, 0.65f, 0.15f},
    {"Pink", "Rosado", 1.00f, 0.45f, 0.70f},
    {"Purple", "Morado", 0.45f, 0.10f, 0.70f},
    {"Cyan", "Cian", 0.10f, 0.80f, 0.90f},
    {"Cobalt", "Cobalto", 0.25f, 0.35f, 0.75f},
    {"Orange", "Naranja", 1.00f, 0.45f, 0.05f},
    {"Teal", "Verde azulado", 0.05f, 0.55f, 0.50f},
    {"Sage", "Salvia", 0.45f, 0.55f, 0.35f},
    {"Brown", "Café", 0.40f, 0.25f, 0.10f},
    {"Tan", "Beige", 0.75f, 0.62f, 0.40f},
    {"Maroon", "Granate", 0.45f, 0.05f, 0.10f},
    {"Salmon", "Salmón", 1.00f, 0.55f, 0.45f},
};
inline constexpr int k_armor_color_count = int(sizeof(k_armor_colors) / sizeof(k_armor_colors[0]));

using ArmorRgb = uint32_t;
inline constexpr ArmorRgb k_armor_set = 0x01000000;      // marca "color elegido" (0 = por defecto)
inline constexpr ArmorRgb k_armor_rainbow = 0x02000000;  // arcoíris animado (recorre todos los colores)
inline constexpr float k_rainbow_cycle_seconds = 6.0f;   // una vuelta completa del arcoíris animado

inline uint8_t armor_byte(float v) { return uint8_t(v <= 0 ? 0 : v >= 1 ? 255 : int(v * 255.0f + 0.5f)); }

inline ArmorRgb armor_rgb(float r, float g, float b) {
    return k_armor_set | (ArmorRgb(armor_byte(r)) << 16) | (ArmorRgb(armor_byte(g)) << 8) | armor_byte(b);
}

// RGB de un color de la lista (0 o fuera de rango: por defecto).
inline ArmorRgb armor_preset(int index) {
    if (index <= 0 || index >= k_armor_color_count) return 0;
    return armor_rgb(k_armor_colors[index].r, k_armor_colors[index].g, k_armor_colors[index].b);
}

// Índice en la lista, o -1 si es un color propio.
inline int armor_preset_index(ArmorRgb color) {
    if (color == 0) return 0;
    for (int i = 1; i < k_armor_color_count; ++i)
        if (armor_preset(i) == color) return i;
    return -1;
}

// Cualquier valor que no sea 0 ni 0x01RRGGBB pasa a "por defecto".
inline ArmorRgb armor_sanitize(ArmorRgb color) {
    if (color == k_armor_rainbow) return color;
    return (color & 0xFF000000) == k_armor_set ? color : 0;
}

inline bool armor_is_special(ArmorRgb color) { return color == k_armor_rainbow; }

// Tono (0..1, saturación y brillo altos) a RGB.
inline void armor_hue(float hue, float out[3]) {
    hue = hue - std::floor(hue);
    float h = hue * 6.0f, x = 1.0f - std::fabs(std::fmod(h, 2.0f) - 1.0f);
    float r = 0, g = 0, b = 0;
    switch (int(h) % 6) {
        case 0: r = 1, g = x; break;
        case 1: r = x, g = 1; break;
        case 2: g = 1, b = x; break;
        case 3: g = x, b = 1; break;
        case 4: r = x, b = 1; break;
        default: r = 1, b = x; break;
    }
    const float low = 0.08f, high = 0.95f;  // ni negro ni blanco puros: en la armadura se ven mejor
    out[0] = low + r * (high - low);
    out[1] = low + g * (high - low);
    out[2] = low + b * (high - low);
}

// RGB de un color normal. false si es "por defecto" o uno especial.
inline bool armor_floats(ArmorRgb color, float out[3]) {
    color = armor_sanitize(color);
    if (!color || armor_is_special(color)) return false;
    out[0] = float((color >> 16) & 0xFF) / 255.0f;
    out[1] = float((color >> 8) & 0xFF) / 255.0f;
    out[2] = float(color & 0xFF) / 255.0f;
    return true;
}

// Los 4 colores de cambio del personaje (12 floats) en el momento "seconds". false: por defecto (no pintar).
inline bool armor_change_colors(ArmorRgb color, double seconds, float out[12]) {
    color = armor_sanitize(color);
    float rgb[3];
    if (color == k_armor_rainbow)
        armor_hue(float(std::fmod(seconds / k_rainbow_cycle_seconds, 1.0)), rgb);
    else if (!armor_floats(color, rgb))
        return false;
    for (int i = 0; i < 4; ++i)
        for (int c = 0; c < 3; ++c) out[i * 3 + c] = rgb[c];
    return true;
}
