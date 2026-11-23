#include "coop_follow.h"
#include "extra_vehicles.h"
#include "halo1_load.h"
#include "halo1_players.h"
#include "log.h"

#include <windows.h>

#include <MinHook.h>

#include <atomic>
#include <cmath>
#include <cstring>
#include <mutex>
#include <thread>

namespace {

// Funciones de halo1.dll (MCC Steam 1.3528). Los guiones de la misión (HaloScript) llaman a una función "evaluar"
// por cada comando; esa función lee los argumentos y llama a la del motor:
//   object_teleport <objeto> <punto de cinemática>: evaluar +0xB196D0 -> motor +0xC57B9C (ecx = objeto,
//     dx = punto, r8b = 1) y después +0xBA83EC (ecx = objeto).
//   unit_suspended <unidad> <bool>: evaluar +0xB1B3FC -> motor +0xB064B0 (ecx = unidad, dl = suspendida).
//   unit_enter_vehicle <unidad> <vehículo> <asiento>: evaluar +0xB1ABF0 -> motor +0xB062C4 (ecx = unidad,
//     edx = vehículo, r8 = nombre del asiento).
//   unit_exit_vehicle <unidad>: evaluar +0xB1AD84 -> motor +0xB06EC8 (ecx = unidad).
// Se desvían las del motor, pero solo se actúa cuando las llama un guion (marca puesta por la de evaluar): el motor
// también las usa por su cuenta (por ejemplo volume_teleport_players_not_inside ya mueve a todos los jugadores).
constexpr uintptr_t k_teleport_eval_rva = 0xB196D0, k_teleport_rva = 0xC57B9C, k_teleport_done_rva = 0xBA83EC;
constexpr uintptr_t k_suspend_eval_rva = 0xB1B3FC, k_suspend_rva = 0xB064B0;
constexpr uintptr_t k_enter_eval_rva = 0xB1ABF0, k_enter_rva = 0xB062C4;
constexpr uintptr_t k_exit_eval_rva = 0xB1AD84, k_exit_rva = 0xB06EC8;
constexpr uintptr_t k_player_count_rva = 0x1B7B910, k_current_map_rva = 0x2B23745;
constexpr uint8_t k_teleport_eval_bytes[] = {0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83, 0xEC, 0x20};
constexpr uint8_t k_teleport_bytes[] = {0x83, 0xF9, 0xFF, 0x0F, 0x84, 0x8A, 0x02, 0x00, 0x00};
constexpr uint8_t k_suspend_eval_bytes[] = {0x40, 0x53, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x0F, 0xBF, 0xC1};
constexpr uint8_t k_suspend_bytes[] = {0x83, 0xF9, 0xFF, 0x0F, 0x84, 0xD2, 0x00, 0x00, 0x00};
constexpr uint8_t k_enter_eval_bytes[] = {0x40, 0x53, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x0F, 0xBF, 0xC1};
constexpr uint8_t k_enter_bytes[] = {0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x6C, 0x24, 0x10};
constexpr uint8_t k_exit_eval_bytes[] = {0x40, 0x53, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x0F, 0xBF, 0xC1};
constexpr uint8_t k_exit_bytes[] = {0x48, 0x83, 0xEC, 0x28, 0x41, 0x83, 0xCB, 0xFF, 0x41, 0x3B, 0xCB};

// Asientos para el compañero en el Pelican, en orden de preferencia. En los mapas originales Master Chief solo tiene
// animaciones para los asientos de pasajero delanteros (P-riderLF y P-riderRF) y el juego no deja subir a un asiento
// sin animación: add_middle_seats le agrega los del medio (LM y RM) con las de los delanteros.
struct SeatPair {
    const char* leader;
    const char* followers[3];
};
constexpr SeatPair k_seat_pairs[] = {
    {"p-riderlf", {"p-riderlm", "p-riderrm", "p-riderrf"}},
    {"p-riderrf", {"p-riderrm", "p-riderlm", "p-riderlf"}},
};

const SeatPair* seat_pair(const char* leader_seat) {
    for (const SeatPair& pair : k_seat_pairs)
        if (_stricmp(leader_seat, pair.leader) == 0) return &pair;
    return nullptr;
}

constexpr uint32_t k_none = 0xFFFFFFFF;
constexpr size_t k_handle_offset = 0x64;  // registro del jugador: handle de su biped
constexpr size_t k_position_offset = 0x18;  // biped: posición (pies)
// Velocidad (3 floats). Los datos de objeto de MCC son los del Halo de PC corridos 0x44 (posición 0x5C -> 0x18,
// centro 0xA0 -> 0x5C, vehículo 0x11C -> 0xD8): la velocidad, 0x68 en PC, queda en 0x24.
constexpr size_t k_velocity_offset = 0x24;

using EvalFn = void(__fastcall*)(int16_t, int, uint8_t);
using TeleportFn = void(__fastcall*)(uint32_t, uint16_t, uint8_t);
using TeleportDoneFn = void(__fastcall*)(uint32_t);
using SuspendFn = void(__fastcall*)(uint32_t, uint8_t);
using EnterFn = void(__fastcall*)(uint32_t, uint32_t, const char*);
using ExitFn = void(__fastcall*)(uint32_t);

EvalFn g_original_teleport_eval = nullptr, g_original_suspend_eval = nullptr;
TeleportFn g_original_teleport = nullptr;
SuspendFn g_original_suspend = nullptr;
EvalFn g_original_enter_eval = nullptr;
EnterFn g_original_enter = nullptr;
EvalFn g_original_exit_eval = nullptr;
ExitFn g_original_exit = nullptr;
std::atomic<uintptr_t> g_halo1{0};
thread_local bool g_in_script = false;
bool g_held[4] = {};  // hilo del juego: J3/J4 congelados esperando que su compañero se baje (ver arrive)

bool safe_read(uintptr_t address, void* out, size_t size) {
    SIZE_T read = 0;
    return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(address), out, size, &read) && read == size;
}

