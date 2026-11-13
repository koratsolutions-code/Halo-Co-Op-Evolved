#include "../common/ui_halo.h"
#include "../common/lang.h"
#include "../common/ui_style.h"
#include "../common/ui_text_atlas.h"

#include <windows.h>
#include <wincodec.h>

#include <cfloat>
#include <cmath>
#include <cstring>
#include <vector>

namespace {

constexpr int k_resource_text = 101, k_resource_logo = 102;  // app.rc

ID3D11ShaderResourceView* g_text = nullptr;
ID3D11ShaderResourceView* g_logo = nullptr;
ImFont* g_tab_font = nullptr;  // Bahnschrift, para las pestañas

// PNG guardado como recurso del .exe -> textura RGBA.
ID3D11ShaderResourceView* load_png(ID3D11Device* device, int resource) {
    HMODULE module = GetModuleHandleW(nullptr);
    HRSRC found = FindResourceW(module, MAKEINTRESOURCEW(resource), MAKEINTRESOURCEW(10));  // RT_RCDATA
    HGLOBAL handle = found ? LoadResource(module, found) : nullptr;
    const void* data = handle ? LockResource(handle) : nullptr;
    DWORD size = found ? SizeofResource(module, found) : 0;
    if (!data || !size) return nullptr;

    HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    IWICImagingFactory* factory = nullptr;
    IWICStream* stream = nullptr;
    IWICBitmapDecoder* decoder = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICFormatConverter* converter = nullptr;
    ID3D11ShaderResourceView* view = nullptr;
    UINT width = 0, height = 0;
    std::vector<uint8_t> pixels;
    if (SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))) &&
        SUCCEEDED(factory->CreateStream(&stream)) &&
        SUCCEEDED(stream->InitializeFromMemory(static_cast<BYTE*>(const_cast<void*>(data)), size)) &&
        SUCCEEDED(factory->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnDemand, &decoder)) &&
        SUCCEEDED(decoder->GetFrame(0, &frame)) && SUCCEEDED(factory->CreateFormatConverter(&converter)) &&
        SUCCEEDED(converter->Initialize(frame, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0,
                                        WICBitmapPaletteTypeCustom)) &&
        SUCCEEDED(converter->GetSize(&width, &height))) {
        pixels.resize(size_t(width) * height * 4);
        if (SUCCEEDED(converter->CopyPixels(nullptr, width * 4, UINT(pixels.size()), pixels.data()))) {
            D3D11_TEXTURE2D_DESC desc{};
            desc.Width = width;
            desc.Height = height;
            desc.MipLevels = 1;
            desc.ArraySize = 1;
            desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            desc.SampleDesc.Count = 1;
            desc.Usage = D3D11_USAGE_IMMUTABLE;
            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            D3D11_SUBRESOURCE_DATA init{pixels.data(), width * 4, 0};
            ID3D11Texture2D* texture = nullptr;
            if (SUCCEEDED(device->CreateTexture2D(&desc, &init, &texture))) {
                device->CreateShaderResourceView(texture, nullptr, &view);
                texture->Release();
            }
        }
    }
    if (converter) converter->Release();
    if (frame) frame->Release();
    if (decoder) decoder->Release();
    if (stream) stream->Release();
    if (factory) factory->Release();
    if (SUCCEEDED(com)) CoUninitialize();
    return view;
}

const UiTextRect* find(const char* key) {
    for (const UiTextEntry& entry : k_ui_texts)
        if (strcmp(entry.key, key) == 0) return g_lang == Lang::es ? &entry.es : &entry.en;
    return nullptr;
}

// Polígono con las esquinas de arriba a la izquierda y de abajo a la derecha recortadas.
void chamfer(ImVec2 a, ImVec2 b, float cut, ImVec2 out[6]) {
    out[0] = ImVec2(a.x + cut, a.y);
    out[1] = ImVec2(b.x, a.y);
    out[2] = ImVec2(b.x, b.y - cut);
    out[3] = ImVec2(b.x - cut, b.y);
    out[4] = ImVec2(a.x, b.y);
    out[5] = ImVec2(a.x, a.y + cut);
}

ImVec4 scale(const ImVec4& c, float f) {
    return ImVec4(std::fmin(c.x * f, 1.0f), std::fmin(c.y * f, 1.0f), std::fmin(c.z * f, 1.0f), c.w);
}

}  // namespace

