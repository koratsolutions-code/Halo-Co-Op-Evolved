// Co Op Evolved dentro del juego (VERSION.dll en mcc\binaries\win64): captura los cuadrantes de J2-J4 y los manda
// como video. No toca los mandos: de eso se encarga Co Op Evolved Host.

#include "aim.h"
#include "armor_colors.h"
#include "character_probe.h"
#include "log.h"
#include "name_tags.h"
#include "p1_view.h"
#include "skins.h"
#include "spawn_probe.h"
#include "video.h"

#include <d3d11.h>
#include <dxgi.h>
#include <windows.h>

#include <MinHook.h>

namespace {

using PresentFn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT);
PresentFn g_original_present = nullptr;
constexpr int k_vtable_present = 8;

HRESULT STDMETHODCALLTYPE hooked_present(IDXGISwapChain* swap_chain, UINT sync, UINT flags) {
    if (!(flags & DXGI_PRESENT_TEST)) {
        static bool described = false;
        if (!described) {
            described = true;
            DXGI_SWAP_CHAIN_DESC desc{};
            swap_chain->GetDesc(&desc);
            log_write("Pantalla del juego: %ux%u formato %d", desc.BufferDesc.Width, desc.BufferDesc.Height,
                      int(desc.BufferDesc.Format));
        }
        name_tags_draw(swap_chain);  // antes de capturar: así los nombres también van en el video
        video_on_frame(swap_chain);
        p1_view_on_frame(swap_chain);  // después de capturar: J2-J4 siguen recibiendo su cuadrante
    }
    return g_original_present(swap_chain, sync, flags);
}

// Dirección de IDXGISwapChain::Present, sacada de un swapchain temporal.
void* find_present() {
    WNDCLASSEXW wc{sizeof(wc), CS_CLASSDC, DefWindowProcW, 0, 0, GetModuleHandleW(nullptr), nullptr, nullptr,
                   nullptr, nullptr, L"CoOpEvolvedDummy", nullptr};
    RegisterClassExW(&wc);
    HWND hwnd = CreateWindowW(wc.lpszClassName, L"", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr, nullptr,
                              wc.hInstance, nullptr);
    DXGI_SWAP_CHAIN_DESC desc{};
    desc.BufferCount = 2;
    desc.BufferDesc.Width = 64;
    desc.BufferDesc.Height = 64;
    desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.OutputWindow = hwnd;
    desc.SampleDesc.Count = 1;
    desc.Windowed = TRUE;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    IDXGISwapChain* swap_chain = nullptr;
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    void* address = nullptr;
    if (SUCCEEDED(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
                                                D3D11_SDK_VERSION, &desc, &swap_chain, &device, nullptr, &context)))
        address = (*reinterpret_cast<void***>(swap_chain))[k_vtable_present];
    if (swap_chain) swap_chain->Release();
    if (context) context->Release();
    if (device) device->Release();
    DestroyWindow(hwnd);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
    return address;
}

BOOL CALLBACK find_game_window(HWND hwnd, LPARAM found) {
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == GetCurrentProcessId() && IsWindowVisible(hwnd) && !GetWindow(hwnd, GW_OWNER)) {
        *reinterpret_cast<HWND*>(found) = hwnd;
        return FALSE;
    }
    return TRUE;
}

DWORD WINAPI init_thread(LPVOID) {
    // Esperamos a que el juego tenga ventana y unos segundos más: así AlphaRing y el overlay de Steam ya
    // instalaron sus propios hooks y no parcheamos el mismo código a la vez.
    HWND window = nullptr;
    while (!window) {
        Sleep(500);
        EnumWindows(find_game_window, reinterpret_cast<LPARAM>(&window));
    }
    Sleep(5000);
    video_start();
    name_tags_start();
    armor_colors_start();
    aim_start();
    character_probe_start();  // diagnóstico temporal de personajes (F9/F10)
    spawn_probe_start();      // diagnóstico temporal: quién hace aparecer a los jugadores (F11)
    void* present = find_present();
    MH_STATUS status = MH_Initialize();
    if (present && (status == MH_OK || status == MH_ERROR_ALREADY_INITIALIZED)) {
        status = MH_CreateHook(present, reinterpret_cast<void*>(&hooked_present),
                               reinterpret_cast<void**>(&g_original_present));
        if (status == MH_OK) status = MH_EnableHook(present);
    }
    if (present && status == MH_OK)
        log_write("Listo: capturando la pantalla del juego");
    else
        log_write("No se pudo enganchar Present (%s): sin video", MH_StatusToString(status));
    skins_start();  // usa MinHook: después de MH_Initialize
    return 0;
}

// Solo dentro de MCC (o del juego de prueba). CoOpEvolvedHost.exe está junto a este DLL y Windows también se lo
// carga como version.dll: ahí tiene que ser solo el proxy, sin hooks ni sockets.
bool is_game_process() {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    const wchar_t* name = wcsrchr(path, L'\\');
    name = name ? name + 1 : path;
    return _wcsicmp(name, L"MCC-Win64-Shipping.exe") == 0 || _wcsicmp(name, L"CoOpEvolvedTestGame.exe") == 0;
}

}  // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        if (!is_game_process()) return TRUE;
        // Que nadie pueda descargar el DLL mientras sus hilos y el hook de Present siguen vivos.
        HMODULE pinned = nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                           reinterpret_cast<LPCWSTR>(&DllMain), &pinned);
        log_init(module);
        log_write("Co Op Evolved cargado en el juego (pid %lu)", GetCurrentProcessId());
        if (HANDLE thread = CreateThread(nullptr, 0, init_thread, nullptr, 0, nullptr)) CloseHandle(thread);
    }
    return TRUE;
}
