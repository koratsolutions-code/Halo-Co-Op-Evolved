#include "skins.h"
#include "halo1_players.h"
#include "log.h"

#include <windows.h>

#include <MinHook.h>

#include <atomic>
#include <cstring>
#include <mutex>
#include <thread>

namespace {

constexpr uintptr_t k_spawn_rva = 0xAD4184;
constexpr uint8_t k_spawn_prologue[] = {0x48, 0x8B, 0xC4, 0x48, 0x89, 0x58, 0x10, 0x48, 0x89, 0x70, 0x18, 0x48,
                                        0x89, 0x78, 0x20, 0x55, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57};
constexpr uintptr_t k_tag_list_rva = 0x1C34FB0;
constexpr uintptr_t k_tag_data_rva = 0x2B22F50;
constexpr uint32_t k_tag_base = 0x50000000;
constexpr size_t k_entry_size = 0x20;
constexpr uint32_t k_bipd = 0x62697064, k_mod2 = 0x6D6F6432, k_antr = 0x616E7472;
constexpr char k_chief[] = "characters\\cyborg\\cyborg";

// Prueba: skin fija por jugador (0 = sin skin).
const char* const k_test_skins[4] = {nullptr, "characters\\marine_armored\\marine_armored", "characters\\marine\\marine",
                                     "characters\\crewman\\crewman"};

std::atomic<bool> g_enabled{false};
uintptr_t g_halo1 = 0;

bool safe_read(uintptr_t address, void* out, size_t size) {
    SIZE_T read = 0;
    return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(address), out, size, &read) &&
           read == size;
}

template <typename T>
T read_as(uintptr_t address, T fallback = T{}) {
    T value = fallback;
    safe_read(address, &value, sizeof(T));
    return value;
}

bool safe_write(uintptr_t address, const void* data, size_t size) {
    SIZE_T written = 0;
    return WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(address), data, size, &written) &&
           written == size;
}

// Tags de la misión cargada.
struct Tags {
    uintptr_t list = 0;
    size_t count = 0;
    uintptr_t base = 0;  // dirección = base + puntero de 32 bits
    uintptr_t at(uint32_t pointer) const { return base + (pointer - k_tag_base); }
    uintptr_t entry(size_t index) const { return list + index * k_entry_size; }
};

bool read_tags(Tags& tags) {
    tags.list = read_as<uintptr_t>(g_halo1 + k_tag_list_rva);
    uintptr_t data = read_as<uintptr_t>(g_halo1 + k_tag_data_rva);
    if (!tags.list || !data || tags.list != data + 0x28) return false;
    tags.base = data;
    tags.count = 0;
    while (tags.count < 65535 && (read_as<uint32_t>(tags.entry(tags.count) + 12, 0xFFFFFFFF) & 0xFFFF) == tags.count)
        ++tags.count;
    return tags.count > 0;
}

uintptr_t find_tag(const Tags& tags, uint32_t cls, const char* name) {
    char text[128];
    size_t length = strlen(name) + 1;
    for (size_t i = 0; i < tags.count; ++i) {
        uintptr_t entry = tags.entry(i);
        if (read_as<uint32_t>(entry) != cls) continue;
        if (safe_read(tags.at(read_as<uint32_t>(entry + 0x10)), text, length) && memcmp(text, name, length) == 0)
            return entry;
    }
    return 0;
}

// Referencia (clase, nombre, 0, ID) a un tag de esa clase dentro de los datos de un personaje.
uintptr_t find_reference(const Tags& tags, uintptr_t data, uint32_t cls) {
    for (size_t offset = 0; offset < 0x200; offset += 4) {
        if (read_as<uint32_t>(data + offset) != cls) continue;
        uint32_t id = read_as<uint32_t>(data + offset + 12);
        if ((id & 0xFFFF) >= tags.count) continue;
        uintptr_t entry = tags.entry(id & 0xFFFF);
        if (read_as<uint32_t>(entry) == cls && read_as<uint32_t>(entry + 12) == id) return data + offset;
    }
    return 0;
}

// Modelo y animaciones de un personaje (32 bytes: las dos referencias completas).
struct Look {
    uint8_t model[16];
    uint8_t animations[16];
};

bool read_look(const Tags& tags, uintptr_t entry, uintptr_t* model_at, uintptr_t* animations_at, Look& look) {
    uintptr_t data = tags.at(read_as<uint32_t>(entry + 0x14));
    uintptr_t model = find_reference(tags, data, k_mod2), animations = find_reference(tags, data, k_antr);
    if (model_at) *model_at = model;
    if (animations_at) *animations_at = animations;
    return model && animations && safe_read(model, look.model, 16) && safe_read(animations, look.animations, 16);
}

// Lo que hace falta de la misión actual; se rehace cuando cambia la lista de tags.
struct Mission {
    uintptr_t list = 0;
    size_t count = 0;
    uintptr_t chief_model = 0, chief_animations = 0;  // dónde escribir
    Look chief{};                                       // lo de Master Chief, para devolverlo
    bool has_skin[4] = {};
    Look skins[4]{};
};
std::mutex g_mission_mutex;
Mission g_mission;