bool safe_write(uintptr_t address, const void* data, size_t size) {
    SIZE_T written = 0;
    return WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(address), data, size, &written) &&
           written == size;
}

// Mapas de mods que ya traen a J3/J4: tienen 4 puntos de inicio y sus guiones ya mueven y suben a J3/J4. Ahí este
// módulo no hace nada, para no pisarlos.
// Datos de los tags: cabecera (+0x04 = ID del tag del escenario) y la lista de tags en +0x28 (ver skins.cpp).
constexpr uintptr_t k_tag_data_rva = 0x2B22F50;
constexpr uint32_t k_tag_base = 0x50000000, k_scnr = 0x73636E72;
constexpr size_t k_tag_entry_size = 0x20, k_player_starts_offset = 0x354;  // escenario: bloque de puntos de inicio
std::atomic<bool> g_map_has_four{false};

int scenario_player_starts(uintptr_t halo1) {
    uintptr_t data = 0;
    uint32_t scenario_id = 0, class_id = 0, pointer = 0;
    int32_t count = 0;
    if (!safe_read(halo1 + k_tag_data_rva, &data, sizeof(data)) || !data ||
        !safe_read(data + 4, &scenario_id, sizeof(scenario_id)))
        return -1;
    uintptr_t entry = data + 0x28 + (scenario_id & 0xFFFF) * k_tag_entry_size;
    if (!safe_read(entry, &class_id, sizeof(class_id)) || class_id != k_scnr ||
        !safe_read(entry + 0x14, &pointer, sizeof(pointer)) || pointer < k_tag_base ||
        !safe_read(data + (pointer - k_tag_base) + k_player_starts_offset, &count, sizeof(count)))
        return -1;
    return count;
}

int mission_players() {
    uintptr_t halo1 = g_halo1;
    int16_t count = 0;
    return halo1 && safe_read(halo1 + k_player_count_rva, &count, sizeof(count)) ? count : 0;
}

uint32_t biped_handle(int player) {
    uintptr_t record = halo1_player_record(player);
    uint32_t handle = k_none;
    if (record) safe_read(record + k_handle_offset, &handle, sizeof(handle));
    return handle;
}

// Quién sigue a quién: J3 (2) a J1 (0), J4 (3) a J2 (1). Con 3 jugadores no hay J4.
int follower_of(uint32_t object) {
    if (object == k_none || g_map_has_four) return -1;
    int players = mission_players();
    for (int leader = 0; leader < 2; ++leader) {
        int follower = leader + 2;
        if (follower < players && biped_handle(leader) == object) return follower;
    }
    return -1;
}

void __fastcall hooked_teleport_eval(int16_t function, int thread, uint8_t initialize) {
    g_in_script = true;
    g_original_teleport_eval(function, thread, initialize);
    g_in_script = false;
}

void __fastcall hooked_suspend_eval(int16_t function, int thread, uint8_t initialize) {
    g_in_script = true;
    g_original_suspend_eval(function, thread, initialize);
    g_in_script = false;
}

