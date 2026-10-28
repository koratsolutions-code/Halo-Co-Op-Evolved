// MCC carga VERSION.dll desde su propia carpeta antes que la de Windows: así entra nuestro código al juego.
// Cada función exportada reenvía la llamada a la version.dll real de System32.
// Todas reciben solo enteros/punteros, así que en x64 basta con pasar los 8 argumentos tal cual.

#include <windows.h>

#include <cstdint>

using Generic = uintptr_t (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t);

static HMODULE real_version() {
    static HMODULE module = [] {
        wchar_t path[MAX_PATH];
        GetSystemDirectoryW(path, MAX_PATH);
        wcscat_s(path, L"\\version.dll");
        return LoadLibraryW(path);
    }();
    return module;
}

#define FORWARD(name)                                                                                         \
    extern "C" uintptr_t proxy_##name(uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e,       \
                                      uintptr_t f, uintptr_t g, uintptr_t h) {                                \
        static auto real = reinterpret_cast<Generic>(GetProcAddress(real_version(), #name));                  \
        return real ? real(a, b, c, d, e, f, g, h) : 0;                                                       \
    }

FORWARD(GetFileVersionInfoA)
FORWARD(GetFileVersionInfoByHandle)
FORWARD(GetFileVersionInfoExA)
FORWARD(GetFileVersionInfoExW)
FORWARD(GetFileVersionInfoSizeA)
FORWARD(GetFileVersionInfoSizeExA)
FORWARD(GetFileVersionInfoSizeExW)
FORWARD(GetFileVersionInfoSizeW)
FORWARD(GetFileVersionInfoW)
FORWARD(VerFindFileA)
FORWARD(VerFindFileW)
FORWARD(VerInstallFileA)
FORWARD(VerInstallFileW)
FORWARD(VerLanguageNameA)
FORWARD(VerLanguageNameW)
FORWARD(VerQueryValueA)
FORWARD(VerQueryValueW)
