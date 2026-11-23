#include "halo1_load.h"
#include "log.h"

#include <windows.h>

#include <MinHook.h>

#include <cwchar>
#include <mutex>
#include <vector>

namespace {

using LoadLibraryExWFn = HMODULE(WINAPI*)(LPCWSTR, HANDLE, DWORD);
LoadLibraryExWFn g_original_load = nullptr;
std::mutex g_mutex;  // también hace que dos avisos no se mezclen
std::vector<Halo1LoadedFn> g_subscribers;

bool is_halo1(LPCWSTR name) {
    if (!name) return false;
    size_t length = wcslen(name);
    return length >= 9 && _wcsicmp(name + length - 9, L"halo1.dll") == 0;
}

HMODULE WINAPI hooked_load(LPCWSTR name, HANDLE file, DWORD flags) {
    HMODULE module = g_original_load(name, file, flags);
    if (module && is_halo1(name)) {
        std::lock_guard lock(g_mutex);
        for (Halo1LoadedFn fn : g_subscribers) fn(reinterpret_cast<uintptr_t>(module));
    }
    return module;
}

}  // namespace

void halo1_load_start() {
    HMODULE kernelbase = GetModuleHandleW(L"KernelBase.dll");
    void* load = kernelbase ? reinterpret_cast<void*>(GetProcAddress(kernelbase, "LoadLibraryExW")) : nullptr;
    MH_STATUS status = load ? MH_CreateHook(load, reinterpret_cast<void*>(&hooked_load),
                                            reinterpret_cast<void**>(&g_original_load))
                            : MH_ERROR_FUNCTION_NOT_FOUND;
    if (status == MH_OK) status = MH_EnableHook(load);
    if (status != MH_OK) log_write("No se pudo vigilar la carga de halo1.dll (%s)", MH_StatusToString(status));
}

void halo1_load_subscribe(Halo1LoadedFn fn) {
    std::lock_guard lock(g_mutex);
    g_subscribers.push_back(fn);
    // Si ya estaba cargado (el mod se cargó tarde): una vez ahora.
    if (HMODULE halo1 = GetModuleHandleW(L"halo1.dll")) fn(reinterpret_cast<uintptr_t>(halo1));
}
