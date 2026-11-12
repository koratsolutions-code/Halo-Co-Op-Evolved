#include "local_players.h"
#include "log.h"
#include "video.h"

#include <windows.h>
#include <xinput.h>

#include <MinHook.h>

#include <atomic>
#include <cmath>
#include <cstring>
#include <mutex>
#include <thread>

namespace {

constexpr int k_players = 4;
constexpr int k_remote = 3;  // J2-J4 = slots XInput 0, 1, 2

// ---- Mandos por red ----
std::mutex g_pads_mutex;
proto::GamepadState g_pads[k_remote]{};
DWORD g_packet[k_remote] = {};

using GetStateFn = DWORD(WINAPI*)(DWORD, XINPUT_STATE*);
using SetStateFn = DWORD(WINAPI*)(DWORD, XINPUT_VIBRATION*);
GetStateFn g_xinput_get = nullptr;
SetStateFn g_xinput_set = nullptr;

DWORD WINAPI hooked_get_state(DWORD slot, XINPUT_STATE* state) {
    if (slot < DWORD(k_remote)) {
        if (!state) return ERROR_BAD_ARGUMENTS;
        std::lock_guard lock(g_pads_mutex);
        const proto::GamepadState& pad = g_pads[slot];
        state->dwPacketNumber = g_packet[slot];
        state->Gamepad.wButtons = pad.buttons;
        state->Gamepad.bLeftTrigger = pad.left_trigger;
        state->Gamepad.bRightTrigger = pad.right_trigger;
        state->Gamepad.sThumbLX = pad.lx;
        state->Gamepad.sThumbLY = pad.ly;
        state->Gamepad.sThumbRX = pad.rx;
        state->Gamepad.sThumbRY = pad.ry;
        return ERROR_SUCCESS;
    }
    if (slot == 3) return g_xinput_get(0, state);  // el mando físico del host
    return ERROR_DEVICE_NOT_CONNECTED;
}

DWORD WINAPI hooked_set_state(DWORD slot, XINPUT_VIBRATION* vibration) {
    if (slot < DWORD(k_remote)) return ERROR_SUCCESS;  // los mandos de red no vibran (todavía)
    if (slot == 3) return g_xinput_set(0, vibration);
    return ERROR_DEVICE_NOT_CONNECTED;
}

bool hook_xinput() {
    HMODULE xinput = GetModuleHandleW(L"XINPUT1_3.dll");
    if (!xinput) xinput = LoadLibraryW(L"XINPUT1_3.dll");
    void* get = xinput ? reinterpret_cast<void*>(GetProcAddress(xinput, "XInputGetState")) : nullptr;
    void* set = xinput ? reinterpret_cast<void*>(GetProcAddress(xinput, "XInputSetState")) : nullptr;
    if (!get || !set) {
        log_write("Jugadores: no se encontró XINPUT1_3.dll");
        return false;
    }
    MH_STATUS status = MH_CreateHook(get, reinterpret_cast<void*>(&hooked_get_state),
                                     reinterpret_cast<void**>(&g_xinput_get));
    if (status == MH_OK)
        status = MH_CreateHook(set, reinterpret_cast<void*>(&hooked_set_state), reinterpret_cast<void**>(&g_xinput_set));
    if (status == MH_OK) status = MH_EnableHook(get);
    if (status == MH_OK) status = MH_EnableHook(set);
    if (status != MH_OK) log_write("Jugadores: no se pudo desviar XInput (%s)", MH_StatusToString(status));
    return status == MH_OK;
}

// ---- Memoria ----
bool safe_read(uintptr_t address, void* out, size_t size) {
    SIZE_T read = 0;
    return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(address), out, size, &read) && read == size;
}

template <typename T>
T read_as(uintptr_t address, T fallback = T{}) {
    T value = fallback;
    safe_read(address, &value, sizeof(T));
    return value;
}