bool ui_halo_init(ID3D11Device* device) {
    ui_halo_release();
    g_text = load_png(device, k_resource_text);
    g_logo = load_png(device, k_resource_logo);
    return g_text && g_logo;
}

void ui_halo_release() {
    if (g_text) g_text->Release();
    if (g_logo) g_logo->Release();
    g_text = g_logo = nullptr;
}

float ui_halo_text_width(const char* key, float height) {
    const UiTextRect* r = find(key);
    return r ? r->w * height / r->h : 0;
}

void ui_halo_text(const char* key, float height, const ImVec4& color) {
    const UiTextRect* r = find(key);
    if (!r || !g_text) {
        ImGui::TextColored(color, "%s", key);
        return;
    }
    ImVec2 uv0(float(r->x) / k_ui_text_atlas_w, float(r->y) / k_ui_text_atlas_h);
    ImVec2 uv1(float(r->x + r->w) / k_ui_text_atlas_w, float(r->y + r->h) / k_ui_text_atlas_h);
    ImGui::Image(ImTextureID(g_text), ImVec2(r->w * height / r->h, height), uv0, uv1, color);
}

// Texto letra por letra, centrado en el rectángulo, con espacio extra entre letras y "negrita" (dos pasadas corridas
// 1 px: Bahnschrift viene como fuente variable y solo se puede cargar su peso normal).
void draw_spaced(ImDrawList* draw, ImFont* font, float font_size, const char* text, ImVec2 a, ImVec2 size, ImU32 color) {
    const float spacing = 1.5f;
    float width = 0;
    for (const char* c = text; *c; ++c) width += font->CalcTextSizeA(font_size, FLT_MAX, 0, c, c + 1).x + spacing;
    width -= spacing;
    float x = a.x + (size.x - width - 1) / 2, y = a.y + (size.y - font_size) / 2;
    for (const char* c = text; *c; ++c) {
        draw->AddText(font, font_size, ImVec2(x, y), color, c, c + 1);
        draw->AddText(font, font_size, ImVec2(x + 1, y), color, c, c + 1);
        x += font->CalcTextSizeA(font_size, FLT_MAX, 0, c, c + 1).x + spacing;
    }
}

bool ui_halo_button(const char* key, const ImVec2& size, const ImVec4& color, float text_height) {
    ImGui::PushID(key);
    ImVec2 a = ImGui::GetCursorScreenPos();
    bool pressed = ImGui::InvisibleButton("##b", size);
    bool hovered = ImGui::IsItemHovered(), held = ImGui::IsItemActive();
    ImVec2 b(a.x + size.x, a.y + size.y);
    ImVec2 shape[6];
    chamfer(a, b, std::fmin(10.0f, size.y * 0.25f), shape);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImVec4 fill = held ? scale(color, 0.8f) : hovered ? scale(color, 1.2f) : color;
    draw->AddConvexPolyFilled(shape, 6, ImGui::GetColorU32(fill));
    draw->AddPolyline(shape, 6, ImGui::GetColorU32(scale(color, 1.45f)), ImDrawFlags_Closed, 1.5f);
    const UiTextRect* r = find(key);
    if (r && g_text) {
        float h = text_height > 0 ? text_height : size.y * 0.62f;
        float w = r->w * h / r->h;
        if (w > size.x - 16) {
            h *= (size.x - 16) / w;
            w = size.x - 16;
        }
        ImVec2 p(a.x + (size.x - w) / 2, a.y + (size.y - h) / 2);
        ImVec2 uv0(float(r->x) / k_ui_text_atlas_w, float(r->y) / k_ui_text_atlas_h);
        ImVec2 uv1(float(r->x + r->w) / k_ui_text_atlas_w, float(r->y + r->h) / k_ui_text_atlas_h);
        draw->AddImage(ImTextureID(g_text), p, ImVec2(p.x + w, p.y + h), uv0, uv1, ImGui::GetColorU32(k_text));
    }
    ImGui::PopID();
    return pressed;
}

void ui_halo_set_tab_font(ImFont* font) {
    g_tab_font = font;
}

