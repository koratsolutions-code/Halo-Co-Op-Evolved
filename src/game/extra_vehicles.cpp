#include "extra_vehicles.h"
#include "halo1_players.h"
#include "log.h"

#include <windows.h>

#include <cmath>
#include <cstring>

namespace {

// Funciones de halo1.dll (MCC Steam 1.3528), del camino de object_create (motor +0xC577D4 -> +0xB3C6C8):
//   object_placement_data_new +0xB35EBC: rcx = datos de colocación (0x8C bytes), edx = tag, r8d = dueño (-1).
//   object_new +0xB35F80: rcx = datos de colocación; devuelve el handle (-1 si no pudo).
// Datos de colocación: +0x00 tag, +0x1C posición, +0x38 adelante, +0x44 arriba.
// Objeto (datos): +0x00 tag, +0x18 posición, +0x30 adelante, +0x3C arriba.
// Nombres de la misión: el escenario tiene sus nombres de objeto en +0x204 (cantidad, puntero de tags; entradas de
// 0x24 bytes con el nombre en +0) y el juego guarda el handle de cada nombre en el arreglo [halo1+0x2D9CDE8].
constexpr uintptr_t k_placement_new_rva = 0xB35EBC, k_object_new_rva = 0xB35F80;
constexpr uint8_t k_placement_new_bytes[] = {0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x74, 0x24, 0x10, 0x57};
constexpr uint8_t k_object_new_bytes[] = {0x48, 0x83, 0xEC, 0x28, 0x8A, 0x05, 0xA6, 0x56, 0x04, 0x01, 0x4C, 0x8B, 0xC9};
constexpr uintptr_t k_names_rva = 0x2D9CDE8, k_tag_data_rva = 0x2B22F50, k_current_map_rva = 0x2B23745;
constexpr uint32_t k_tag_base = 0x50000000, k_scnr = 0x73636E72, k_none = 0xFFFFFFFF;
constexpr size_t k_object_names_offset = 0x204, k_object_name_size = 0x24, k_max_names = 0x280;

using PlacementNewFn = void(__fastcall*)(void*, uint32_t, uint32_t);
using ObjectNewFn = uint32_t(__fastcall*)(void*);

bool safe_read(uintptr_t address, void* out, size_t size) {
    SIZE_T read = 0;
    return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(address), out, size, &read) && read == size;
}

// Índice del nombre de objeto en el escenario de la misión cargada (-1 si no está, o si los tags cargados todavía
// son los de la misión anterior: el nombre del mapa cambia apenas empieza la carga).
int name_index(uintptr_t halo1, const char* map, const char* wanted) {
    uintptr_t data = 0;
    uint32_t scenario_id = 0, entry[6] = {}, pointer = 0;
    int32_t count = 0;
    if (!safe_read(halo1 + k_tag_data_rva, &data, sizeof(data)) || !data ||
        !safe_read(data + 4, &scenario_id, sizeof(scenario_id)) ||
        !safe_read(data + 0x28 + (scenario_id & 0xFFFF) * 0x20, entry, sizeof(entry)) || entry[0] != k_scnr ||
        entry[5] < k_tag_base)
        return -1;
    char scenario_name[64] = {};
    if (entry[4] < k_tag_base || !safe_read(data + (entry[4] - k_tag_base), scenario_name, sizeof(scenario_name) - 1) ||
        _stricmp(scenario_name, map) != 0)
        return -1;
    uintptr_t scenario = data + (entry[5] - k_tag_base);
    if (!safe_read(scenario + k_object_names_offset, &count, sizeof(count)) ||
        !safe_read(scenario + k_object_names_offset + 4, &pointer, sizeof(pointer)) || count <= 0 ||
        count > int32_t(k_max_names) || pointer < k_tag_base)
        return -1;
    for (int32_t i = 0; i < count; ++i) {
        char name[32] = {};
        if (safe_read(data + (pointer - k_tag_base) + i * k_object_name_size, name, sizeof(name) - 1) &&
            strcmp(name, wanted) == 0)
            return i;
    }
    return -1;
}