void __fastcall hooked_enter_eval(int16_t function, int thread, uint8_t initialize) {
    g_in_script = true;
    g_original_enter_eval(function, thread, initialize);
    g_in_script = false;
}

void __fastcall hooked_teleport(uint32_t object, uint16_t flag, uint8_t flags) {
    g_original_teleport(object, flag, flags);
    if (!g_in_script) return;
    int follower = follower_of(object);
    uint32_t handle = follower >= 0 ? biped_handle(follower) : k_none;
    if (handle == k_none) return;
    g_original_teleport(handle, flag, flags);
    if (uintptr_t halo1 = g_halo1) reinterpret_cast<TeleportDoneFn>(halo1 + k_teleport_done_rva)(handle);
    log_write("J3/J4: el guion movió a J%d al punto %u: J%d también", follower - 1, flag, follower + 1);
}

void __fastcall hooked_suspend(uint32_t unit, uint8_t suspended) {
    g_original_suspend(unit, suspended);
    if (!g_in_script) return;
    int follower = follower_of(unit);
    if (follower >= 0 && g_held[follower]) return;  // esperando en el aire: que no lo suelte un guion
    uint32_t handle = follower >= 0 ? biped_handle(follower) : k_none;
    if (handle != k_none) g_original_suspend(handle, suspended);
}

bool seat_beside(int follower, uint32_t vehicle, const char* leader_seat, const char** used);  // más abajo
constexpr size_t k_parent_offset = 0xD8;  // personaje: handle del vehículo que lo lleva (ver "Llegar con su compañero")

void __fastcall hooked_exit_eval(int16_t function, int thread, uint8_t initialize) {
    g_in_script = true;
    g_original_exit_eval(function, thread, initialize);
    g_in_script = false;
}

// Cuando un guion baja a J1/J2 (por ejemplo, al aterrizar el Pelican), J3/J4 se bajan también si van en el mismo
// vehículo: si no, se irían volando con él.
void __fastcall hooked_exit(uint32_t unit) {
    if (g_in_script) extra_vehicles_on_script_exit(g_halo1, unit);  // por ejemplo, la nave suelta el Warthog
    int follower = g_in_script ? follower_of(unit) : -1;
    uint32_t vehicle = k_none, theirs = k_none;
    uintptr_t leader_unit = follower >= 0 ? halo1_object(unit) : 0;
    uintptr_t follower_unit = follower >= 0 ? halo1_player_biped(follower) : 0;
    if (leader_unit) safe_read(leader_unit + k_parent_offset, &vehicle, sizeof(vehicle));
    if (follower_unit) safe_read(follower_unit + k_parent_offset, &theirs, sizeof(theirs));
    g_original_exit(unit);
    if (vehicle != k_none && theirs == vehicle) {
        g_original_exit(biped_handle(follower));
        log_write("J3/J4: el guion bajó a J%d del vehículo: J%d también", follower - 1, follower + 1);
    }
}

void __fastcall hooked_enter(uint32_t unit, uint32_t vehicle, const char* seat) {
    g_original_enter(unit, vehicle, seat);
    if (!g_in_script || !seat) return;
    int follower = follower_of(unit);
    if (follower < 0 || biped_handle(follower) == k_none) return;
    char name[32] = {};
    if (!safe_read(reinterpret_cast<uintptr_t>(seat), name, sizeof(name) - 1)) return;
    const char* used = nullptr;
    if (seat_beside(follower, vehicle, name, &used))
        log_write("J3/J4: el guion subió a J%d al asiento %s: J%d al %s", follower - 1, name, follower + 1, used);
}

// Crea el desvío (todavía sin activar: se activan todos juntos en hook_all, una sola pausa de los hilos del juego).
template <typename Fn, size_t N>
void hook(uintptr_t halo1, uintptr_t rva, const uint8_t (&bytes)[N], void* detour, Fn* original, const char* what) {
    uint8_t current[N];
    if (!safe_read(halo1 + rva, current, N) || memcmp(current, bytes, N) != 0) return;  // ya desviada
    auto* target = reinterpret_cast<void*>(halo1 + rva);
    MH_RemoveHook(target);  // el de la carga anterior: MinHook todavía cree que existe
    MH_STATUS status = MH_CreateHook(target, detour, reinterpret_cast<void**>(original));
    if (status == MH_OK) status = MH_QueueEnableHook(target);
    if (status != MH_OK) log_write("J3/J4: no se pudo desviar %s (%s)", what, MH_StatusToString(status));
}

