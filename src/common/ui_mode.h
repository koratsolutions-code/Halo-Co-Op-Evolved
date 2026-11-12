#pragma once
// Selector "Crear partida / Unirse" arriba a la derecha, en el panel del host y en el menú del cliente. Elegir el
// otro modo cierra esta ventana y el programa se vuelve a abrir en ese modo (ver src/app/main.cpp).
#include "lang.h"

#include <imgui.h>

// Lo implementa src/app/main.cpp: guarda el modo y pide cerrar la ventana actual.
void app_request_mode(bool host);

// is_host: el modo en el que está esta ventana.
inline void ui_mode_combo(bool is_host, float width) {
    const char* modes[] = {T("Host a game", "Crear partida"), T("Join a game", "Unirse a una partida")};
    int mode = is_host ? 0 : 1;
    ImGui::SetNextItemWidth(width);
    if (ImGui::Combo("##mode", &mode, modes, 2) && (mode == 0) != is_host) app_request_mode(mode == 0);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", T("Host: you are player 1 and Halo runs on this PC.\n"
                                  "Join: you are player 2, 3 or 4 on a friend's game (you do not need Halo).",
                                  "Crear partida: eres el jugador 1 y Halo corre en este PC.\n"
                                  "Unirse: eres el jugador 2, 3 o 4 en la partida de un amigo (no necesitas Halo)."));
}
