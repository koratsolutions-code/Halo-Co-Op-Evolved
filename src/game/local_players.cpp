#include "local_players.h"
#include "coop_follow.h"
#include "halo1_load.h"
#include "log.h"
#include "video.h"

#include <windows.h>
#include <xinput.h>

#include <MinHook.h>

#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>

#include <mutex>
#include <string>
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

bool safe_write(uintptr_t address, const void* data, size_t size) {
    SIZE_T written = 0;
    return WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(address), data, size, &written) &&
           written == size;
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
// Cuántos jugadores locales tiene la partida. MCC pregunta por los usuarios solo al cargar una misión (al
// empezarla, reiniciarla o pasar de nivel), así que se decide en ese momento: J1 más hasta el amigo conectado con
// el número más alto (cada uno conserva su número: si están J2 y J4, son 4 y J3 queda quieto). Sin el host
// abierto, 4 como antes.
std::atomic<int> g_player_count{k_players};
std::atomic<bool> g_block_host{false};  // menú F2 del host abierto

int connected_players() {
    proto::VideoConfig config;
    if (!video_config(config)) return k_players;
    int count = 1;
    for (int i = 0; i < k_players - 1; ++i)
        if (config.targets[i].active) count = i + 2;
    return count;
}
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
    // MCC solo pregunta al cargar una misión: la cantidad que se responde ahí es la de la misión.
    int count = connected_players();
    if (count != g_player_count.exchange(count))
        log_write("Jugadores: la misión carga con %d jugadores (J1 y los amigos conectados)", count);
    if (index == 0) return g_original_user_id(manager, id, name, size, index);
    if (index < 0 || index >= g_player_count) return false;
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

void reload_if_requested();

bool __fastcall hooked_key_state(void* manager, DWORD index, void* input) {
    if (index == 0) {
        reload_if_requested();    // menú F2 del host: "Recargar la misión"
        coop_follow_game_tick();  // J3/J4 que acaban de aparecer: junto a su compañero
    }
    if (!input) return false;
    memset(input, 0, k_input_size);
    if (index >= DWORD(g_player_count)) return false;
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
        delta = fminf(fmaxf(g_delta_time(a >= b ? b : a) * 1000.0f, 0.1f), 1000.0f);  // ms entre lecturas, acotado
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
    if (index == 0 && g_block_host) memset(input, 0, k_input_size);  // menú F2 abierto: el jugador 1 se queda quieto
    return result;
}

// Perfil: los cuatro usan el de J1 (los usuarios falsos de J2-J4 no tienen perfil propio en MCC).
void* __fastcall hooked_profile(void* manager, int64_t) { return g_original_profile(manager, host_xuid(manager)); }

// Botones de J2-J4. MCC guarda, por acción, qué botón del mando la hace (s_gamepad_mapping de libmcc: 66 bytes, un
// botón abstracto por acción; 0xFF = ninguno). Los de J1 son su configuración de control de MCC, que puede ser
// cualquiera (por ejemplo X = linterna). J2-J4 juegan con el programa de los amigos, que convierte teclas en botones
// fijos (R -> X, E -> RB, F -> B...: src/client/input.cpp): se parte de la tabla de J1 y se fijan las acciones que
// usa ese programa, así R siempre recarga y E siempre es acción, como el teclado de J1.
constexpr size_t k_mapping_size = 0x42;
enum : uint8_t {  // botones abstractos (e_abstract_gamepad_button)
    k_left_trigger = 0, k_right_trigger = 1, k_dpad_up = 2, k_select = 7, k_left_stick = 8, k_right_stick = 9,
    k_left_bumper = 10, k_right_bumper = 11, k_button_a = 12, k_button_b = 13, k_button_x = 14, k_button_y = 15,
};
struct ActionButton {
    uint8_t action;  // e_game_abstract_button
    uint8_t button;
};
constexpr ActionButton k_friend_buttons[] = {
    {0, k_button_a},          // saltar: Espacio
    {1, k_left_bumper},       // cambiar granada: G
    {2, k_right_bumper},      // acción (vehículos, recoger): E
    {3, k_button_x},          // recargar: R
    {4, k_button_y},          // cambiar arma: Q / rueda
    {5, k_button_b},          // golpe: F / V
    {6, k_dpad_up},           // linterna: 1 / flecha arriba
    {7, k_left_trigger},      // lanzar granada: clic derecho
    {8, k_right_trigger},     // disparar: clic izquierdo
    {9, k_left_stick},        // agacharse: Ctrl / C
    {10, k_right_stick},      // zoom: Z / clic central
    {20, k_select},           // marcador: Tab
    {64, k_dpad_up},          // linterna (alternativa)
};
uint8_t g_friend_mapping[k_mapping_size];