uint32_t named_object(uintptr_t halo1, int index) {
    uintptr_t names = 0;
    uint32_t handle = k_none;
    if (index < 0 || !safe_read(halo1 + k_names_rva, &names, sizeof(names)) || !names) return k_none;
    safe_read(names + size_t(index) * 4, &handle, sizeof(handle));
    return handle;
}

bool functions_ok(uintptr_t halo1) {
    uint8_t a[sizeof(k_placement_new_bytes)], b[sizeof(k_object_new_bytes)];
    return safe_read(halo1 + k_placement_new_rva, a, sizeof(a)) && memcmp(a, k_placement_new_bytes, sizeof(a)) == 0 &&
           safe_read(halo1 + k_object_new_rva, b, sizeof(b)) && memcmp(b, k_object_new_bytes, sizeof(b)) == 0;
}

// Copia del vehículo (mismo tag y orientación) en otra posición. Solo desde el hilo del juego.
uint32_t clone_vehicle(uintptr_t halo1, uint32_t source, const float position[3]) {
    uintptr_t object = halo1_object(source);
    uint32_t tag = 0;
    float forward[3], up[3];
    if (!object || !functions_ok(halo1) || !safe_read(object, &tag, sizeof(tag)) ||
        !safe_read(object + 0x30, forward, sizeof(forward)) || !safe_read(object + 0x3C, up, sizeof(up)))
        return k_none;
    alignas(16) uint8_t placement[0x90] = {};
    reinterpret_cast<PlacementNewFn>(halo1 + k_placement_new_rva)(placement, tag, k_none);
    memcpy(placement + 0x1C, position, sizeof(float) * 3);
    memcpy(placement + 0x38, forward, sizeof(forward));
    memcpy(placement + 0x44, up, sizeof(up));
    return reinterpret_cast<ObjectNewFn>(halo1 + k_object_new_rva)(placement);
}

bool position_of(uint32_t handle, float out[3]) {
    uintptr_t object = halo1_object(handle);
    return object && safe_read(object + 0x18, out, sizeof(float) * 3);
}

// Estado por misión (solo del hilo del juego).
struct Mission {
    char map[64] = {};
    int jeep = -1, fly_away_1 = -1, fly_away_2 = -1;  // índices de nombre
    ULONGLONG jeep_dropped_at = 0;                     // 0: todavía no lo soltó la nave
    bool jeep_done = false;
    // De qué vehículo ya se hicieron copias: al reiniciar la misión el guion crea otro (otro handle) y se copia de
    // nuevo; al volver a un checkpoint vuelve el mismo, con sus copias.
    uint32_t jeep_source = k_none, banshee_source = k_none;
    uint32_t jeep_carried = k_none;  // el Warthog que se vio dentro de la nave
} g_mission;

}  // namespace

