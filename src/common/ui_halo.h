#pragma once
// Estilo "launcher de Halo" del panel del host y del cliente: fondo con hexágonos, paneles con esquinas recortadas,
// y los textos fijos (títulos, etiquetas, botones) como imágenes hechas con la fuente de Halo
// (tools/make_ui_images.py: la fuente no se distribuye, solo las imágenes). Implementación en src/app/ui_halo.cpp.
#include <d3d11.h>
#include <imgui.h>

bool ui_halo_init(ID3D11Device* device);  // carga las imágenes del .exe; después de crear el dispositivo
void ui_halo_release();

// Texto fijo (clave de tools/make_ui_images.py) en el idioma actual, de height píxeles de alto.
float ui_halo_text_width(const char* key, float height);
void ui_halo_text(const char* key, float height, const ImVec4& color);
// Botón con esquinas recortadas y el texto fijo centrado. Respeta BeginDisabled.
bool ui_halo_button(const char* key, const ImVec2& size, const ImVec4& color, float text_height = 0);
// Pestaña: la elegida va rellena en azul con borde celeste; las otras, oscuras. true si se hizo clic.
// label en mayúsculas, con la fuente de ui_halo_set_tab_font (Bahnschrift) en negrita y letras espaciadas.
bool ui_halo_tab(const char* id, const char* label, const ImVec2& size, bool selected);
void ui_halo_set_tab_font(ImFont* font);
void ui_halo_logo(float height, const ImVec4& color);

void ui_halo_background();  // degradado y hexágonos detrás de todo (cada fotograma, antes de la ventana)
// Panel con esquinas recortadas y línea de color arriba. title_key: texto fijo del título o nullptr.
// size.y = 0: alto según el contenido.
void ui_halo_panel_begin(const char* id, const char* title_key, const ImVec2& size = ImVec2(0, 0));
void ui_halo_panel_end();
void ui_halo_status(const ImVec4& color, const char* text);  // rombo de color y texto
