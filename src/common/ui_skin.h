#pragma once
// Selector de skin (lista desplegable), común al panel y al cliente.
#include "lang.h"
#include "skins.h"

#include <imgui.h>

#include <cstdint>
#include <cstdio>

// available: bit i = la skin i está en la misión actual (0: no se sabe, p. ej. sin misión cargada).
inline bool ui_skin_available(int index, uint16_t available) {
    return index == 0 || !available || (available & (1u << index));
}

// Nombre de la skin y, si no está en la misión actual, el aviso de que se verá como Master Chief.
inline const char* ui_skin_label(int index, uint16_t available, char* out, size_t size) {
    const char* name = T(k_skins[index].name_en, k_skins[index].name_es);
    if (ui_skin_available(index, available))
        snprintf(out, size, "%s", name);
    else
        snprintf(out, size, "%s %s", name, T("(not in this mission: Master Chief)", "(no está en esta misión: Master Chief)"));
    return out;
}

// true si el usuario eligió otra skin. Las que no están en la misión se pueden elegir igual (sirven en otras).
inline bool ui_skin_combo(const char* id, int* index, float width, uint16_t available) {
    if (*index < 0 || *index >= k_skin_count) *index = 0;
    bool changed = false;
    char label[160];
    ImGui::SetNextItemWidth(width);
    if (ImGui::BeginCombo(id, ui_skin_label(*index, available, label, sizeof(label)), ImGuiComboFlags_HeightLarge)) {
        for (int i = 0; i < k_skin_count; ++i) {
            ImGui::PushID(i);
            bool here = ui_skin_available(i, available);
            if (!here) ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            if (ImGui::Selectable(ui_skin_label(i, available, label, sizeof(label)), i == *index)) {
                *index = i;
                changed = true;
            }
            if (!here) ImGui::PopStyleColor();
            if (i == *index) ImGui::SetItemDefaultFocus();
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    if (ImGui::IsItemHovered()) {
        const char* when = T("A new skin shows the next time you respawn.",
                             "La skin nueva se ve la próxima vez que reaparezcas.");
        if (available)
            ImGui::SetTooltip("%s", when);
        else
            ImGui::SetTooltip("%s\n%s", when,
                              T("Open a mission to see which skins it has. Skins that are not in the mission show "
                                "as Master Chief.",
                                "Abre una misión para ver qué skins tiene. Las que no están en la misión se ven "
                                "como Master Chief."));
    }
    return changed;
}