bool patch_code(uintptr_t address, const void* bytes, size_t size) {
    DWORD old = 0;
    if (!VirtualProtect(reinterpret_cast<void*>(address), size, PAGE_EXECUTE_READWRITE, &old)) return false;
    memcpy(reinterpret_cast<void*>(address), bytes, size);
    VirtualProtect(reinterpret_cast<void*>(address), size, old, &old);
    FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(address), size);
    return true;
}

// ---- MCC Steam 1.3528 ----
constexpr uint32_t k_mcc_timestamp = 0x68A0E5D7;  // cabecera PE de MCC-Win64-Shipping.exe 1.3528
constexpr uintptr_t k_game_manager_rva = 0x3F7B190;    // el gestor de juego (objeto: su tabla está en +0)
constexpr uintptr_t k_device_manager_rva = 0x4001B78;  // puntero al gestor de mandos
constexpr uintptr_t k_delta_time_rva = 0x5A16AC;
constexpr uintptr_t k_index_by_xuid_rva = 0x38A09C;
constexpr uint8_t k_delta_time_bytes[] = {0x40, 0x53, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x8B, 0xD9};
constexpr uint8_t k_index_by_xuid_bytes[] = {0x48, 0x8B, 0xC4, 0x55, 0x57, 0x41, 0x54, 0x41, 0x56, 0x41, 0x57};

// Posiciones en la tabla del gestor de juego.
constexpr size_t k_get_player_profile = 0x110, k_get_key_state = 0x120, k_set_vibration = 0x138,
                 k_get_xbox_user_id = 0x2C0, k_get_gamepad_mapping = 0x3A0;

// Gestor de mandos: +0x00 tabla (+0x38 update_state), +0x08 dispositivos[5] (4 mandos + teclado/mouse),
// +0xC8 último QueryPerformanceCounter. Dispositivo: +0x00 tabla (+0x38 set_state, +0x40 check), +0x808 slot XInput,
// +0x91C XINPUT_STATE.
constexpr size_t k_dm_devices = 0x08, k_dm_qpc = 0xC8, k_dm_update_state = 0x38;
constexpr size_t k_dev_set_state = 0x38, k_dev_check = 0x40, k_dev_slot = 0x808, k_dev_state = 0x91C;
constexpr int k_keyboard_device = 4;
constexpr size_t k_input_size = 0x130;  // lo que llena get_key_state

uintptr_t g_mcc = 0;
uint64_t g_ids[k_players] = {};
float(__fastcall* g_delta_time)(int64_t) = nullptr;

using ProfileFn = void*(__fastcall*)(void*, int64_t);
using KeyStateFn = bool(__fastcall*)(void*, DWORD, void*);
using VibrationFn = void(__fastcall*)(void*, DWORD, XINPUT_VIBRATION*);
using UserIdFn = bool(__fastcall*)(void*, int64_t*, wchar_t*, int, int);
using IndexFn = int64_t(__fastcall*)(void*, int64_t);
ProfileFn g_original_profile = nullptr, g_original_mapping = nullptr;
KeyStateFn g_original_key_state = nullptr;
VibrationFn g_original_vibration = nullptr;
UserIdFn g_original_user_id = nullptr;
IndexFn g_original_index = nullptr;

int index_of(int64_t xuid) {
    for (int i = 1; i < k_players; ++i)
        if (uint64_t(xuid) == g_ids[i]) return i;
    return 0;
}

int64_t host_xuid(void* manager) {
    int64_t id = 0;
    return g_original_user_id(manager, &id, nullptr, 0, 0) ? id : 0;
}

// J1 = teclado y mouse; J2-J4 = mandos 0-2.
uintptr_t device_of(int player) {
    uintptr_t manager = read_as<uintptr_t>(g_mcc + k_device_manager_rva);
    if (!manager) return 0;
    int device = player == 0 ? k_keyboard_device : player - 1;
    return read_as<uintptr_t>(manager + k_dm_devices + device * 8);
}