bool ui_halo_tab(const char* id, const char* label, const ImVec2& size, bool selected) {
    ImGui::PushID(id);
    ImVec2 a = ImGui::GetCursorScreenPos();
    bool pressed = ImGui::InvisibleButton("##t", size);
    bool hovered = ImGui::IsItemHovered();
    ImVec2 b(a.x + size.x, a.y + size.y);
    ImVec2 shape[6];
    chamfer(a, b, std::fmin(10.0f, size.y * 0.25f), shape);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImVec4 fill = selected ? k_blue : hovered ? ImVec4(0.10f, 0.20f, 0.29f, 0.95f) : ImVec4(0.04f, 0.08f, 0.12f, 0.9f);
    draw->AddConvexPolyFilled(shape, 6, ImGui::GetColorU32(fill));
    draw->AddPolyline(shape, 6, ImGui::GetColorU32(selected || hovered ? k_cyan : k_panel_edge), ImDrawFlags_Closed,
                      selected ? 2.0f : 1.0f);
    ImFont* font = g_tab_font ? g_tab_font : ImGui::GetFont();
    draw_spaced(draw, font, font->FontSize, label, a, size,
                ImGui::GetColorU32(selected || hovered ? k_text : k_muted));
    ImGui::PopID();
    return pressed;
}

void ui_halo_logo(float height, const ImVec4& color) {
    if (!g_logo) {
        ImGui::TextColored(color, "HALO: CO OP EVOLVED");
        return;
    }
    ImGui::Image(ImTextureID(g_logo), ImVec2(height * k_ui_logo_w / k_ui_logo_h, height), ImVec2(0, 0),
                 ImVec2(1, 1), color);
}

void ui_halo_background() {
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    ImVec2 size = ImGui::GetIO().DisplaySize;
    draw->AddRectFilledMultiColor(ImVec2(0, 0), size, ImGui::GetColorU32(k_bg_top), ImGui::GetColorU32(k_bg_top),
                                  ImGui::GetColorU32(k_bg_bottom), ImGui::GetColorU32(k_bg_bottom));
    // Hexágonos tenues, como el fondo de los menús de Halo.
    const float r = 24, w = r * 1.7320508f, h = r * 1.5f;
    ImU32 line = ImGui::GetColorU32(k_hex_line);
    for (int row = -1; row * h < size.y + r; ++row) {
        for (int col = -1; col * w < size.x + w; ++col) {
            ImVec2 c(col * w + (row & 1 ? w / 2 : 0), row * h);
            ImVec2 points[6];
            for (int k = 0; k < 6; ++k) {
                float angle = 3.14159265f / 180.0f * (60 * k + 30);
                points[k] = ImVec2(c.x + r * std::cos(angle), c.y + r * std::sin(angle));
            }
            draw->AddPolyline(points, 6, line, ImDrawFlags_Closed, 1.0f);
        }
    }
}

void ui_halo_panel_begin(const char* id, const char* title_key, const ImVec2& size) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 10));
    ImGui::BeginChild(id, size,
                      ImGuiChildFlags_AlwaysUseWindowPadding | (size.y > 0 ? ImGuiChildFlags_None : ImGuiChildFlags_AutoResizeY),
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    if (title_key) ui_halo_text(title_key, 28, k_cyan);
}

void ui_halo_panel_end() {
    ImVec2 a = ImGui::GetWindowPos(), s = ImGui::GetWindowSize();
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
    // El fondo va en la lista de la ventana de afuera: se dibuja antes que el contenido del panel.
    ImVec2 b(a.x + s.x, a.y + s.y);
    ImVec2 shape[6];
    chamfer(a, b, 14, shape);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddConvexPolyFilled(shape, 6, ImGui::GetColorU32(k_panel));
    draw->AddPolyline(shape, 6, ImGui::GetColorU32(k_panel_edge), ImDrawFlags_Closed, 1.0f);
    draw->AddLine(ImVec2(a.x + 14, a.y + 1), ImVec2(a.x + 110, a.y + 1), ImGui::GetColorU32(k_cyan), 3.0f);
}

void ui_halo_status(const ImVec4& color, const char* text) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    float h = ImGui::GetTextLineHeight(), r = 5;
    ImVec2 c(p.x + r + 1, p.y + h / 2);
    ImGui::GetWindowDrawList()->AddQuadFilled(ImVec2(c.x, c.y - r), ImVec2(c.x + r, c.y), ImVec2(c.x, c.y + r),
                                              ImVec2(c.x - r, c.y), ImGui::GetColorU32(color));
    ImGui::Dummy(ImVec2(r * 2 + 4, h));
    ImGui::SameLine(0, 4);
    ImGui::TextUnformatted(text);
}