std::mutex g_hook_mutex;

// Solo desde el aviso de carga de halo1.dll (halo1_load.h): nunca mientras se descarga.
void hook_all(uintptr_t halo1) {
    std::lock_guard lock(g_hook_mutex);
    g_halo1 = halo1;
    hook(halo1, k_teleport_eval_rva, k_teleport_eval_bytes, reinterpret_cast<void*>(&hooked_teleport_eval),
         &g_original_teleport_eval, "object_teleport (guion)");
    hook(halo1, k_teleport_rva, k_teleport_bytes, reinterpret_cast<void*>(&hooked_teleport), &g_original_teleport,
         "object_teleport");
    hook(halo1, k_suspend_eval_rva, k_suspend_eval_bytes, reinterpret_cast<void*>(&hooked_suspend_eval),
         &g_original_suspend_eval, "unit_suspended (guion)");
    hook(halo1, k_suspend_rva, k_suspend_bytes, reinterpret_cast<void*>(&hooked_suspend), &g_original_suspend,
         "unit_suspended");
    hook(halo1, k_enter_eval_rva, k_enter_eval_bytes, reinterpret_cast<void*>(&hooked_enter_eval),
         &g_original_enter_eval, "unit_enter_vehicle (guion)");
    hook(halo1, k_enter_rva, k_enter_bytes, reinterpret_cast<void*>(&hooked_enter), &g_original_enter,
         "unit_enter_vehicle");
    hook(halo1, k_exit_eval_rva, k_exit_eval_bytes, reinterpret_cast<void*>(&hooked_exit_eval),
         &g_original_exit_eval, "unit_exit_vehicle (guion)");
    hook(halo1, k_exit_rva, k_exit_bytes, reinterpret_cast<void*>(&hooked_exit), &g_original_exit,
         "unit_exit_vehicle");
    MH_STATUS status = MH_ApplyQueued();
    if (status != MH_OK) log_write("J3/J4: no se pudieron activar los desvíos (%s)", MH_StatusToString(status));
}

// Al empezar la misión los puntos de inicio del mapa son solo dos: J3 y J4 caen en cualquier lado. La primera vez que
// aparecen en cada mapa se ponen junto a J1 / J2, en un lugar donde su compañero estuvo parado hace poco: ahí cabe
// un personaje seguro. Un punto fijo a su costado puede caer dentro de una pared (por ejemplo, en la cápsula del
// principio de Halo) y lo deja atrapado.
struct Trail {
    uint32_t biped = k_none;
    float points[24][3] = {};  // del más nuevo al más viejo
    int count = 0;
} g_trail[2];  // hilo del juego: por dónde caminó J1 / J2 (pies), mientras van a pie

float flat_distance(const float a[3], const float b[3]) {
    return std::sqrt((a[0] - b[0]) * (a[0] - b[0]) + (a[1] - b[1]) * (a[1] - b[1]));
}

void record_trails(const Halo1Player players[4]) {
    for (int leader = 0; leader < 2; ++leader) {
        Trail& trail = g_trail[leader];
        uint32_t handle = players[leader].alive ? biped_handle(leader) : k_none;
        if (handle != trail.biped) trail = Trail{}, trail.biped = handle;  // otro personaje u otro mapa
        uintptr_t unit = handle != k_none ? halo1_player_biped(leader) : 0;
        uint32_t vehicle = k_none;
        if (!unit || !safe_read(unit + k_parent_offset, &vehicle, sizeof(vehicle)) || vehicle != k_none) continue;
        const float* here = players[leader].position;
        if (trail.count && flat_distance(trail.points[0], here) < 0.4f && std::fabs(trail.points[0][2] - here[2]) < 0.4f)
            continue;
        memmove(trail.points[1], trail.points[0], sizeof(trail.points) - sizeof(trail.points[0]));
        memcpy(trail.points[0], here, sizeof(trail.points[0]));
        if (trail.count < 24) ++trail.count;
    }
}