bool __fastcall hooked_user_id(void* manager, int64_t* id, wchar_t* name, int size, int index) {
    if (index == 0) return g_original_user_id(manager, id, name, size, index);
    if (index < 0 || index >= k_players) return false;
    if (id) *id = int64_t(g_ids[index]);
    if (name && size >= 2) {
        proto::VideoConfig config;
        wchar_t fallback[16];
        swprintf(fallback, 16, L"Player %d", index + 1);
        int written = 0;
        if (video_config(config) && config.names[index][0])
            written = MultiByteToWideChar(CP_UTF8, 0, config.names[index], -1, name, size / 2);
        if (written <= 0) wcsncpy_s(name, size / 2, fallback, _TRUNCATE);
    }
    return true;
}

bool __fastcall hooked_key_state(void* manager, DWORD index, void* input) {
    if (!input) return false;
    memset(input, 0, k_input_size);
    if (index >= DWORD(k_players)) return false;
    uintptr_t device = device_of(int(index));
    if (!device) return true;  // ese mando no está registrado: sin controles
    float delta = 0;
    uintptr_t methods = read_as<uintptr_t>(device);
    if (index == 0) {
        uintptr_t dm = read_as<uintptr_t>(g_mcc + k_device_manager_rva);
        auto update_state = reinterpret_cast<void(__fastcall*)(uintptr_t, int64_t, int64_t, bool)>(
            read_as<uintptr_t>(read_as<uintptr_t>(dm) + k_dm_update_state));
        update_state(dm, 0, 0, false);
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        auto* last = reinterpret_cast<LARGE_INTEGER*>(dm + k_dm_qpc);
        int64_t a = now.QuadPart - last->QuadPart, b = last->QuadPart - now.QuadPart;
        delta = fminf(fmaxf(g_delta_time(a >= b ? b : a) * 1000.0f, 0.1f), 1000.0f);  // igual que AlphaRing
        *last = now;
    } else {
        auto* state = reinterpret_cast<XINPUT_STATE*>(device + k_dev_state);
        memset(state, 0, sizeof(*state));
        hooked_get_state(read_as<DWORD>(device + k_dev_slot), state);
    }
    auto set_state = reinterpret_cast<bool(__fastcall*)(uintptr_t, float, void*)>(read_as<uintptr_t>(methods + k_dev_set_state));
    auto check = reinterpret_cast<void(__fastcall*)(uintptr_t)>(read_as<uintptr_t>(methods + k_dev_check));
    bool result = set_state(device, delta, input);
    check(device);
    return result;
}

// Perfil y botones: los cuatro usan los de J1 (como "Use player1's profile" de AlphaRing).
void* __fastcall hooked_profile(void* manager, int64_t) { return g_original_profile(manager, host_xuid(manager)); }
void* __fastcall hooked_mapping(void* manager, int64_t) { return g_original_mapping(manager, host_xuid(manager)); }

void __fastcall hooked_vibration(void*, DWORD index, XINPUT_VIBRATION* vibration) {
    if (index >= DWORD(k_players) || !vibration) return;
    if (uintptr_t device = device_of(int(index)); device && index > 0)
        hooked_set_state(read_as<DWORD>(device + k_dev_slot), vibration);
}

int64_t __fastcall hooked_index(void*, int64_t xuid) { return index_of(xuid); }

bool in_image(uintptr_t address) {
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(g_mcc);
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(g_mcc + dos->e_lfanew);
    return address >= g_mcc && address < g_mcc + nt->OptionalHeader.SizeOfImage;
}

bool hook_at(uintptr_t target, void* detour, void** original, const char* name) {
    MH_STATUS status = MH_CreateHook(reinterpret_cast<void*>(target), detour, original);
    if (status == MH_OK) status = MH_EnableHook(reinterpret_cast<void*>(target));
    if (status != MH_OK) log_write("Jugadores: no se pudo desviar %s (%s)", name, MH_StatusToString(status));
    return status == MH_OK;
}

