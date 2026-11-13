#pragma once
// Pestañas de arriba a la derecha, en el panel del host y en el menú del cliente:
//   CREAR PARTIDA | UNIRSE   — elegir el otro modo cierra esta ventana y el programa se vuelve a abrir en ese modo
//                              (ver src/app/main.cpp).
//   EN | ES                  — idioma de la interfaz.
#include "lang.h"
#include "ui_halo.h"

#include <imgui.h>

// Lo implementa src/app/main.cpp: guarda el modo y pide cerrar la ventana actual.
void app_request_mode(bool host);

constexpr float k_mode_tab_w = 200, k_lang_tab_w = 52, k_tab_h = 40, k_tab_gap = 6, k_tab_groups_gap = 18;

// Ancho total de las pestañas (para alinearlas a la derecha).
inline float ui_mode_tabs_width() {
    return k_mode_tab_w * 2 + k_tab_gap + k_tab_groups_gap + k_lang_tab_w * 2 + k_tab_gap;
}

// is_host: el modo en el que está esta ventana. Devuelve true si cambió el idioma (*lang ya tiene el nuevo).
inline bool ui_mode_tabs(bool is_host, Lang* lang) {
    const float y = ImGui::GetCursorPosY();  // todas a la misma altura
    auto next = [y](float gap) {
        ImGui::SameLine(0, gap);
        ImGui::SetCursorPosY(y);
    };
    if (ui_halo_tab("mode_host", T("HOST A GAME", "CREAR PARTIDA"), ImVec2(k_mode_tab_w, k_tab_h), is_host) && !is_host) app_request_mode(true);
    bool hovered = ImGui::IsItemHovered();
    next(k_tab_gap);
    if (ui_halo_tab("mode_join", T("JOIN A GAME", "UNIRSE"), ImVec2(k_mode_tab_w, k_tab_h), !is_host) && is_host) app_request_mode(false);
    if (hovered || ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", T("Host a game: you are player 1 and Halo runs on this PC.\n"
                                  "Join a game: you are player 2, 3 or 4 on a friend's game (you do not need Halo).",
                                  "Crear partida: eres el jugador 1 y Halo corre en este PC.\n"
                                  "Unirse: eres el jugador 2, 3 o 4 en la partida de un amigo (no necesitas Halo)."));
    next(k_tab_groups_gap);
    bool changed = false;
    if (ui_halo_tab("lang_en", "EN", ImVec2(k_lang_tab_w, k_tab_h), *lang == Lang::en) && *lang != Lang::en) {
        *lang = Lang::en;
        changed = true;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("English");
    next(k_tab_gap);
    if (ui_halo_tab("lang_es", "ES", ImVec2(k_lang_tab_w, k_tab_h), *lang == Lang::es) && *lang != Lang::es) {
        *lang = Lang::es;
        changed = true;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Español");
    if (changed) g_lang = *lang;
    return changed;
}