void extra_vehicles_game_tick(uintptr_t halo1, int players) {
    if (!halo1 || players < 3) return;
    char map[64] = {};
    safe_read(halo1 + k_current_map_rva, map, sizeof(map) - 1);
    static uintptr_t last_halo1 = 0;
    if (halo1 != last_halo1 || strcmp(map, g_mission.map) != 0) {  // otra misión: se buscan sus nombres de nuevo
        last_halo1 = halo1;
        g_mission = Mission{};
        strcpy_s(g_mission.map, map);
    }
    Mission& m = g_mission;
    // Los nombres se buscan hasta encontrarlos, ya con los tags de esta misión cargados.
    if (m.jeep < 0 && strcmp(map, "levels\\a30\\a30") == 0) {
        m.jeep = name_index(halo1, map, "jeep");
        if (m.jeep >= 0) log_write("Vehículos: Halo, Warthog de la misión = nombre %d", m.jeep);
    }
    // Halo: el Warthog viaja en la nave ("cargo": su vehículo padre en +0xD8) y el guion lo suelta. Además del aviso del
    // guion (extra_vehicles_on_script_exit) se mira aquí: cuando estaba en la nave y deja de estarlo, ya se soltó.
    if (m.jeep >= 0 && !m.jeep_dropped_at) {
        uint32_t jeep = named_object(halo1, m.jeep), parent = k_none;
        uintptr_t object = jeep != k_none && jeep != m.jeep_source ? halo1_object(jeep) : 0;
        if (object && safe_read(object + 0xD8, &parent, sizeof(parent))) {
            if (parent != k_none && m.jeep_carried != jeep) {
                m.jeep_carried = jeep;
                log_write("Vehículos: Halo, el Warthog viaja en la nave");
            } else if (parent == k_none && m.jeep_carried == jeep) {
                extra_vehicles_on_script_exit(halo1, jeep);
            }
        }
    }
    if (m.fly_away_2 < 0 && strcmp(map, "levels\\c40\\c40") == 0) {
        m.fly_away_2 = name_index(halo1, map, "fly_away_2");
        m.fly_away_1 = name_index(halo1, map, "fly_away_1");
    }
    // Halo: 3 s después de que la nave suelta el Warthog (ya en el suelo), otro a 3,5 m a su costado.
    if (players >= 4 && m.jeep >= 0 && !m.jeep_done && m.jeep_dropped_at &&
        GetTickCount64() - m.jeep_dropped_at > 3000) {
        m.jeep_done = true;
        uint32_t jeep = named_object(halo1, m.jeep);
        uintptr_t object = halo1_object(jeep);
        float position[3], forward[3], up[3];
        if (object && position_of(jeep, position) && safe_read(object + 0x30, forward, sizeof(forward)) &&
            safe_read(object + 0x3C, up, sizeof(up))) {
            float right[3] = {forward[1] * up[2] - forward[2] * up[1], forward[2] * up[0] - forward[0] * up[2],
                              forward[0] * up[1] - forward[1] * up[0]};
            float spot[3] = {position[0] - right[0] * 3.5f, position[1] - right[1] * 3.5f,
                             position[2] - right[2] * 3.5f + 0.5f};
            uint32_t copy = clone_vehicle(halo1, jeep, spot);
            log_write(copy != k_none ? "Vehículos: segundo Warthog para 4 jugadores"
                                     : "Vehículos: no se pudo crear el segundo Warthog");
        } else {
            log_write("Vehículos: no se encontró el Warthog soltado (handle %08X)", jeep);
        }
    }
    // Two Betrayals: cuando existe el segundo Banshee, uno más por cada jugador extra, en la misma fila.
    if (m.fly_away_2 >= 0) {
        uint32_t second = named_object(halo1, m.fly_away_2);
        float p2[3];
        if (second != k_none && second != m.banshee_source && position_of(second, p2)) {
            m.banshee_source = second;
            float p1[3], step[3] = {0, 2.2f, 0};
            uint32_t first = named_object(halo1, m.fly_away_1);
            if (first != k_none && position_of(first, p1)) {
                for (int i = 0; i < 3; ++i) step[i] = p2[i] - p1[i];
                float length = std::sqrt(step[0] * step[0] + step[1] * step[1]);
                if (length < 1.5f || length > 8.0f) step[0] = 0, step[1] = 2.2f, step[2] = 0;
            }
            int made = 0;
            for (int n = 1; n <= players - 2; ++n) {
                float spot[3] = {p2[0] + step[0] * n, p2[1] + step[1] * n, p2[2] + step[2] * n + 0.3f};
                if (clone_vehicle(halo1, second, spot) != k_none) ++made;
            }
            log_write("Vehículos: %d Banshee extra para %d jugadores", made, players);
        }
    }
}

void extra_vehicles_on_script_exit(uintptr_t halo1, uint32_t unit) {
    Mission& m = g_mission;
    if (halo1 && m.jeep >= 0 && unit != m.jeep_source && unit == named_object(halo1, m.jeep)) {
        m.jeep_source = unit;
        m.jeep_dropped_at = GetTickCount64();
        m.jeep_done = false;
        log_write("Vehículos: Halo, la nave soltó el Warthog");
    }
}
