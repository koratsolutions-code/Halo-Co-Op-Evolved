#include "subtitles.h"
#include "log.h"
#include "video.h"

#include <windows.h>

#include <MinHook.h>

#include <cstring>
#include <cwchar>
#include <mutex>
#include <thread>

namespace {

constexpr uintptr_t k_read_option_rva = 0x47B180;  // lee GameOpt.Subtitles y la guarda en objeto+0x50
constexpr size_t k_flag_offset = 0x50;
constexpr uint8_t k_read_option_bytes[] = {0x40, 0x57, 0x48, 0x83, 0xEC, 0x30, 0x48, 0xC7,
                                           0x44, 0x24, 0x20, 0xFE, 0xFF, 0xFF, 0xFF};

using ReadOptionFn = void(__fastcall*)(void* options);
using LoadLibraryExWFn = HMODULE(WINAPI*)(LPCWSTR, HANDLE, DWORD);
ReadOptionFn g_original = nullptr;
LoadLibraryExWFn g_original_load = nullptr;
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

// Pone el desvío si la función tiene sus bytes originales (recién cargado o recargado). Seguro de llamar varias veces.
void hook_if_needed(uintptr_t halo1) {
    std::lock_guard lock(g_mutex);
    if (g_gave_up || !halo1) return;
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

// MCC carga halo1.dll con LoadLibraryExW: el desvío se pone apenas vuelve, antes de que MCC lo use.
HMODULE WINAPI hooked_load(LPCWSTR name, HANDLE file, DWORD flags) {
    HMODULE module = g_original_load(name, file, flags);
    if (module && name) {
        size_t length = wcslen(name);
        if (length >= 9 && _wcsicmp(name + length - 9, L"halo1.dll") == 0)
            hook_if_needed(reinterpret_cast<uintptr_t>(module));
    }
    return module;
}

}  // namespace

void subtitles_start() {
    if (HMODULE kernelbase = GetModuleHandleW(L"KernelBase.dll")) {
        void* load = reinterpret_cast<void*>(GetProcAddress(kernelbase, "LoadLibraryExW"));
        if (load && MH_CreateHook(load, reinterpret_cast<void*>(&hooked_load),
                                  reinterpret_cast<void**>(&g_original_load)) == MH_OK)
            MH_EnableHook(load);
    }
    // Por si halo1.dll ya estaba cargado o se cargó por otro camino.
    std::thread([] {
        while (!g_gave_up) {
            hook_if_needed(reinterpret_cast<uintptr_t>(GetModuleHandleW(L"halo1.dll")));
            Sleep(50);
        }
    }).detach();
}
