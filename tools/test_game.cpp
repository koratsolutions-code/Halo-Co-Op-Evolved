// Juego de prueba: dibuja 4 cuadrantes de colores que se mueven y carga VERSION.dll (el mod) como lo haría MCC.
// Sirve para probar el video de punta a punta sin abrir el juego. Uso: CoOpEvolvedTestGame.exe <ruta\VERSION.dll>

#include <d3d11_1.h>
#include <dxgi1_2.h>
#include <windows.h>

#include <mmsystem.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

static LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (msg == WM_DESTROY) PostQuitMessage(0);
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR command_line, int) {
    WNDCLASSW wc{0, proc, 0, 0, instance, nullptr, LoadCursorW(nullptr, IDC_ARROW), nullptr, nullptr, L"TestGame"};
    RegisterClassW(&wc);
    const int width = 1280, height = 720;
    RECT rect{0, 0, width, height};
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
    HWND hwnd = CreateWindowW(L"TestGame", L"Juego de prueba Co Op Evolved", WS_OVERLAPPEDWINDOW, 50, 50,
                              rect.right - rect.left, rect.bottom - rect.top, nullptr, nullptr, instance, nullptr);
    ShowWindow(hwnd, SW_SHOWNOACTIVATE);

    DXGI_SWAP_CHAIN_DESC desc{};
    desc.BufferCount = 2;
    desc.BufferDesc.Width = width;
    desc.BufferDesc.Height = height;
    desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.OutputWindow = hwnd;
    desc.SampleDesc.Count = 1;
    desc.Windowed = TRUE;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    IDXGISwapChain* swap_chain = nullptr;
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    // En laptops con dos GPU, usar la NVIDIA como hace MCC (si no, NVENC no está disponible).
    IDXGIFactory1* factory = nullptr;
    IDXGIAdapter1* nvidia = nullptr;
    CreateDXGIFactory1(IID_PPV_ARGS(&factory));
    for (UINT i = 0; factory && !nvidia; ++i) {
        IDXGIAdapter1* adapter = nullptr;
        if (factory->EnumAdapters1(i, &adapter) == DXGI_ERROR_NOT_FOUND) break;
        DXGI_ADAPTER_DESC1 adapter_desc;
        adapter->GetDesc1(&adapter_desc);
        if (adapter_desc.VendorId == 0x10DE)
            nvidia = adapter;
        else
            adapter->Release();
    }
    if (FAILED(D3D11CreateDeviceAndSwapChain(nvidia, nvidia ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE,
                                             nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &desc, &swap_chain, &device,
                                             nullptr, &context)))
        return 1;
    ID3D11Texture2D* back = nullptr;
    swap_chain->GetBuffer(0, IID_PPV_ARGS(&back));
    ID3D11RenderTargetView* target = nullptr;
    device->CreateRenderTargetView(back, nullptr, &target);
    back->Release();
    ID3D11DeviceContext1* context1 = nullptr;
    context->QueryInterface(IID_PPV_ARGS(&context1));

    if (command_line && *command_line) LoadLibraryW(command_line);  // el mod, como VERSION.dll en MCC

    // COOPEVOLVED_TONE=1: un tono de 440 Hz casi inaudible (-60 dB) para probar la captura de audio.
    static std::vector<uint8_t> wav;
    if (GetEnvironmentVariableW(L"COOPEVOLVED_TONE", nullptr, 0)) {
        const uint32_t rate = 48000, samples = rate;  // 1 s en bucle
        uint32_t data_size = samples * 4;
        wav.resize(44 + data_size);
        auto put32 = [&](size_t at, uint32_t v) { memcpy(&wav[at], &v, 4); };
        auto put16 = [&](size_t at, uint16_t v) { memcpy(&wav[at], &v, 2); };
        memcpy(&wav[0], "RIFF", 4); put32(4, 36 + data_size); memcpy(&wav[8], "WAVEfmt ", 8);
        put32(16, 16); put16(20, 1); put16(22, 2); put32(24, rate); put32(28, rate * 4); put16(32, 4); put16(34, 16);
        memcpy(&wav[36], "data", 4); put32(40, data_size);
        for (uint32_t i = 0; i < samples; ++i) {
            int16_t v = int16_t(30 * std::sin(2 * 3.14159265 * 440 * i / rate));
            put16(44 + i * 4, uint16_t(v));
            put16(46 + i * 4, uint16_t(v));
        }
        PlaySoundW(reinterpret_cast<LPCWSTR>(wav.data()), nullptr, SND_MEMORY | SND_ASYNC | SND_LOOP);
    }

    const float colors[4][4] = {{0.8f, 0.2f, 0.2f, 1}, {0.2f, 0.7f, 0.2f, 1}, {0.2f, 0.3f, 0.9f, 1}, {0.9f, 0.8f, 0.1f, 1}};
    MSG msg{};
    ULONGLONG start = GetTickCount64();
    while (msg.message != WM_QUIT) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);
        float t = (GetTickCount64() - start) / 1000.0f;
        const float black[4] = {0, 0, 0, 1};
        context->ClearRenderTargetView(target, black);
        for (int q = 0; q < 4; ++q) {
            // Cada cuadrante: su color y una barra blanca que se mueve (para ver que el video está vivo).
            LONG x0 = (q % 2) * width / 2, y0 = (q / 2) * height / 2;
            D3D11_RECT r{x0, y0, x0 + width / 2, y0 + height / 2};
            context1->ClearView(target, colors[q], &r, 1);
            LONG bar = x0 + LONG((std::sin(t * 2 + q) * 0.5f + 0.5f) * (width / 2 - 40));
            D3D11_RECT b{bar, y0 + 40, bar + 40, y0 + height / 2 - 40};
            const float white[4] = {1, 1, 1, 1};
            context1->ClearView(target, white, &b, 1);
        }
        swap_chain->Present(1, 0);
    }
    return 0;
}
