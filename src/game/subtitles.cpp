#include "subtitles.h"
#include "halo1_load.h"
#include "log.h"
#include "video.h"

#include <windows.h>

#include <MinHook.h>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <cwchar>
#include <mutex>
#include <thread>
#include <vector>

namespace {

constexpr uintptr_t k_read_option_rva = 0x47B180;  // lee GameOpt.Subtitles y la guarda en objeto+0x50
constexpr size_t k_flag_offset = 0x50;
constexpr uint8_t k_read_option_bytes[] = {0x40, 0x57, 0x48, 0x83, 0xEC, 0x30, 0x48, 0xC7,
                                           0x44, 0x24, 0x20, 0xFE, 0xFF, 0xFF, 0xFF};

using ReadOptionFn = void(__fastcall*)(void* options);
ReadOptionFn g_original = nullptr;
std::mutex g_mutex;
bool g_gave_up = false;

bool hide_wanted() {
    proto::VideoConfig config;
    return video_config(config) && config.hide_subtitles;
}

void __fastcall hooked_read_option(void* options) {
    g_original(options);
    if (options && hide_wanted()) {
        auto* flag = reinterpret_cast<volatile uint8_t*>(reinterpret_cast<uintptr_t>(options) + k_flag_offset);
        if (*flag) {
            *flag = 0;  // quien llamó ve 0 y le dice a la interfaz de subtítulos que oculte todo
            log_write("Subtítulos: ocultos (opción del host)");
        }
    }
}

// ---- Líneas en pantalla: cuándo Halo le pide a MCC mostrar u ocultar una línea (para copiarla a cada jugador) ----
// halo1+0x47A860 manda "ShowText" (rcx, rdx = puntero a la línea, xmm2 = duración, siempre 0: dura hasta "HideText");
// halo1+0x47AB40 manda "HideText" (rcx, rdx = puntero a la línea). *rdx apunta al nombre del sonido de la línea
// (p. ej. "sound\dialog\a30\a30_030_cortana"): es igual en las dos llamadas y sirve para seguir cada línea.
// ShowText no hace nada si los subtítulos están apagados (objeto de opciones en halo1+0x2E3D1F8, marca en +0x50).
constexpr uintptr_t k_show_text_rva = 0x47A860;
constexpr uintptr_t k_hide_text_rva = 0x47AB40;
constexpr uintptr_t k_options_rva = 0x2E3D1F8;
constexpr uint8_t k_show_text_bytes[] = {0x40, 0x55, 0x56, 0x57, 0x41, 0x56, 0x41, 0x57, 0x48, 0x8D, 0x6C, 0x24, 0xC9};
constexpr uint8_t k_hide_text_bytes[] = {0x4C, 0x8B, 0xDC, 0x57, 0x48, 0x81, 0xEC, 0x80, 0x00, 0x00, 0x00};
constexpr uint64_t k_line_timeout_ms = 30000;  // por si un "HideText" no llega: ninguna línea dura tanto

using ShowTextFn = void*(__fastcall*)(void*, void*, float);
using HideTextFn = void*(__fastcall*)(void*, void*);
ShowTextFn g_original_show = nullptr;
HideTextFn g_original_hide = nullptr;
bool g_lines_gave_up = false;
std::atomic<uintptr_t> g_halo1{0};

struct Line {
    uint64_t key;    // *rdx: dirección del nombre del sonido
    uint64_t start;  // GetTickCount64
};
std::mutex g_lines_mutex;
std::vector<Line> g_lines;

uint64_t line_key(void* text) {
    uint64_t value = 0;
    SIZE_T read = 0;
    if (!text || !ReadProcessMemory(GetCurrentProcess(), text, &value, sizeof(value), &read)) return 0;
    return value;
}

// La marca de subtítulos de MCC (también la apaga "Ocultar los subtítulos de MCC"). true si no se puede leer.
bool subtitles_enabled() {
    uintptr_t halo1 = g_halo1;
    uint64_t options = 0;
    uint8_t flag = 1;
    SIZE_T read = 0;
    if (!halo1 || !ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(halo1 + k_options_rva), &options,
                                     sizeof(options), &read) ||
        !options)
        return true;
    if (!ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(options + k_flag_offset), &flag, 1, &read))
        return true;
    return flag != 0;
}