// El punto más reciente del camino que ya quedó libre: a más de 0,8 m de quien caminó por ahí y lejos de los demás.
// Primero el de su compañero; si él todavía no se movió, el del otro (J4 aparece junto a J1 si J2 sigue quieto).
bool trail_spot(int follower, int leader, const Halo1Player players[4], float out[3], int* who) {
    const int owners[2] = {leader, 1 - leader};
    for (int owner : owners) {
        if (!players[owner].alive) continue;
        const Trail& trail = g_trail[owner];
        for (int i = 0; i < trail.count; ++i) {
            const float* point = trail.points[i];
            float away = flat_distance(point, players[owner].position);
            if (away < 0.8f || away > 4.0f || std::fabs(point[2] - players[owner].position[2]) > 1.5f) continue;
            bool free = true;
            for (int other = 0; other < 4 && free; ++other)
                if (other != follower && players[other].alive)
                    free = flat_distance(point, players[other].position) >= 0.7f;
            if (!free) continue;
            memcpy(out, point, sizeof(float) * 3);
            *who = owner;
            return true;
        }
    }
    return false;
}

bool place_at(int follower, const float spot[3]) {
    uintptr_t biped = halo1_player_biped(follower);
    if (!biped) return false;
    float position[3] = {spot[0], spot[1], spot[2] + 0.05f};
    static const float k_still[3] = {};  // sin la velocidad que traía (por ejemplo, de una caída)
    safe_write(biped + k_velocity_offset, k_still, sizeof(k_still));
    return safe_write(biped + k_position_offset, position, sizeof(position));
}

// ---- Llegar con su compañero ----
// Si J1/J2 van en un vehículo cuando J3/J4 aparecen (por ejemplo, el Pelican del principio de The Silent
// Cartographer), ponerlos al costado los deja en el aire: caen y mueren. Entonces se los sienta en el asiento de al
// lado. Datos de un personaje (vistos en unit_enter_vehicle, motor +0xB062C4): +0xD8 = handle del vehículo que lo
// lleva (0xFFFFFFFF = ninguno), +0x2D0 = índice de su asiento (int16). Definición de un vehículo (tag): +0x2E4 =
// cantidad de asientos, +0x2E8 = puntero (de tags) a los asientos, de 0x11C bytes cada uno con el nombre en +4.
constexpr size_t k_seat_index_offset = 0x2D0;
constexpr size_t k_seat_count_offset = 0x2E4, k_seats_offset = 0x2E8, k_seat_size = 0x11C, k_seat_name_offset = 4;

std::atomic<int> g_arrive_leader[4] = {-1, -1, -1, -1};  // J3/J4 recién aparecidos: a quién seguir (-1: nada)

// Nombre del asiento que ocupa un personaje en su vehículo ("" si no va en uno).
bool seat_of(uintptr_t unit, uint32_t* vehicle, char name[32]) {
    uintptr_t halo1 = g_halo1;
    int16_t index = -1;
    uint32_t tag_id = 0, definition = 0, seats = 0;
    int32_t count = 0;
    uintptr_t data = 0;
    if (!safe_read(unit + k_parent_offset, vehicle, sizeof(*vehicle)) || *vehicle == k_none ||
        !safe_read(unit + k_seat_index_offset, &index, sizeof(index)) || index < 0)
        return false;
    uintptr_t object = halo1_object(*vehicle);
    if (!object || !safe_read(object, &tag_id, sizeof(tag_id)) || !safe_read(halo1 + k_tag_data_rva, &data, sizeof(data)))
        return false;
    uintptr_t entry = data + 0x28 + (tag_id & 0xFFFF) * k_tag_entry_size;
    if (!safe_read(entry + 0x14, &definition, sizeof(definition)) || definition < k_tag_base) return false;
    uintptr_t vehicle_tag = data + (definition - k_tag_base);
    if (!safe_read(vehicle_tag + k_seat_count_offset, &count, sizeof(count)) || index >= count ||
        !safe_read(vehicle_tag + k_seats_offset, &seats, sizeof(seats)) || seats < k_tag_base)
        return false;
    memset(name, 0, 32);
    return safe_read(data + (seats - k_tag_base) + index * k_seat_size + k_seat_name_offset, name, 31) && name[0];
}

// Sube al seguidor al vehículo de su compañero: primero a los asientos de al lado conocidos (Pelican), si no al mismo
// nombre de asiento. El juego no lo sube si el asiento está ocupado o no tiene animación: se revisa si quedó arriba.
bool seat_beside(int follower, uint32_t vehicle, const char* leader_seat, const char** used) {
    uint32_t handle = biped_handle(follower);
    uintptr_t unit = halo1_player_biped(follower);
    if (handle == k_none || !unit) return false;
    EnterFn enter = g_original_enter ? g_original_enter : reinterpret_cast<EnterFn>(g_halo1.load() + k_enter_rva);
    const SeatPair* pair = seat_pair(leader_seat);
    const char* same[3] = {leader_seat, nullptr, nullptr};
    for (const char* seat : pair ? pair->followers : same) {
        if (!seat) break;
        enter(handle, vehicle, seat);
        uint32_t now = k_none;
        if (safe_read(unit + k_parent_offset, &now, sizeof(now)) && now == vehicle) {
            *used = seat;
            return true;
        }
    }
    return false;
}

