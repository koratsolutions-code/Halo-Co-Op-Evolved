#pragma once
// Selector de color de armadura (lista desplegable con la muestra de cada color), común al panel y al cliente.
#include "armor_colors.h"
#include "lang.h"

#include <imgui.h>
#include <imgui_internal.h>  // vista previa propia de la lista (BeginComboPreview)

inline ImU32 armor_color_u32(int index) {
    const ArmorColor& c = k_armor_colors[index];
    return ImGui::GetColorU32(ImVec4(c.r, c.g, c.b, 1));
}

// Muestra cuadrada del color, del alto de una línea de texto.
inline void ui_armor_swatch(int index) {
    float h = ImGui::GetTextLineHeight();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(pos, ImVec2(pos.x + h, pos.y + h), armor_color_u32(index), 4);
    draw->AddRect(pos, ImVec2(pos.x + h, pos.y + h), IM_COL32(255, 255, 255, 60), 4);
    ImGui::Dummy(ImVec2(h, h));
}

// true si el usuario eligió otro color.
inline bool ui_armor_color_combo(const char* id, int* index, float width) {
    if (*index < 0 || *index >= k_armor_color_count) *index = 0;
    bool changed = false;
    ImGui::SetNextItemWidth(width);
    if (ImGui::BeginCombo(id, "", ImGuiComboFlags_CustomPreview | ImGuiComboFlags_HeightLarge)) {
        for (int i = 0; i < k_armor_color_count; ++i) {
            ImGui::PushID(i);
            ImVec2 pos = ImGui::GetCursorPos();
            if (ImGui::Selectable("##color", i == *index)) {
                *index = i;
                changed = true;
            }
            if (i == *index) ImGui::SetItemDefaultFocus();
            ImGui::SetCursorPos(pos);
            ui_armor_swatch(i);
            ImGui::SameLine();
            ImGui::TextUnformatted(T(k_armor_colors[i].name_en, k_armor_colors[i].name_es));
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    if (ImGui::BeginComboPreview()) {
        ui_armor_swatch(*index);
        ImGui::SameLine();
        ImGui::TextUnformatted(T(k_armor_colors[*index].name_en, k_armor_colors[*index].name_es));
        ImGui::EndComboPreview();
    }
    return changed;
}