void* __fastcall hooked_show_text(void* self, void* text, float duration) {
    uint64_t key = line_key(text);
    bool enabled = subtitles_enabled();
    if (key && !enabled) {
        static bool logged = false;
        if (!logged) log_write("Subtítulos: línea sin mostrar (subtítulos apagados en MCC o por el host)");
        logged = true;
    }
    if (key && enabled) {
        std::lock_guard lock(g_lines_mutex);
        bool known = false;
        for (Line& line : g_lines)
            if (line.key == key) line.start = GetTickCount64(), known = true;
        if (!known) g_lines.push_back({key, GetTickCount64()});
    }
    return g_original_show(self, text, duration);
}

void* __fastcall hooked_hide_text(void* self, void* text) {
    uint64_t key = line_key(text);
    {
        std::lock_guard lock(g_lines_mutex);
        std::erase_if(g_lines, [key](const Line& line) { return line.key == key; });
    }
    return g_original_hide(self, text);
}

// Mismo criterio que el de la opción: solo si los bytes son los originales.
void line_hook(uintptr_t halo1, uintptr_t rva, const uint8_t* expected, size_t size, void* detour, void** original,
               const char* name) {
    void* target = reinterpret_cast<void*>(halo1 + rva);
    uint8_t bytes[16];
    SIZE_T read = 0;
    if (!ReadProcessMemory(GetCurrentProcess(), target, bytes, size, &read) || read != size) return;
    if (memcmp(bytes, expected, size) != 0) {
        if (bytes[0] != 0xE9) {
            log_write("Subtítulos: halo1.dll no es el esperado: no se copian a cada jugador (%s)", name);
            g_lines_gave_up = true;
        }
        return;
    }
    MH_RemoveHook(target);
    MH_STATUS status = MH_CreateHook(target, detour, original);
    if (status == MH_OK) status = MH_EnableHook(target);
    if (status != MH_OK) {
        log_write("Subtítulos: no se pudo desviar %s (%s)", name, MH_StatusToString(status));
        g_lines_gave_up = true;
    } else {
        std::lock_guard lock(g_lines_mutex);
        g_lines.clear();  // halo1.dll recién cargado: las líneas de la misión anterior ya no están
    }
}

// Pone el desvío si la función tiene sus bytes originales (recién cargado o recargado). Seguro de llamar varias veces.
void hook_if_needed(uintptr_t halo1) {
    std::lock_guard lock(g_mutex);
    if (!halo1) return;
    g_halo1 = halo1;
    if (!g_lines_gave_up) {
        line_hook(halo1, k_show_text_rva, k_show_text_bytes, sizeof(k_show_text_bytes),
                   reinterpret_cast<void*>(&hooked_show_text), reinterpret_cast<void**>(&g_original_show), "ShowText");
        line_hook(halo1, k_hide_text_rva, k_hide_text_bytes, sizeof(k_hide_text_bytes),
                   reinterpret_cast<void*>(&hooked_hide_text), reinterpret_cast<void**>(&g_original_hide), "HideText");
    }
    if (g_gave_up) return;
    void* target = reinterpret_cast<void*>(halo1 + k_read_option_rva);
    uint8_t bytes[sizeof(k_read_option_bytes)];
    SIZE_T read = 0;
    if (!ReadProcessMemory(GetCurrentProcess(), target, bytes, sizeof(bytes), &read) || read != sizeof(bytes)) return;
    if (memcmp(bytes, k_read_option_bytes, sizeof(bytes)) != 0) {
        if (bytes[0] != 0xE9) {  // ni el original ni nuestro salto: otra versión de halo1.dll
            log_write("Subtítulos: halo1.dll no es el esperado: la opción de ocultarlos no funciona");
            g_gave_up = true;
        }
        return;
    }
    MH_RemoveHook(target);  // el de la carga anterior: MinHook todavía cree que existe
    MH_STATUS status = MH_CreateHook(target, reinterpret_cast<void*>(&hooked_read_option),
                                     reinterpret_cast<void**>(&g_original));
    if (status == MH_OK) status = MH_EnableHook(target);
    if (status != MH_OK) {
        log_write("Subtítulos: no se pudo desviar la opción (%s)", MH_StatusToString(status));
        g_gave_up = true;
    }
}

}  // namespace

bool subtitles_showing() {
    if (!GetModuleHandleW(L"halo1.dll")) return false;
    std::lock_guard lock(g_lines_mutex);
    uint64_t now = GetTickCount64();
    std::erase_if(g_lines, [now](const Line& line) { return now - line.start > k_line_timeout_ms; });
    return !g_lines.empty();
}

void subtitles_start() {
    halo1_load_subscribe(&hook_if_needed);  // en cada carga de halo1.dll (ver halo1_load.h)
}
