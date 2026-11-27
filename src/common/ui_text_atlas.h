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

inline constexpr int k_ui_text_atlas_w = 2048, k_ui_text_atlas_h = 1020;
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
    {"remove_mod", {0, 438, 688, 71}, {690, 438, 611, 71}},
    {"move", {1303, 438, 297, 71}, {1602, 438, 366, 71}},
    {"kick", {0, 511, 226, 71}, {228, 511, 530, 71}},
    {"p1", {760, 511, 94, 71}, {856, 511, 78, 71}},
    {"p2", {936, 511, 126, 71}, {1064, 511, 110, 71}},
    {"p3", {1176, 511, 129, 71}, {1307, 511, 113, 71}},
    {"p4", {1422, 511, 121, 71}, {1545, 511, 105, 71}},
    {"join_title", {0, 584, 644, 71}, {646, 584, 1145, 71}},
    {"host_ip", {0, 657, 886, 71}, {0, 730, 1187, 71}},
    {"your_name", {1189, 730, 576, 71}, {0, 803, 570, 71}},
    {"armor_color", {572, 803, 749, 71}, {0, 876, 1111, 71}},
    {"player", {1113, 876, 391, 71}, {1506, 876, 469, 71}},
    {"control", {0, 949, 464, 71}, {466, 949, 464, 71}},
    {"connect", {932, 949, 465, 71}, {1399, 949, 545, 71}},
};