bool hook_mcc() {
    g_mcc = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(g_mcc);
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(g_mcc + dos->e_lfanew);
    if (nt->FileHeader.TimeDateStamp != k_mcc_timestamp) {
        log_write("Jugadores: esta versión de MCC no es la 1.3528 de Steam: sin 4 jugadores propios");
        return false;
    }
    uint8_t bytes[16];
    if (!safe_read(g_mcc + k_delta_time_rva, bytes, sizeof(k_delta_time_bytes)) ||
        memcmp(bytes, k_delta_time_bytes, sizeof(k_delta_time_bytes)) != 0 ||
        !safe_read(g_mcc + k_index_by_xuid_rva, bytes, sizeof(k_index_by_xuid_bytes)) ||
        memcmp(bytes, k_index_by_xuid_bytes, sizeof(k_index_by_xuid_bytes)) != 0) {
        log_write("Jugadores: el código de MCC no es el esperado: sin 4 jugadores propios");
        return false;
    }
    g_delta_time = reinterpret_cast<decltype(g_delta_time)>(g_mcc + k_delta_time_rva);
    // El gestor de juego se construye al arrancar MCC (después de cargar este DLL): esperar su tabla.
    uintptr_t table = 0;
    for (int tries = 0; tries < 600 && !(table && in_image(table)); ++tries) {
        Sleep(100);
        table = read_as<uintptr_t>(g_mcc + k_game_manager_rva);
    }
    if (!table || !in_image(table)) {
        log_write("Jugadores: no apareció el gestor de juego de MCC");
        return false;
    }
    uintptr_t functions[5];
    const size_t offsets[5] = {k_get_player_profile, k_get_key_state, k_set_vibration, k_get_xbox_user_id,
                               k_get_gamepad_mapping};
    for (int i = 0; i < 5; ++i) {
        functions[i] = read_as<uintptr_t>(table + offsets[i]);
        if (!in_image(functions[i])) {
            log_write("Jugadores: la tabla del gestor de juego no es la esperada");
            return false;
        }
    }
    GUID guid;
    CoCreateGuid(&guid);
    uint64_t base;
    memcpy(&base, &guid, 8);
    base ^= *reinterpret_cast<uint64_t*>(reinterpret_cast<uint8_t*>(&guid) + 8);
    for (int i = 0; i < k_players; ++i) g_ids[i] = base + i;
    bool ok = hook_at(functions[0], &hooked_profile, reinterpret_cast<void**>(&g_original_profile), "el perfil") &&
              hook_at(functions[1], &hooked_key_state, reinterpret_cast<void**>(&g_original_key_state),
                      "los controles") &&
              hook_at(functions[2], &hooked_vibration, reinterpret_cast<void**>(&g_original_vibration),
                      "la vibración") &&
              hook_at(functions[3], &hooked_user_id, reinterpret_cast<void**>(&g_original_user_id),
                      "los usuarios") &&
              hook_at(functions[4], &hooked_mapping, reinterpret_cast<void**>(&g_original_mapping), "los botones") &&
              hook_at(g_mcc + k_index_by_xuid_rva, &hooked_index, reinterpret_cast<void**>(&g_original_index),
                      "el índice por xuid");
    if (ok) log_write("Jugadores: MCC con 4 jugadores locales (sin AlphaRing)");
    return ok;
}

// ---- halo1.dll ----
struct Patch {
    const char* name;
    uintptr_t rva;
    uint8_t original[6], patched[6];
    size_t size;
};
constexpr Patch k_halo1_patches[] = {
    {"más de 2 jugadores", 0x67492, {0x75, 0x11}, {0xEB, 0x18}, 2},
    {"pausa", 0x427978, {0x77}, {0xEB}, 1},
    {"congelamiento al terminar el nivel", 0x85428, {0x0F, 0x84, 0x15, 0x01, 0x00, 0x00},
     {0x90, 0x90, 0x90, 0x90, 0x90, 0x90}, 6},
};