bool prepare_mission() {
    Tags tags;
    if (!read_tags(tags)) return false;
    if (g_mission.list == tags.list && g_mission.count == tags.count && g_mission.chief_model) return true;
    Mission m;
    m.list = tags.list;
    m.count = tags.count;
    uintptr_t chief = find_tag(tags, k_bipd, k_chief);
    if (!chief || !read_look(tags, chief, &m.chief_model, &m.chief_animations, m.chief)) {
        log_write("Skins: no se encontró Master Chief en la lista de tags");
        return false;
    }
    for (int p = 0; p < 4; ++p) {
        if (!k_test_skins[p]) continue;
        uintptr_t entry = find_tag(tags, k_bipd, k_test_skins[p]);
        m.has_skin[p] = entry && read_look(tags, entry, nullptr, nullptr, m.skins[p]);
        log_write("Skins: J%d %s %s", p + 1, k_test_skins[p], m.has_skin[p] ? "disponible" : "no está en la misión");
    }
    g_mission = m;
    return true;
}

// Qué jugador va a aparecer: algún argumento es el índice del jugador (16 bits bajos 0-3 y los 16 altos iguales a
// la "sal" que está al principio de su registro).
int player_of(const uint64_t* args, int count, int* which) {
    for (int i = 0; i < count; ++i) {
        uint32_t value = uint32_t(args[i]);
        int index = value & 0xFFFF;
        if (index > 3) continue;
        uintptr_t record = halo1_player_record(index);
        if (record && read_as<uint16_t>(record) == uint16_t(value >> 16)) {
            *which = i;
            return index;
        }
    }
    return -1;
}

using SpawnFn = uint64_t(__fastcall*)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
SpawnFn g_original_spawn = nullptr;

// Mismos 8 argumentos enteros que recibe (4 en registros, 4 en la pila) y se los pasa tal cual al original.
uint64_t __fastcall spawn_hook(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6,
                               uint64_t a7, uint64_t a8) {
    if (!g_enabled) return g_original_spawn(a1, a2, a3, a4, a5, a6, a7, a8);
    uint64_t args[4] = {a1, a2, a3, a4};
    int which = -1;
    int player = player_of(args, 4, &which);
    static int logged = 0;
    if (logged < 12) {
        ++logged;
        log_write("Skins: aparición · argumentos %llX %llX %llX %llX · jugador %d (argumento %d)",
                  (unsigned long long)a1, (unsigned long long)a2, (unsigned long long)a3, (unsigned long long)a4,
                  player + 1, which + 1);
    }
    std::lock_guard lock(g_mission_mutex);
    bool swapped = false;
    if (player >= 0 && prepare_mission() && g_mission.has_skin[player]) {
        swapped = safe_write(g_mission.chief_model, g_mission.skins[player].model, 16) &&
                  safe_write(g_mission.chief_animations, g_mission.skins[player].animations, 16);
    }
    uint64_t result = g_original_spawn(a1, a2, a3, a4, a5, a6, a7, a8);
    if (swapped) {
        safe_write(g_mission.chief_model, g_mission.chief.model, 16);
        safe_write(g_mission.chief_animations, g_mission.chief.animations, 16);
        log_write("Skins: J%d apareció con %s", player + 1, k_test_skins[player]);
    }
    return result;
}

bool f10_pressed() {
    static bool was_down = false;
    bool down = (GetAsyncKeyState(VK_F10) & 0x8000) != 0;
    bool pressed = down && !was_down;
    was_down = down;
    if (!pressed) return false;
    DWORD pid = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &pid);
    return pid == GetCurrentProcessId();
}

}  // namespace

void skins_start() {
    g_halo1 = reinterpret_cast<uintptr_t>(GetModuleHandleW(L"halo1.dll"));
    std::thread([] {
        // halo1.dll se carga al abrir Halo CE desde el menú de MCC: esperarlo.
        while (!g_halo1) {
            Sleep(1000);
            g_halo1 = reinterpret_cast<uintptr_t>(GetModuleHandleW(L"halo1.dll"));
        }
        void* spawn = reinterpret_cast<void*>(g_halo1 + k_spawn_rva);
        uint8_t prologue[sizeof(k_spawn_prologue)];
        if (!safe_read(uintptr_t(spawn), prologue, sizeof(prologue)) ||
            memcmp(prologue, k_spawn_prologue, sizeof(prologue)) != 0) {
            log_write("Skins: esta versión de MCC no coincide con la esperada, skins desactivadas");
            return;
        }
        MH_STATUS status = MH_CreateHook(spawn, reinterpret_cast<void*>(&spawn_hook),
                                         reinterpret_cast<void**>(&g_original_spawn));
        if (status == MH_OK) status = MH_EnableHook(spawn);
        log_write("Skins: %s", status == MH_OK ? "listas (F10 activa la prueba)" : MH_StatusToString(status));
        while (true) {
            Sleep(50);
            if (!f10_pressed()) continue;
            g_enabled = !g_enabled;
            log_write("Skins: prueba %s", g_enabled ? "ACTIVADA (J2 marine con armadura, J3 marine, J4 tripulante)"
                                                    : "desactivada");
        }
    }).detach();
}