// ---- Asientos del medio del Pelican ----
// La lista de animaciones de Master Chief (tag antr characters\cyborg\cyborg) tiene una entrada por asiento ("units":
// en +0x0C cantidad y en +0x10 puntero de tags; entradas de 0x64 bytes con el nombre en +0). En los mapas originales
// solo hay P-riderLF y P-riderRF para el Pelican: se agregan P-riderLM y P-riderRM, copias de esas dos (las mismas
// animaciones, el asiento las ubica en su lugar). La lista nueva va en memoria propia al alcance de los punteros de
// tags de 32 bits (dirección = datos de tags + (puntero - 0x50000000)). Cada misión trae sus tags de nuevo: se
// revisa seguido y se vuelve a poner.
constexpr uint32_t k_antr = 0x616E7472;
constexpr size_t k_units_offset = 0x0C, k_unit_size = 0x64, k_max_units = 64;
constexpr char k_chief_animations[] = "characters\\cyborg\\cyborg";
uintptr_t g_units_buffer = 0, g_units_buffer_data = 0;  // memoria para la lista nueva y los datos de tags que sirve

uintptr_t units_buffer(uintptr_t data) {
    if (g_units_buffer && g_units_buffer_data == data) return g_units_buffer;
    for (uintptr_t offset = 0x08000000; offset < 0xA0000000; offset += 0x100000) {
        void* block = VirtualAlloc(reinterpret_cast<void*>(data + offset), 0x10000, MEM_RESERVE | MEM_COMMIT,
                                   PAGE_READWRITE);
        if (block) {
            g_units_buffer = reinterpret_cast<uintptr_t>(block);
            g_units_buffer_data = data;
            return g_units_buffer;
        }
    }
    return 0;
}

// true si la lista de Master Chief ya tiene (o ahora tiene) los asientos del medio.
bool add_middle_seats(uintptr_t halo1) {
    uintptr_t data = 0;
    uint32_t tag_count = 0;
    if (!safe_read(halo1 + k_tag_data_rva, &data, sizeof(data)) || !data ||
        !safe_read(data + 0x0C, &tag_count, sizeof(tag_count)) || tag_count == 0 || tag_count > 0x8000)
        return false;
    uintptr_t definition = 0;
    for (uint32_t i = 0; i < tag_count && !definition; ++i) {
        uint32_t entry[6] = {};
        if (!safe_read(data + 0x28 + i * k_tag_entry_size, entry, sizeof(entry))) return false;
        if (entry[0] != k_antr || entry[4] < k_tag_base || entry[5] < k_tag_base) continue;
        char name[sizeof(k_chief_animations)] = {};
        if (safe_read(data + (entry[4] - k_tag_base), name, sizeof(name)) &&
            memcmp(name, k_chief_animations, sizeof(name)) == 0)
            definition = data + (entry[5] - k_tag_base);
    }
    int32_t count = 0;
    uint32_t pointer = 0;
    if (!definition || !safe_read(definition + k_units_offset, &count, sizeof(count)) ||
        !safe_read(definition + k_units_offset + 4, &pointer, sizeof(pointer)) || count <= 0 ||
        count + 2 > int32_t(k_max_units) || pointer < k_tag_base)
        return false;
    static uint8_t units[k_max_units * k_unit_size];
    if (!safe_read(data + (pointer - k_tag_base), units, size_t(count) * k_unit_size)) return false;
    int left = -1, right = -1;
    for (int i = 0; i < count; ++i) {
        const char* label = reinterpret_cast<const char*>(units + i * k_unit_size);
        if (_stricmp(label, "P-riderLM") == 0 || _stricmp(label, "P-riderRM") == 0) return true;  // ya están
        if (_stricmp(label, "P-riderLF") == 0) left = i;
        if (_stricmp(label, "P-riderRF") == 0) right = i;
    }
    if (left < 0 || right < 0) return true;  // este mapa no tiene animaciones de Pelican: nada que hacer
    memcpy(units + count * k_unit_size, units + left * k_unit_size, k_unit_size);
    memcpy(units + (count + 1) * k_unit_size, units + right * k_unit_size, k_unit_size);
    memset(units + count * k_unit_size, 0, 32);
    memset(units + (count + 1) * k_unit_size, 0, 32);
    strcpy_s(reinterpret_cast<char*>(units + count * k_unit_size), 32, "P-riderLM");
    strcpy_s(reinterpret_cast<char*>(units + (count + 1) * k_unit_size), 32, "P-riderRM");
    uintptr_t buffer = units_buffer(data);
    if (!buffer) {
        log_write("J3/J4: no hay memoria cerca de los tags para los asientos del medio del Pelican");
        return true;  // no reintentar
    }
    int32_t new_count = count + 2;
    uint32_t new_pointer = uint32_t(buffer - data + k_tag_base);
    // Primero la lista (con las entradas de antes en su lugar), después el puntero y al final la cantidad: quien
    // la lea en medio ve una lista válida.
    if (!safe_write(buffer, units, size_t(new_count) * k_unit_size) ||
        !safe_write(definition + k_units_offset + 4, &new_pointer, sizeof(new_pointer)) ||
        !safe_write(definition + k_units_offset, &new_count, sizeof(new_count)))
        return false;
    log_write("J3/J4: Master Chief ya puede sentarse en los asientos del medio del Pelican");
    return true;
}