void* __fastcall hooked_mapping(void* manager, int64_t xuid) {
    void* host = g_original_mapping(manager, host_xuid(manager));
    if (index_of(xuid) == 0 || !host) return host;  // J1: su propia configuración
    uint8_t mapping[k_mapping_size];
    if (!safe_read(reinterpret_cast<uintptr_t>(host), mapping, sizeof(mapping))) return host;
    for (uint8_t button : mapping)
        if (button > k_button_y && button != 0xFF) {  // no parece la tabla esperada: se deja la de J1
            static bool logged = false;
            if (!logged) log_write("Jugadores: la tabla de botones de MCC no es la esperada: J2-J4 usan la de J1");
            logged = true;
            return host;
        }
    for (const ActionButton& entry : k_friend_buttons) {
        // Si ese botón hacía otra acción en la tabla de J1, se le quita (que R no recargue y prenda la linterna).
        for (uint8_t& button : mapping)
            if (button == entry.button) button = 0xFF;
    }
    for (const ActionButton& entry : k_friend_buttons) mapping[entry.action] = entry.button;
    if (memcmp(mapping, g_friend_mapping, sizeof(mapping)) != 0) {
        memcpy(g_friend_mapping, mapping, sizeof(mapping));
        log_write("Jugadores: botones de J2-J4 iguales a las teclas del programa de los amigos");
    }
    return g_friend_mapping;
}

