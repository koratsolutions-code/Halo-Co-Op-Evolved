#pragma once
// Selector de color de armadura (lista desplegable con la muestra de cada color), común al panel y al cliente.
// La lista tiene los colores de Halo CE y al final "Más colores...", que abre un selector de cualquier color.
#include "armor_colors.h"
#include "lang.h"

#include <imgui.h>
#include <imgui_internal.h>  // vista previa propia de la lista (BeginComboPreview)

#include <cmath>
#include <cstdio>

inline ImU32 armor_color_u32(ArmorRgb color) {
    float rgb[3];
    if (color == k_armor_rainbow)
        armor_hue(float(std::fmod(ImGui::GetTime() / k_rainbow_cycle_seconds, 1.0)), rgb);
    else if (!armor_floats(color, rgb)) {
        rgb[0] = k_armor_colors[0].r;
        rgb[1] = k_armor_colors[0].g;
        rgb[2] = k_armor_colors[0].b;
    }
    return ImGui::GetColorU32(ImVec4(rgb[0], rgb[1], rgb[2], 1));
}

// Nombre para mostrar: el de la lista o "Personalizado #RRGGBB".
inline const char* armor_name(ArmorRgb color) {
    if (color == k_armor_rainbow) return T("Rainbow (Gaylo)", "Arcoíris (Gaylo)");
    int index = armor_preset_index(color);
    if (index >= 0) return T(k_armor_colors[index].name_en, k_armor_colors[index].name_es);
    static char text[48];
    snprintf(text, sizeof(text), "%s #%06X", T("Custom", "Personalizado"), unsigned(color & 0xFFFFFF));
    return text;
}

// Muestra cuadrada del color, del alto de una línea de texto (el arcoíris animado va cambiando).
inline void ui_armor_swatch(ArmorRgb color) {
    float h = ImGui::GetTextLineHeight();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(pos, ImVec2(pos.x + h, pos.y + h), armor_color_u32(color), 4);
    draw->AddRect(pos, ImVec2(pos.x + h, pos.y + h), IM_COL32(255, 255, 255, 60), 4);
    ImGui::Dummy(ImVec2(h, h));
}

// Muestra de "Más colores": un degradado de varios colores.
inline void ui_armor_rainbow_swatch() {
    float h = ImGui::GetTextLineHeight();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilledMultiColor(pos, ImVec2(pos.x + h, pos.y + h), IM_COL32(230, 40, 40, 255),
                                  IM_COL32(240, 220, 40, 255), IM_COL32(40, 200, 90, 255), IM_COL32(50, 90, 230, 255));
    draw->AddRect(pos, ImVec2(pos.x + h, pos.y + h), IM_COL32(255, 255, 255, 60));
    ImGui::Dummy(ImVec2(h, h));
}

// true si el usuario eligió otro color.
inline bool ui_armor_color_combo(const char* id, ArmorRgb* color, float width) {
    *color = armor_sanitize(*color);
    int current = armor_is_special(*color) ? -2 : armor_preset_index(*color);  // -1: color propio, -2: arcoíris
    bool changed = false, open_picker = false;
    static float s_edit[3] = {};  // color en edición en "Más colores" (solo hay uno abierto a la vez)

    ImGui::SetNextItemWidth(width);
    if (ImGui::BeginCombo(id, "", ImGuiComboFlags_CustomPreview | ImGuiComboFlags_HeightLarge)) {
        for (int i = 0; i < k_armor_color_count; ++i) {
            ImGui::PushID(i);
            ImVec2 pos = ImGui::GetCursorPos();
            if (ImGui::Selectable("##color", i == current)) {
                *color = armor_preset(i);
                changed = true;
            }
            if (i == current) ImGui::SetItemDefaultFocus();
            ImGui::SetCursorPos(pos);
            ui_armor_swatch(armor_preset(i));
            ImGui::SameLine();
            ImGui::TextUnformatted(T(k_armor_colors[i].name_en, k_armor_colors[i].name_es));
            ImGui::PopID();
        }
        ImGui::Separator();
        for (ArmorRgb special : {k_armor_rainbow}) {
            ImGui::PushID(int(special >> 24));
            ImVec2 pos = ImGui::GetCursorPos();
            if (ImGui::Selectable("##special", *color == special)) {
                *color = special;
                changed = true;
            }
            if (*color == special) ImGui::SetItemDefaultFocus();
            ImGui::SetCursorPos(pos);
            ui_armor_swatch(special);
            ImGui::SameLine();
            ImGui::TextUnformatted(armor_name(special));
            ImGui::PopID();
        }
        ImGui::Separator();
        ImVec2 pos = ImGui::GetCursorPos();
        if (ImGui::Selectable("##more", current == -1)) open_picker = true;
        if (current == -1) ImGui::SetItemDefaultFocus();
        ImGui::SetCursorPos(pos);
        if (current == -1)
            ui_armor_swatch(*color);
        else
            ui_armor_rainbow_swatch();
        ImGui::SameLine();
        ImGui::TextUnformatted(T("More colors...", "Más colores..."));
        ImGui::EndCombo();
    }
    if (ImGui::BeginComboPreview()) {
        ui_armor_swatch(*color);
        ImGui::SameLine();
        ImGui::TextUnformatted(armor_name(*color));
        ImGui::EndComboPreview();
    }

    ImGui::PushID(id);
    if (open_picker) {
        float rgb[3];
        if (*color == k_armor_rainbow)
            armor_hue(0, rgb);
        else if (!armor_floats(*color, rgb)) {
            rgb[0] = k_armor_colors[0].r;
            rgb[1] = k_armor_colors[0].g;
            rgb[2] = k_armor_colors[0].b;
        }
        s_edit[0] = rgb[0];
        s_edit[1] = rgb[1];
        s_edit[2] = rgb[2];
        ImGui::OpenPopup("##picker");
    }
    if (ImGui::BeginPopup("##picker")) {
        ImGui::TextUnformatted(T("Pick any armor color", "Elige cualquier color de armadura"));
        ImGui::SetNextItemWidth(280);
        ImGui::ColorPicker3("##rgb", s_edit,
                            ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_PickerHueWheel |
                                ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_DisplayHex);
        ImGui::TextDisabled("%s", T("The game mixes it with the armor's light: it can look a bit different.",
                                    "El juego lo mezcla con la luz de la armadura: puede verse algo distinto."));
        if (ImGui::Button(T("Apply", "Aplicar"), ImVec2(120, 0))) {
            *color = armor_rgb(s_edit[0], s_edit[1], s_edit[2]);
            changed = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(T("Cancel", "Cancelar"), ImVec2(120, 0))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    ImGui::PopID();
    return changed;
}