// Solo del hilo del juego, por seguidor y por aparecer: ya se intentó sentarlo (g_held está arriba).
bool g_seat_tried[4] = {};

void suspend(uint32_t unit, bool suspended) {
    SuspendFn fn = g_original_suspend ? g_original_suspend : reinterpret_cast<SuspendFn>(g_halo1.load() + k_suspend_rva);
    fn(unit, suspended ? 1 : 0);
}

// Desde el hilo del juego (coop_follow_game_tick). Devuelve true cuando terminó con este seguidor:
// - Compañero en un vehículo: se intenta (una vez) sentarlo en un asiento libre a su lado (Pelican: los del medio,
//   ver add_middle_seats). Si no se puede (todos ocupados, u otro vehículo), se lo congela (unit_suspended) y viaja
//   escondido dentro del vehículo hasta que el compañero se baje.
// - Compañero a pie: se suelta si estaba congelado y se pone a su costado.
bool arrive(int follower, int leader) {
    Halo1Player state[4];
    if (!halo1_players_read(state)) return true;
    if (!state[follower].alive) {  // murió o se fue: nada que hacer
        if (g_held[follower]) log_write("J3/J4: J%d murió mientras esperaba a J%d", follower + 1, leader + 1);
        return true;
    }
    if (!state[leader].alive) return false;
    uintptr_t leader_unit = halo1_player_biped(leader), follower_unit = halo1_player_biped(follower);
    uint32_t vehicle = k_none, follower_handle = biped_handle(follower);
    char seat[32];
    if (leader_unit && follower_unit && follower_handle != k_none && seat_of(leader_unit, &vehicle, seat)) {
        if (g_held[follower]) {
            // Viaja escondido con su compañero: donde aparecen J3/J4 en los mapas originales (por ejemplo, la playa
            // de The Silent Cartographer, adonde llegan los Pelican) los enemigos los matan antes de que lleguen.
            // El centro del cuerpo está siempre en coordenadas del mundo (la posición de alguien sentado, no).
            static const float k_still[3] = {};
            float position[3] = {state[leader].center[0], state[leader].center[1], state[leader].center[2] - 0.5f};
            safe_write(follower_unit + k_velocity_offset, k_still, sizeof(k_still));
            safe_write(follower_unit + k_position_offset, position, sizeof(position));
            return false;
        }
        if (g_seat_tried[follower]) {
            suspend(follower_handle, true);
            static const float k_still[3] = {};
            safe_write(follower_unit + k_velocity_offset, k_still, sizeof(k_still));
            g_held[follower] = true;
            log_write("J3/J4: J%d viaja escondido con J%d hasta que se baje (%s)", follower + 1, leader + 1, seat);
            return false;
        }
        g_seat_tried[follower] = true;
        const char* used = nullptr;
        if (seat_pair(seat) && seat_beside(follower, vehicle, seat, &used)) {
            log_write("J3/J4: J%d aparece y sube con J%d (%s): asiento %s", follower + 1, leader + 1, seat, used);
            return true;
        }
        log_write("J3/J4: J%d no cabe junto a J%d (%s): aparece a su lado cuando se baje", follower + 1, leader + 1,
                  seat);
        return false;  // en el próximo cuadro se congela
    }
    // A pie: si su compañero todavía no se movió, no hay lugar libre conocido; espera congelado hasta que camine.
    float spot[3];
    int beside = leader;
    if (!trail_spot(follower, leader, state, spot, &beside)) {
        if (!g_held[follower] && follower_handle != k_none) {
            suspend(follower_handle, true);
            g_held[follower] = true;
            log_write("J3/J4: J%d espera a que J1 o J2 se muevan para aparecer donde estuvieron", follower + 1);
        }
        return false;
    }
    if (g_held[follower] && follower_handle != k_none) {
        suspend(follower_handle, false);
        g_held[follower] = false;
    }
    if (place_at(follower, spot))
        log_write("J3/J4: J%d aparece junto a J%d (donde estuvo hace poco)", follower + 1, beside + 1);
    return true;
}

