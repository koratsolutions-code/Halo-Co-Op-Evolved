#pragma once
// Estilo ImGui común del cliente y del panel del host: gris muy oscuro con acentos verdes.
#include <imgui.h>

#include <windows.h>

#include <cstring>

// Paleta "launcher de Halo": azul acero muy oscuro, bordes azul grisáceo y acentos celestes.
inline const ImVec4 k_green(0.43f, 0.84f, 0.55f, 1);
inline const ImVec4 k_orange(0.95f, 0.70f, 0.30f, 1);
inline const ImVec4 k_red(0.93f, 0.42f, 0.38f, 1);
inline const ImVec4 k_muted(0.48f, 0.56f, 0.62f, 1);
inline const ImVec4 k_text(0.87f, 0.91f, 0.94f, 1);
inline const ImVec4 k_blue(0.20f, 0.45f, 0.66f, 1);       // botones
inline const ImVec4 k_kick(0.62f, 0.22f, 0.20f, 1);       // botón de expulsar
inline const ImVec4 k_cyan(0.50f, 0.83f, 1.00f, 1);       // acentos
inline const ImVec4 k_bg_top(0.024f, 0.043f, 0.067f, 1);
inline const ImVec4 k_bg_bottom(0.055f, 0.10f, 0.15f, 1);
inline const ImVec4 k_hex_line(0.07f, 0.13f, 0.18f, 1);
inline const ImVec4 k_panel(0.05f, 0.09f, 0.13f, 0.92f);
inline const ImVec4 k_panel_edge(0.18f, 0.31f, 0.41f, 1);
inline const float k_background[4] = {0.024f, 0.043f, 0.067f, 1};

inline void ui_apply_style() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImGui::StyleColorsDark();
    style.WindowRounding = 0;
    style.ChildRounding = 0;
    style.FrameRounding = 0;
    style.GrabRounding = 0;
    style.PopupRounding = 0;
    style.ScrollbarRounding = 0;
    style.FrameBorderSize = 1;
    style.FramePadding = ImVec2(10, 7);
    style.ItemSpacing = ImVec2(10, 10);
    style.WindowPadding = ImVec2(18, 16);
    ImVec4* c = style.Colors;
    c[ImGuiCol_Text] = k_text;
    c[ImGuiCol_TextDisabled] = k_muted;
    c[ImGuiCol_WindowBg] = ImVec4(0.04f, 0.08f, 0.12f, 0.97f);
    c[ImGuiCol_ChildBg] = ImVec4(0.05f, 0.09f, 0.13f, 1);
    c[ImGuiCol_PopupBg] = ImVec4(0.05f, 0.09f, 0.13f, 0.98f);
    c[ImGuiCol_Border] = k_panel_edge;
    c[ImGuiCol_FrameBg] = ImVec4(0.03f, 0.06f, 0.09f, 1);
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.06f, 0.12f, 0.17f, 1);
    c[ImGuiCol_FrameBgActive] = ImVec4(0.08f, 0.16f, 0.22f, 1);
    c[ImGuiCol_Button] = k_blue;
    c[ImGuiCol_ButtonHovered] = ImVec4(0.26f, 0.55f, 0.80f, 1);
    c[ImGuiCol_ButtonActive] = ImVec4(0.16f, 0.37f, 0.55f, 1);
    c[ImGuiCol_SliderGrab] = k_cyan;
    c[ImGuiCol_SliderGrabActive] = ImVec4(0.70f, 0.90f, 1.0f, 1);
    c[ImGuiCol_CheckMark] = k_cyan;
    c[ImGuiCol_Header] = ImVec4(0.14f, 0.32f, 0.48f, 1);
    c[ImGuiCol_HeaderHovered] = ImVec4(0.20f, 0.42f, 0.62f, 1);
    c[ImGuiCol_HeaderActive] = ImVec4(0.16f, 0.37f, 0.55f, 1);
    c[ImGuiCol_Separator] = k_panel_edge;
    c[ImGuiCol_TitleBg] = ImVec4(0.04f, 0.08f, 0.12f, 1);
    c[ImGuiCol_TitleBgActive] = ImVec4(0.08f, 0.16f, 0.24f, 1);
}

// Segoe UI de Windows (la carpeta de Windows puede no estar en C:); si falta, la fuente de ImGui.
inline ImFont* ui_load_font(float size, bool bold = false) {
    static const ImWchar k_ranges[] = {0x0020, 0x00FF, 0x2010, 0x2027, 0x25CF, 0x25CF, 0};
    char font[MAX_PATH] = "";
    GetWindowsDirectoryA(font, MAX_PATH);
    strcat_s(font, bold ? "\\Fonts\\segoeuib.ttf" : "\\Fonts\\segoeui.ttf");
    ImFont* loaded = ImGui::GetIO().Fonts->AddFontFromFileTTF(font, size, nullptr, k_ranges);
    return loaded ? loaded : ImGui::GetIO().Fonts->AddFontDefault();
}

// Bahnschrift (viene con Windows 10/11): la de las pestañas de modo e idioma. Si falta, Segoe UI negrita.
inline ImFont* ui_load_tab_font(float size) {
    static const ImWchar k_ranges[] = {0x0020, 0x00FF, 0};
    char font[MAX_PATH] = "";
    GetWindowsDirectoryA(font, MAX_PATH);
    strcat_s(font, "\\Fonts\\bahnschrift.ttf");
    if (GetFileAttributesA(font) == INVALID_FILE_ATTRIBUTES) return ui_load_font(size, true);
    ImFont* loaded = ImGui::GetIO().Fonts->AddFontFromFileTTF(font, size, nullptr, k_ranges);
    return loaded ? loaded : ui_load_font(size, true);
}