void __fastcall hooked_vibration(void*, DWORD index, XINPUT_VIBRATION* vibration) {
    if (index >= DWORD(g_player_count) || !vibration) return;
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

// Al cerrar MCC, un hilo de MCC todavía manda un aviso (evento 0x40F35C79, desde +0x2B3E50) a su
// gestor de eventos ([MCC+0x4004230]) cuando ese gestor ya se borró: la función que lo recibe (+0x8AD5E0) lee
// [nullptr+0x540] y MCC se cierra con el mensaje de error. Sin gestor el aviso no tiene a quién llegar: se ignora.
constexpr uintptr_t k_post_event_rva = 0x8AD5E0;
constexpr uint8_t k_post_event_bytes[] = {0x48, 0x8B, 0xC4, 0x48, 0x89, 0x58, 0x08, 0x48,
                                          0x89, 0x68, 0x10, 0x48, 0x89, 0x70, 0x18, 0x57};
using PostEventFn = void*(__fastcall*)(void*, uint32_t, uint32_t);
PostEventFn g_post_event = nullptr;

void* __fastcall hooked_post_event(void* manager, uint32_t event, uint32_t argument) {
    if (!manager) return nullptr;
    return g_post_event(manager, event, argument);
}

void hook_shutdown_event() {
    uint8_t bytes[sizeof(k_post_event_bytes)];
    if (!safe_read(g_mcc + k_post_event_rva, bytes, sizeof(bytes)) || memcmp(bytes, k_post_event_bytes, sizeof(bytes)) != 0) {
        log_write("Jugadores: el código de avisos de MCC no es el esperado: puede fallar al cerrar");
        return;
    }
    hook_at(g_mcc + k_post_event_rva, reinterpret_cast<void*>(&hooked_post_event),
            reinterpret_cast<void**>(&g_post_event), "avisos al cerrar");
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
    hook_shutdown_event();
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
    if (ok) log_write("Jugadores: MCC con hasta 4 jugadores locales");
    return ok;
}

// ---- halo1.dll ----
struct Patch {
    const char* name;
    uintptr_t rva;
    uint8_t original[8], patched[8];
    size_t size;
};
// Instrucciones de halo1.dll que se cambian (cada una se comprueba antes con sus bytes originales):
// - Más de 2 jugadores: la función que cuenta los usuarios al cargar la misión (halo1+0x66DF0), después de contarlos
//   (+0x67464) compara la cantidad con 1, 2 o 4 según dos banderas del modo de juego (+0x2E3B822 y +0x2E3B823) y, si
//   sobran, deja el código de salida 4 en +0x2E3B9B0 y levanta +0x2EA32E9: MCC vuelve al menú. Con 3 o 4 jugadores
//   locales en la campaña eso pasa siempre, así que se salta directo a guardar la cantidad (+0x674AC):
//   "cmp byte ptr [...],0" (7 bytes) -> "jmp +0x46" y relleno.
// - Pausa solo para J1: el menú de pausa de MCC busca, entre los jugadores locales 0-3, el primero que pidió pausa
//   (bucle en +0x427960, una entrada de 0x60 bytes por jugador desde +0x2EA2899). Solo sabe abrirse para el usuario
//   principal, así que se mira solo el jugador 0: si él no la pidió, se sale del bucle con "nadie" (edx = -1):
//   "mov r8d,edx" (+0x42796E) -> "jmp +0x22" (a +0x427992, donde se guarda el resultado).
// - Paso al siguiente nivel: al ganar un nivel halo1 pone en 0 su bandera "nivel en
//   curso" (+0x2E3B825) y le pasa a MCC la siguiente misión. Jugando solo MCC la pide y, al cargarla, la bandera vuelve
//   a 1; con más de un jugador local MCC no responde, y como el bucle principal solo avanza la partida con la bandera
//   en 1 (+0x85421), el juego se congela. Se quita esa condición para que el bucle avance siempre:
//   "je +0x115" (+0x85428, 6 bytes) -> nops.
// - Reaparecer en combate: en la campaña cooperativa un jugador muerto reaparece junto a un compañero ~3 s después
//   (contador global de ticks, +0xAC5184), siempre que la función de reaparición (+0xAD03C0, un solo llamador) lo
//   permita. Esa función rechaza si hay proyectiles peligrosos o unidades peligrosas cerca (motivo 1) o si enemigos
//   ven a un jugador (motivo 2, "Waiting to respawn...teammate in combat"), salvo que la bandera 0x08 de
//   jugadores+0x7E diga que no se revise. Se salta esa revisión como si la bandera estuviera puesta: "jne +0x17"
//   (+0xAD0402) -> "jmp +0x66" (a +0xAD046A, donde elige al compañero; ahí se recargan todos los registros que usa).
//   Sigue la revisión del motivo 3 (no aparecer junto a un compañero en el aire o en un vehículo en marcha). Las
//   funciones de "peligro" no se tocan: también las usa el guardado de checkpoints.
constexpr Patch k_halo1_patches[] = {
    {"más de 2 jugadores", 0x67464, {0x80, 0x3D, 0xB7, 0x43, 0xDD, 0x02, 0x00},
     {0xEB, 0x46, 0x90, 0x90, 0x90, 0x90, 0x90}, 7},
    {"pausa solo para el jugador 1", 0x42796E, {0x44, 0x8B, 0xC2}, {0xEB, 0x22, 0x90}, 3},
    {"paso al siguiente nivel", 0x85428, {0x0F, 0x84, 0x15, 0x01, 0x00, 0x00},
     {0x90, 0x90, 0x90, 0x90, 0x90, 0x90}, 6},
    {"reaparecer en combate", 0xAD0402, {0x75, 0x17}, {0xEB, 0x66}, 2},
};

void patch_halo1(uintptr_t halo1) {
    // Primero se comprueban todos: o se aplican todos, o ninguno. Si ya están puestos, nada.
    int missing = 0;
    for (const Patch& patch : k_halo1_patches) {
        uint8_t bytes[8];
        if (!safe_read(halo1 + patch.rva, bytes, patch.size)) return;
        if (memcmp(bytes, patch.patched, patch.size) == 0) continue;  // ya puesto
        if (memcmp(bytes, patch.original, patch.size) != 0) {
            static bool logged = false;
            if (!logged) log_write("Jugadores: halo1.dll no es el esperado (%s): sin 4 jugadores", patch.name);
            logged = true;
            return;
        }
        ++missing;
    }
    if (!missing) return;
    for (const Patch& patch : k_halo1_patches)
        if (!patch_code(halo1 + patch.rva, patch.patched, patch.size))
            log_write("Jugadores: no se pudo cambiar halo1.dll (%s)", patch.name);
    log_write("Jugadores: halo1.dll listo para 4 jugadores");
}

// ---- Entrar con la misión empezada ----
// halo1.dll cuenta los jugadores locales solo en la carga completa de una misión: la orden LoadLevelSolo(mapa,
// dificultad, isNew, fillStat) de su capa de órdenes (halo1+0x66AF0: rcx = objeto de órdenes, rdx = puntero al texto
// del mapa, r8d = dificultad, r9b y el quinto argumento = las dos opciones). Esa función pregunta por los usuarios 0-3
// y guarda la cantidad en el int16 halo1+0x1B7B910. "Reiniciar misión" de la pausa (RestartLevel) no vuelve a
// contar. Por eso se guardan los argumentos de la última carga y, cuando el host lo pide, se repite la misma carga
// desde el hilo del juego (el desvío de los controles de J1, que el juego llama en cada cuadro): la misión empieza
// de nuevo con los conectados. Como la cantidad de jugadores cambia, MCC hace enseguida su propia carga (isNew = 1)
// y maneja su pantalla de carga. Si la cantidad no cambia, MCC no la hace y su pantalla de carga queda encima para
// siempre (probado también con isNew = 1): por eso solo se recarga si hay amigos esperando. Hacerla dentro del
// reinicio de la pausa de MCC también deja la pantalla de carga encima.
// Texto de halo1.dll: contador de referencias (int), largo, capacidad y los caracteres en +12; halo1.dll descuenta
// una referencia al terminar, así que el nuestro empieza con un contador alto y nunca se libera.
constexpr uintptr_t k_load_level_solo_rva = 0x66AF0, k_mission_players_rva = 0x1B7B910;
constexpr uint8_t k_load_level_solo_bytes[] = {0x40, 0x55, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56};

using LoadLevelFn = void(__fastcall*)(void*, void**, int, uint8_t, uint8_t);
LoadLevelFn g_original_load_level = nullptr;
std::atomic<uintptr_t> g_halo1{0};

struct LastLoad {
    std::atomic<bool> valid{false};
    void* commands = nullptr;
    alignas(8) uint8_t text[12 + 256] = {};
    void* text_pointer = nullptr;
    int difficulty = 0;
    uint8_t is_new = 0, fill = 0;
} g_last_load;
std::atomic<bool> g_reload_requested{false};

void __fastcall hooked_load_level(void* commands, void** text, int difficulty, uint8_t is_new, uint8_t fill) {
    int header[3] = {};
    // Mientras se repite la carga se llama con nuestro propio texto: no se vuelve a copiar.
    if (text && *text && *text != g_last_load.text &&
        safe_read(reinterpret_cast<uintptr_t>(*text), header, sizeof(header)) && header[1] > 0 && header[1] < 255) {
        LastLoad& last = g_last_load;
        last.valid = false;
        memset(last.text, 0, sizeof(last.text));
        const int ours[3] = {1 << 24, header[1], header[1]};
        memcpy(last.text, ours, sizeof(ours));
        if (safe_read(reinterpret_cast<uintptr_t>(*text) + 12, last.text + 12, size_t(header[1]))) {
            last.text_pointer = last.text;
            last.commands = commands;
            last.difficulty = difficulty;
            last.is_new = is_new;
            last.fill = fill;
            last.valid = true;
        }
    }
    g_original_load_level(commands, text, difficulty, is_new, fill);
}

// Cada carga de halo1.dll trae su código original: se quita el desvío de la anterior y se pone otra vez.
void hook_load_level(uintptr_t halo1) {
    uint8_t bytes[sizeof(k_load_level_solo_bytes)];
    if (!safe_read(halo1 + k_load_level_solo_rva, bytes, sizeof(bytes)) ||
        memcmp(bytes, k_load_level_solo_bytes, sizeof(bytes)) != 0)
        return;  // ya desviada (o no es el halo1.dll esperado)
    g_last_load.valid = false;
    auto* target = reinterpret_cast<void*>(halo1 + k_load_level_solo_rva);
    MH_RemoveHook(target);
    MH_STATUS status = MH_CreateHook(target, reinterpret_cast<void*>(&hooked_load_level),
                                     reinterpret_cast<void**>(&g_original_load_level));
    if (status == MH_OK) status = MH_EnableHook(target);
    if (status != MH_OK) log_write("Jugadores: no se pudo desviar la carga de misión (%s)", MH_StatusToString(status));
}

int mission_players() {
    uintptr_t halo1 = g_halo1;
    return halo1 ? read_as<int16_t>(halo1 + k_mission_players_rva, -1) : -1;
}

// ---- Paso a la siguiente misión con 2+ jugadores ----
// halo1.dll guarda estados con nombre en un gestor (dsSTATE_MGR); uno es "IsMP" (¿partida multijugador?). Con 2+
// jugadores locales queda en verdadero y, al terminar un nivel, el paso al siguiente toma el camino de multijugador:
// MCC pone su pantalla de carga y no la quita. Por eso, cada vez que se lee "IsMP" se devuelve falso.
// GetState (+0x18B150): rcx = gestor, edx = id del estado; devuelve un dato {uint32 valor; const tipo*}.
// RegisterState (+0x5DB40): rcx = gestor, rdx = nombre, r8b = avisar al cambiar; devuelve el id (el mismo si ya existe).
constexpr uintptr_t k_get_state_rva = 0x18B150, k_register_state_rva = 0x5DB40;
constexpr uint8_t k_get_state_bytes[] = {0x48, 0x83, 0xEC, 0x28, 0x8B, 0xC2, 0x4C, 0x8B, 0xC9, 0x99};

struct StateData {
    uint32_t value;
    const void* type;
};
using HaloGetStateFn = StateData*(__fastcall*)(void*, int);
using RegisterStateFn = int(__fastcall*)(void*, const char*, bool);
HaloGetStateFn g_original_halo_state = nullptr;
std::atomic<void*> g_state_manager{nullptr};
std::atomic<int> g_is_mp_id{-1};
std::atomic<bool> g_is_mp_logged{false};

StateData* __fastcall hooked_halo_state(void* manager, int id) {
    StateData* result = g_original_halo_state(manager, id);
    uintptr_t halo1 = g_halo1;
    if (!halo1 || !result) return result;
    if (manager != g_state_manager.load()) {  // gestor nuevo (otra carga de halo1.dll): se vuelve a pedir el id
        g_is_mp_id = reinterpret_cast<RegisterStateFn>(halo1 + k_register_state_rva)(manager, "IsMP", true);
        g_state_manager = manager;
        g_is_mp_logged = false;
    }
    if (id == g_is_mp_id && result->value) {
        if (!g_is_mp_logged.exchange(true))
            log_write("Jugadores: Halo se creía en multijugador (IsMP = %u): se deja en falso", result->value);
        result->value = 0;
    }
    return result;
}

void hook_get_state(uintptr_t halo1) {
    uint8_t bytes[sizeof(k_get_state_bytes)];
    if (!safe_read(halo1 + k_get_state_rva, bytes, sizeof(bytes)) ||
        memcmp(bytes, k_get_state_bytes, sizeof(bytes)) != 0)
        return;  // ya desviada (o no es el halo1.dll esperado)
    g_state_manager = nullptr;
    auto* target = reinterpret_cast<void*>(halo1 + k_get_state_rva);
    MH_RemoveHook(target);
    MH_STATUS status = MH_CreateHook(target, reinterpret_cast<void*>(&hooked_halo_state),
                                     reinterpret_cast<void**>(&g_original_halo_state));
    if (status == MH_OK) status = MH_EnableHook(target);
    if (status != MH_OK) log_write("Jugadores: no se pudo desviar los estados de Halo (%s)", MH_StatusToString(status));
}

// Desde el hilo del juego.
void reload_if_requested() {
    if (!g_reload_requested.exchange(false)) return;
    LastLoad& last = g_last_load;
    if (!last.valid || !g_original_load_level) return;
    log_write("Jugadores: recargando la misión con %d jugadores (tenía %d)", connected_players(), mission_players());
    g_original_load_level(last.commands, &last.text_pointer, last.difficulty, last.is_new, last.fill);
}

// halo1.dll se carga al elegir Halo CE en el menú y se vuelve a cargar al empezar cada misión (a veces en la misma
// dirección): se parchea cada carga apenas vuelve LoadLibraryExW, antes de que empiece la misión (ver halo1_load.h).
void on_halo1_loaded(uintptr_t halo1) {
    g_halo1 = halo1;
    patch_halo1(halo1);
    hook_load_level(halo1);
    hook_get_state(halo1);
}

// Solo sigue la dirección (para leer): si halo1.dll se descargó, g_halo1 = 0.
void watch_halo1() {
    halo1_load_subscribe(&on_halo1_loaded);
    while (true) {
        uintptr_t halo1 = reinterpret_cast<uintptr_t>(GetModuleHandleW(L"halo1.dll"));
        if (g_halo1.exchange(halo1) != halo1) g_last_load.valid = false;
        Sleep(20);
    }
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

void local_players_block_host(bool block) {
    g_block_host = block;
}

bool local_players_own_input() {
    return g_original_key_state != nullptr;
}

int local_players_waiting() {
    int mission = mission_players();
    if (!g_original_key_state || !g_last_load.valid || mission < 1 || mission > k_players) return 0;
    int connected = connected_players();
    return connected > mission ? connected - mission : 0;
}

void local_players_reload_mission() {
    if (local_players_waiting() > 0) g_reload_requested = true;
}

void local_players_start() {
    if (!is_mcc()) return;
    MH_STATUS status = MH_Initialize();
    if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED) return;
    halo1_load_start();  // lo antes posible: todos los desvíos de halo1.dll cuelgan de este aviso
    if (!hook_xinput()) return;
    log_write("Jugadores: mandos de J2-J4 por red");
    std::thread([] {
        if (hook_mcc()) watch_halo1();
    }).detach();
}
