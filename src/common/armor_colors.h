#pragma once
// Colores de armadura: los 18 de Halo CE más "por defecto" (el verde oliva de la campaña, sin cambiar nada).

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