void spawn_watch() {
    char last_map[64] = {};
    uintptr_t last_halo1 = 0;
    bool placed[4] = {};
    uint32_t old_biped[4] = {k_none, k_none, k_none, k_none};  // los del mapa anterior: no cuentan como aparecer
    int reported_starts = -2;
    while (true) {
        Sleep(100);
        // Solo para leer (con safe_read): si halo1.dll no está, no hay misión.
        uintptr_t halo1 = reinterpret_cast<uintptr_t>(GetModuleHandleW(L"halo1.dll"));
        if (!halo1 || halo1 != g_halo1) continue;
        char map[64] = {};
        safe_read(halo1 + k_current_map_rva, map, sizeof(map) - 1);
        if (halo1 != last_halo1 || strcmp(map, last_map) != 0) {  // otro mapa: J3/J4 todavía no aparecieron
            last_halo1 = halo1;
            strcpy_s(last_map, map);
            memset(placed, 0, sizeof(placed));
            // El nombre cambia apenas empieza la carga, cuando todavía están los personajes del mapa anterior.
            for (int p = 0; p < 4; ++p) old_biped[p] = biped_handle(p);
            reported_starts = -2;
        }
        int players = mission_players();
        if (players < 3) continue;
        // Los tags del mapa nuevo llegan un poco después del nombre: se mira cada vez (es barato).
        int starts = scenario_player_starts(halo1);
        g_map_has_four = starts >= 4;
        if (starts >= 0 && starts != reported_starts) {
            reported_starts = starts;
            if (starts >= 4)
                log_write("J3/J4: %s ya trae puntos para J3/J4 (%d puntos de inicio): no se mueven por nuestra cuenta",
                          map, starts);
        }
        if (g_map_has_four) continue;
        Halo1Player state[4];
        if (!halo1_players_read(state)) continue;
        // Con la misión ya corriendo (sus tags cargados): asientos del medio del Pelican, revisados cada 2 s.
        static ULONGLONG seats_checked = 0;
        if (GetTickCount64() - seats_checked > 2000) {
            seats_checked = GetTickCount64();
            add_middle_seats(halo1);
        }
        for (int follower = 2; follower < players && follower < 4; ++follower) {
            if (placed[follower] || !state[follower].alive) continue;
            int leader = follower == 3 && state[1].alive ? 1 : 0;
            uint32_t mine = biped_handle(follower), theirs = biped_handle(leader);
            if (mine == k_none || mine == old_biped[follower] || theirs == old_biped[leader]) continue;
            // Subir a un vehículo cambia objetos del juego: se hace en su hilo, en el próximo cuadro.
            placed[follower] = true;
            g_arrive_leader[follower] = leader;
        }
    }
}

}  // namespace

void coop_follow_game_tick() {
    extra_vehicles_game_tick(g_halo1, mission_players());
    if (g_map_has_four || mission_players() < 3) return;
    Halo1Player state[4];
    if (halo1_players_read(state)) record_trails(state);
    for (int follower = 2; follower < 4; ++follower) {
        int leader = g_arrive_leader[follower];
        if (leader < 0) {
            g_seat_tried[follower] = g_held[follower] = false;
            continue;
        }
        if (arrive(follower, leader)) g_arrive_leader[follower].compare_exchange_strong(leader, -1);
    }
}

void coop_follow_start() {
    halo1_load_subscribe(&hook_all);  // en cada carga de halo1.dll (ver halo1_load.h)
    std::thread(spawn_watch).detach();
}
