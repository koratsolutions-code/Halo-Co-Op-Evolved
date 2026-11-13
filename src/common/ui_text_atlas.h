#pragma once
// Generado por tools/make_ui_images.py: no editar a mano.
// Textos fijos dibujados con la fuente de Halo (src/app/ui_text.png): x, y, ancho, alto en inglés y en español.

struct UiTextRect {
    int x, y, w, h;
};

struct UiTextEntry {
    const char* key;
    UiTextRect en, es;
};

inline constexpr int k_ui_text_atlas_w = 2048, k_ui_text_atlas_h = 947;
inline constexpr int k_ui_logo_w = 1200, k_ui_logo_h = 49;

inline constexpr UiTextEntry k_ui_texts[] = {
    {"connection", {0, 0, 627, 71}, {629, 0, 503, 71}},
    {"options", {1134, 0, 432, 71}, {0, 73, 518, 71}},
    {"admin_panel", {520, 73, 670, 71}, {1192, 73, 834, 71}},
    {"radmin_ip", {0, 146, 804, 71}, {806, 146, 819, 71}},
    {"nickname", {0, 219, 508, 71}, {510, 219, 508, 71}},
    {"armor", {1020, 219, 369, 71}, {1391, 219, 566, 71}},
    {"skin", {0, 292, 223, 71}, {225, 292, 223, 71}},
    {"copy", {450, 292, 279, 71}, {731, 292, 387, 71}},
    {"open_mcc", {1120, 292, 535, 71}, {0, 365, 574, 71}},
    {"install_mod", {576, 365, 657, 71}, {1235, 365, 742, 71}},
    {"move", {0, 438, 297, 71}, {299, 438, 366, 71}},
    {"kick", {667, 438, 226, 71}, {895, 438, 530, 71}},
    {"p1", {1427, 438, 94, 71}, {1523, 438, 78, 71}},
    {"p2", {1603, 438, 126, 71}, {1731, 438, 110, 71}},
    {"p3", {1843, 438, 129, 71}, {0, 511, 113, 71}},
    {"p4", {115, 511, 121, 71}, {238, 511, 105, 71}},
    {"join_title", {345, 511, 644, 71}, {0, 584, 1145, 71}},
    {"host_ip", {1147, 584, 886, 71}, {0, 657, 1187, 71}},
    {"your_name", {1189, 657, 576, 71}, {0, 730, 570, 71}},
    {"armor_color", {572, 730, 749, 71}, {0, 803, 1111, 71}},
    {"player", {1113, 803, 391, 71}, {1506, 803, 469, 71}},
    {"control", {0, 876, 464, 71}, {466, 876, 464, 71}},
    {"connect", {932, 876, 465, 71}, {1399, 876, 545, 71}},
};
