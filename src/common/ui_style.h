#pragma once
// Estilo ImGui común del cliente y del panel del host: gris muy oscuro con acentos verdes.
#include <imgui.h>

#include <windows.h>

#include <cstring>

inline const ImVec4 k_green(0.45f, 0.80f, 0.55f, 1);
inline const ImVec4 k_orange(0.95f, 0.70f, 0.30f, 1);
inline const ImVec4 k_red(0.95f, 0.45f, 0.40f, 1);
inline const ImVec4 k_muted(0.55f, 0.60f, 0.64f, 1);
inline const float k_background[4] = {0.07f, 0.09f, 0.11f, 1};

inline void ui_apply_style() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImGui::StyleColorsDark();
    style.WindowRounding = 8;
    style.ChildRounding = 8;
    style.FrameRounding = 6;
    style.GrabRounding = 6;
    style.PopupRounding = 6;
    style.FramePadding = ImVec2(10, 7);
    style.ItemSpacing = ImVec2(10, 10);
    style.WindowPadding = ImVec2(18, 16);
    ImVec4* c = style.Colors;
    c[ImGuiCol_WindowBg] = ImVec4(0.07f, 0.09f, 0.11f, 0.97f);
    c[ImGuiCol_ChildBg] = ImVec4(0.10f, 0.12f, 0.15f, 1);
    c[ImGuiCol_PopupBg] = ImVec4(0.10f, 0.12f, 0.15f, 1);
    c[ImGuiCol_Border] = ImVec4(0.20f, 0.24f, 0.28f, 1);
    c[ImGuiCol_FrameBg] = ImVec4(0.14f, 0.17f, 0.20f, 1);
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.18f, 0.22f, 0.26f, 1);
    c[ImGuiCol_FrameBgActive] = ImVec4(0.20f, 0.25f, 0.29f, 1);
    c[ImGuiCol_Button] = ImVec4(0.20f, 0.45f, 0.30f, 1);
    c[ImGuiCol_ButtonHovered] = ImVec4(0.25f, 0.55f, 0.36f, 1);
    c[ImGuiCol_ButtonActive] = ImVec4(0.17f, 0.38f, 0.25f, 1);
    c[ImGuiCol_SliderGrab] = ImVec4(0.35f, 0.70f, 0.45f, 1);
    c[ImGuiCol_SliderGrabActive] = ImVec4(0.45f, 0.80f, 0.55f, 1);
    c[ImGuiCol_CheckMark] = ImVec4(0.35f, 0.70f, 0.45f, 1);
    c[ImGuiCol_Header] = ImVec4(0.20f, 0.45f, 0.30f, 1);
    c[ImGuiCol_HeaderHovered] = ImVec4(0.25f, 0.55f, 0.36f, 1);
    c[ImGuiCol_HeaderActive] = ImVec4(0.17f, 0.38f, 0.25f, 1);
    c[ImGuiCol_Separator] = ImVec4(0.20f, 0.24f, 0.28f, 1);
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