void patch_halo1(uintptr_t halo1) {
    // Primero se comprueban todos: o se aplican los tres, o ninguno.
    for (const Patch& patch : k_halo1_patches) {
        uint8_t bytes[6];
        if (!safe_read(halo1 + patch.rva, bytes, patch.size)) return;
        if (memcmp(bytes, patch.patched, patch.size) == 0) continue;  // ya puesto
        if (memcmp(bytes, patch.original, patch.size) != 0) {
            log_write("Jugadores: halo1.dll no es el esperado (%s): sin 4 jugadores", patch.name);
            return;
        }
    }
    for (const Patch& patch : k_halo1_patches)
        if (!patch_code(halo1 + patch.rva, patch.patched, patch.size))
            log_write("Jugadores: no se pudo cambiar halo1.dll (%s)", patch.name);
    log_write("Jugadores: halo1.dll listo para 4 jugadores");
}

// halo1.dll se carga al elegir Halo CE en el menú y se descarga al salir: se vigila para parchear cada carga
// antes de que empiece la misión.
void watch_halo1() {
    uintptr_t patched = 0;
    while (true) {
        uintptr_t halo1 = reinterpret_cast<uintptr_t>(GetModuleHandleW(L"halo1.dll"));
        if (halo1 && halo1 != patched) {
            patch_halo1(halo1);
            patched = halo1;
        } else if (!halo1) {
            patched = 0;
        }
        Sleep(20);
    }
}

bool alpharing_loaded() {
    HMODULE wtsapi = GetModuleHandleW(L"WTSAPI32.dll");
    if (!wtsapi) return false;
    wchar_t path[MAX_PATH] = {}, exe[MAX_PATH] = {};
    GetModuleFileNameW(wtsapi, path, MAX_PATH);
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    const wchar_t* slash = wcsrchr(exe, L'\\');
    size_t folder = slash ? size_t(slash - exe) + 1 : 0;
    return folder && _wcsnicmp(path, exe, folder) == 0;  // el de System32 es el de Windows
}

bool direct_mode() {
    HMODULE self = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&direct_mode), &self);
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(self, path, MAX_PATH);
    if (wchar_t* slash = wcsrchr(path, L'\\')) wcscpy_s(slash + 1, MAX_PATH - (slash + 1 - path), L"coopevolved-juego.ini");
    wchar_t mode[16] = {};
    GetPrivateProfileStringW(L"juego", L"mandos", L"direct", mode, 16, path);
    return _wcsicmp(mode, L"vigem") != 0;
}

bool is_mcc() {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    const wchar_t* name = wcsrchr(path, L'\\');
    return _wcsicmp(name ? name + 1 : path, L"MCC-Win64-Shipping.exe") == 0;
}

}  // namespace

void local_players_set_pads(const proto::Pads& pads) {
    std::lock_guard lock(g_pads_mutex);
    for (int i = 0; i < k_remote; ++i) {
        if (memcmp(&g_pads[i], &pads.pads[i], sizeof(proto::GamepadState)) != 0) ++g_packet[i];
        g_pads[i] = pads.pads[i];
    }
}

void local_players_start() {
    if (!is_mcc()) return;
    if (!direct_mode()) {
        log_write("Jugadores: modo ViGEmBus + AlphaRing (coopevolved-juego.ini)");
        return;
    }
    MH_STATUS status = MH_Initialize();
    if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED) return;
    if (!hook_xinput()) return;
    log_write("Jugadores: mandos de J2-J4 por red (sin ViGEmBus)");
    if (alpharing_loaded()) {
        log_write("Jugadores: AlphaRing está instalado: él pone los 4 jugadores");
        return;
    }
    std::thread([] {
        if (hook_mcc()) watch_halo1();
    }).detach();
}
